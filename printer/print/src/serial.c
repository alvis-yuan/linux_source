#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include "libcommon.h"
#include "glbvar.h"
#include "Util.h"
#include "serial.h"

#define SERIAL_ENABLED 1

#if (SERIAL_ENABLED)

#define SERIAL_NODE				"/dev/ttyUSB0"
#define SERIAL_SECTION			"serial"
#define SERIAL_KEY_BAUDRATE		"BaudRate"
#define SERIAL_KEY_DATABITS		"DataBits"
#define SERIAL_KEY_STOPBITS		"StopBits"
#define SERIAL_KEY_PARITY		"Parity"

serial_settings_t serial_settings;

static uint8_t serial_buffer[4096];
static pthread_t m_tid;
static int serial_fd = -1, settings_changed = 0;

#define WRITE_LOG_TO_FILE 0

#if (WRITE_LOG_TO_FILE)

static FILE *logfile;
static char logbuf[1024];

static void logfile_init(void)
{
	logfile = fopen("/tmp/serial.log", "w+");
}

static void logfile_write(const char *fmt, ...)
{
	va_list ap;
	int len;

	va_start(ap, fmt);
	len = vsnprintf(logbuf, sizeof(logbuf) - 2, fmt, ap);
	va_end(ap);
	if (len >= 0)
		logbuf[len++] = '\n';

	fwrite(logbuf, 1, len, logfile);
	fflush(logfile);
}

#undef LogError
#undef LogInfo
#undef LogDbg

#define LogError(fmt,...) logfile_write(fmt, __VA_ARGS__)
#define LogInfo(fmt,...)  logfile_write(fmt, __VA_ARGS__)
#define LogDbg(fmt,...)   logfile_write(fmt, __VA_ARGS__)

#endif /* WRITE_LOG_TO_FILE */

static void load_settings(void)
{
	serial_settings.baudrate = app_setting_get_int(SERIAL_SECTION, SERIAL_KEY_BAUDRATE, 115200);
	serial_settings.databits = app_setting_get_int(SERIAL_SECTION, SERIAL_KEY_DATABITS, 8     );
	serial_settings.stopbits = app_setting_get_int(SERIAL_SECTION, SERIAL_KEY_STOPBITS, 1     );
	serial_settings.parity   = app_setting_get_int(SERIAL_SECTION, SERIAL_KEY_PARITY  , 0     );
}

static void save_settings(void)
{
	app_setting_set_int(SERIAL_SECTION, SERIAL_KEY_BAUDRATE, serial_settings.baudrate);
	app_setting_set_int(SERIAL_SECTION, SERIAL_KEY_DATABITS, serial_settings.databits);
	app_setting_set_int(SERIAL_SECTION, SERIAL_KEY_STOPBITS, serial_settings.stopbits);
	app_setting_set_int(SERIAL_SECTION, SERIAL_KEY_PARITY  , serial_settings.parity  );
}

static int serial_set(int fd,int speed,int bits,int parity,int stop)
{
	struct termios newtio,oldtio;
	speed_t iospeed;

	if(tcgetattr(fd,&oldtio) != 0) {
		LogError("tcgetattr() fail(%d),%s",errno,strerror(errno));
		return 1;
	}

	bzero(&newtio,sizeof(newtio));

	newtio.c_cflag |= CLOCAL | CREAD;
	newtio.c_cflag &= ~CSIZE;

	switch(bits) {
		case 7:
			newtio.c_cflag |= CS7;
			break;
		case 8:
		default:
			newtio.c_cflag |= CS8;
			break;
	}

	switch(parity) {
		case 1:
			newtio.c_cflag |= PARENB;
			newtio.c_cflag |= PARODD;
			newtio.c_cflag |= (INPCK | ISTRIP);
			break;
		case 2:
			newtio.c_cflag |= (INPCK | ISTRIP);
			newtio.c_cflag |= PARENB;
			newtio.c_cflag &= ~PARODD;
			break;
		case 0:
		default:
			newtio.c_cflag &= ~PARENB;
			break;
	}

	switch(speed) {
#define CASE(n) case n: iospeed = B##n; break
		CASE(2400);
		CASE(4800);
		CASE(9600);
		CASE(19200);
		CASE(38400);
		CASE(57600);
		CASE(115200);
		CASE(230400);
		CASE(460800);
		CASE(921600);
		CASE(2000000);
#undef CASE
		default:
			iospeed = B115200;
			break;
	}
	cfsetispeed(&newtio,iospeed);
	cfsetospeed(&newtio,iospeed);

	switch(stop) {
		case 2:
			newtio.c_cflag |= CSTOPB;
			break;
		case 1:
		default:
			newtio.c_cflag &= ~CSTOPB;
			break;
	}

	newtio.c_cc[VTIME] = 0;
	newtio.c_cc[VMIN] = 0;
	tcflush(fd,TCIOFLUSH);

	if(tcsetattr(fd,TCSANOW,&newtio) != 0) {
		LogError("tcsetattr(TCSANOW) error(%d),%s",errno,strerror(errno));
	}

	return 0;
}

static int serial_open(const char *dev,int speed,int bits,int parity,int stop)
{
	int fd = open(dev,O_RDWR|O_NOCTTY|O_CLOEXEC|O_NDELAY|O_NONBLOCK|O_SYNC);
	if( fd<0 ) {
//		LogError("Open %s fail(%d),%s",dev,errno,strerror(errno));
		return fd;
	}
	serial_set(fd,speed,bits,parity,stop);
	return fd;
}

static void *serial_thread(void *arg)
{
	struct pollfd fds;
	int ret;

	while (g_process_running) {
		if (settings_changed) {
			save_settings();
			settings_changed = 0;
			if (serial_fd >= 0) {
				close(serial_fd);
				serial_fd = -1;
			}
		}
		if (serial_fd < 0) {
			serial_fd = serial_open(SERIAL_NODE,
									serial_settings.baudrate,
									serial_settings.databits,
									serial_settings.parity,
									serial_settings.stopbits);
			if (serial_fd < 0) {
//				LogError("open() failed: errno=%d (%s)", errno, strerror(errno));
				SysDelay(500);
				continue;
			}
			fds.fd = serial_fd;
			fds.events = POLLIN;
			fds.revents = 0;
		}

		ret = poll(&fds, 1, 1000);
//		if (ret < 0)
//			LogError("poll() returns %d (revent=%d, errno=%d)", ret, fds.revents, errno);

		if (ret <= 0 || (fds.revents & POLLIN) == 0)
			continue;

		ret = read(serial_fd, serial_buffer, sizeof(serial_buffer));
		if (ret <= 0) {
			LogError("read() failed: ret=%d, errno=%d (%s)", ret, errno, strerror(errno));
			close(serial_fd);
			serial_fd = -1;
			continue;
		}

		print_channel_send_data(PRINT_CHN_ID_SERIAL, serial_buffer, ret, 0);
	}

	return NULL;
}

#endif /* SERIAL_ENABLED */

void serial_settings_changed(void)
{
#if (SERIAL_ENABLED)
	settings_changed = 1;
#endif /* SERIAL_ENABLED */
}

int serial_init(void)
{
#if (SERIAL_ENABLED)

	if (sys_global_var()->boot_mode != BOOT_MODE_NORMAL)
		return 0;

#if (WRITE_LOG_TO_FILE)
	logfile_init();
#endif

	load_settings();

	/*
	 * Create thread.
	 */
	if (pthread_create(&m_tid, &g_pthread_attr, serial_thread, NULL)) {
		LogFatal("pthread_create() failed.");
		return -1;
	}

#endif /* SERIAL_ENABLED */

	return 0;
}
