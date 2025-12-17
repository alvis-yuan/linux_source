/*! \file print_api.c
 *  \brief Source file for the APIs to access the print server.
 */

#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "libcommon.h"

static sem_t *sem_buffer_pool;
static sem_t *sem_task_pool;
static sem_t *sem_channels[PRINT_CHN_ID_MAX];
static sem_t *sem_task_queue;
static sem_t *sem_status;
static sem_t *sem_request;
static print_shm_t *shm = NULL;

static pthread_t m_tid;
static int m_thread_running = 0;
static print_status_change_handler m_status_change_handler = NULL;

#define IS_INITIALIZED(rv__) \
do { \
	if (shm == NULL || shm == MAP_FAILED) { \
		if (print_api_init()) \
			return rv__; \
	} \
} while (0)

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

#define buffer_pool_lock()			SEM_TIMEDWAIT(sem_buffer_pool)
#define buffer_pool_unlock()		sem_post(sem_buffer_pool)
#define task_pool_lock()			SEM_TIMEDWAIT(sem_task_pool)
#define task_pool_unlock()			sem_post(sem_task_pool)
#define channels_lock(i)			SEM_TIMEDWAIT(sem_channels[i])
#define channels_unlock(i)			sem_post(sem_channels[i])
#define task_queue_lock()			SEM_TIMEDWAIT(sem_task_queue)
#define task_queue_unlock()			sem_post(sem_task_queue)
#define request_post_change()		sem_post(sem_request)

#define INDEX_NULL					(0xFFFF)
#define IS_VALID_BUFFER_INDEX(b)	((b) < PRINT_BUFFER_POOL_SIZE)
#define IS_VALID_TASK_INDEX(t)		((t) < PRINT_TASK_POOL_SIZE)
#define BUFFER_AT(b)				(&shm->buffer_pool.mem[b])
#define TASK_AT(t)					(&shm->task_pool.mem[t])

static int send_msg(const void *buf, size_t len)
{
	mqd_t mq;
	int rc = 0;

	mq = mq_open(PRINT_SERVER_MQ, O_WRONLY|O_NONBLOCK);
	if (mq == -1) {
		LogError("mq_open() failed. errno=%d (%s)", errno, strerror(errno));
		return -1;
	}

	if (mq_send(mq, buf, len, 0)) {
		LogError("mq_send() failed. errno=%d (%s)", errno, strerror(errno));
		rc = -1;
	}

	mq_close(mq);
	return rc;
}

static uint32_t handle_realtime_command(int chn_id, const uint8_t *data, uint32_t len)
{
	print_msg_t msg;
	uint32_t bytes_handled = 0;
	const char* blesetnetwork = "sunmibluetooth";

	if (len >= 3 &&
		data[0] == 0x10 &&
		data[1] == 0x04 &&
		data[2] >= 1 && data[2] <= 4) { /* DLE EOT n */
		bytes_handled = 3;
	}
	else if (len >= 3 &&
		data[0] == 0x1D &&
		data[1] == 0x49) { /* [GS I] Transmit printer ID */
		bytes_handled = 3;
	}
	else if (len >= 5 &&
		data[0] == 0x1B &&
		data[1] == 0x70) { /* ESC p m t1 t2 */
		bytes_handled = 5;
	}
	else if (len >= 6 &&
		data[0] == 0x1D &&
		data[1] == 0x28 &&
		data[2] == 0x54 &&
		data[3] >= 1) { /* GS ( T pL pH fn */
		bytes_handled = (data[4] << 8) + data[3] + 5;
		if(len < bytes_handled){
			LogError("%02x%02x%02x%02x need:%d, actual:%d, fail!", data[0],data[1],data[2],data[3], bytes_handled, len);
			return 0;
		}
	}
	else if (len > 8 &&
		data[0] == 0x1D &&
		data[1] == 0x28 &&
		data[2] == 0x45 &&
		data[3] == 0x03 &&
		data[4] == 0x0 &&
		data[5] == 0x13 ){   //start ble set network func
			if((len == 8+data[6]+data[7]) && (data[6] == strlen(blesetnetwork)) && (data[7] == strlen(sys_global_var()->sn)) && \
			  (chn_id == PRINT_CHN_ID_SPP || chn_id == PRINT_CHN_ID_SPP2 || chn_id == PRINT_CHN_ID_SPP3 || chn_id == PRINT_CHN_ID_SPP4 || chn_id == PRINT_CHN_ID_BLE)){
				BtStartParing();
				return len;
		}
	}
	else
		return 0;

	if (bytes_handled > PRINT_MQ_MSG_SIZE - 3)
		return 0;

	msg.cmd = PRINT_CMD_HANDLE_REALTIME_COMMAND;
	msg.handle_realtime_command.chn_id = chn_id;
	msg.handle_realtime_command.len = bytes_handled;
	memcpy(msg.handle_realtime_command.data, data, bytes_handled);
	send_msg(&msg, bytes_handled + 3);

	return bytes_handled;
}

/*----------------------------------------------*
 | BUFFER POOL                                  |
 *----------------------------------------------*/
static uint16_t buffer_new(void)
{
	uint16_t b;

	b = shm->buffer_pool.head;
	if (IS_VALID_BUFFER_INDEX(b)) {
		buffer_pool_lock();
		shm->buffer_pool.head = BUFFER_AT(b)->next;
		if (!IS_VALID_BUFFER_INDEX(shm->buffer_pool.head))
			shm->buffer_pool.tail = INDEX_NULL;
		shm->buffer_pool.free_count--;
		if (shm->buffer_pool.free_count < shm->buffer_pool.free_count_min)
			shm->buffer_pool.free_count_min = shm->buffer_pool.free_count;
		buffer_pool_unlock();
		BUFFER_AT(b)->next = INDEX_NULL;
		BUFFER_AT(b)->id = 0;
		BUFFER_AT(b)->len = 0;
	}
	else {
		LogFatal("buffer pool exhausted!");
	}

	return b;
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

/*----------------------------------------------*
 | TASK POOL                                    |
 *----------------------------------------------*/
static uint16_t task_new(void)
{
	uint16_t t;

	t = shm->task_pool.head;
	if (IS_VALID_TASK_INDEX(t)) {
		task_pool_lock();
		shm->task_pool.head = TASK_AT(t)->next;
		if (!IS_VALID_TASK_INDEX(shm->task_pool.head))
			shm->task_pool.tail = INDEX_NULL;
		shm->task_pool.free_count--;
		if (shm->task_pool.free_count < shm->task_pool.free_count_min)
			shm->task_pool.free_count_min = shm->task_pool.free_count;
		task_pool_unlock();
		memset(TASK_AT(t), 0, sizeof(print_task_t));
		TASK_AT(t)->prev = INDEX_NULL;
		TASK_AT(t)->next = INDEX_NULL;
		TASK_AT(t)->head = INDEX_NULL;
		TASK_AT(t)->tail = INDEX_NULL;
	}
	else {
		LogFatal("task pool exhausted!");
	}

	return t;
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

static void append_buffer_to_task(uint16_t t, uint16_t b)
{
	BUFFER_AT(b)->id = TASK_AT(t)->next_buffer_id++;
	if (IS_VALID_BUFFER_INDEX(TASK_AT(t)->tail))
		BUFFER_AT(TASK_AT(t)->tail)->next = b;
	else
		TASK_AT(t)->head = b;
	TASK_AT(t)->tail = b;
	TASK_AT(t)->buffer_total++;
}

static void append_task_to_channel_task_queue(int chn_id, uint16_t t)
{
	print_channel_t *channel = &shm->channels[chn_id];

	if (IS_VALID_TASK_INDEX(channel->task_queue.tail))
		TASK_AT(channel->task_queue.tail)->next = t;
	else
		channel->task_queue.head = t;
	/*记录打印任务的接收时间*/
	time(&shm->print_log_mem[t].reception_time);
	channel->task_queue.tail = t;
}

static int next_task_id(void)
{
	int id;

	task_pool_lock();
	id = shm->task_pool.next_task_id;
	if (++shm->task_pool.next_task_id <= 0)
		shm->task_pool.next_task_id = 1;
	task_pool_unlock();
	return id;
}

static int append_data_to_channel(int chn_id, const void *data, uint32_t len, uint32_t flag)
{
#define RETURN(r) do { rc = (r); goto _exit; } while (0)

	print_channel_t *p;
	uint32_t b, l, bytes_copied, free_bytes, ignored_bytes = 0;
	int rc = 0;

	if (chn_id < 0 || chn_id >= PRINT_CHN_ID_MAX || !data)
		return -1;

	if (len == 0)
		return 0;

	p = &shm->channels[chn_id];

	channels_lock(chn_id);

	if (!IS_VALID_TASK_INDEX(p->task)) {
		/*
		 * Handle realtime commands.
		 */
		ignored_bytes = handle_realtime_command(chn_id, data, len);
		len -= ignored_bytes;
		if (len == 0)
			RETURN(0);

		p->task = task_new();
		if (!IS_VALID_TASK_INDEX(p->task))
			RETURN(-1);
		TASK_AT(p->task)->chn_id = chn_id;
		TASK_AT(p->task)->id = next_task_id();
		TASK_AT(p->task)->next_buffer_id = 1;
		p->last_task_id = TASK_AT(p->task)->id;
		/*记录打印任务的接收时间*/
		time(&shm->print_log_mem[p->task].reception_time);
	}

	bytes_copied = 0;

	while (len > 0) {
		b = TASK_AT(p->task)->tail;
		free_bytes = (IS_VALID_BUFFER_INDEX(b)) ? (PRINT_BUFFER_SIZE - BUFFER_AT(b)->len) : 0;
		if (free_bytes == 0) {
			b = buffer_new();
			if (!IS_VALID_BUFFER_INDEX(b))
				RETURN(-1);
			append_buffer_to_task(p->task, b);
			free_bytes = PRINT_BUFFER_SIZE;
		}
		l = (len > free_bytes) ? free_bytes : len;
		memcpy(BUFFER_AT(b)->data + BUFFER_AT(b)->len, data + ignored_bytes + bytes_copied, l);
		BUFFER_AT(b)->len += l;
		len -= l;
		bytes_copied += l;
	}

	TASK_AT(p->task)->bytes_received += bytes_copied;
	p->no_data_received = 0;

	TASK_FLAG_SET(TASK_AT(p->task), flag);

	rc = TASK_AT(p->task)->id;

_exit:
	channels_unlock(chn_id);
	return rc;

#undef RETURN
}

static void *status_thread(void *arg)
{
	print_status_t status;
	int rc;

	task_queue_lock();
	shm->request.status_change_subscribers++;
	task_queue_unlock();

	while (m_thread_running) {
		rc = sem_wait(sem_status);
		if (rc == 0 && m_status_change_handler) {
			memcpy(&status, &shm->status, sizeof(print_status_t));
			(*m_status_change_handler)(&status);
		}
	}

	task_queue_lock();
	if (shm->request.status_change_subscribers > 0)
		shm->request.status_change_subscribers--;
	task_queue_unlock();

	return NULL;
}

int print_set_feeding_state(uint8_t state)
{
	print_msg_t msg;

	msg.cmd = PRINT_CMD_SET_FEEDING_STATE;
	msg.set_value.value = state;
	msg.set_value.save = false;

	return send_msg(&msg, sizeof(msg.set_value));
}

int print_buffer_is_full(uint32_t bytes_to_reserve)
{
	uint32_t count = 1;

	IS_INITIALIZED(1);

	if (bytes_to_reserve > 0)
		count = (bytes_to_reserve - 1) / PRINT_BUFFER_SIZE + 1;
	return (shm->buffer_pool.free_count < count) ? 1 : 0;
}

int print_channel_send_data_with_flag(int chn_id, const void *data, uint32_t len, uint32_t copies, uint32_t flag)
{
#define RETURN(r) do { rc = (r); goto _exit; } while (0)

	uint32_t t, b, l, bytes_copied, ignored_bytes = 0;
	int rc = 0;

	IS_INITIALIZED(-1);

	if (copies == 0)
		return append_data_to_channel(chn_id, data, len, flag);

	if (chn_id < 0 || chn_id >= PRINT_CHN_ID_MAX || !data)
		return -1;

	if (len == 0)
		return 0;

	channels_lock(chn_id);

	/*
	 * Handle realtime commands.
	 */
	ignored_bytes = handle_realtime_command(chn_id, data, len);
	len -= ignored_bytes;
	if (len == 0)
		RETURN(0);

	t = task_new();
	if (!IS_VALID_TASK_INDEX(t))
		RETURN(-1);
	TASK_AT(t)->chn_id = chn_id;
	TASK_AT(t)->id = next_task_id();
	TASK_AT(t)->copies = copies;
	TASK_AT(t)->next_buffer_id = 1;
	shm->channels[chn_id].last_task_id = TASK_AT(t)->id;

	bytes_copied = 0;

	while (len > 0) {
		b = buffer_new();
		if (!IS_VALID_BUFFER_INDEX(b)) {
			task_free(t);
			RETURN(-1);
		}
		append_buffer_to_task(t, b);
		l = (len > PRINT_BUFFER_SIZE) ? PRINT_BUFFER_SIZE : len;
		memcpy(BUFFER_AT(b)->data, data + ignored_bytes + bytes_copied, l);
		BUFFER_AT(b)->len = l;
		len -= l;
		bytes_copied += l;
	}

	TASK_AT(t)->bytes_received = bytes_copied;

	TASK_FLAG_SET(TASK_AT(t), flag);

	append_task_to_channel_task_queue(chn_id, t);

	rc = TASK_AT(t)->id;

_exit:
	channels_unlock(chn_id);
	return rc;
}

int print_channel_send_data(int chn_id, const void *data, uint32_t len, uint32_t copies)
{
	return print_channel_send_data_with_flag(chn_id, data, len, copies, 0);
}

int print_channel_send_data_with_flag_cloud(int chn_id, const void *data, uint32_t len, uint32_t copies, uint32_t flag, char *orderNo)
{
	#define RETURN(r) do { rc = (r); goto _exit; } while (0)

	uint32_t t, b, l, bytes_copied, ignored_bytes = 0;
	int rc = 0;

	IS_INITIALIZED(-1);

	if (copies == 0)
		return append_data_to_channel(chn_id, data, len, flag);
	if (chn_id < 0 || chn_id >= PRINT_CHN_ID_MAX || !data)
		return -1;
	if (len == 0)
		return 0;

	channels_lock(chn_id);
	ignored_bytes = handle_realtime_command(chn_id, data, len);
	len -= ignored_bytes;
	if (len == 0)
		RETURN(0);

	t = task_new();
	if (!IS_VALID_TASK_INDEX(t))
		RETURN(-1);
	TASK_AT(t)->chn_id = chn_id;
	strncpy(shm->print_log_mem[t].orderId, orderNo, sizeof(shm->print_log_mem[t].orderId) - 1);
	shm->print_log_mem[t].orderId[sizeof(shm->print_log_mem[t].orderId) - 1] = '\0';
	TASK_AT(t)->id = next_task_id();
	TASK_AT(t)->copies = copies;
	TASK_AT(t)->next_buffer_id = 1;
	shm->channels[chn_id].last_task_id = TASK_AT(t)->id;

	bytes_copied = 0;

	while (len > 0) {
		b = buffer_new();
		if (!IS_VALID_BUFFER_INDEX(b)) {
			task_free(t);
			RETURN(-1);
		}
		append_buffer_to_task(t, b);
		l = (len > PRINT_BUFFER_SIZE) ? PRINT_BUFFER_SIZE : len;
		memcpy(BUFFER_AT(b)->data, data + ignored_bytes + bytes_copied, l);
		BUFFER_AT(b)->len = l;
		len -= l;
		bytes_copied += l;
	}

	TASK_AT(t)->bytes_received = bytes_copied;
	TASK_FLAG_SET(TASK_AT(t), flag);
	append_task_to_channel_task_queue(chn_id, t);
	rc = TASK_AT(t)->id;

_exit:
	channels_unlock(chn_id);
	return rc;
}

int print_channel_send_data_cloud(int chn_id, const void *data, uint32_t len, uint32_t copies, char *orderNo)
{
	return print_channel_send_data_with_flag_cloud(chn_id, data, len, copies, 0, orderNo);
}

int print_supplemental_data(const char *seqnum, time_t timestamp, const void *data, uint32_t len, uint32_t copies)
{
#define NO_SUPPLEMENT_PRINTED  0
#define SEQ_NOT_SAME          (-1)
#define OUT_OF_DATE           (-2)
#define TASKID_NOT_SAME       (-3)

	int task_id;
	time_t ts;

	IS_INITIALIZED(-1);

	if (!strlen(shm->data_collection.seqnum))
		return NO_SUPPLEMENT_PRINTED;
	if (strcmp(shm->data_collection.seqnum, seqnum))
		return SEQ_NOT_SAME;
	time(&ts);
	if (ts > timestamp)
		return OUT_OF_DATE;
	if (shm->data_collection.task_id + 1 != shm->task_pool.next_task_id)
		return TASKID_NOT_SAME;

	task_id = print_channel_send_data_with_flag(PRINT_CHN_ID_MQTT, data, len, copies, TASK_FLAG_DONT_COLLECT_DATA);

	return task_id;

#undef NO_SUPPLEMENT_PRINTED
#undef SEQ_NOT_SAME
#undef OUT_OF_DATE
#undef TASKID_NOT_SAME
}

int print_task_is_complete(int task_id)
{
	IS_INITIALIZED(0);
	return (task_id <= shm->task_pool.last_completed_task_id) ? 1 : 0;
}

int print_task_is_complete_or_reset(int task_id)
{
	IS_INITIALIZED(0);
	if(task_id <= shm->task_pool.last_completed_task_id){
		return 1;
	}
	else{
		if(shm->task_pool.last_completed_task_id == 0 && shm->task_pool.next_task_id == 1){ //PrintServer reset
			return -1;
		}
		else{
			return 0;
		}
	}
}

int print_task_set_flag(int task_id, uint32_t flag)
{
	uint16_t t;

	IS_INITIALIZED(-1);

	if (task_id < 0)
		return -1;

	if (task_id == 0) {
		if (IS_VALID_TASK_INDEX(shm->task_queue.head))
			TASK_FLAG_SET(TASK_AT(shm->task_queue.head), flag);
		return 0;
	}

	t = shm->task_queue.head;
	while (IS_VALID_TASK_INDEX(t)) {
		if (TASK_AT(t)->id == task_id) {
			TASK_FLAG_SET(TASK_AT(t), flag);
			return 0;
		}
		t = TASK_AT(t)->next;
	}

	return -1;
}

int print_task_append_data(uint16_t task_index, uint16_t after, const void *data, uint32_t len)
{
	print_task_t *task;
	const uint8_t *d = (const uint8_t *)data;
	uint16_t b, l, p, n;

	IS_INITIALIZED(-1);

	if (!IS_VALID_TASK_INDEX(task_index))
		return -1;

	task = TASK_AT(task_index);
	p = INDEX_NULL;
	n = task->head;
	while (IS_VALID_BUFFER_INDEX(n)) {
		if (BUFFER_AT(n)->id > after)
			break;
		p = n;
		n = BUFFER_AT(n)->next;
	}

	while (len > 0) {
		b = buffer_new();
		if (!IS_VALID_BUFFER_INDEX(b))
			return -1;
		if (IS_VALID_BUFFER_INDEX(p))
			BUFFER_AT(p)->next = b;
		else
			task->head = b;
		p = b;
		l = (len > PRINT_BUFFER_SIZE) ? PRINT_BUFFER_SIZE : len;
		memcpy(BUFFER_AT(b)->data, d, l);
		BUFFER_AT(b)->len = l;
		len -= l;
		d += l;
	}
	BUFFER_AT(p)->next = n;

	task->next_buffer_id = 1;
	task->buffer_total = 0;
	b = task->head;
	while (IS_VALID_BUFFER_INDEX(b)) {
		BUFFER_AT(b)->id = task->next_buffer_id++;
		task->buffer_total++;
		task->tail = b;
		b = BUFFER_AT(b)->next;
	}

	return 0;
}

/*
切刀指令汇总：
    1B 69		半切
    1B 6D		半切
    1D 56 00	全切
    1D 56 30	全切
    1D 56 01	半切
    1D 56 31	半切
    1D 56 61 n	延迟切纸、全切，
    1D 56 62 n	延迟切纸、半切
    1D 56 41 n	立即切纸、全切、n+=144
    1D 56 67 n	立即切纸、全切、n+=144
    1D 56 42 n	立即切纸、半切、n+=144
    1D 56 68 n	立即切纸、半切、n+=144
注：n表示在当前打印位置起走纸n点，然后执行切刀
返回值ret：-1:错误；0:无切刀，其他（正整）:切刀指令的起始位置（地址为data[ret-1]）
*/
static int check_cutting_cmd(uint8_t *data, uint32_t len, uint32_t *blank_line_num)
{
    uint32_t i = 0;
    uint8_t buf[3] = {0}, index = 0;
    uint8_t index_pre = 0;
    int32_t get = -1;
    
    if(data == NULL || len < 2 || blank_line_num == NULL)
        return -1;
    *blank_line_num = 0;
    for(i=0; i<len; i++)
    {            
        switch(index)
        {
        case 0:
            if(data[i] == 0x1B || data[i] == 0x1D)
            {
                buf[index] = data[i];
                index = 1;
            }
            break;
        case 1:
            if(buf[0] == 0x1B && (data[i] == 0x69 || data[i] == 0x6D))
            {
                get =  i;
            }
            else if(buf[0] == 0x1D && data[i] == 0x56)
            {
                buf[index] = data[i];
                index = 2;
            }
            else
            {
                index = 0;
            }
            break;
        case 2:
            if(buf[0] == 0x1D && buf[1] == 0x56 && (data[i] == 0x0 || data[i] == 0x01 || data[i] == 0x30 || data[i] == 0x31))
            {
                get = (i-1);
            }
            else if(buf[0] == 0x1D && buf[1] == 0x56 && (data[i] == 0x41 || data[i] == 0x42 || data[i] == 0x61 || \
                data[i] == 0x62 || data[i] == 0x67 || data[i] == 0x68) && i+1 < len)
            {
                get = (i-1);
            }
            else
            {
                index = 0;
            }
            break;
        default:
            index = 0;
            break;
        }
        //updata blank line count
        if(data[i] == 0x0A && index == 0)
        {
            *blank_line_num += 1;
        }
        else
        {
            if(index <= index_pre && get < 0) //get cmd
                *blank_line_num = 0;
        }
        index_pre = index;
        
        if(get >= 0)
            return get;
    }

    return 0;
}

int print_task_append_data_before_final_cutting(uint16_t task_index, void **data, uint32_t *data_len)
{
#define MAXLEN 64
	print_task_t *task;
	uint8_t *d = NULL;
    uint32_t len = 0;
	uint16_t b, l, p, n;
    uint32_t bytes_received = 0;
    uint16_t last_1 = INDEX_NULL, target = INDEX_NULL;
    uint8_t tmpbuf[MAXLEN] = {0};
    uint16_t i = 0, j = 0, buflen = 0;
    uint8_t *tmp_data = NULL;
    uint32_t tmp_data_len = 0;
    int index = 0;
    uint32_t blank_line_num = 0, blank_line_add = 0;

    if(data == NULL || *data == NULL || data_len == NULL || *data_len == 0)        
        return -1;
    d = *(uint8_t **)data;
    len = *data_len;
    
	IS_INITIALIZED(-1);
	if (!IS_VALID_TASK_INDEX(task_index))
		return -1;

	task = TASK_AT(task_index);
	p = INDEX_NULL;
	n = task->head;
	while (IS_VALID_BUFFER_INDEX(n)) {
        last_1 = p;
		p = n;
		n = BUFFER_AT(n)->next;
	}

    //get last buflen
    if(p == INDEX_NULL)
        return -1;
    i = BUFFER_AT(p)->len;
    if(i < MAXLEN)
    {
        if(last_1 != INDEX_NULL)
        {
            j = BUFFER_AT(last_1)->len; 
            if(i + j >= MAXLEN)
            {
                memcpy(tmpbuf, &BUFFER_AT(last_1)->data[j-(MAXLEN-i)], MAXLEN-i);
                memcpy(tmpbuf+(MAXLEN-i), BUFFER_AT(p)->data, i);
                buflen = MAXLEN;
            }
            else
            {
                memcpy(tmpbuf, &BUFFER_AT(last_1)->data, j);
                memcpy(tmpbuf+j, BUFFER_AT(p)->data, i);
                buflen = i+j;
            }
        }
        else
        {
            memcpy(tmpbuf, BUFFER_AT(p)->data, i);
            buflen = i;
        }
    }
    else
    {
        memcpy(tmpbuf, &BUFFER_AT(p)->data[i-MAXLEN], MAXLEN);
        buflen = MAXLEN;
    }
    //check last cutting cmd
    index = check_cutting_cmd(tmpbuf, buflen, &blank_line_num);
    if(index < 0)
    {    
        LogInfo("check_cutting_cmd failed");
        return 0;
    }
    else if(index == 0 && blank_line_num == 0)  //订单末尾无切刀无0A
    {
        LogInfo("append to final");
        if(strcmp(sys_global_var()->project,"NT211") == 0)
            blank_line_add = 4;     //58打印机附加4个0A
        else
            blank_line_add = 7;     //80打印机附加7个0A
        tmp_data = realloc(d, len + blank_line_add);
        if(tmp_data == NULL)
            return -1;
        *data = tmp_data;
        *data_len = len + blank_line_add;
        memset(tmp_data+len, 0x0A, blank_line_add);
        target = p;
        d = tmp_data;
        len = *data_len;
    }
    else{
        //get cutting cmd buff index
        LogInfo("index %d, blank:%u", index, blank_line_num);
        if(index == 0){  //订单末尾无切刀有0A
            index = buflen + 1; //index为指向buflen的下一个地址
            if(strcmp(sys_global_var()->project,"NT211") == 0)
                blank_line_add = blank_line_num > 3 ? 0 : (4 - blank_line_num); //58打印机附加4个0A
            else
                blank_line_add = blank_line_num > 6 ? 0 : (7 - blank_line_num); //80打印机附加7个0A
        }
        else{   //订单末尾有切刀，80需要4个0A；58也是(切刀指令不执行)
            blank_line_add = blank_line_num > 3 ? 0 : (4 - blank_line_num);
        }
        index -= blank_line_num; 
        len += blank_line_add;
        if(buflen > i && index <= buflen - i)    //last_1
        {
            index = j - ((buflen - i) - index) - 1;
            target = last_1;
            n = p;  //p = BUFFER_AT(last_1)->next;
        }
        else    //p
        {     
            index = index - (buflen - i) - 1;
            if(len <= PRINT_BUFFER_SIZE - i)
            {
                memmove(BUFFER_AT(p)->data + index + len, BUFFER_AT(p)->data + index, BUFFER_AT(p)->len - index);
                memcpy(BUFFER_AT(p)->data + index, d, len - blank_line_add);
                if(blank_line_add)
                    memset(BUFFER_AT(p)->data + index + len - blank_line_add, 0x0A, blank_line_add);
                BUFFER_AT(p)->len += len;
                //LogInfo("No additional memory blocks required, index:%d, blank_line_add:%u, len:%u, next:%u", index, blank_line_add, len, BUFFER_AT(p)->next);
                goto exit;
            }         
            target = p;
        }
        //LogInfo("new index:%d, blank_line_add:%u, len:%u, target_buf_len:%u", index, blank_line_add, len, BUFFER_AT(target)->len);
        //append data
        tmp_data = realloc(d, len + (BUFFER_AT(target)->len - index));
        if(tmp_data == NULL)
            return -1;
        tmp_data_len = len + (BUFFER_AT(target)->len - index);
        *data = tmp_data;
        *data_len = tmp_data_len;
        if(blank_line_add)
        {
            memset(tmp_data + (len - blank_line_add), 0x0A, blank_line_add);
        }
        memcpy(tmp_data + len, BUFFER_AT(target)->data + index, (BUFFER_AT(target)->len - index));
        d = tmp_data;
        memcpy(BUFFER_AT(target)->data + index, d, (BUFFER_AT(target)->len - index));
        d += BUFFER_AT(target)->len - index;
        len = tmp_data_len - (d - tmp_data);
        //LogInfo("tmp_data_len:%u, len:%u", tmp_data_len, len);
    }

    //append remain data
	while (len > 0) {
		b = buffer_new();
		if (!IS_VALID_BUFFER_INDEX(b))
			return -1;
		if (IS_VALID_BUFFER_INDEX(target))
			BUFFER_AT(target)->next = b;
		else
			task->head = b;
		target = b;
		l = (len > PRINT_BUFFER_SIZE) ? PRINT_BUFFER_SIZE : len;
		memcpy(BUFFER_AT(b)->data, d, l);
		BUFFER_AT(b)->len = l;
		len -= l;
		d += l;
	}
	BUFFER_AT(target)->next = n;

exit:
	task->next_buffer_id = 1;
	task->buffer_total = 0;
	b = task->head;
	while (IS_VALID_BUFFER_INDEX(b)) {
		BUFFER_AT(b)->id = task->next_buffer_id++;
		task->buffer_total++;
		task->tail = b;
        bytes_received += BUFFER_AT(b)->len;
		b = BUFFER_AT(b)->next;
	}
    //LogInfo("bytes_received %d, %d", task->bytes_received, bytes_received);
    //task->bytes_received = bytes_received;

#undef MAXLEN

	return 0;
}


int print_get_status(print_status_t *status)
{
	IS_INITIALIZED(-1);
	memcpy(status, &shm->status, sizeof(print_status_t));
	return 0;
}

int print_get_settings(print_settings_t *settings)
{
	print_msg_t msg;
	uint32_t tries;

	IS_INITIALIZED(-1);

	shm->settings.DataReady = 0;

	msg.cmd = PRINT_CMD_GET_SETTINGS;

	tries = 100;
	do {
		if (send_msg(&msg, sizeof(msg.cmd)))
			return -1;
		usleep(1000);
		if (shm->settings.DataReady != 0) {
			memcpy(settings, &shm->settings, sizeof(print_settings_t));
			return 0;
		}
	} while (tries-- > 0);

	return -1;
}

int print_status_change_subscribe(print_status_change_handler handler)
{
	IS_INITIALIZED(-1);

	if (!handler)
		return -1;

	m_status_change_handler = handler;

	if (!m_thread_running) {
		m_thread_running = 1;
		if (pthread_create(&m_tid, NULL, status_thread, NULL)) {
			LogError("pthread_create() failed.");
			return -1;
		}
		pthread_detach(m_tid);
	}

	return 0;
}

int print_status_change_unsubscribe(void)
{
	if (m_thread_running) {
		m_thread_running = 0;
		pthread_kill(m_tid, SIGUSR1);
	}
	return 0;
}

int print_get_bad_points(uint32_t from, uint32_t dots, void *result, uint32_t *size)
{
	print_msg_t msg;

	IS_INITIALIZED(-1);

	if (shm->bad_points.state == PRN_BAD_POINT_DETECT_FINISHED) {
		if ((*size) > shm->bad_points.size)
			(*size) = shm->bad_points.size;
		memcpy(result, shm->bad_points.result, (*size));
		//sync the state as it in kernel that it would be
		//PRN_BAD_POINT_DETECT_INIT automatically after PRN_BAD_POINT_DETECT_FINISHED
		shm->bad_points.state = PRN_BAD_POINT_DETECT_INIT;
		return PRN_BAD_POINT_DETECT_FINISHED;
	}

	msg.cmd = PRINT_CMD_GET_BAD_POINTS;
	msg.get_bad_points.from = from;
	msg.get_bad_points.dots = dots;
	if (send_msg(&msg, sizeof(msg.get_bad_points)))
		return -1;
	return shm->bad_points.state;
}

int print_set_density(uint8_t density, bool save)
{
	print_msg_t msg;

	msg.cmd = PRINT_CMD_SET_DENSITY;
	msg.set_value.value = density;
	msg.set_value.save = save;

	return send_msg(&msg, sizeof(msg.set_value));
}

int print_set_maxspeed(uint8_t maxspeed, bool save)
{
	print_msg_t msg;

	msg.cmd = PRINT_CMD_SET_MAXSPEED;
	msg.set_value.value = maxspeed;
	msg.set_value.save = save;

	return send_msg(&msg, sizeof(msg.set_value));
}

int print_generate_pulse(uint8_t pin, uint16_t on_time, uint16_t off_time)
{
	print_msg_t msg;

	msg.cmd = PRINT_CMD_GENERATE_PULSE;
	msg.generate_pulse.pin = pin;
	msg.generate_pulse.on_time = on_time;
	msg.generate_pulse.off_time = off_time;

	return send_msg(&msg, sizeof(msg.generate_pulse));
}

int print_get_fontlib_version(char *buf)
{
	IS_INITIALIZED(-1);
	strcpy(buf, shm->fontlib.version);
	return 0;
}

int print_get_fontlib_language(char *buf)
{
	IS_INITIALIZED(-1);
	strcpy(buf, shm->fontlib.language);
	return 0;
}

int print_set_task_logging_state(int enabled)
{
	IS_INITIALIZED(-1);
	shm->request.task_logging = (enabled) ? 1 : 0;
	return 0;
}

int print_set_task_print_strategy(int strategy)
{
	IS_INITIALIZED(-1);
	shm->request.task_print_strategy = strategy;
	return 0;
}

int print_get_last_channel_id(void)
{
	IS_INITIALIZED(-1);
	return shm->task_queue.last_channel_id;
}

int print_data_share_init(const char *name, print_data_share_t *pds, bool create)
{
	memset(pds, 0, sizeof(print_data_share_t));
	if (create) {
		pds->sem_notification = sem_open(PRINT_DATA_SHARE_POST_DATA_COLLECTION ".noti", O_RDWR|O_CREAT, 0600, 0);
		pds->sem_lock         = sem_open(PRINT_DATA_SHARE_POST_DATA_COLLECTION ".lock", O_RDWR|O_CREAT, 0600, 1);
		pds->sem_completion   = sem_open(PRINT_DATA_SHARE_POST_DATA_COLLECTION ".cmpl", O_RDWR|O_CREAT, 0600, 0);
	}
	else {
		pds->sem_notification = sem_open(PRINT_DATA_SHARE_POST_DATA_COLLECTION ".noti", O_RDWR);
		pds->sem_lock         = sem_open(PRINT_DATA_SHARE_POST_DATA_COLLECTION ".lock", O_RDWR);
		pds->sem_completion   = sem_open(PRINT_DATA_SHARE_POST_DATA_COLLECTION ".cmpl", O_RDWR);
	}
	if (pds->sem_notification == SEM_FAILED ||
		pds->sem_lock         == SEM_FAILED ||
		pds->sem_completion   == SEM_FAILED) {
		LogError("sem_open() failed. errno=%d (%s)", errno, strerror(errno));
		return -1;
	}
	return 0;
}

int print_data_share_notify(print_data_share_t *pds)
{
	int sv, count, retries;

	if (sem_getvalue(pds->sem_notification, &sv) != 0)
		return -1;
	if (sv > 0) { /* No one is waiting for it */
		while (sv > 0) {
			sem_wait(pds->sem_notification);
			sem_getvalue(pds->sem_notification, &sv);
		}
		return 0;
	}

	count = 0;
	/* Take the lock */
	sem_wait(pds->sem_lock);

	while (1) {
		/* Try to wake someone up */
		sem_post(pds->sem_notification);
		retries = 10;
		while (retries-- > 0) {
			SysDelay(10);
			sem_getvalue(pds->sem_notification, &sv);
			if (sv > 0)
				retries = 0;
		}
		if (sv > 0) {
			/* Decrease the semaphore's value to make it zero */
			sem_wait(pds->sem_notification);
			break;
		}
		/* Someone wakes up, and he is trying to obtain the lock */
		count++;
	}
	LogInfo("%d user(s) notified.", count);

	/* Release the lock */
	sem_post(pds->sem_lock);

	/* Wait for completion of the data sharing */
	while (count > 0) {
		sem_wait(pds->sem_completion);
		count--;
	}

	LogInfo("finish data sharing.");

	return 0;
}

int print_data_share_wait(print_data_share_t *pds)
{
	if (sem_wait(pds->sem_notification) != 0)
		return -1;
	sem_wait(pds->sem_lock);
	return 0;
}

int print_data_share_complete(print_data_share_t *pds)
{
	sem_post(pds->sem_lock);
	sem_post(pds->sem_completion);
	return 0;
}

const print_status_t *print_status(void)
{
	static print_status_t status = {};

	return (shm) ? &shm->status : &status;
}

const print_shm_t *print_shm(void)
{
	return shm;
}

const print_task_t *task_at(uint16_t t)
{
	return (IS_VALID_TASK_INDEX(t)) ? TASK_AT(t) : NULL;
}

int print_api_init(void)
{
	char name[64];
	int i, fd;

#if 0
#define P(name) printf("sizeof(" #name "): %u\n", sizeof(name))
	P(void*);
	P(print_buffer_t);
#undef P
	printf("%u\n", __builtin_offsetof(print_buffer_t, next));
	printf("%u\n", __builtin_offsetof(print_buffer_t, id));
	printf("%u\n", __builtin_offsetof(print_buffer_t, len));
	printf("%u\n", __builtin_offsetof(print_buffer_t, data));
#endif

#define OPEN_SEM(sem__,n__) \
do { \
	if (sem__ == NULL || sem__ == SEM_FAILED) { \
		sem__ = sem_open(n__, O_RDWR); \
		if (sem__ == SEM_FAILED) { \
			LogFatal("sem_open() failed. name=\"%s\", errno=%d (%s)", n__, errno, strerror(errno)); \
			return -1; \
		} \
	} \
} while (0)

	OPEN_SEM(sem_buffer_pool, PRINT_SERVER_SEM_BUFFER_POOL);
	OPEN_SEM(sem_task_pool,   PRINT_SERVER_SEM_TASK_POOL  );
	OPEN_SEM(sem_task_queue,  PRINT_SERVER_SEM_TASK_QUEUE );
	OPEN_SEM(sem_status,      PRINT_SERVER_SEM_STATUS     );
	OPEN_SEM(sem_request,     PRINT_SERVER_SEM_REQUEST    );

	for (i = 0; i < PRINT_CHN_ID_MAX; i++) {
		sprintf(name, PRINT_SERVER_SEM_CHANNELS, i);
		OPEN_SEM(sem_channels[i], name);
	}

#undef OPEN_SEM

	if (shm == NULL || shm == MAP_FAILED) {
		fd = shm_open(PRINT_SERVER_SHM, O_RDWR, 0600);
		if (fd == -1) {
//			LogFatal("shm_open() failed. errno=%d (%s)", errno, strerror(errno));
			return -1;
		}

		shm = (print_shm_t *)mmap(NULL, sizeof(print_shm_t),
				PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
		close(fd);
		if (shm == MAP_FAILED) {
			LogFatal("mmap() failed. errno=%d (%s)", errno, strerror(errno));
			return -1;
		}
	}

	return 0;
}

/*
 * 仅供产测使用。正常情况下不应调用。
 */
int print_locate_black_mark(
		uint8_t sensor_id,          //  [in][黑标传感器] 1:使用1#传感器(旁边的) 2:使用2#传感器(中间的)
		uint8_t cut_mode,           //  [in][定位黑标并走纸后的切纸动作] 0:不切 1:半切 2:全切
		uint16_t acc_step_limit,    //  [in][步进电机最高加速步数] 1~256
		uint32_t dot_lines_to_feed, //  [in][定位黑标后走纸点行数] 0~4294967295
		uint32_t max_feed_mm,       //  [in][定位黑标过程最长走纸距离(mm)] 0~4294967295
		int *found,                 // [out][是否成功定位黑标] 0:失败 1:成功
		uint32_t *feeded_dot_lines) // [out][走纸距离(点行)]
{
#define NO_SUSPEND_AND_WAIT 1

	print_msg_t msg;
	uint32_t tries, start_location;
	uint8_t buf[16];

	(*found) = 0;
	(*feeded_dot_lines) = 0;

	IS_INITIALIZED(-1);

	msg.cmd = PRINT_CMD_GET_PAPER_LOCATION;

	/* 获取当前位置 */
	tries = 100;
	shm->paper_location.current_location = 0xFFFFFFFF;
	do {
		if (send_msg(&msg, sizeof(msg.cmd)))
			return -1;
		usleep(20000);
	} while (shm->paper_location.current_location != 0xFFFFFFFF && tries-- > 0);
	if (shm->paper_location.current_location == 0xFFFFFFFF) {
		LogError("Failed to get paper location.");
		return -1;
	}
	start_location = shm->paper_location.current_location;
	LogInfo("Start location: %u", start_location);

	buf[0] = 0x11;
	buf[1] = sensor_id;
	buf[2] = 1;
	buf[3] = cut_mode;
	*((uint16_t *)(buf + 4 )) = acc_step_limit;
	*((uint32_t *)(buf + 6 )) = dot_lines_to_feed;
	*((uint32_t *)(buf + 10)) = max_feed_mm;
	print_channel_send_data(PRINT_CHN_ID_CMD, buf, 14, 1);

#if NO_SUSPEND_AND_WAIT

	(*found) = 1;
	return 0;

#else

	uint32_t last = 0xFFFFFFFF;

	tries = 100;
	do {
		usleep(80000);
		if (send_msg(&msg, sizeof(msg.cmd)))
			return -1;
		usleep(20000);
		if (shm->paper_location.current_location == last)
			break;
		last = shm->paper_location.current_location;
	} while (tries-- > 0);

	LogInfo("Black Mark location: %u", shm->paper_location.black_mark_location);
	LogInfo("   Current location: %u", shm->paper_location.current_location);

	if (shm->paper_location.black_mark_location > 0) {
		(*found) = 1;
		(*feeded_dot_lines) = shm->paper_location.current_location - start_location;
		return 0;
	}
	return -1;

#endif
}

int badpoint_info_from_file(void *buf,int buf_len,badpoint_info_hdr_t *info)
{
	int ret = 0,sz = PRINTER_DOT_PER_LINE/8;
	badpoint_info_hdr_t hdr = {};
	FILE *fp = fopen(BAD_POINT_INF_FILE,"rb");

	if( !fp ) return -1;

	ret = fread(&hdr,sizeof(hdr),1,fp);
	if( ret<0 ) goto out;
	if( hdr.offset<0 || hdr.chk_dots != BAD_POINT_CHECK_NUM || hdr.offset+hdr.chk_dots > PRINTER_DOT_PER_LINE )
		goto out;
	if( buf_len<sz ) {
		LogError("buf len must great than %d",sz);
		goto out;
	}
	ret = fread(buf,sz,1,fp);
	if( ret<=0 ) goto out;
	fclose(fp);
	if( info )
		memcpy(info,&hdr,sizeof(hdr));
	return sz;

out:
	memset(buf,0,buf_len);
	fclose(fp);
	return -1;
}

int badpoint_info_save_file(const void *buf,int buf_len,const badpoint_info_hdr_t *info)
{
	FILE *fp = fopen(BAD_POINT_INF_FILE,"wb");
	if( !fp ) return -1;

	if( fwrite(info,sizeof(*info),1,fp)<=0 ) {
		LogError("fwrite error(%d),%s",errno,strerror(errno));
		goto out;
	}

	if( fwrite(buf,buf_len,1,fp)<=0 ) {
		LogError("fwrite error(%d),%s",errno,strerror(errno));
		goto out;
	}
	fclose(fp);
	system("sync");
	return 0;
out:
	if( fp ) fclose(fp);
	return -1;
}

void set_data_collection_state(int enabled)
{
	IS_INITIALIZED();
	shm->data_collection.enabled = (enabled) ? 1 : 0;
}

void set_third_party_data_collection_state(int enabled, uint64_t channel_bits)
{
	IS_INITIALIZED();
	shm->third_party_data_collection.enabled = (enabled) ? 1 : 0;
	shm->third_party_data_collection.channel_allow = channel_bits;
}

uint8_t get_paper_not_taken_actions(void)
{
	IS_INITIALIZED(0);
	return shm->request.paper_not_taken_actions;
}

void set_paper_not_taken_actions(uint8_t actions)
{
	print_msg_t msg;

	msg.cmd = PRINT_CMD_SET_PAPER_NOT_TAKEN_ACTIONS;
	msg.set_value.value = actions;

	send_msg(&msg, sizeof(msg.set_value));
}

/*
 * func: 清除指定的打印任务
 * param: task_id = 0, 清除全部任务; > 0, 清除指定任务；<0 编号不存在
 * ret: 0：未知错误（例如编号不存在）；1：清除完成；
 */
int print_task_delete(int task_id)
{
    int ret = 0;
    print_channel_t *channel;
    uint16_t t;
    int i, complete = 0;

#define IS_MATCH(ID_)  if(task_id == 0 || ID_ == task_id)

    IS_INITIALIZED(0);
    if(task_id < 0){
        LogError("task_id(%d), error!",task_id);
        goto exit;
    }
    if(task_id != 0 && (task_id <= shm->task_pool.last_completed_task_id || task_id >= shm->task_pool.next_task_id)){
        LogInfo("task_id:%d, expect range:(%d, %d), fail!", task_id, shm->task_pool.last_completed_task_id, shm->task_pool.next_task_id);
        goto exit;
    }

    for (i = 0; i < PRINT_CHN_ID_MAX; i++) {
        channel = &shm->channels[i];
        channels_lock(i);
        t = channel->task_queue.head;
        while (IS_VALID_TASK_INDEX(t)) {
            IS_MATCH(TASK_AT(t)->id){
                TASK_FLAG_SET(TASK_AT(t), TASK_FLAG_DONT_PRINT);
                complete++;
            }
            t = TASK_AT(t)->next;
        }
        t = channel->task;
        if (IS_VALID_TASK_INDEX(t)) {
            IS_MATCH(TASK_AT(t)->id){
                TASK_FLAG_SET(TASK_AT(t), TASK_FLAG_DONT_PRINT);
                complete++;
            }
        }
        channels_unlock(i);
        if(task_id > 0 && complete > 0){//匹配成功退出
            goto exit;
        }
    }
    task_queue_lock();
    t = shm->task_queue.head;
    while (IS_VALID_TASK_INDEX(t)) {
        IS_MATCH(TASK_AT(t)->id){
            TASK_FLAG_SET(TASK_AT(t), TASK_FLAG_DONT_PRINT);
            complete++;
        }
        t = TASK_AT(t)->next;
        if(task_id > 0 && complete > 0){//匹配成功退出
            break;
        }
    }
    task_queue_unlock();

exit:
    if(complete > 0 || task_id == 0)
        ret = 1;

#undef IS_MATCH

    return ret;
}
