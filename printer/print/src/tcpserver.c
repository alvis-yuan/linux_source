#include <sys/epoll.h>
#include "libcommon.h"
#include "print_server.h"

#define TCP_SERVER_ENABLED  1

#define TCP_SERVER_PORT     9100
#define RCVBUF_SIZE         2048

#if (TCP_SERVER_ENABLED)

/*----------------------------------------------*
 | LOCAL VARIABLES                              |
 *----------------------------------------------*/
typedef struct {
	int sock[NUMBER_OF_TCP_CHN];
	int used;
	int next_free;
} clisock_pool_t;

static clisock_pool_t clisock_pool;
static int epfd = -1, srvsock = -1;

#define EPOLL_GET_INFO(event)       ((uint32_t)(*((uint32_t *)(&((event).data.u64)) + 1)))
#define EPOLL_SET_INFO(event, info) (*((uint32_t *)(&((event).data.u64)) + 1) = (uint32_t)(info))

static int epoll_add(int epfd, int fd, uint32_t events, uint32_t info)
{
	struct epoll_event ev;

	memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;
	EPOLL_SET_INFO(ev, info);
	return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

static int epoll_delete(int epfd, int fd)
{
	return epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
}

static bool should_receive(void)
{
//	const print_status_t *status;

	/* 缓冲区满了，不接收数据 */
	if (print_buffer_is_full(RCVBUF_SIZE))
		return false;

#if 0
	/* 状态异常，无法打印，不接收数据 */
	status = print_status();
	if (status->no_paper || status->paper_jam || status->platen_opened ||
		status->thermal_head_overheated || status->step_motor_overheated)
		return false;
#endif

	return true;
}

static void clisock_pool_init(void)
{
	int i;

	for (i = 0; i < NUMBER_OF_TCP_CHN; i++)
		clisock_pool.sock[i] = -1;
	clisock_pool.used = 0;
	clisock_pool.next_free = 0;
}

static int clisock_pool_take(void)
{
	int i, j, ret;

	if (clisock_pool.next_free < 0)
		return -1;

	ret = clisock_pool.next_free;
	clisock_pool.next_free = -1;
	clisock_pool.used++;
	if (clisock_pool.used == NUMBER_OF_TCP_CHN)
		return ret;

	for (i = 0, j = ret + 1; i < NUMBER_OF_TCP_CHN; i++, j++) {
		if (j >= NUMBER_OF_TCP_CHN)
			j = 0;
		if (clisock_pool.sock[j] == -1) {
			clisock_pool.next_free = j;
			break;
		}
	}
	return ret;
}

static void clisock_pool_return(int index)
{
	if (clisock_pool.sock[index] >= 0) {
		clisock_pool.sock[index] = -1;
		clisock_pool.used--;
		if (clisock_pool.next_free == -1)
			clisock_pool.next_free = index;
	}
}

/*----------------------------------------------*
 | TCP SERVER THREAD                            |
 *----------------------------------------------*/
static void *tcp_server_thread(void *arg)
{
	struct epoll_event events[NUMBER_OF_TCP_CHN + 1];
	struct sockaddr_in addr;
	socklen_t socklen;
	int i, ret, nfds, newsock, index;
	uint8_t rcvbuf[RCVBUF_SIZE];

	LogDbg("tcp_server_thread started");

	clisock_pool_init();

	while (1) {
		nfds = epoll_wait(epfd, events, NUMBER_OF_TCP_CHN + 1, 1000);

		if (nfds <= 0)
			continue;

		for (i = 0; i < nfds; i++) {
			if (events[i].data.fd == srvsock) {
				/* Event from server socket */
				socklen = sizeof(addr);
				newsock = accept(srvsock, (struct sockaddr *)&addr, &socklen);
				if (newsock < 0) {
					LogError("accept() failed (errno=%d)", errno);
					continue;
				}
				LogInfo("TCP client connected from %s:%u", inet_ntoa(addr.sin_addr), htons(addr.sin_port));
				index = clisock_pool_take();
				if (index == -1) {
					close(newsock);
				}
				else {
					clisock_pool.sock[index] = newsock;
					epoll_add(epfd, newsock, EPOLLIN, index);
				}
			}
			else {
				/* Event from client socket */
				if (!should_receive())
					continue;
				ret = recv(events[i].data.fd, rcvbuf, sizeof(rcvbuf), 0);
				index = EPOLL_GET_INFO(events[i]);
				if (ret > 0)
					print_channel_send_data(PRINT_CHN_ID_TCP_BEGIN + index, rcvbuf, ret, 0);
				else /*if (ret == 0)*/ { /* Client disconnected */
					LogInfo("TCP client disconnected (ret=%d, errno=%d)", ret, errno);
					epoll_delete(epfd, events[i].data.fd);
					close(events[i].data.fd);
					clisock_pool_return(index);
				}
			}
		}
	}

	LogDbg("tcp_server_thread exited");

	return NULL;
}

#endif /* TCP_SERVER_ENABLED */

int tcp_server_send_data(int chn_id, const void *data, uint32_t len)
{
	int ret = 0;

#if (TCP_SERVER_ENABLED)
	chn_id -= PRINT_CHN_ID_TCP_BEGIN;
	if (chn_id >= 0 && chn_id < NUMBER_OF_TCP_CHN &&
		clisock_pool.sock[chn_id] >= 0)
		ret = send(clisock_pool.sock[chn_id], data, len, 0);
#endif /* TCP_SERVER_ENABLED */

	return ret;
}

int tcp_server_init(void)
{
#if (TCP_SERVER_ENABLED)

	pthread_attr_t m_pthread_attr;
	pthread_t m_tid;
	struct sockaddr_in addr;

	epfd = epoll_create(1);
	if (epfd < 0) {
		LogError("Failed to create epoll instance (errno=%d)", errno);
		return -1;
	}

	srvsock = socket(AF_INET, SOCK_STREAM, 0);
	if (srvsock < 0) {
		LogError("Failed to create TCP socket (errno=%d)", errno);
		goto _failed;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(TCP_SERVER_PORT);
	if (bind(srvsock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		LogError("Failed to bind server socket (errno=%d)", errno);
		goto _failed;
	}

	if (listen(srvsock, 1) < 0) {
		LogError("Failed to listen on server socket (errno=%d)", errno);
		goto _failed;
	}

	if (epoll_add(epfd, srvsock, EPOLLIN, 0)) {
		LogError("Failed to add server socket to epoll (errno=%d)", errno);
		goto _failed;
	}

	/*
	 * Create thread.
	 */
	if (pthread_attr_init(&m_pthread_attr)) {
		LogError("pthread_attr_init failed.");
		goto _failed;
	}
	if (pthread_attr_setdetachstate(&m_pthread_attr, PTHREAD_CREATE_DETACHED)) {
		LogError("pthread_attr_setdetachstate failed.");
		goto _failed;
	}
	if (pthread_create(&m_tid, &m_pthread_attr, tcp_server_thread, NULL)) {
		LogError("pthread_create failed.");
		goto _failed;
	}

	return 0;

_failed:
	if (srvsock != -1)
		close(srvsock);
	if (epfd != -1)
		close(epfd);
	return -1;

#else

	return 0;

#endif /* TCP_SERVER_ENABLED */
}
