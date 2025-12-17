#include <poll.h>
#include "libcommon.h"
#include "host_cmd_def.h"
#include "host_cmd.h"

#define TCP_SERVER_PORT 17900

/*----------------------------------------------*
 | LOCAL VARIABLES                              |
 *----------------------------------------------*/
static const uint8_t HEARTBEAT_PACKET[4] = {0x10, 0x01, 0x11, 0xFF};
static pthread_mutex_t clisock_mutex = PTHREAD_MUTEX_INITIALIZER;
static int srvsock = -1, clisock = -1;

/*----------------------------------------------*
 | TCP SERVER THREAD                            |
 *----------------------------------------------*/
static void *tcp_thread(void *arg)
{
	struct pollfd fds[2];
	struct sockaddr_in addr;
	socklen_t socklen;
	int ret, size, offset, rcvptr = 0, nfds, newsock;
	uint8_t rcvbuf[4096];

	LogDbg("tcp_thread started");

	while (1) {
		fds[0].fd = srvsock;
		fds[0].events = POLLIN;
		fds[0].revents = 0;
		nfds = 1;

		if (clisock >= 0) {
			fds[1].fd = clisock;
			fds[1].events = POLLIN;
			fds[1].revents = 0;
			nfds++;
		}

		ret = poll(fds, nfds, 1000);

		if (ret <= 0) {
			if (clisock >= 0)
				send(clisock, HEARTBEAT_PACKET, sizeof(HEARTBEAT_PACKET), 0);
			continue;
		}

		/* Check event from client socket */
		if (nfds > 1 && (fds[1].revents & POLLIN)) {
			ret = recv(clisock, rcvbuf + rcvptr, sizeof(rcvbuf) - rcvptr, 0);
			if (ret > 0) {
				rcvptr += ret;
				offset = 0;
				while (rcvptr > 0 && (size = host_cmd_handler(rcvbuf + offset, rcvptr, HOST_CMD_SRC_TCP)) > 0) {
					offset += size;
					rcvptr -= size;
				}
				if (rcvptr > 0)
					memmove(rcvbuf, rcvbuf + offset, rcvptr);
			}
			else /*if (ret == 0)*/ { /* Client disconnected */
				LogInfo("TCP client disconnected (ret=%d, errno=%d)", ret, errno);
				pthread_mutex_lock(&clisock_mutex);
				close(clisock);
				clisock = -1;
				rcvptr = 0;
				pthread_mutex_unlock(&clisock_mutex);
			}
		}

		/* Check event from server socket */
		if (fds[0].revents & POLLIN) {
			socklen = sizeof(addr);
			newsock = accept(srvsock, (struct sockaddr *)&addr, &socklen);
			if (newsock < 0) {
				LogError("accept() failed (errno=%d)", errno);
				continue;
			}
			LogInfo("TCP client connected from %s:%u", inet_ntoa(addr.sin_addr), htons(addr.sin_port));
			pthread_mutex_lock(&clisock_mutex);
			if (clisock >= 0)
				close(clisock);
			clisock = newsock;
			rcvptr = 0;
			pthread_mutex_unlock(&clisock_mutex);
		}
	}

	LogDbg("tcp_thread exited");

	return NULL;
}

int tcp_send_data(const void *data, uint32_t len)
{
	int ret = 0;

	pthread_mutex_lock(&clisock_mutex);
	if (clisock >= 0)
		ret = send(clisock, data, len, 0);
	pthread_mutex_unlock(&clisock_mutex);
	return ret;
}

int tcp_init(void)
{
	pthread_t tid;
	struct sockaddr_in addr;

	srvsock = socket(AF_INET, SOCK_STREAM, 0);
	if (srvsock < 0) {
		LogError("Failed to create TCP socket (errno=%d)", errno);
		return -1;
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

	/*
	 * Create thread.
	 */
	if (pthread_create(&tid, NULL, tcp_thread, NULL)) {
		LogError("pthread_create failed.");
		goto _failed;
	}
	pthread_detach(tid);

	return 0;

_failed:
	if (srvsock != -1)
		close(srvsock);
	return -1;
}
