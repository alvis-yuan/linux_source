#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include "libcommon.h"
#include "glbvar.h"
#include "arg.h"
#include "printer.h"
#include "print_queue.h"
#include "serial.h"
#include "simple_timer.h"
#include "tcp.h"
#include "tcpserver.h"
#include "usb.h"
#include "print_server.h"
#include "tspl_api.h"

#define PRINT_SERVER_PID_FILE "/tmp/PrintServer.pid"

enum {
	APP_RC_SUCCESS = 0,
	APP_RC_1ST_FORK_FAIL = 11,
	APP_RC_SETSID_FAIL,
	APP_RC_2ND_FORK_FAIL,
	APP_RC_CLOSE_FAIL,
	APP_RC_CHDIR_FAIL,
	APP_RC_EXCEPTION
};

static sigset_t sig_excset;
static sigset_t sig_onset;

static void app_sig_exception(int sig)
{
	switch (sig) {
	case SIGABRT: LogFatal("Got SIGABRT"); break;
	case SIGFPE:  LogFatal("Got SIGFPE");  break;
	case SIGILL:  LogFatal("Got SIGILL");  break;
	case SIGPIPE: LogFatal("Got SIGPIPE"); break;
	case SIGSEGV: LogFatal("Got SIGSEGV"); break;
	default:      LogFatal("Got SIG%02d", sig);
	}

	print_queue_exit();
}

static void app_sig_exit(int sig)
{
	switch (sig) {
	case SIGINT:  LogInfo("Got SIGINT");  break;
	case SIGQUIT: LogInfo("Got SIGQUIT"); break;
	case SIGTERM: LogInfo("Got SIGTERM"); break;
	default:      LogInfo("Got SIG%02d", sig);
	}

	print_queue_exit();
}

/*! \fn void init_daemon(void);
 *  \brief Make the process a daemon process.
 */
static void init_daemon(void)
{
	pid_t pid;

	/*
	 * Detach from the parent process.
	 */
	if ((pid = fork()) < 0) /* fork() failed */
		exit(APP_RC_1ST_FORK_FAIL);
	else if (pid > 0) /* the parent process */
		exit(APP_RC_SUCCESS);

	/*
	 * Create a new session.
	 */
	if (setsid() == -1)
		exit(APP_RC_SETSID_FAIL);

	/*
	 * Prevent the process from opening new controlling tty
	 * by making it not to be the session leader.
	 */
	if ((pid = fork()) < 0) /* fork() failed */
		exit(APP_RC_2ND_FORK_FAIL);
	else if (pid > 0) /* the parent process */
		exit(APP_RC_SUCCESS);

	/*
	 * Close stdin, stdout and stderr.
	 */
	if (close(0) || close(1) || close(2))
		exit(APP_RC_CLOSE_FAIL);

	/*
	 * Change the current directory to "/tmp/".
	 */
	if (chdir("/tmp/"))
		exit(APP_RC_CHDIR_FAIL);

	/*
	 * Clear the file mode creation mask.
	 */
	umask(0);
}

static int pidfile_create(void)
{
	int fd, size;
	char str[8];

	size = sprintf(str, "%d", (int)getpid());
	fd = open(PRINT_SERVER_PID_FILE, O_RDWR|O_CREAT|O_TRUNC, 0664);
	if (fd == -1)
		return -1;
	write(fd, str, size);
	close(fd);
	return 0;
}

static void pidfile_delete(void)
{
	unlink(PRINT_SERVER_PID_FILE);
}

static int signal_register(sigset_t *set, void (*handler)(int), int flags, int sig, ...)
{
	struct sigaction sigact;
	va_list ap;
	int i, rc = -1;

	sigemptyset(set);

	memset(&sigact, 0, sizeof(sigact));
	sigact.sa_handler = handler;
	sigfillset(&sigact.sa_mask);
	sigact.sa_flags = flags;

	if (sigaction(sig, &sigact, NULL))
		return -1;
	if (sigaddset(set, sig))
		return -1;

	va_start(ap, sig);
	while ((i = va_arg(ap, int)) != -1) {
		if (sigaction(i, &sigact, NULL))
			break;
		if (sigaddset(set, i))
			break;
	}
	if (i == -1)
		rc = 0;
	va_end(ap);

	return rc;
}

static void get_bad_point(void)
{
	LogDbg("start");

	int rt = 0, count;
	char read_BadPointInf[512] = {};
	char buff[256] = {};
	unsigned from  = 0;
	unsigned bytes = 0;
	badpoint_info_hdr_t hdr = {};
	char debug_buf[512];

	int sz=sizeof(buff),len = badpoint_info_from_file(read_BadPointInf,sizeof(read_BadPointInf),&hdr);
	memset(debug_buf,0,sizeof(debug_buf));
	if( len<=0 ) {
		hdr.offset = -1; //scan position
		hdr.chk_dots = 0; //scan dots
		len = sys_global_var()->prt_dots_per_line/8;
		LogError("get badpoints from file fail,do full scan!");
	} else {
		LogWarn("get badpoints from file OK,offset=%d,dots=%d",hdr.offset,hdr.chk_dots);
	}
	from  = (unsigned)hdr.offset;
	bytes = (unsigned)hdr.chk_dots;

	do {
		rt = print_get_bad_points(from, bytes, buff, (unsigned *)&sz); //len is suppose to be PRINTER_DOT_PER_LINE/8

		LogDbg("rt=%d, size=%d", rt, sz);
		if( rt<0 ) {
			SysDelay(2000);
			continue;
		}
		if( rt != PRN_BAD_POINT_DETECT_FINISHED ) {
			SysDelay(600);
			continue;
		}

		if( sz>len )
			LogError("size(%d) mismatch,expect %d,Please check!",sz,len);
		if( hdr.offset<0 ) {
			hdr.offset = 0; // set next scan position
			hdr.chk_dots = BAD_POINT_CHECK_NUM;
			LogDbg("full scan save bad point inf, dots_num=%d",len*8);
			count = 0;
			for(int i=0; i<len; i++)
			{
				count += sprintf(debug_buf + count, "%02X", (uint8_t)buff[i]);
			}
			LogDbg("full scan save bad point inf:%s",debug_buf);
			badpoint_info_save_file(buff,len,&hdr);
		} else {
			LogDbg("part scan save bad point inf, offset=%d ,dots_num=%d",hdr.offset, hdr.chk_dots);
			count = 0;
			for(int i=0; i<hdr.chk_dots/8; i++)
			{
				count += sprintf(debug_buf + count, "%02X", (uint8_t)(*(buff+(hdr.offset>>3)+i)));
			}
			LogDbg("part scan save bad point inf:%s",debug_buf);
			memcpy(read_BadPointInf + (hdr.offset>>3),buff+(hdr.offset>>3), hdr.chk_dots>>3);
			hdr.offset = (hdr.offset+BAD_POINT_CHECK_NUM)%PRINTER_DOT_PER_LINE;
			badpoint_info_save_file(read_BadPointInf,len,&hdr);
		}
	}while( rt!=PRN_BAD_POINT_DETECT_FINISHED );

	LogDbg("end");
}

#include "Runtime_Data.h"

static bool g_tspl_debugmode = 0;
static int  (*tspl_sdk_init_func)(tspl_sdk_event_callback cb, uint32_t dots_per_line) = NULL;
static void (*tspl_parser_func)(char *buf, uint32_t buf_len, char *file) = NULL;
static void (*tspl_swap_int32_func)(uint32_t *word, uint32_t len) = NULL;
static void (*tspl_sdk_destroy_func)(void) = NULL;

static void tspl_sdk_event_handler(tspl_sdk_event_t *event)
{
#define OPEN_HIGHQUALITY_PRINT() do { \
	uint8_t buf[4]; \
	buf[0] = 0x0B; \
	buf[1] = 1; \
	printer_send_command(buf, 2); \
} while (0)
#define CLOSE_HIGHQUALITY_PRINT() do { \
	uint8_t buf[4]; \
	buf[0] = 0x0B; \
	buf[1] = 0; \
	printer_send_command(buf, 2); \
} while (0)

	switch (event->id)
	{
	case TSPL_EVENT_SIZE:
		if (event->status) {
			// printServer.width = event->data->two_param.m;
			// printServer.height = event->data->two_param.n;
		}
		break;
	case TSPL_EVENT_GAP:
		if (event->status) {
			// printServer.gap_betweenPaper = event->data->two_param.m; // 两张纸的间距
			// printServer.gap_offset = event->data->two_param.n; // 不清楚这个参数是指什么
		}
		break;
	case TSPL_EVENT_BLINE:
		if (event->status) {
			// printServer.bline_height = event->data->two_param.m;
			// printServer.bline_offset = event->data->two_param.n;
		}
		break;
	case TSPL_EVENT_OFFSET:
		if (event->status) {
			// printServer.offset = event->data->one_param.n;
		}
		break;
	case TSPL_EVENT_SPEED:
		if (event->status) {
			// printServer.speed = event->data->one_param.n;
			uint8_t buf[4];
			buf[0] = 0x09;
			*((uint16_t *)(buf + 1)) = event->data->one_param.n; // 设置速度0-65535 一般500就很慢了
			printer_send_command(buf, 3);
			LogInfo("speed=%f", event->data->one_param.n);
		}
		break;
	case TSPL_EVENT_DENSITY:
		if (event->status) {
			// printServer.density = event->data->one_param.n;
		}
		break;
	case TSPL_EVENT_DIRECTION:
		if (event->status) {
			// printServer.direction = event->data->one_param.n;
		}
		break;
	case TSPL_EVENT_FEED:
		if (event->status) {
			// printServer.feed = event->data->one_param.n;
		}
		break;
	case TSPL_EVENT_FORMFEED:
		if (event->status) {
		}
		break;
	case TSPL_EVENT_HOME:
		if (event->status) {
		}
		break;
	case TSPL_EVENT_PRINT:
		if (event->status) {
			//TODO: 将拿出来的缓冲用于打印
			uint32_t height = event->data->zero_param.BitMap.validHeight;
			uint32_t lines, bytes;
			uint8_t *bitmap = (uint8_t *)malloc(BYTES_PER_LINE * height);
			uint8_t *bitmap_origin = bitmap;

			memcpy(bitmap, event->data->zero_param.BitMap.bitmap, BYTES_PER_LINE * height);
			tspl_swap_int32_func((uint32_t *)bitmap, WORDS_PER_LINE * height);
			LogInfo("pageBuffer->height=%d", height);

			OPEN_HIGHQUALITY_PRINT();
			while (height > 0) {
				lines = (height > 8) ? 8 : height;
				bytes = BYTES_PER_LINE * lines;
				if (PrnDotLine(bitmap, bytes, PRN_DATA_MONO) == PRN_BUF_FULL) {
					while (1) {
						SysDelay(100);
						if (PrnDotLine(NULL, 0, PRN_DATA_MONO) != PRN_BUF_FULL)
							break;
					}
				}
				bitmap += bytes;
				height -= lines;
			}
			CLOSE_HIGHQUALITY_PRINT();

			free(bitmap_origin);
		}
		break;
	case TSPL_EVENT_CUT:
		if (event->status) {
		}
		break;
	case TSPL_EVENT_LIMITFEED:
		if (event->status) {
			// printServer.limitfeed_dotline = event->data->one_param.n * 8;
		}
		break;
	case TSPL_EVENT_DEBUG:
		if (event->status)
			g_tspl_debugmode = event->data->one_param.n;
		LogInfo("g_tspl_debugmode=%d", g_tspl_debugmode);
		break;
	case TSPL_EVENT_ERROR:
		if (event->status) {
			LogError("%s", event->data->err_info.errtext);
			LogError("%s", event->data->err_info.errline);
		}
		break;
	case TSPL_EVENT_SLEEP:
		if (event->status)
			SysDelay(event->data->one_param.n);
		break;
	default:
		break;
	}
#undef OPEN_HIGHQUALITY_PRINT
#undef CLOSE_HIGHQUALITY_PRINT
}

static void *tspl_thread(void *arg)
{
	struct mq_attr attr = {0,10,256,0};
	mqd_t mq_id;
	char name[256];
	void *tspl_handle = NULL;
	char *error;
	mq_id = mq_open(MQ_TSPL, O_CREAT | O_RDONLY, 0666, &attr);

	mq_receive(mq_id, name, 256, (uint32_t *)0);

	tspl_handle = dlopen("/home/lib/libtspl.so", RTLD_NOW);
	if (!tspl_handle) {
		LogInfo("libtspl.so not available.");
		goto _fail;
	}

#define RESOLVE(handle__,name__) \
do { \
	dlerror(); \
	name__##_func = (typeof(name__##_func))dlsym(handle__, #name__); \
	if (name__##_func == NULL || (error = dlerror()) != NULL) { \
		LogError("Failed to resolve " #name__); \
		goto _fail; \
	} \
} while (0)

	RESOLVE(tspl_handle, tspl_sdk_init);
	RESOLVE(tspl_handle, tspl_parser);
	RESOLVE(tspl_handle, tspl_swap_int32);
	RESOLVE(tspl_handle, tspl_sdk_destroy);

#define TSPLPARSER_FILE(file) tspl_parser_func(NULL, 0, file)

	tspl_sdk_init_func(tspl_sdk_event_handler, DOTS_PER_LINE);
	LogDbg("name=%s", name);
	TSPLPARSER_FILE(name);
	if (!g_tspl_debugmode)
		unlink(name);

	while (g_process_running)
	{
		mq_receive(mq_id, name, 256, (uint32_t *)0);
		LogDbg("name=%s", name);
		TSPLPARSER_FILE(name);
		if (!g_tspl_debugmode)
			unlink(name);
	}

_fail:
	if (tspl_sdk_destroy_func != NULL)
		tspl_sdk_destroy_func();
	mq_close(mq_id);
	mq_unlink(MQ_TSPL);

	return NULL;

#undef RESOLVE
#undef TSPLPARSER_FILE
}

static void tspl_parser_run(void)
{
	int iRet = -1;
	pthread_t tid;
	if ((iRet = pthread_create(&tid, NULL, tspl_thread, NULL)))
		LogError("error return %d", iRet);
	pthread_detach(tid);
}

/*----------------------------------------------*
 | PROGRAM ENTRY                                |
 *----------------------------------------------*/
int main(int argc, char *argv[])
{
	arg_parse(argc, argv);

	LogDbg("sizeof(print_shm_t)=%u", sizeof(print_shm_t));

	if (arg_param.daemon)
		init_daemon();

	if (signal_register(&sig_excset, app_sig_exception, SA_RESETHAND,
			SIGABRT, SIGFPE, SIGILL, SIGPIPE, SIGSEGV, -1)) {
		LogFatal("Registering signal handler failed.");
		exit(1);
	}

	if (signal_register(&sig_onset, app_sig_exit, 0,
			SIGINT, SIGQUIT, SIGTERM, -1)) {
		LogFatal("Registering signal handler failed.");
		exit(1);
	}

	pthread_sigmask(SIG_UNBLOCK, &sig_onset, NULL);

	if (glbvar_init())
		exit(1);

	if (simple_timer_init())
		exit(1);

	if (printer_init())
		exit(1);

	if (print_queue_init())
		exit(1);

	usb_init();
	tcp_server_init();
	tcp_init();
	serial_init();

	get_bad_point();

	pidfile_create();

	tspl_parser_run();
	print_queue_run();

	pidfile_delete();

	return 0;
}
