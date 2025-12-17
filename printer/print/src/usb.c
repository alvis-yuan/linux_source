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
#include <unistd.h>
#include "libcommon.h"
#include "glbvar.h"
#include "host_cmd.h"
#include "Util.h"
#include "usb.h"

#define USB_PRINTER_NODE "/dev/g_printer0"

static uint8_t usb_buffer[4096];
static pthread_t m_tid;
static int usb_fd = -1;

static void *usb_thread(void *arg)
{
	struct pollfd fds;
	int ret, size, offset;

	while (g_process_running) {
		if (usb_fd < 0) {
			usb_fd = open(USB_PRINTER_NODE, O_RDWR/*|O_NONBLOCK*/);
			if (usb_fd < 0) {
				LogError("open() failed: errno=%d", errno);
				SysDelay(500);
				continue;
			}
			fds.fd = usb_fd;
			fds.events = POLLIN;
			fds.revents = 0;
		}

		ret = poll(&fds, 1, 1000);
//		LogDbg("poll() returns %d (revent=%d, errno=%d)", ret, fds.revents, errno);

		if (ret <= 0 || (fds.revents & POLLIN) == 0)
			continue;

		while (print_buffer_is_full(sizeof(usb_buffer)))
			usleep(100000);
		ret = read(usb_fd, usb_buffer, sizeof(usb_buffer));
		if (ret <= 0) {
			LogError("read() failed: errno=%d", errno);
			continue;
		}

		offset = 0;
		while (ret > 0 && (size = host_cmd_handler(usb_buffer + offset, ret, HOST_CMD_SRC_USB)) > 0) {
			offset += size;
			ret -= size;
		}
		if (offset == 0)
			print_channel_send_data(PRINT_CHN_ID_USB, usb_buffer, ret, 0);
	}

	return NULL;
}

int usb_send_data(const void *data, uint32_t len)
{
	if (usb_fd < 0)
		return -1;
	return write(usb_fd, data, len);
}

int usb_init(void)
{
	if (print_api_init())
		return -1;

	if (sys_global_var()->boot_mode == BOOT_MODE_FACTORY)
		return 0;

	/*
	 * Create thread.
	 */
	if (pthread_create(&m_tid, &g_pthread_attr, usb_thread, NULL)) {
		LogFatal("pthread_create() failed.");
		return -1;
	}

	return 0;
}
