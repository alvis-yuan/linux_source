#include <pthread.h>
#include <time.h>
#include "libcommon.h"
#include "glbvar.h"
#include "Util.h"
#include "simple_timer.h"

#define MAX_TIMERS 16

typedef struct {
	simple_timer_handler_t handler;
	struct timespec ts;
	uint32_t context;
	bool running;
	uint8_t prev;
	uint8_t next;
} simple_timer_t;

typedef struct {
	pthread_mutex_t mutex;
	uint8_t head;
	uint8_t tail;
} timer_list_t;

#define TIMER_ID_NULL 0xFF

static simple_timer_t m_timers[MAX_TIMERS];
static timer_list_t m_timer_list;
static pthread_t m_tid;
static pthread_mutex_t m_mutex;
static pthread_cond_t m_cond;

/*----------------------------------------------*
 | Timer Management                             |
 *----------------------------------------------*/
static bool is_valid_timer(uint8_t timer_id)
{
	if (timer_id >= MAX_TIMERS) /* Invalid 'timer_id' */
		return false;
	if (m_timers[timer_id].handler == NULL) /* Timer not created */
		return false;
	return true;
}

static void clear_timer(uint8_t timer_id)
{
	m_timers[timer_id].ts.tv_sec = 0;
	m_timers[timer_id].ts.tv_nsec = 0;
	m_timers[timer_id].context = 0;
	m_timers[timer_id].running = false;
	m_timers[timer_id].prev = TIMER_ID_NULL;
	m_timers[timer_id].next = TIMER_ID_NULL;
}

static void init_timer(uint8_t timer_id)
{
	m_timers[timer_id].handler = NULL;
	clear_timer(timer_id);
}

/*----------------------------------------------*
 | Timer List Management                        |
 *----------------------------------------------*/
#define timer_list_lock()	pthread_mutex_lock(&m_timer_list.mutex)
#define timer_list_unlock()	pthread_mutex_unlock(&m_timer_list.mutex)

static inline bool timer_list_is_empty(void)
{
	return (m_timer_list.head == TIMER_ID_NULL);
}

static void timer_list_init(void)
{
	pthread_mutex_init(&m_timer_list.mutex, &g_mutex_attr);
	m_timer_list.head = TIMER_ID_NULL;
	m_timer_list.tail = TIMER_ID_NULL;
}

static void timer_list_insert(uint8_t next, uint8_t timer_id)
{
	uint8_t prev;

	timer_list_lock();

	prev = (next == TIMER_ID_NULL) ? m_timer_list.tail : m_timers[next].prev;

	m_timers[timer_id].prev = prev;
	m_timers[timer_id].next = next;

	if (prev != TIMER_ID_NULL)
		m_timers[prev].next = timer_id;
	else /* Insert to the head of the list */
		m_timer_list.head = timer_id;

	if (next != TIMER_ID_NULL)
		m_timers[next].prev = timer_id;
	else /* Append to the tail of the list */
		m_timer_list.tail = timer_id;

	timer_list_unlock();

	m_timers[timer_id].running = true;
}

static void timer_list_remove(uint8_t timer_id)
{
	uint8_t prev, next;

	timer_list_lock();

	prev = m_timers[timer_id].prev;
	next = m_timers[timer_id].next;

	if (prev != TIMER_ID_NULL)
		m_timers[prev].next = next;
	else
		m_timer_list.head = next;

	if (next != TIMER_ID_NULL)
		m_timers[next].prev = prev;
	else
		m_timer_list.tail = prev;

	timer_list_unlock();

	clear_timer(timer_id);
}

static void timer_list_remove_first(void)
{
	uint8_t timer_id, next;

	if (timer_list_is_empty())
		return;

	timer_list_lock();

	timer_id = m_timer_list.head;
	next = m_timers[timer_id].next;

	m_timer_list.head = next;
	if (next != TIMER_ID_NULL)
		m_timers[next].prev = TIMER_ID_NULL;
	else
		m_timer_list.tail = TIMER_ID_NULL;

	timer_list_unlock();

	clear_timer(timer_id);
}

static void timer_list_check_expirations(void)
{
	struct timespec ts;
	uint32_t context;
	uint8_t head;

	clock_gettime(CLOCK_REALTIME, &ts);
	head = m_timer_list.head;
	while (head != TIMER_ID_NULL) {
		if (timespec_compare(&m_timers[head].ts, &ts) > 0)
			break;
		context = m_timers[head].context;
		timer_list_remove_first();
		(*m_timers[head].handler)(context);
		head = m_timer_list.head;
	}
}

static void *timer_thread(void *arg)
{
	while (g_process_running) {
		pthread_mutex_lock(&m_mutex);
		if (timer_list_is_empty())
			pthread_cond_wait(&m_cond, &m_mutex);
		else
			pthread_cond_timedwait(&m_cond, &m_mutex, &m_timers[m_timer_list.head].ts);
		pthread_mutex_unlock(&m_mutex);
		timer_list_check_expirations();
	}

	return NULL;
}

int simple_timer_create(uint8_t *p_timer_id, simple_timer_handler_t handler)
{
#define timer_id (*p_timer_id)

	uint8_t i;

	for (i = 0; i < MAX_TIMERS; i++) {
		if (m_timers[i].handler == NULL)
			break;
	}
	if (i == MAX_TIMERS)
		return -1;

	m_timers[i].handler = handler;
	clear_timer(i);

	timer_id = i;

	return 0;

#undef timer_id
}

int simple_timer_start(uint8_t timer_id, uint32_t ms_to_expire, uint32_t context)
{
	uint8_t next;

	if (!is_valid_timer(timer_id))
		return -1;

	simple_timer_stop(timer_id);

	if (ms_to_expire == 0)
		return 0;

	clock_gettime(CLOCK_REALTIME, &m_timers[timer_id].ts);
	timespec_add_msec(&m_timers[timer_id].ts, ms_to_expire);
	m_timers[timer_id].context = context;

	if (timer_list_is_empty()) {
		next = TIMER_ID_NULL;
		goto _found;
	}

	next = m_timer_list.head;
	do {
		if (timespec_compare(&m_timers[timer_id].ts, &m_timers[next].ts) < 0)
			break;
		next = m_timers[next].next;
	} while (next != TIMER_ID_NULL);

_found:
	timer_list_insert(next, timer_id);
	pthread_cond_signal(&m_cond);
	return 0;
}

void simple_timer_stop(uint8_t timer_id)
{
	if (simple_timer_is_running(timer_id))
		timer_list_remove(timer_id);
}

bool simple_timer_is_running(uint8_t timer_id)
{
	if (!is_valid_timer(timer_id))
		return false;
	return m_timers[timer_id].running;
}

int simple_timer_init(void)
{
	uint8_t i;

	for (i = 0; i < MAX_TIMERS; i++)
		init_timer(i);

	timer_list_init();

	if (pthread_mutex_init(&m_mutex, &g_mutex_attr)) {
		LogFatal("pthread_mutex_init() failed.");
		return -1;
	}
	if (pthread_cond_init(&m_cond, NULL)) {
		LogFatal("pthread_cond_init() failed.");
		return -1;
	}

	/*
	 * Create thread.
	 */
	if (pthread_create(&m_tid, &g_pthread_attr, timer_thread, NULL)) {
		LogFatal("pthread_create() failed.");
		return -1;
	}

	return 0;
}
