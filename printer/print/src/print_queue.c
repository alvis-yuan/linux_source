#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>
#include <zlib.h>
#include "libcommon.h"
#include "glbvar.h"
#include "simple_timer.h"
#include "Instruct_Proc.h"
#include "Realtime.h"
#include "Runtime_Data.h"
#include "Util.h"
#include "WordSet.h"
#include "printer.h"
#include "print_server.h"
#include "print_queue.h"
#include "tspl_api.h"
#include "ESC_Instruct.h"

static sem_t *sem_buffer_pool;
static sem_t *sem_task_pool;
static sem_t *sem_channels[PRINT_CHN_ID_MAX];
static sem_t *sem_task_queue;
static sem_t *sem_status;
static sem_t *sem_request;
static print_shm_t *shm = NULL;
static mqd_t mq;
static pthread_t m_tid;
static uint8_t feed_and_cut_timer;
static uint8_t reback_flag = 1; //1-电机回退，0-电机不回退 

static print_data_share_t pds_post_data_collection;

typedef struct {
    sem_t *sem_notice;  //通知数采线程执行数采
    bool inited;        //未初始化false, 已初始化true
    bool share_completion;  //未完成数采false, 已完成数采true
    uint16_t task_id;   //最近已完成的task_id，避免copies>0时重复采集
    //time_t end_time;    //记录超时时间，避免长时间堵塞
    pthread_t tid;
} print_data_collection_t;


#define SEM_TIMEDWAIT(sem__) \
do { \
	struct timespec abstime; \
	clock_gettime(CLOCK_REALTIME, &abstime); \
	abstime.tv_nsec += 100000000L; /* 100ms */ \
	if (abstime.tv_nsec > 999999999L) { \
		abstime.tv_nsec -= 1000000000L; \
		abstime.tv_sec++; \
	} \
	sem_timedwait((sem__), &abstime); \
} while (0)

#define buffer_pool_lock()					SEM_TIMEDWAIT(sem_buffer_pool)
#define buffer_pool_unlock()				sem_post(sem_buffer_pool)
#define task_pool_lock()					SEM_TIMEDWAIT(sem_task_pool)
#define task_pool_unlock()					sem_post(sem_task_pool)
#define channels_lock(i)					SEM_TIMEDWAIT(sem_channels[i])
#define channels_unlock(i)					sem_post(sem_channels[i])
#define task_queue_lock()					SEM_TIMEDWAIT(sem_task_queue)
#define task_queue_unlock()					sem_post(sem_task_queue)
#define request_wait_for_change()			sem_wait(sem_request)
#define request_post_change()				sem_post(sem_request)

#define INDEX_NULL							(0xFFFF)
#define IS_VALID_BUFFER_INDEX(b)			((b) < PRINT_BUFFER_POOL_SIZE)
#define IS_VALID_TASK_INDEX(t)				((t) < PRINT_TASK_POOL_SIZE)
#define BUFFER_AT(b)						(&shm->buffer_pool.mem[b])
#define TASK_AT(t)							(&shm->task_pool.mem[t])

static void set_feeding_state(uint8_t state)
{
	PrnIoctl(PRN_CMD_SET_FEED_STATE, &state, 1, NULL, NULL);
}

static void dump_buffer(uint16_t b)
{
	char str[512];
	uint16_t i, offset;

	offset = sprintf(str, "[id=%04x] ", BUFFER_AT(b)->id);
	for (i = 0; i < BUFFER_AT(b)->len; i++)
		offset += sprintf(str + offset, "%02X", BUFFER_AT(b)->data[i]);
	LogDbg("%s", str);
}

/*----------------------------------------------*
 | PRINTER MOTOR TEMPERATURE CONTROL            |
 *----------------------------------------------*/
static uint32_t WATERLEVEL_OVERHEATED_BEGIN = 6000;
static uint32_t WATERLEVEL_OVERHEATED_END = 480;
static uint32_t TEMPERATURE_FALLING_COEFFICIENT = 23;
static uint32_t waterlevel = 0;

static void update_waterlevel(void)
{
	static uint32_t tick_last = 0, dist_last = 0;
	static uint8_t motor_overheated_last = 0;
	uint32_t tick_curr, tick_diff, dist_curr, dist_diff, wl;

	if (tick_last == 0 && dist_last == 0) { /* The first time */
		tick_last = SysGetTickCount();
		dist_last = shm->status.print_distance;
		return;
	}

	tick_curr = SysGetTickCount();
	tick_diff = tick_curr - tick_last;
	if (tick_diff < 1000)
		return;
	tick_last = tick_curr;

	dist_curr = shm->status.print_distance;
	dist_diff = dist_curr - dist_last;
	if (dist_diff == 0 && waterlevel == 0)
		return;
	dist_last = dist_curr;

	if (dist_diff == 0) {
		/* 每停止1秒，waterlevel减小100或23 */
		wl = tick_diff * TEMPERATURE_FALLING_COEFFICIENT / 1000;
		if (waterlevel > wl)
			waterlevel -= wl;
		else
			waterlevel = 0;
	}
	else {
		/* 每工作1秒，waterlevel增加100 */
		waterlevel += tick_diff / 10;
	}

	if (DOTS_PER_LINE == 384 || shm->status.motor_temperature == -274) {
		if (waterlevel > WATERLEVEL_OVERHEATED_BEGIN)
			shm->status.step_motor_overheated = 1;
		else if (waterlevel < WATERLEVEL_OVERHEATED_END)
			shm->status.step_motor_overheated = 0;
	}
	else {
		if (shm->status.motor_temperature > 105)
			shm->status.step_motor_overheated = 1;
		else if (shm->status.motor_temperature < 100)
			shm->status.step_motor_overheated = 0;
	}

	if (shm->status.step_motor_overheated != motor_overheated_last) {
		motor_overheated_last = shm->status.step_motor_overheated;
		PrnIoctl(PRN_CMD_SET_MOTOR_OVERHEATED_STATE,
			&motor_overheated_last, 1, NULL, NULL);
	}

	/* Stop feeding if printer motor is overheated */
	if (shm->status.step_motor_overheated)
		set_feeding_state(0);

#if 0
	LogDbg("waterlevel=%u, temperature=%d",
		waterlevel, shm->status.motor_temperature);
#endif
}

static uint32_t task_delay(void)
{
	/* 任务之间的延时，单位为ms */
	if (DOTS_PER_LINE == 384) {
		if (waterlevel > 24000)
			return 5000;
		if (waterlevel > 18000)
			return 2000;
		if (waterlevel > 12000)
			return 1000;
	}
	else {
		/* If motor temperature is invalid, use waterlevel instead */
		if (shm->status.motor_temperature == -274) {
			if (waterlevel > 3000)
				return 5000;
			if (waterlevel > 1000)
				return 2000;
			if (waterlevel > 500)
				return 1000;
		}
		else {
			if (shm->status.motor_temperature > 90)
				return 5000;
			if (shm->status.motor_temperature > 70)
				return 2000;
			if (shm->status.motor_temperature > 60)
				return 1000;
		}
	}
	return 0;
}

/*----------------------------------------------*
 | TASK LOGGING                                 |
 *----------------------------------------------*/
#define LOG_FILE_PATH "/tmp/logdir/files/"
#define DATA_COLLECTION_PATH "/tmp/collection/"
#define THIRD_PARTY_DATA_COLLECTION_PATH "/tmp/third_party_collection/"

static int last_logged_task_ids[PRINT_CHN_ID_MAX];

/*----------------------------------------------*
 | PRINT LOGGING                                 |
 *----------------------------------------------*/
#define SAVE_FILE_PATH "/data/PUB/printLog/"
#define ABSOLUTE_SERIAL_NUMBER_FILE "/data/PUB/sequence_number.bin"
static int last_saved_task_ids[PRINT_CHN_ID_MAX];

static const char *channel_name(int chn_id)
{
	switch (chn_id) {
	case PRINT_CHN_ID_INT:
		return "int";
	case PRINT_CHN_ID_HTTPS:
		return "https";
	case PRINT_CHN_ID_MQTT:
		return "mqtt";
	case PRINT_CHN_ID_USB:
		return "usb";
	case PRINT_CHN_ID_SPP ... PRINT_CHN_ID_SPP4:
		return "spp";
	case PRINT_CHN_ID_BLE:
		return "ble";
	case PRINT_CHN_ID_TCP_BEGIN ... PRINT_CHN_ID_TCP_END:
		return "tcp";
	case PRINT_CHN_ID_SERIAL:
		return "serial";
	case PRINT_CHN_ID_AM:
		return "am";
	case PRINT_CHN_ID_CGI:
		return "cgi";
	case PRINT_CHN_ID_CMD:
		return "cmd";
	default:
		break;
	}
	return "unknown";
}

static void log_task_data(print_task_t *task)
{
	char pathname[64];
	struct tm *loctim;
	time_t t = 0;
	FILE *f;
	uint16_t b;

	if (task->chn_id <= PRINT_CHN_ID_INT || task->chn_id >= PRINT_CHN_ID_MAX)
		return;
	if (task->id <= last_logged_task_ids[task->chn_id]) /* Log file already generated */
		return;

	if (access("/dev/debug_mode", F_OK) == 0 || shm->request.task_logging != 0) {
		/* Make sure the directory exists */
		system("/bin/mkdir -p " LOG_FILE_PATH);

		t = time(NULL);
		loctim = localtime(&t);

		sprintf(pathname, LOG_FILE_PATH "%02d%02d%02d%02d%02d%02d_dt.%s",
				loctim->tm_year % 100,
				loctim->tm_mon + 1,
				loctim->tm_mday,
				loctim->tm_hour,
				loctim->tm_min,
				loctim->tm_sec,
				channel_name(task->chn_id));

		f = fopen(LOG_FILE_PATH ".file_being_written__", "w+");
		if (!f)
			return;

		b = task->head;
		while (IS_VALID_BUFFER_INDEX(b)) {
			fwrite(BUFFER_AT(b)->data, BUFFER_AT(b)->len, 1, f);
			b = BUFFER_AT(b)->next;
		}

		fclose(f);
		sync();

		rename(LOG_FILE_PATH ".file_being_written__", pathname);

		LogDbgWithFile(pathname, "%s created.", pathname);
	}

	last_logged_task_ids[task->chn_id] = task->id;
}

uint32_t get_absolute_serial_number() {
    FILE* file;
    uint32_t number;

    file = fopen(ABSOLUTE_SERIAL_NUMBER_FILE, "r+b");
    if (file == NULL) {
        file = fopen(ABSOLUTE_SERIAL_NUMBER_FILE, "w+b");
        if (file == NULL) {
            return 0;
        }
        number = 1;
        fwrite(&number, sizeof(number), 1, file);
    } else {
        if (fread(&number, sizeof(number), 1, file) == 1) {
            number++;
            fseek(file, 0, SEEK_SET);
            fwrite(&number, sizeof(number), 1, file);
        } else {
            fclose(file);
            return 0;
        }
    }
	
	fflush(file);
	fsync(fileno(file));
    fclose(file);
    return number;
}

static void compress_file_gzip(const char* source_path, const char* dest_path) {
    char buffer[1024];
    int num_read;

    gzFile gz_dest = gzopen(dest_path, "wb");
    if(!gz_dest){
		return;
	}

    FILE* source = fopen(source_path, "rb");
	if(!source){
		return;
	}

    while ((num_read = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        gzwrite(gz_dest, buffer, num_read);
    }

    fclose(source);
    gzclose(gz_dest);

    if (remove(source_path) != 0) {
		return;
    }
}

/*保存该打印任务至flash中*/
static void save_task_data(uint16_t t)
{
	char pathname[128];
	FILE *f;
	uint16_t b;
	uint32_t serialNum;

	if (TASK_AT(t)->id <= last_saved_task_ids[TASK_AT(t)->chn_id])
		return;

	if(access(SAVE_FILE_PATH, F_OK) != 0){
		system("/bin/mkdir -p " SAVE_FILE_PATH);
	}

	serialNum = get_absolute_serial_number();
	if(strlen(shm->print_log_mem[t].orderId) != 0){
		if(TASK_AT(t)->chn_id == PRINT_CHN_ID_HTTPS){
			sprintf(pathname, SAVE_FILE_PATH "%u_%d_%d_%s_%s_%u_%u_%u_%u_%u_%u.gz", serialNum, (int)shm->print_log_mem[t].reception_time, (int)shm->print_log_mem[t].finish_time, shm->print_log_mem[t].orderId, "dp", 
			print_log_config.asc_wordset, print_log_config.cjk_wordset, print_log_config.code_page, print_log_config.utf8_wordset, print_log_config.locale, print_log_config.fine_mode);
		}else{
			sprintf(pathname, SAVE_FILE_PATH "%u_%d_%d_%s_%s_%u_%u_%u_%u_%u_%u.gz", serialNum, (int)shm->print_log_mem[t].reception_time, (int)shm->print_log_mem[t].finish_time, shm->print_log_mem[t].orderId, channel_name(TASK_AT(t)->chn_id), 
			print_log_config.asc_wordset, print_log_config.cjk_wordset, print_log_config.code_page, print_log_config.utf8_wordset, print_log_config.locale, print_log_config.fine_mode);
		}
	}else{
		sprintf(shm->print_log_mem[t].orderId, "%d", TASK_AT(t)->id);
		sprintf(pathname, SAVE_FILE_PATH "%u_%d_%d_%s_%s_%u_%u_%u_%u_%u_%u.gz", serialNum, (int)shm->print_log_mem[t].reception_time, (int)shm->print_log_mem[t].finish_time, shm->print_log_mem[t].orderId, channel_name(TASK_AT(t)->chn_id), 
		print_log_config.asc_wordset, print_log_config.cjk_wordset, print_log_config.code_page, print_log_config.utf8_wordset, print_log_config.locale, print_log_config.fine_mode);
	}
	LogInfo("pathname = %s.", pathname);

	f = fopen(SAVE_FILE_PATH ".file_flash_being_written__", "w+");
	if (!f)
		return;

	b = TASK_AT(t)->head;
	while (IS_VALID_BUFFER_INDEX(b)) {
		fwrite(BUFFER_AT(b)->data, BUFFER_AT(b)->len, 1, f);
		b = BUFFER_AT(b)->next;
	}

	fclose(f);
	compress_file_gzip(SAVE_FILE_PATH ".file_flash_being_written__", pathname);
	last_saved_task_ids[TASK_AT(t)->chn_id] = TASK_AT(t)->id;
}

#if 0
static void collect_data(uint16_t t)
{
	static time_t last_tim = 0;
	static int suffix = 0;
	char pathname[64], seqnum[16];
	print_task_t *task = TASK_AT(t);
	time_t tim = 0;
	FILE *f;
	uint16_t b;

	if (TASK_FLAG_IS_SET(task, TASK_FLAG_DONT_COLLECT_DATA))
		return;

	if (shm->data_collection.enabled != 0 &&
		( task->chn_id == PRINT_CHN_ID_HTTPS ||
		  task->chn_id == PRINT_CHN_ID_USB ||
		 (task->chn_id >= PRINT_CHN_ID_SPP && task->chn_id <= PRINT_CHN_ID_SPP4) ||
		  task->chn_id == PRINT_CHN_ID_BLE ||
		 (task->chn_id >= PRINT_CHN_ID_TCP_BEGIN && task->chn_id <= PRINT_CHN_ID_TCP_END) ||
		  task->chn_id == PRINT_CHN_ID_SERIAL ||
		  task->chn_id == PRINT_CHN_ID_CGI)) {
		/* Make sure the directory exists */
		system("/bin/mkdir -p " DATA_COLLECTION_PATH);

		if (tim == 0)
			tim = time(NULL);
		if (tim == last_tim)
			suffix++;
		else
			suffix = 0;
		last_tim = tim;

		sprintf(seqnum, "%d%d", (int)tim, suffix);
		sprintf(pathname, DATA_COLLECTION_PATH "%s.%s",
				seqnum, channel_name(task->chn_id));

		f = fopen(DATA_COLLECTION_PATH ".file_being_written__", "w+");
		if (!f)
			return;

		b = task->head;
		while (IS_VALID_BUFFER_INDEX(b)) {
			fwrite(BUFFER_AT(b)->data, BUFFER_AT(b)->len, 1, f);
			b = BUFFER_AT(b)->next;
		}

		fclose(f);
		sync();

		rename(DATA_COLLECTION_PATH ".file_being_written__", pathname);

		strcpy(shm->data_collection.seqnum, seqnum);
		shm->data_collection.timestamp = tim;
		shm->data_collection.task_id = task->id;
		shm->data_collection.task_index = t;

		print_data_share_notify(&pds_post_data_collection);
	}
}
#endif

static int third_party_collect_data_check(int chn_id)
{
	if(chn_id < 0 || chn_id > 63)
	{
		LogFatal("chn_id=%d not exit!!!", chn_id);
		return -1;
	}
	if (shm->third_party_data_collection.enabled != 0 &&
		(shm->third_party_data_collection.channel_allow & (1 << chn_id)))
		return 0;
	else
		return -1;
}

static void collect_data(uint16_t t)
{
	static time_t last_tim = 0;
	static int suffix = 0;
	char pathname[64], seqnum[16];
	print_task_t *task = TASK_AT(t);
	time_t tim = 0;
	FILE *f;
	uint16_t b;
	uint8_t collect_data_enable = 0;
	uint8_t third_party_collect_enable = 0;
	const char *targetpath = NULL;
	char thirdparty_pathname[64];
	char tmpbuf[64]={0};

	if (TASK_FLAG_IS_SET(task, TASK_FLAG_DONT_COLLECT_DATA))
		return;

	if (shm->data_collection.enabled != 0 &&
		( task->chn_id == PRINT_CHN_ID_HTTPS ||
		task->chn_id == PRINT_CHN_ID_USB ||
		(task->chn_id >= PRINT_CHN_ID_SPP && task->chn_id <= PRINT_CHN_ID_SPP4) ||
		task->chn_id == PRINT_CHN_ID_BLE ||
		(task->chn_id >= PRINT_CHN_ID_TCP_BEGIN && task->chn_id <= PRINT_CHN_ID_TCP_END) ||
		task->chn_id == PRINT_CHN_ID_SERIAL ||
		task->chn_id == PRINT_CHN_ID_CGI)) {
		collect_data_enable = 1;
		if(access(DATA_COLLECTION_PATH, F_OK) != 0)
		{
			/* Make sure the directory exists */
			system("/bin/mkdir -p " DATA_COLLECTION_PATH);
		}
		targetpath = DATA_COLLECTION_PATH;
	}
	if(third_party_collect_data_check(task->chn_id) == 0)
	{
		third_party_collect_enable = 1;
		if(access(THIRD_PARTY_DATA_COLLECTION_PATH, F_OK) != 0)
		{
			/* Make sure the directory exists */
			system("/bin/mkdir -p " THIRD_PARTY_DATA_COLLECTION_PATH);
		}
		if(targetpath == NULL)
			targetpath = THIRD_PARTY_DATA_COLLECTION_PATH;
	}

	if(collect_data_enable || third_party_collect_enable)
	{
		if (tim == 0)
			tim = time(NULL);
		if (tim == last_tim)
			suffix++;
		else
			suffix = 0;
		last_tim = tim;

		sprintf(seqnum, "%d%d", (int)tim, suffix);
		snprintf(pathname, sizeof(pathname), "%s%s.%s", targetpath, seqnum, channel_name(task->chn_id));

		snprintf(tmpbuf, sizeof(tmpbuf), "%s.file_being_written__", targetpath);
		f = fopen(tmpbuf, "w+");
		if (!f)
			return;

		b = task->head;
		while (IS_VALID_BUFFER_INDEX(b)) {
			fwrite(BUFFER_AT(b)->data, BUFFER_AT(b)->len, 1, f);
			b = BUFFER_AT(b)->next;
		}

		fclose(f);
		sync();

		rename(tmpbuf, pathname);
		if(third_party_collect_enable && strcmp(targetpath, THIRD_PARTY_DATA_COLLECTION_PATH) != 0)  //third party make hardlink
		{
			sprintf(thirdparty_pathname, THIRD_PARTY_DATA_COLLECTION_PATH "%s.%s",seqnum, channel_name(task->chn_id));
			if(link(pathname, thirdparty_pathname) != 0)
			{
				LogFatal("link():%s failed. errno=%d (%s)", pathname, errno, strerror(errno));
				third_party_collect_enable = 0;
			}
		}

		if(collect_data_enable)
		{
			strcpy(shm->data_collection.seqnum, seqnum);
			shm->data_collection.timestamp = tim;
			shm->data_collection.task_id = task->id;
			shm->data_collection.task_index = t;
			//print_data_share_notify(&pds_post_data_collection);
		}
		else
		{
			shm->data_collection.seqnum[0] = 0;
			shm->data_collection.timestamp = 0;
			shm->data_collection.task_id = INDEX_NULL;
			shm->data_collection.task_index = INDEX_NULL;
		}
		if(third_party_collect_enable)
		{
			strcpy(shm->third_party_data_collection.seqnum, seqnum);
			shm->third_party_data_collection.timestamp = tim;
			shm->third_party_data_collection.task_id = task->id;
			shm->third_party_data_collection.task_index = t;
		}
		else
		{
			shm->third_party_data_collection.seqnum[0] = 0;
			shm->third_party_data_collection.timestamp = 0;
			shm->third_party_data_collection.task_id = INDEX_NULL;
			shm->third_party_data_collection.task_index = INDEX_NULL;
		}
		if(collect_data_enable || third_party_collect_enable)
			print_data_share_notify(&pds_post_data_collection);
	}
}

/*----------------------------------------------*
 | INITIALIZATION                               |
 *----------------------------------------------*/
static void buffer_pool_init(void)
{
	int i;

	for (i = 0; i < PRINT_BUFFER_POOL_SIZE; i++)
		shm->buffer_pool.mem[i].next = i + 1;
	shm->buffer_pool.head = 0;
	shm->buffer_pool.tail = PRINT_BUFFER_POOL_SIZE - 1;
	shm->buffer_pool.free_count = PRINT_BUFFER_POOL_SIZE;
	shm->buffer_pool.free_count_min = PRINT_BUFFER_POOL_SIZE;
}

static void task_pool_init(void)
{
	int i;

	for (i = 0; i < PRINT_TASK_POOL_SIZE; i++)
		shm->task_pool.mem[i].next = i + 1;
	shm->task_pool.head = 0;
	shm->task_pool.tail = PRINT_TASK_POOL_SIZE - 1;
	shm->task_pool.free_count = PRINT_TASK_POOL_SIZE;
	shm->task_pool.free_count_min = PRINT_TASK_POOL_SIZE;
	shm->task_pool.next_task_id = 1;
	shm->task_pool.last_completed_task_id = 0;
}

static void channels_init(void)
{
	int i;

	for (i = 0; i < PRINT_CHANNELS_MAX; i++) {
		shm->channels[i].flags = 0;
		shm->channels[i].task_end_timeout = 600;
		shm->channels[i].no_data_received = 0;
		shm->channels[i].last_task_id = 0;
		shm->channels[i].task = INDEX_NULL;
		shm->channels[i].task_queue.head = INDEX_NULL;
		shm->channels[i].task_queue.tail = INDEX_NULL;
	}

	shm->channels[PRINT_CHN_ID_INT].flags = PRINT_CHN_FLAG_NO_REPRINT;
	shm->channels[PRINT_CHN_ID_CMD].flags = PRINT_CHN_FLAG_NO_REPRINT;

	shm->channels[PRINT_CHN_ID_INT  ].task_end_timeout = 200;
	shm->channels[PRINT_CHN_ID_HTTPS].task_end_timeout = 200;
	shm->channels[PRINT_CHN_ID_MQTT ].task_end_timeout = 200;
	shm->channels[PRINT_CHN_ID_USB  ].task_end_timeout = 200;
}

static void task_queue_init(void)
{
	shm->task_queue.head = INDEX_NULL;
	shm->task_queue.tail = INDEX_NULL;
	shm->task_queue.buf_to_send = INDEX_NULL;
	shm->task_queue.buf_offset = 0;
	shm->task_queue.last_channel_id = -1;
}

static void print_log_init(void)
{
    for (int i = 0; i < PRINT_TASK_POOL_SIZE; i++) {
        memset(shm->print_log_mem[i].orderId, 0, sizeof(shm->print_log_mem[i].orderId));
        shm->print_log_mem[i].reception_time = 0;
        shm->print_log_mem[i].finish_time = 0;
    }
}

static void buffer_free(uint16_t b)
{
	if (!IS_VALID_BUFFER_INDEX(b))
		return;

	buffer_pool_lock();
	BUFFER_AT(b)->next = INDEX_NULL;
	if (IS_VALID_BUFFER_INDEX(shm->buffer_pool.tail))
		BUFFER_AT(shm->buffer_pool.tail)->next = b;
	else
		shm->buffer_pool.head = b;
	shm->buffer_pool.tail = b;
	shm->buffer_pool.free_count++;
	buffer_pool_unlock();
}

static void task_free(uint16_t t)
{
	uint16_t b, n;

	if (!IS_VALID_TASK_INDEX(t))
		return;

	b = TASK_AT(t)->head;
	while (IS_VALID_BUFFER_INDEX(b)) {
		n = BUFFER_AT(b)->next;
		buffer_free(b);
		b = n;
	}

	task_pool_lock();
	TASK_AT(t)->next = INDEX_NULL;
	TASK_AT(t)->flags = 0;
	if (IS_VALID_TASK_INDEX(shm->task_pool.tail))
		TASK_AT(shm->task_pool.tail)->next = t;
	else
		shm->task_pool.head = t;
	shm->task_pool.tail = t;
	shm->task_pool.free_count++;
	task_pool_unlock();
	LogDbg("task pool: free=%u, min=%u; buffer pool: free=%u, min=%u",
		shm->task_pool.free_count, shm->task_pool.free_count_min,
		shm->buffer_pool.free_count, shm->buffer_pool.free_count_min);
}

static void free_buffers_to(uint16_t t, uint16_t b_to_keep)
{
	uint16_t b;

	if (!IS_VALID_TASK_INDEX(t) || !IS_VALID_BUFFER_INDEX(b_to_keep))
		return;
	while (IS_VALID_BUFFER_INDEX(TASK_AT(t)->head) && TASK_AT(t)->head != b_to_keep) {
		b = TASK_AT(t)->head;
		TASK_AT(t)->head = BUFFER_AT(b)->next;
		if (!IS_VALID_BUFFER_INDEX(TASK_AT(t)->head))
			TASK_AT(t)->tail = INDEX_NULL;
		buffer_free(b);
	}
}

static void append_task_to_queue(uint16_t t)
{
	if (shm->task_queue.head == t)
		return;

	if (TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_DONT_PRINT) && !TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_RECV_NOT_COMPLETE)){//已接收完成的待删除任务
		LogInfo("task %d delete, free (bytes=%u).", TASK_AT(t)->id, TASK_AT(t)->bytes_received);
		task_free(t);
		return;
	}

	if (shm->channels[TASK_AT(t)->chn_id].flags & PRINT_CHN_FLAG_NO_REPRINT)
		TASK_FLAG_SET(TASK_AT(t), TASK_FLAG_DONT_REPRINT);

	task_queue_lock();
	if (IS_VALID_TASK_INDEX(shm->task_queue.tail))
		TASK_AT(shm->task_queue.tail)->next = t;
	else {
		shm->task_queue.head = t;
		shm->task_queue.buf_to_send = TASK_AT(t)->head;
		shm->task_queue.buf_offset = 0;
	}
	shm->task_queue.tail = t;
	task_queue_unlock();
}

static uint16_t remove_task_from_queue(void)
{
	uint16_t t;

	task_queue_lock();
	t = shm->task_queue.head;
	shm->task_pool.last_completed_task_id = TASK_AT(t)->id;
	shm->task_queue.head = TASK_AT(t)->next;
	if (IS_VALID_TASK_INDEX(shm->task_queue.head))
		TASK_AT(t)->next = INDEX_NULL;
	else
		shm->task_queue.tail = INDEX_NULL;
	task_queue_unlock();

	return t;
}

static void reset_buffer_id(void)
{
	uint8_t buf[8];

	memset(buf, 0, sizeof(buf));
	buf[0] = 0x01;
	*((uint32_t *)(buf + 1)) = 0x7FFF7FFF;
	printer_send_command(buf, 5);
}

static void send_buffer_id(uint16_t t, uint16_t b)
{
	uint8_t buf[8];
	int task_id;
	uint16_t buf_id;

	if (!IS_VALID_TASK_INDEX(t))
		return;
	task_id = TASK_AT(t)->id;
	buf_id = (IS_VALID_BUFFER_INDEX(b)) ? BUFFER_AT(b)->id : 0xFFFF;

	buf[0] = 0x01;
	*((uint16_t *)(buf + 1)) = buf_id;
	*((uint16_t *)(buf + 3)) = (uint16_t)(task_id); /* Use the lower 16-bit only */
	printer_send_command(buf, 5);
}

static int compare_buffer_id(uint16_t t, uint16_t b)
{
	uint16_t task_id, buf_id, tid, bid;
	uint32_t LastBufferId, dataOutLength;

	if (!IS_VALID_TASK_INDEX(t))
		return 0;

	dataOutLength = sizeof(LastBufferId);
	if (PrnIoctl(PRN_CMD_GET_BUFFER_ID, NULL, 0, &LastBufferId, &dataOutLength) != 0)
		return 0;

	task_id = (uint16_t)(TASK_AT(t)->id);
	buf_id = (IS_VALID_BUFFER_INDEX(b)) ? BUFFER_AT(b)->id : 0xFFFF;

	tid = ((LastBufferId >> 16) & 0xFFFF);
	bid = ((LastBufferId      ) & 0xFFFF);
	LogDbg("LastBufferId=%08X (%04X%04X)", LastBufferId, task_id, buf_id);
	return (task_id == tid && buf_id == bid) ? 1 : 0;
}

static void out_of_paper_detected(uint64_t cut_cmd_id)
{
	uint16_t b, buffer_id;

	if (IS_VALID_TASK_INDEX(shm->task_queue.head)) { /* A task is printing */
		const print_task_t *task = TASK_AT(shm->task_queue.head);
		if (TASK_FLAG_IS_SET(task, TASK_FLAG_DISCARD_DATA_ON_OOP) || app_setting_get_int("printer", "ReprintFlag", 1) == 0) {
			if(app_setting_get_int("printer", "ReprintFlag", 1) == 0){
				PrnIoctl(PRN_CMD_CLEAR_REPRINT_FLAG, NULL, 0, NULL, NULL);
			}
			printer_clear_buffer();
			task_free(remove_task_from_queue());
			if (IS_VALID_TASK_INDEX(shm->task_queue.head)) {
				shm->task_queue.buf_to_send = task->head;
				shm->task_queue.buf_offset = 0;
			}
			return;
		}
		if (!TASK_FLAG_IS_SET(task, TASK_FLAG_DONT_CARE_PAPER_STATUS) &&
			!TASK_FLAG_IS_SET(task, TASK_FLAG_DONT_REPRINT) &&
			Cmd_DontClearBuffer == 0) {
			LogDbg("clear printer buffer");
			printer_clear_buffer();
			/* Resend the task from beginning */
			printer_send_debug_string("\n>> REPRINT (OOP,%d,%u) <<\n",
				task->id,
				task->copies);
			shm->task_queue.buf_to_send = task->head;
			shm->task_queue.buf_offset = 0;
			reback_flag = 0; //重打的订单电机不回退

			if ((((task->id << 16) & 0xFFFF0000) | ((task->copies) & 0x0000FFFF)) == (uint32_t)(cut_cmd_id >> 32)) {
				/* Resend the task from the last cut command */
				buffer_id = (uint16_t)((cut_cmd_id >> 16) & 0xFFFFULL);
				b = task->head;
				while (IS_VALID_BUFFER_INDEX(b) && BUFFER_AT(b)->id != buffer_id)
					b = BUFFER_AT(b)->next;
				if (IS_VALID_BUFFER_INDEX(b)) {
					shm->task_queue.buf_to_send = b;
					shm->task_queue.buf_offset = (uint16_t)((cut_cmd_id) & 0xFFFFULL);
					LogDbg("reprint from %u byte(s) of buffer %u", BUFFER_AT(b)->id, shm->task_queue.buf_offset);
				}
			}
		}
	}
}

static void flush_channels(int msec_elapsed, int printer_status)
{
	print_channel_t *channel;
	uint16_t t;
	int i, complete = 0;

	for (i = 0; i < PRINT_CHN_ID_MAX; i++) {
		channel = &shm->channels[i];
		channels_lock(i);
		while (IS_VALID_TASK_INDEX(channel->task_queue.head)) {
			t = channel->task_queue.head;
			channel->task_queue.head = TASK_AT(t)->next;
			if (IS_VALID_TASK_INDEX(channel->task_queue.head))
				TASK_AT(t)->next = INDEX_NULL;
			else
				channel->task_queue.tail = INDEX_NULL;
			LogInfo("task %d complete (bytes=%u).", TASK_AT(t)->id, TASK_AT(t)->bytes_received);
			//collect_data(t);
			append_task_to_queue(t);
		}
		if (IS_VALID_TASK_INDEX(channel->task)) {
			channel->no_data_received += msec_elapsed;
			if (TASK_AT(channel->task)->bytes_to_receive == 0)
				complete = (channel->no_data_received >= channel->task_end_timeout) ? 1 : 0; /* There is no data in a certain period */
			else
				complete = (TASK_AT(channel->task)->bytes_received >= TASK_AT(channel->task)->bytes_to_receive) ? 2 : 0;
			if (complete) {
				LogInfo("task %d complete (%d)(bytes=%u).", TASK_AT(channel->task)->id, complete, TASK_AT(channel->task)->bytes_received);
				TASK_FLAG_CLEAR(TASK_AT(channel->task), TASK_FLAG_RECV_NOT_COMPLETE);
				//collect_data(channel->task);
				append_task_to_queue(channel->task);
				channel->no_data_received = 0;
				channel->task = INDEX_NULL;
			}
			else if (shm->request.task_print_strategy == PRN_START_PRINT_DURING_RECEIVING &&
					!IS_VALID_TASK_INDEX(shm->task_queue.head) && printer_status == 0 && /* Task queue is empty, and printer is idle */
					TASK_AT(channel->task)->buffer_total > 1) {
				TASK_FLAG_SET(TASK_AT(channel->task), TASK_FLAG_RECV_NOT_COMPLETE);
				append_task_to_queue(channel->task);
			}
		}
		channels_unlock(i);
	}
}

static void check_task_print_status(void)
{
	if (IS_VALID_TASK_INDEX(shm->task_queue.head) && TASK_FLAG_IS_SET(TASK_AT(shm->task_queue.head), TASK_FLAG_DONT_PRINT)){  //打印任务需删除
		bool need_reset = true;
		if(TASK_FLAG_IS_SET(TASK_AT(shm->task_queue.head), TASK_FLAG_RECV_NOT_COMPLETE)){
			//LogDbg("wait task:%d receive complete!", TASK_AT(shm->task_queue.head)->id);
			return;
		}
		LogDbg("task %d delete, free (bytes=%u).", TASK_AT(shm->task_queue.head)->id, TASK_AT(shm->task_queue.head)->bytes_received);
		//remove task
		if (TASK_FLAG_IS_SET(TASK_AT(shm->task_queue.head), TASK_FLAG_DISABLE_UTF8))
			Settings.Utf8_WordSet = 0;
		if(shm->task_queue.buf_to_send == TASK_AT(shm->task_queue.head)->head && shm->task_queue.buf_offset == 0)//打印任务未开始打印,不需恢复默认
			need_reset = false;
		task_free(remove_task_from_queue());
		if (IS_VALID_TASK_INDEX(shm->task_queue.head)) {
			shm->task_queue.buf_to_send = TASK_AT(shm->task_queue.head)->head;
			shm->task_queue.buf_offset = 0;
		}
		//清除缓存并恢复默认
		if(need_reset){
			printer_clear_buffer();
			reset_buffer_id();
			RuntimeDataInit();
		}
		return;
	}
	if (!IS_VALID_BUFFER_INDEX(shm->task_queue.buf_to_send) && IS_VALID_TASK_INDEX(shm->task_queue.head)) { /* All buffers in this task has been sent */
		if (compare_buffer_id(shm->task_queue.head, TASK_AT(shm->task_queue.head)->tail) || /* The last buffer of this task has been printed */
			TASK_AT(shm->task_queue.head)->buffer_id_resend_count >= 600) {
			if (TASK_AT(shm->task_queue.head)->copies > 0)
				TASK_AT(shm->task_queue.head)->copies--;
			if (TASK_AT(shm->task_queue.head)->copies > 0) {
				printer_clear_buffer();
				reset_buffer_id();
				shm->task_queue.buf_to_send = TASK_AT(shm->task_queue.head)->head; /* Resend the task from beginning */
				shm->task_queue.buf_offset = 0;
			}
			else {
				/*记录打印任务的完成时间*/
				// time(&TASK_AT(shm->task_queue.head)->finish_time);
				time(&shm->print_log_mem[shm->task_queue.head].finish_time);

				/*不支持58打印机*/	
				if (strcmp(sys_global_var()->project,"NT211") && ( TASK_AT(shm->task_queue.head)->chn_id == PRINT_CHN_ID_HTTPS ||
				TASK_AT(shm->task_queue.head)->chn_id == PRINT_CHN_ID_USB ||
				(TASK_AT(shm->task_queue.head)->chn_id >= PRINT_CHN_ID_SPP && TASK_AT(shm->task_queue.head)->chn_id <= PRINT_CHN_ID_SPP4) ||
				TASK_AT(shm->task_queue.head)->chn_id == PRINT_CHN_ID_BLE ||
				(TASK_AT(shm->task_queue.head)->chn_id >= PRINT_CHN_ID_TCP_BEGIN && TASK_AT(shm->task_queue.head)->chn_id <= PRINT_CHN_ID_TCP_END) ||
				TASK_AT(shm->task_queue.head)->chn_id == PRINT_CHN_ID_SERIAL ||
				TASK_AT(shm->task_queue.head)->chn_id == PRINT_CHN_ID_AM ||
				TASK_AT(shm->task_queue.head)->chn_id == PRINT_CHN_ID_CGI)) {
					save_task_data(shm->task_queue.head);
				}
				memset(shm->print_log_mem[shm->task_queue.head].orderId, 0, sizeof(shm->print_log_mem[shm->task_queue.head].orderId));
				shm->print_log_mem[shm->task_queue.head].reception_time = 0;
				shm->print_log_mem[shm->task_queue.head].finish_time = 0;
				log_task_data(TASK_AT(shm->task_queue.head));
				LogDbg("remove task");
				if (TASK_FLAG_IS_SET(TASK_AT(shm->task_queue.head), TASK_FLAG_DISABLE_UTF8))
					Settings.Utf8_WordSet = 0;
				task_free(remove_task_from_queue());
				if (IS_VALID_TASK_INDEX(shm->task_queue.head)) {
					shm->task_queue.buf_to_send = TASK_AT(shm->task_queue.head)->head;
					shm->task_queue.buf_offset = 0;
				}
			}
		}
		else {
#if 0
			if (shm->status.no_paper == 0 &&
				shm->status.platen_opened == 0 &&
				shm->status.paper_jam == 0 &&
				shm->status.thermal_head_overheated == 0 &&
				shm->status.step_motor_overheated == 0) {
				send_buffer_id(shm->task_queue.head, TASK_AT(shm->task_queue.head)->tail);
				TASK_AT(shm->task_queue.head)->buffer_id_resend_count++;
			}
#else
			send_buffer_id(shm->task_queue.head, TASK_AT(shm->task_queue.head)->tail);
			TASK_AT(shm->task_queue.head)->buffer_id_resend_count++;
#endif
		}
	}
}

static int get_scriptname(char *path, uint32_t len)
{
	struct dirent *dir_property;
	DIR *dir = NULL;
	uint16_t filecnt = 0;

	if ((dir = opendir(TSPL_SCRIPT_DIR)) == NULL) {
		LogError("open dir %s failed!", TSPL_SCRIPT_DIR);
		return -1;
	}
	while ((dir_property = readdir(dir))) {
		/* skip any dir */
		if (dir_property->d_type == 4)
			continue;
		filecnt++;
	}
	snprintf(path, len, "%sbasic_script%d.bas", TSPL_SCRIPT_DIR, filecnt);
	return 0;
}

static void send_name2tspl_parser(char *name)
{
	mqd_t mq_id = mq_open(MQ_TSPL, O_WRONLY);
	if (mq_id < 0) {
		LogError("mq open fail");
		return;	
	}
	mq_send(mq_id, name, strlen(name)+1, 0);
	mq_close(mq_id);
}

static void *collection_data_thread(void *arg)
{
    print_data_collection_t *p_conf = (print_data_collection_t *)arg;

    LogDbg("collection_data_thread init");
    while (1)
    {
        LogDbg("task_id:%u start!", p_conf->task_id);
        sem_wait(p_conf->sem_notice);
        if(p_conf->task_id != INDEX_NULL)
        {
            collect_data(p_conf->task_id);
        }
        p_conf->share_completion = true;
    }

    LogDbg("collection_data_thread exit now");
    pthread_exit(0);
}

static int collection_data_func(uint16_t task_id)
{
    static print_data_collection_t data_collection = {NULL, false, false, INDEX_NULL, 0};
    int ret = 0;
    
    if(data_collection.inited == false)
    {
        if(data_collection.sem_notice == NULL)
        {
            data_collection.sem_notice = sem_open(PRINT_DATA_SHARE_POST_DATA_COLLECTION ".internal_noti", O_RDWR|O_CREAT, 0600, 0);
            if(data_collection.sem_notice == NULL)
            {
                LogError("sem_notif:%s failed!\n", PRINT_DATA_SHARE_POST_DATA_COLLECTION ".internal_noti");
                ret = -1;
                goto exit;
            }
        }
        if (pthread_create(&data_collection.tid, NULL, collection_data_thread, (void*)&data_collection)) {
            LogError("collection_realtime_thread create fail");
            ret = -1;
            goto exit;
        }            
        pthread_detach(data_collection.tid);
        data_collection.inited = true;
        data_collection.share_completion = true;
        data_collection.task_id = INDEX_NULL;
    }
    if(data_collection.share_completion == true)
    {
        if(data_collection.task_id != task_id && !TASK_FLAG_IS_SET(TASK_AT(task_id), TASK_FLAG_RECV_NOT_COMPLETE))
        {
           data_collection.task_id = task_id;
           data_collection.share_completion = false;
           sem_post(data_collection.sem_notice);
           ret = -1;
           goto exit;
        }
    }
    else    //waiting for data collection to complete
    {
        ret = -1;
        goto exit;
    }

exit:

    return ret;
}

static void send_next_buffer(int no_paper)
{
	uint16_t t = shm->task_queue.head;
	uint16_t buffers_sent = 0, buffers_to_send;
	int ret;
	int CuttingAutoLogo = 0;
/* save the TSPL sript to file */
/* If the task will not be e-printed, sent buffers can be freed */
#define save_script2file() do { \
	fwrite(BUFFER_AT(shm->task_queue.buf_to_send)->data, 1, BUFFER_AT(shm->task_queue.buf_to_send)->len, script_fp); \
	TASK_AT(t)->buffer_sent++; \
	buffers_sent++; \
	if (TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_DONT_REPRINT)) \
		free_buffers_to(t, shm->task_queue.buf_to_send); \
	shm->task_queue.buf_to_send = BUFFER_AT(shm->task_queue.buf_to_send)->next; \
} while (0)

	/*
	 * Return if the task queue is empty.
	 */
	if (!IS_VALID_TASK_INDEX(t))
		return;
	if (!IS_VALID_BUFFER_INDEX(shm->task_queue.buf_to_send))
		return;

	/*
	 * Return if there is no paper and the task's DONT_CARE_PAPER_STATUS flag is not set.
	 */
	if (!TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_DONT_CARE_PAPER_STATUS) && no_paper)
		return;

	//data collection
	if(!TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_DONT_PRINT) && collection_data_func(t) != 0)
	{
		return;
	}
	/*
	 * Set the maximum number of buffers to send for this time.
	 * If the task is still receiving, keep a margin of 1 buffers,
	 * otherwise, send maximum 128 buffers every 100ms.
	 */
	if (TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_RECV_NOT_COMPLETE)) {
		if (TASK_AT(t)->buffer_sent + 1 >= TASK_AT(t)->buffer_total)
			return;
		buffers_to_send = TASK_AT(t)->buffer_total - TASK_AT(t)->buffer_sent - 1;
	}
	else {
		buffers_to_send = 128;
	}

	/*
	 * Enable UTF-8 mode at the beginning of an MQTT task,
	 * and disable it at the end of the task.
	 */
	if (TASK_AT(t)->buffer_sent == 0 &&
		(TASK_AT(t)->chn_id == PRINT_CHN_ID_HTTPS ||
		 TASK_AT(t)->chn_id == PRINT_CHN_ID_MQTT ||
		 TASK_AT(t)->chn_id == PRINT_CHN_ID_AM ||
		 TASK_AT(t)->chn_id == PRINT_CHN_ID_CGI) &&
		Settings.Utf8_WordSet == 0) {
		/* Enable UTF-8 mode */
		Settings.Utf8_WordSet = 1;
		/* Set the flag to indicate that UTF-8 mode should be disabled at the end of the task */
		TASK_FLAG_SET(TASK_AT(t), TASK_FLAG_DISABLE_UTF8);
	}

	if (shm->task_queue.buf_to_send == TASK_AT(t)->head) {
		if (TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_REPRINTED_BY_FEED_KEY)) {
			printer_send_debug_string("\n>> REPRINT (FED,%d,%u) <<\n",
				TASK_AT(shm->task_queue.head)->id,
				TASK_AT(shm->task_queue.head)->copies);
			reback_flag = 0; //按键重打历史订单不回退省纸
		}
		print_log_config.cjk_wordset = Settings.CJK_WordSet;
		print_log_config.code_page = Settings.CodePage;
		print_log_config.utf8_wordset = Settings.Utf8_WordSet;
		print_log_config.locale = Settings.Locale;
		print_log_config.asc_wordset = Settings.ASC_WordSet;
		print_log_config.fine_mode = Settings.BitsPerDot;
#define P (Settings.HeaderFooterImage.Header)
		SavePrintTaskBitmap_Begin();
		CuttingAutoLogo = app_setting_get_int("printer", "CuttingAutoLogo", 0);
		if((TASK_AT(t)->chn_id != PRINT_CHN_ID_INT  && TASK_AT(t)->chn_id != PRINT_CHN_ID_CMD) && (!TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_DONT_PRINT))){
			print_chn_id_filter = 0;
			if((cutter_flag == 0 || cutter_flag == 1 || cutter_flag == 2 || cutter_flag == 3 || cutter_flag == 4)){
				if(cutter_flag == 1){ /*省纸模式下，当设置了上电后自动切纸，那么切纸后打印第一单之前如果已取纸则回退电机省纸，其余均不省纸*/
					if(Settings.FeedAndCutOnElectrify > 0 && shm->status.paper_not_taken == 0){
						if (reback_flag == 0) { /*重打不回退省纸*/
							if(CuttingAutoLogo > 0 && P.type != 0){ /*需要打印顶部图像*/
								PrintHeaderLogo(0);
							}
							reback_flag = 1;
						}else{
							if(CuttingAutoLogo > 0 && P.type != 0){
								PrintHeaderLogo(1);
							}else if(P.dots_back > 0){
								ReverseFeed(P.dots_back);
							}
						}
					}else{
						if(CuttingAutoLogo > 0 && P.type != 0){
							PrintHeaderLogo(0);
						}
					}
				}else if(cutter_flag == 2){ /*省纸模式下，当设置了合盖后自动切纸，那么切纸后打印第一单之前如果已取纸则回退电机省纸，其余均不省纸*/
					if(Settings.FeedAndCutOnCoverClosed > 0 && shm->status.paper_not_taken == 0){
						if (reback_flag == 0) { /*重打不回退省纸*/
							if(CuttingAutoLogo > 0 && P.type != 0){ /*需要打印顶部图像*/
								PrintHeaderLogo(0);
							}
							reback_flag = 1;
						}else{
							if(CuttingAutoLogo > 0 && P.type != 0){
								PrintHeaderLogo(1);
							}else if(P.dots_back > 0){
								ReverseFeed(P.dots_back);
							}
						}
					}else{
						if(CuttingAutoLogo > 0 && P.type != 0){
							PrintHeaderLogo(0);
						}
					}
				}else if(cutter_flag == 3){ /*前一次为全切*/
					if (reback_flag == 0) {
						if(CuttingAutoLogo > 0 && P.type != 0){
							PrintHeaderLogo(0);
						}
						reback_flag = 1; 
					}else{
						if(CuttingAutoLogo > 0 && P.type != 0){
							PrintHeaderLogo(1);
						}else if(P.dots_back > 0){
							ReverseFeed(P.dots_back);
						}
					}
				}else if(cutter_flag == 4){ /*前一次为半切*/
					if(shm->status.paper_not_taken == 0){ /*已取纸*/
						if (reback_flag == 0) {
							if(CuttingAutoLogo > 0 && P.type != 0){
								PrintHeaderLogo(0);
							}
							reback_flag = 1;
						}else{
							if(CuttingAutoLogo > 0 && P.type != 0){
								PrintHeaderLogo(1);
							}else if(P.dots_back > 0){
								ReverseFeed(P.dots_back);
							}						
						}
					}else{
						if(CuttingAutoLogo > 0 && P.type != 0){
							PrintHeaderLogo(0);
						}
					}
				}
				cutter_flag = 5;
			}
		}else{
			print_chn_id_filter = 1;
		}
#undef P
	}

	shm->task_queue.last_channel_id = TASK_AT(t)->chn_id;

	FILE *script_fp = NULL;
	static char scriptname[256] = {};
	while (IS_VALID_BUFFER_INDEX(shm->task_queue.buf_to_send) && buffers_sent < buffers_to_send) {
		if (!Settings.TSPLMode) {
			static int task_delete = 0;
			if (TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_DONT_PRINT)) {//需删除的打印任务，不解析丢弃
				if(task_delete != TASK_AT(t)->id){
					task_delete = TASK_AT(t)->id;
					printer_clear_buffer();
					reset_buffer_id();
				}
				ret = 0;
			}
			else{
				if (printer_send_data(NULL, 0) != 0) {
					LogError("printer_send_data failed.");
					return;
				}
				if (access("/tmp/dump_task_data", F_OK) == 0)
					dump_buffer(shm->task_queue.buf_to_send);

				if (TASK_AT(t)->chn_id == PRINT_CHN_ID_CMD)
					ret = printer_send_command(BUFFER_AT(shm->task_queue.buf_to_send)->data, BUFFER_AT(shm->task_queue.buf_to_send)->len);
				else {
					Cmd_ChnId = TASK_AT(t)->chn_id;
					ret = printer_send_data(BUFFER_AT(shm->task_queue.buf_to_send), shm->task_queue.buf_offset);
					Cmd_ChnId = -1;
				}
				if (ret == 0) {
					if (!TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_DONT_SEND_BUFFER_ID)) {
						if (!IS_VALID_BUFFER_INDEX(BUFFER_AT(shm->task_queue.buf_to_send)->next)) {
							SavePrintTaskBitmap_End();
							send_buffer_id(t, shm->task_queue.buf_to_send);
						}
					}
				}
			}
			if (ret == 0) {
				TASK_AT(t)->buffer_sent++;
				buffers_sent++;
				/* If the task will not be re-printed, sent buffers can be freed */
				if (TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_DONT_REPRINT))
					free_buffers_to(t, shm->task_queue.buf_to_send);
				shm->task_queue.buf_to_send = BUFFER_AT(shm->task_queue.buf_to_send)->next;
				shm->task_queue.buf_offset = 0;
			}
			else
				return;
		}
		else { /* TSPL mode */
			/* create the file to save sript */
			mkdir("/tmp/print", 0666);

			if (TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_RECV_NOT_COMPLETE)) {
				if (scriptname[0] == 0) { /* 从头开始接收文件 */
					get_scriptname(scriptname, sizeof(scriptname));
					if (!(script_fp = fopen(scriptname, "w"))) {
						LogError("create bas file(%s) failed! err:%s", scriptname, strerror(errno));
						return;
					}
					save_script2file();
				}
				else {
					if (script_fp == NULL) {
						if (!(script_fp = fopen(scriptname, "a"))) {
							LogError("open bas file(%s) failed! err:%s", scriptname, strerror(errno));
							return;
						}
					}
					save_script2file();
				}
			}
			else { /* TASK COMPLETED */
				if (scriptname[0] == 0) /* 有可能数据短，直接就task 完成 */
					get_scriptname(scriptname, sizeof(scriptname));
				if (script_fp == NULL) {
					if (!(script_fp = fopen(scriptname, "a"))) {
						LogError("open bas file(%s) failed! err:%s", scriptname, strerror(errno));
						return;
					}
				}
				save_script2file();
			}
		}
	}

	if (Settings.TSPLMode) {
		if (script_fp != NULL)
			fclose(script_fp);
		if (!TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_RECV_NOT_COMPLETE) && scriptname[0] != 0) {/* 脚本接收完毕，告诉处理线程开始解析脚本 */
			if (TASK_FLAG_IS_SET(TASK_AT(t), TASK_FLAG_DONT_PRINT)) {//需删除的打印任务，不解析丢弃
				unlink(scriptname);
				scriptname[0] = 0;
			}
			else{
				send_name2tspl_parser(scriptname);//FIXME: 临时调试屏蔽
				scriptname[0] = 0;
			}
		}
	}
}

static void PrnStatusToShmStatus(const PRN_STATUS *status)
{
	shm->status.printing                = (status->state       ) ? 1 : 0;
	shm->status.no_paper                = (status->paper & 0x01) ? 0 : 1;
#if 0
	shm->status.paper_running_out       = (status->paper & 0x02) ? 1 : 0;
#else
	shm->status.paper_running_out       = (0                   ); /* Ignore paper-running-out status */
#endif
	shm->status.paper_jam               = (status->paper & 0x04) ? 1 : 0;
	shm->status.paper_not_taken         = (status->paper & 0x08) ? 1 : 0;
	shm->status.platen_opened           = (status->cap         ) ? 0 : 1;
	shm->status.thermal_head_overheated = (status->temp    == 2) ? 1 : 0;
	shm->status.vpp_too_low             = (status->voltage == 1) ? 1 : 0;
	shm->status.vpp_too_high            = (status->voltage == 2) ? 1 : 0;
	shm->status.cutter                  = (status->cut   & 0x07);
	shm->status.paper_size              = (status->paper & 0x20) ? 1 : 0;
	shm->status.print_distance          = (status->printedMM   );
	shm->status.cashbox_open_count      = (status->cashbox_open_count);
	shm->status.motor_temperature       = (status->motor_temperature);
}

static void *print_mqueue_thread(void *arg)
{
	print_msg_t msg;
	print_paper_location_t location;
	uint8_t dataIn[8];
	uint32_t dataOutLength;
	int ret;

	while (g_process_running) {
		ret = mq_receive(mq, (char *)&msg, sizeof(msg), NULL);
		if (ret <= 0) {
			LogError("mq_receive() failed. errno=%d (%s)", errno, strerror(errno));
			continue;
		}

		switch (msg.cmd) {
		case PRINT_CMD_SET_FEEDING_STATE:
			dataIn[0] = (shm->status.step_motor_overheated == 0 && msg.set_value.value != 0) ? 1 : 0;
			set_feeding_state(dataIn[0]);
			break;

		case PRINT_CMD_SET_DENSITY:
			printer_set_density(msg.set_value.value, msg.set_value.save);
			break;

		case PRINT_CMD_SET_MAXSPEED:
			printer_set_maxspeed(msg.set_value.value, msg.set_value.save);
			break;

		case PRINT_CMD_GENERATE_PULSE:
			dataIn[0] = msg.generate_pulse.pin;
			*((uint16_t *)(dataIn + 1)) = msg.generate_pulse.on_time;
			*((uint16_t *)(dataIn + 3)) = msg.generate_pulse.off_time;
			PrnIoctl(PRN_CMD_OPEN_CASHBOX, dataIn, 5, NULL, NULL);
			break;

		case PRINT_CMD_GET_PAPER_LOCATION:
			dataOutLength = sizeof(location);
			if (PrnIoctl(PRN_CMD_GET_PAPER_LOCATION, NULL, 0, &location, &dataOutLength) == 0)
				memcpy(&shm->paper_location, &location, sizeof(location));
			break;

		case PRINT_CMD_SET_PAPER_NOT_TAKEN_ACTIONS:
			if (msg.set_value.value != shm->request.paper_not_taken_actions) {
				shm->request.paper_not_taken_actions = msg.set_value.value;
				app_setting_set_int(PRINTER_SECTION, "PaperNotTakenActions",
					shm->request.paper_not_taken_actions);
				dataIn[0] = msg.set_value.value;
				PrnIoctl(PRN_CMD_SET_PAPER_NOT_TAKEN_ACTION, dataIn, 1, NULL, NULL);
			}
			break;

		case PRINT_CMD_GET_SETTINGS:
			shm->settings.CJKMode      = Settings.CJKMode;
			shm->settings.CodePage     = Settings.CodePage;
			shm->settings.ASC_WordSet  = Settings.ASC_WordSet;
			shm->settings.CJK_WordSet  = Settings.CJK_WordSet;
			shm->settings.Utf8_WordSet = Settings.Utf8_WordSet;
			shm->settings.DataReady    = 1;
			break;

		case PRINT_CMD_GET_BAD_POINTS:
			*((uint32_t *)(dataIn    )) = msg.get_bad_points.from;
			*((uint32_t *)(dataIn + 4)) = msg.get_bad_points.dots;
			dataOutLength = sizeof(shm->bad_points.result);
			ret = PrnIoctl(PRN_CMD_GET_DOTS, dataIn, 8, shm->bad_points.result, &dataOutLength);
			shm->bad_points.state = ret;
			shm->bad_points.size = dataOutLength;
			break;

		case PRINT_CMD_HANDLE_REALTIME_COMMAND:
			Realtime_Proc(
				msg.handle_realtime_command.chn_id,
				msg.handle_realtime_command.data,
				msg.handle_realtime_command.len);
			break;

		default:
			break;
		}
	}

	return NULL;
}

static void feed_and_cut_timer_handler(uint32_t context)
{
	uint8_t buf[16];

	if (!shm->status.printing && !shm->status.no_paper &&
		!IS_VALID_TASK_INDEX(shm->task_queue.head)) {
		cutter_flag = 2;
		memset(buf, 0, sizeof(buf));
		buf[0] = 0x21;
		buf[1] = 1;
		*((uint32_t *)(buf + 2)) = Settings.FeedAndCutOnCoverClosed;
		printer_send_command(buf, 14);
	}
}

static void feed_and_cut_on_electrify(PRN_STATUS *status)
{
	unsigned char buf[16];
	
	if (Settings.FeedAndCutOnElectrify > 0 && status->cap == 1 &&
		!shm->status.printing && !shm->status.no_paper &&
		!IS_VALID_TASK_INDEX(shm->task_queue.head)) {
		cutter_flag = 1;
		memset(buf, 0, sizeof(buf));
		buf[0] = 0x21;
		buf[1] = 1;
		*((uint32_t *)(buf + 2)) = Settings.FeedAndCutOnElectrify;
		printer_send_command(buf, 14);
	}

}

void print_queue_run(void)
{
#define DELAY_MS (100)

	struct timespec ts;
	PRN_STATUS status, last_status;
	int last_paper_status, curr_paper_status, msec = DELAY_MS;
	uint32_t task_delay_ms = 0;
	uint8_t posts;

	/* 首次获取状态 */
	if (PrnGetStatus(&last_status) == 0) {
		PrnStatusToShmStatus(&last_status);
		/* 通知状态变化订阅进程 */
		for (posts = 0; posts < shm->request.status_change_subscribers; posts++)
			sem_post(sem_status);
	}

	last_paper_status = shm->status.no_paper + shm->status.platen_opened;
	feed_and_cut_on_electrify(&last_status);
	SysSleepStart(&ts);

	while (g_process_running) {
		if (PrnGetStatus(&status) == 0) {
			PrnStatusToShmStatus(&status);
			if (memcmp(&last_status, &status, sizeof(status))) {
				if (last_status.state == 0 && status.state != 0)
					LogDbg("print started");
				else if (last_status.state != 0 && status.state == 0)
					LogDbg("print stopped");
				/* 开盖后或已开始打印，停止走纸并切纸的定时器 */
				if (status.cap == 0 || (last_status.state == 0 && status.state != 0)) {
					simple_timer_stop(feed_and_cut_timer);
				}
				/* 使用普通热敏纸，合盖0.6秒后，如果无打印任务，走一段纸并切纸 */
				else {
					if (Settings.PaperLayout.sa == 48 &&
						last_status.cap == 0 &&
						status.state == 0) {
						if(Settings.FeedAndCutOnCoverClosed > 0)
							simple_timer_start(feed_and_cut_timer, 2000, 0);
						else
							cutter_flag = 2;
					}
				}
				/* 通知状态变化订阅进程 */
				for (posts = 0; posts < shm->request.status_change_subscribers; posts++)
					sem_post(sem_status);
				memcpy(&last_status, &status, sizeof(status));
			}
		}

		update_waterlevel();

		curr_paper_status = shm->status.no_paper + shm->status.platen_opened;
		if (last_paper_status == 0 && curr_paper_status != 0) { /* Out-of-paper detected */
			LogDbg("out-of-paper");
			out_of_paper_detected(status.cut_cmd_id);
		}
		last_paper_status = curr_paper_status;

		flush_channels(msec, shm->status.printing);

		if (shm->status.printing == 0) { /* The printer is idle (No data to print) */
			/*
			 * The task_delay_ms indicates the number of milliseconds to delay.
			 */
			if (task_delay_ms == 0)
				task_delay_ms = task_delay();
			if (task_delay_ms > (uint32_t)msec)
				task_delay_ms -= msec;
			else {
				task_delay_ms = 0;
				check_task_print_status();
			}
		}
		else
			task_delay_ms = 0;

		if ((shm->status.step_motor_overheated == 0) &&
			(shm->status.printing != 0 || task_delay_ms == 0))
			send_next_buffer(curr_paper_status);

		msec = (IS_VALID_TASK_INDEX(shm->task_queue.head) && IS_VALID_BUFFER_INDEX(shm->task_queue.buf_to_send)) ? 20 : DELAY_MS;
		SysSleep(&ts, msec);
	}

#undef DELAY_MS
}

void print_queue_exit(void)
{
	g_process_running = 0;
	request_post_change();
}

int print_queue_init(void)
{
	struct mq_attr attr = {
		.mq_flags = 0,
		.mq_maxmsg = 8,
		.mq_msgsize = PRINT_MQ_MSG_SIZE,
		.mq_curmsgs = 0
	};
	char name[64];
	int i, fd;

	WATERLEVEL_OVERHEATED_BEGIN     = (DOTS_PER_LINE == 384) ? 30000 : 6000; /* Keep feeding for n seconds */
	WATERLEVEL_OVERHEATED_END       = (DOTS_PER_LINE == 384) ? 6000  : 480;  /* =6000-23*240 */
	TEMPERATURE_FALLING_COEFFICIENT = (DOTS_PER_LINE == 384) ? 100   : 23;

#define OPEN_SEM(sem__,n__,v__) \
do { \
	sem__ = sem_open(n__, O_RDWR|O_CREAT, 0600, v__); \
	if (sem__ == SEM_FAILED) { \
		LogFatal("sem_open() failed. name=\"%s\", errno=%d (%s)", n__, errno, strerror(errno)); \
		return -1; \
	} \
} while (0)

	OPEN_SEM(sem_buffer_pool, PRINT_SERVER_SEM_BUFFER_POOL, 1);
	OPEN_SEM(sem_task_pool,   PRINT_SERVER_SEM_TASK_POOL,   1);
	OPEN_SEM(sem_task_queue,  PRINT_SERVER_SEM_TASK_QUEUE,  1);
	OPEN_SEM(sem_status,      PRINT_SERVER_SEM_STATUS,      0);
	OPEN_SEM(sem_request,     PRINT_SERVER_SEM_REQUEST,     0);

	for (i = 0; i < PRINT_CHN_ID_MAX; i++) {
		sprintf(name, PRINT_SERVER_SEM_CHANNELS, i);
		OPEN_SEM(sem_channels[i], name, 1);
	}

#undef OPEN_SEM

	fd = shm_open(PRINT_SERVER_SHM, O_RDWR|O_CREAT, 0600);
	if (fd == -1) {
		LogFatal("shm_open() failed. errno=%d (%s)", errno, strerror(errno));
		return -1;
	}

	if (ftruncate(fd, sizeof(print_shm_t))) {
		LogFatal("ftruncate() failed. errno=%d (%s)", errno, strerror(errno));
		return -1;
	}

	shm = (print_shm_t *)mmap(NULL, sizeof(print_shm_t),
			PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	close(fd);
	if (shm == MAP_FAILED) {
		LogFatal("mmap() failed. errno=%d (%s)", errno, strerror(errno));
		return -1;
	}

	mq = mq_open(PRINT_SERVER_MQ, O_RDONLY|O_CREAT, 0600, &attr);
	if (mq == -1) {
		LogFatal("mq_open() failed. errno=%d (%s)", errno, strerror(errno));
		return -1;
	}

	buffer_pool_init();
	task_pool_init();
	channels_init();
	task_queue_init();
	print_log_init();

	simple_timer_create(&feed_and_cut_timer, feed_and_cut_timer_handler);

	/*
	 * Create thread.
	 */
	if (pthread_create(&m_tid, &g_pthread_attr, print_mqueue_thread, NULL)) {
		LogFatal("pthread_create() failed.");
		return -1;
	}

	i = app_setting_get_int(PRINTER_SECTION, "PaperNotTakenActions", -1);
	if (i == -1) {
		shm->request.paper_not_taken_actions = PAPER_NOT_TAKEN_ACTION_PLAY_AUDIO;
		app_setting_set_int(PRINTER_SECTION, "PaperNotTakenActions",
			shm->request.paper_not_taken_actions);
	}
	else {
		shm->request.paper_not_taken_actions = i;
	}

	PrnIoctl(PRN_CMD_SET_PAPER_NOT_TAKEN_ACTION,
		&shm->request.paper_not_taken_actions, 1, NULL, NULL);

	strcpy(shm->fontlib.version, fontVersion);
	strcpy(shm->fontlib.language, fontLanguage);

	print_data_share_init(PRINT_DATA_SHARE_POST_DATA_COLLECTION,
		&pds_post_data_collection, true);
#define P (Settings.HeaderFooterImage.Header)
	if(app_setting_get_int("printer", "CuttingAutoLogo", 0) > 0 || P.dots_back > 0){
		cutter_flag = 1;
	}
#undef P
	return 0;
}
