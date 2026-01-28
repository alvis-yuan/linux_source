/**
 * usb_core.c - USB Dual Channel Management System
 * 
 * Implements a state machine to manage switching between Printer and iAP
 * channels based on USB hardware state and protocol detection.
 */

#include "glbvar.h"
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <sys/ioctl.h>

/* External dependencies as per user provided code */
#include "libcommon.h"
#include "host_cmd.h"
#include "usb.h"
#include "iAP2Adapt.h"

/* ==========================================================================
 * Configuration & Macros
 * ========================================================================== */

#define USB_PRINTER_NODE    "/dev/g_printer0"
#define USB_IAP_NODE        "/dev/iap20"
#define USB_STATE_PATH      "/sys/class/udc/13500000.otg_new/state"
#define USB_BUF_SIZE        4096
#define IAP_DETECT_RETRIES  3
#define IAP_DETECT_TIMEOUT  1000 /* ms */

typedef enum {
    USB_STATE_IDLE,
    USB_STATE_DETECTING,
    USB_STATE_PRINTER,
    USB_STATE_IAP,
} usb_state_t;

struct usb_context {
    /* Synchronization */
    pthread_mutex_t lock;
    pthread_t monitor_tid;
    pthread_t worker_tid;
    
    /* State */
    usb_state_t state;
    int fd;
    volatile bool worker_running;
    volatile bool monitor_running;
    
    /* Data */
    uint8_t buffer[USB_BUF_SIZE];
};

/* Global instance for legacy API support */
static struct usb_context *g_ctx = NULL;

/* ==========================================================================
 * iAP Detection Logic
 * ========================================================================== */

/**
 * detect_iap_support - Probes the interface for iAP protocol support
 * 
 * Returns: 0 if iAP detected, -1 otherwise.
 */
static int detect_iap_support(void)
{
    const uint8_t handshake_req[] = {0xff, 0x55, 0x02, 0x00, 0xee, 0x10};
    uint8_t handshake_rsp[6] = {0};
    struct pollfd pfd;
    int fd = -1;
    int i, ret;
    int res = -1;

    LogInfo("Starting iAP detection sequence");

    /* Using blocking mode for precise timing control during handshake */
    fd = open(USB_IAP_NODE, O_RDWR);
    if (fd < 0) {
        LogError("Failed to open %s for detection: %d", USB_IAP_NODE, errno);
        return -1;
    }

    pfd.fd = fd;
    pfd.events = POLLIN;

    for (i = 0; i < IAP_DETECT_RETRIES; i++) {
        /* 1. Send Handshake */
        if (write(fd, handshake_req, sizeof(handshake_req)) != sizeof(handshake_req)) {
            LogError("Write handshake failed: %d", errno);
            continue;
        }

        /* 2. Wait for Response */
        ret = poll(&pfd, 1, IAP_DETECT_TIMEOUT);
        if (ret <= 0) {
            LogInfo("Poll timeout or error during detection (attempt %d)", i + 1);
            continue;
        }

        /* 3. Read and Verify */
        if (pfd.revents & POLLIN) {
            memset(handshake_rsp, 0, sizeof(handshake_rsp));
            if (read(fd, handshake_rsp, sizeof(handshake_rsp)) == sizeof(handshake_req)) {
                if (memcmp(handshake_req, handshake_rsp, sizeof(handshake_req)) == 0) {
                    LogInfo("iAP Handshake confirmed");
                    res = 0; /* Success */
                    break;
                }
            }
        }
    }

    close(fd);
    return res;
}

/* ==========================================================================
 * Channel Workers
 * ========================================================================== */

/**
 * iap_data_handler_cb - Callback for iAP library to send data
 */
static int iap_send_cb(const uint8_t *data, uint32_t len, void *ctx)
{
    int fd;

    if (!g_ctx) {
        LogError("[Transport] Context is NULL in iap_send_cb");
        return -1;
    }

    pthread_mutex_lock(&g_ctx->lock);
    fd = g_ctx->fd;
    pthread_mutex_unlock(&g_ctx->lock);

    if (!data || len == 0) {
        LogError("[Transport] Invalid send parameters: data=%p, len=%u", data, len);
        return -1;
    }

    if (fd < 0) {
        LogError("[Transport] Invalid context or fd: fd=%d", fd);
        return -1;
    }

    LogInfo("[Transport] Sending %u bytes to usb_fd=%d", len, fd);
    int ret = write(fd, (char*)data, (int)len);
    if (ret <= 0) {
        LogError("[Transport] Send failed, ret=%d", ret);
        return -1;
    }
    
    return ret;
}

/**
 * iap_session_data_cb - Callback when iAP library receives valid session data
 */
static int8_t iap_session_cb(const uint8_t *data, uint32_t len, void *ctx)
{
    /* Forward decoded EA session data to printer logic */
    print_channel_send_data(PRINT_CHN_ID_USB, data, len, 0);
    return 1;
}

static void *worker_thread_iap(void *arg)
{
    struct usb_context *ctx = arg;
    struct pollfd pfd;
    int ret;

    LogInfo("iAP worker thread started");

    /* Initialize iAP Library */
    iAP2AdaptOnConnected(kIAP2TransportTypeUSB, iap_session_cb, iap_send_cb, g_ctx);

    pfd.fd = ctx->fd;
    pfd.events = POLLIN;

    while (ctx->worker_running && g_process_running) {
        ret = poll(&pfd, 1, 500);
        
        if (ret < 0) {
            LogError("Poll failed: %d", errno);
            break;
        }
        
        if (ret > 0 && (pfd.revents & POLLIN)) {
            /* Check flow control if necessary */
            while (print_buffer_is_full(sizeof(ctx->buffer)))
                usleep(10000);

            ret = read(ctx->fd, ctx->buffer, sizeof(ctx->buffer));
            if (ret > 0) {
                /* Feed raw data to iAP parser */
                iAP2AdaptDataHandler(ctx->buffer, ret);
            } else if (ret < 0 && errno != EAGAIN) {
                LogError("Read error: %d", errno);
                break; /* Fatal error, exit loop to trigger restart */
            }
        }
    }

    /* Cleanup iAP Library */
    iAP2AdaptOnDisconnected();
    LogInfo("iAP worker thread exited");
    return NULL;
}

static void *worker_thread_printer(void *arg)
{
    struct usb_context *ctx = arg;
    struct pollfd pfd;
    int ret, size, offset;

    LogInfo("Printer worker thread started");

    pfd.fd = ctx->fd;
    pfd.events = POLLIN;

    while (ctx->worker_running && g_process_running) {
        ret = poll(&pfd, 1, 500);

        if (ret < 0) {
            LogError("Poll failed: %d", errno);
            break;
        }

        if (ret > 0 && (pfd.revents & POLLIN)) {
            /* Flow control */
            while (print_buffer_is_full(sizeof(ctx->buffer)))
                usleep(10000);

            ret = read(ctx->fd, ctx->buffer, sizeof(ctx->buffer));
            if (ret > 0) {
                offset = 0;
                while (ret > 0 && (size = host_cmd_handler(ctx->buffer + offset, ret, HOST_CMD_SRC_USB)) > 0) {
                    offset += size;
                    ret -= size;
                }
                
                /* Forward remaining raw data to printer channel */
                if (offset == 0) {
                    print_channel_send_data(PRINT_CHN_ID_USB, ctx->buffer, ret, 0);
                }
            } else if (ret < 0 && errno != EAGAIN) {
                LogError("Read error: %d", errno);
                break;
            }
        }
    }

    LogInfo("Printer worker thread exited");
    return NULL;
}

/* ==========================================================================
 * Channel Management (Start/Stop)
 * ========================================================================== */

static void stop_active_channel(struct usb_context *ctx)
{
    pthread_mutex_lock(&ctx->lock);

    if (ctx->state == USB_STATE_IDLE || ctx->state == USB_STATE_DETECTING) {
        pthread_mutex_unlock(&ctx->lock);
        return;
    }

    LogInfo("Stopping channel, current state: %d", ctx->state);

    /* Signal worker to stop */
    ctx->worker_running = false;
    
    /* Wait for thread to finish */
    if (ctx->worker_tid) {
        pthread_join(ctx->worker_tid, NULL);
        ctx->worker_tid = 0;
    }

    /* Close file descriptor */
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }

    ctx->state = USB_STATE_IDLE;
    pthread_mutex_unlock(&ctx->lock);
}

static int start_channel(struct usb_context *ctx, usb_state_t type)
{
    const char *dev_node;
    void *(*thread_func)(void *);
    int ret;

    /* Ensure previous channel is stopped */
    stop_active_channel(ctx);

    pthread_mutex_lock(&ctx->lock);

    if (type == USB_STATE_IAP) {
        dev_node = USB_IAP_NODE;
        thread_func = worker_thread_iap;
    } else {
        dev_node = USB_PRINTER_NODE;
        thread_func = worker_thread_printer;
    }

    LogInfo("Starting channel type %d on %s", type, dev_node);

    ctx->fd = open(dev_node, O_RDWR);
    if (ctx->fd < 0) {
        LogError("Failed to open %s: %d", dev_node, errno);
        ret = -errno;
        goto err_unlock;
    }

    ctx->state = type;
    ctx->worker_running = true;

    ret = pthread_create(&ctx->worker_tid, &g_pthread_attr, thread_func, ctx);
    if (ret) {
        LogError("Failed to create worker thread: %d", ret);
        close(ctx->fd);
        ctx->fd = -1;
        ctx->state = USB_STATE_IDLE;
        goto err_unlock;
    }

    pthread_mutex_unlock(&ctx->lock);
    return 0;

err_unlock:
    pthread_mutex_unlock(&ctx->lock);
    return ret;
}

/* ==========================================================================
 * Monitor Thread (Sysfs Polling)
 * ========================================================================== */

static void handle_usb_configured(struct usb_context *ctx)
{
    int ret;

    LogInfo("USB Configured event received");

    /* Transition to detecting state */
    /* Note: No need to lock here for state assignment as we are in the orchestrator thread 
       and detecting logic doesn't use the shared fd yet */
    
    /* Perform detection (Blocking is acceptable here as it delays startup slightly) */
    ret = detect_iap_support();

    if (ret == 0) {
        LogInfo("Detection result: iAP Supported");
        start_channel(ctx, USB_STATE_IAP);
    } else {
        LogInfo("Detection result: iAP Not Supported, fallback to Printer");
        start_channel(ctx, USB_STATE_PRINTER);
    }
}

static void handle_usb_disconnected(struct usb_context *ctx)
{
    LogInfo("USB Disconnected/Suspended event received");
    stop_active_channel(ctx);
}

static void *monitor_thread(void *arg)
{
    struct usb_context *ctx = arg;
    struct pollfd pfd;
    char buf[64];
    char old_state[64] = {0};
    int fd;
    int ret;

    LogInfo("Monitor thread started watching %s", USB_STATE_PATH);

    fd = open(USB_STATE_PATH, O_RDONLY);
    if (fd < 0) {
        LogError("Failed to open sysfs state: %d", errno);
        return NULL;
    }

    pfd.fd = fd;
    pfd.events = POLLPRI;

    /* Initial read to get current state */
    ret = read(fd, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret - 1] = '\0';
        strncpy(old_state, buf, sizeof(old_state));
        
        if (strstr(buf, "configured")) {
            handle_usb_configured(ctx);
        }
    }

    while (ctx->monitor_running && g_process_running) {
        lseek(fd, 0, SEEK_SET);
        
        ret = poll(&pfd, 1, 1000); /* 1s timeout */
        
        if (ret < 0) {
            LogError("Monitor poll failed: %d", errno);
            break;
        }

        if (pfd.revents & POLLPRI) {
            lseek(fd, 0, SEEK_SET);
            ret = read(fd, buf, sizeof(buf) - 1);
            if (ret > 0) {
                buf[ret - 1] = '\0';

                if (strcmp(buf, old_state) != 0) {
                    LogInfo("State change: %s -> %s", old_state, buf);
                    strncpy(old_state, buf, sizeof(old_state));

                    if (strstr(buf, "configured")) {
                        handle_usb_configured(ctx);
                    } else {
                        /* Handle anything not "configured" as a disconnect/stop */
                        handle_usb_disconnected(ctx);
                    }
                }
            }
        }
    }

    close(fd);
    LogInfo("Monitor thread exited");
    return NULL;
}

static int _ctx_init(void)
{
    g_ctx = calloc(1, sizeof(*g_ctx));
    if (!g_ctx)
        return -ENOMEM;

    pthread_mutex_init(&g_ctx->lock, NULL);
    g_ctx->state = USB_STATE_IDLE;
    g_ctx->fd = -1;
    g_ctx->worker_running = false;
    g_ctx->monitor_running = true;

    return 0;
}

/* ==========================================================================
 * Public APIs
 * ========================================================================== */
int usb_send_data(const void *data, uint32_t len)
{
    if (g_ctx->fd < 0)
        return -1;
    return write(g_ctx->fd, data, len);
}

int usb_init(void)
{
    pthread_t tid;

    if (print_api_init()) {
        LogError("print_api_init() failed.");
        return -1;
    }

    if (sys_global_var()->boot_mode == BOOT_MODE_FACTORY)
        return 0;

    if (_ctx_init()) {
        LogError("USB context initialization failed.");
        return -1;
    }

    /*
     * Create thread.
     */
    if (pthread_create(&tid, &g_pthread_attr, monitor_thread, g_ctx)) {
        LogFatal("pthread_create() failed.");
        return -1;
    }

    return 0;
}

void usb_deinit(void)
{
    if (!g_ctx)
        return;

    /* Stop monitor thread */
    g_ctx->monitor_running = false;
    if (g_ctx->monitor_tid) {
        pthread_join(g_ctx->monitor_tid, NULL);
        g_ctx->monitor_tid = 0;
    }

    /* Stop any active channel */
    stop_active_channel(g_ctx);

    pthread_mutex_destroy(&g_ctx->lock);
    free(g_ctx);
    g_ctx = NULL;
}