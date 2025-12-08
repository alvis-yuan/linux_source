/**
 * @file libmyipc.c
 * @brief 通用 IPC 库实现文件
 * @details 使用 libmnl 进行 Netlink 通信，包含内置 I/O 线程实现
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <libmnl/libmnl.h>
#include <linux/genetlink.h>

#include "libipc.h"

/* --- 宏定义 --- */
#define MYIPC_FAMILY_NAME "MYIPC"
#define MYIPC_VERSION 1
#define MAX_METHOD_NAME 32
#define MAX_TOPIC_NAME 32
#define RCV_BUF_SIZE 8192

/* 这里的 Attribute 定义必须与内核模块保持一致 */
enum {
	MYIPC_ATTR_UNSPEC,
	MYIPC_ATTR_NAME,
	MYIPC_ATTR_TARGET_ID,
	MYIPC_ATTR_DATA,
	__MYIPC_ATTR_MAX,
};
#define MYIPC_ATTR_MAX (__MYIPC_ATTR_MAX - 1)

enum {
	MYIPC_CMD_UNSPEC,
	MYIPC_CMD_REGISTER,
	MYIPC_CMD_UNREGISTER,
	MYIPC_CMD_LOOKUP,
	MYIPC_CMD_LIST,
	MYIPC_CMD_SEND,
	MYIPC_CMD_PUBLISH,
	__MYIPC_CMD_MAX,
};

/* --- 内部数据结构 --- */

/** @brief 应用层协议头，嵌入在 ATTR_DATA 中 */
struct rpc_header {
	char method[MAX_METHOD_NAME];
	/* 实际数据紧跟在 header 后面 */
};

struct event_header {
	char topic[MAX_TOPIC_NAME];
};

struct reply_header {
	int32_t status;
	int32_t sys_errno;
};

struct family_info {
	uint16_t id;
	uint32_t mcast_id;
	const char *mcast_name;
};

/* 辅助结构体用于嵌套解析 */
struct mcast_parse_ctx {
	const char *target_name;
	uint32_t found_id;
};

/* Linux 内核风格链表简单实现 (避免依赖 kernel header) */
struct list_head {
	struct list_head *next, *prev;
};

/** @brief 挂起的同步请求节点 */
struct pending_call {
	struct list_head list;      /**< 链表节点 */
	uint32_t seq;               /**< 请求序列号 */
	pthread_cond_t cond;        /**< 唤醒条件变量 */
	pthread_mutex_t mutex;      /**< 保护条件变量的锁 */
	
	void *resp_buf;             /**< 用户提供的结果缓冲区 */
	size_t max_len;             /**< 缓冲区大小 */
	int ret_len;                /**< 实际返回长度或错误码 */
	int completed;              /**< 完成标志 */
	
	myipc_async_cb_t async_cb;  /**< 如果非空，则为异步调用 */
	void *async_priv;
};

/** @brief 本地方法注册节点 */
struct method_entry {
	struct method_entry *next;
	char name[MAX_METHOD_NAME];
	myipc_method_handler_t handler;
};

static int resolve_family_info(struct mnl_socket *nl, const char *family_name, 
			       uint16_t *fam_id, uint32_t *mcast_id);

static inline void INIT_LIST_HEAD(struct list_head *list) {
	list->next = list;
	list->prev = list;
}

static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next) {
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head) {
	__list_add(new, head, head->next);
}

static inline void __list_del(struct list_head *prev, struct list_head *next) {
	next->prev = prev;
	prev->next = next;
}

static inline void list_del(struct list_head *entry) {
	__list_del(entry->prev, entry->next);
}

#define list_entry(ptr, type, member) \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))

#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

/** @brief IPC 上下文主结构 */
struct myipc_ctx {
	struct mnl_socket *nl;
	uint16_t family_id;
    uint32_t mcast_group_id;
	uint32_t local_pid;
	uint32_t seq_counter;

	/* 线程相关 */
	pthread_t rx_thread;
	int running;
	
	/* 锁 */
	pthread_mutex_t send_lock;   /**< 保护发送 socket 和 seq */
	pthread_mutex_t list_lock;   /**< 保护 pending_list */
	pthread_rwlock_t method_lock;/**< 保护 method_list */

	/* 链表 */
	struct list_head pending_list;
	struct method_entry *methods;

	/* 事件回调 */
	myipc_event_cb_t event_cb;
};

/* --- 内部辅助函数 --- */
/**
 * @brief 内部通用函数：将请求挂入链表并等待结果
 * @note 调用前必须已加锁并构造好 req
 */
static int __wait_for_reply(myipc_ctx_t *ctx, struct pending_call *req, int timeout_ms)
{
	struct timespec ts;
	int ret = 0;

	/* 计算绝对超时时间 */
	clock_gettime(CLOCK_REALTIME, &ts);
	ts.tv_sec += timeout_ms / 1000;
	ts.tv_nsec += (timeout_ms % 1000) * 1000000;
	if (ts.tv_nsec >= 1000000000) {
		ts.tv_sec++;
		ts.tv_nsec -= 1000000000;
	}

	/* 挂入链表 (需持有 list_lock) */
	pthread_mutex_lock(&ctx->list_lock);
	list_add(&req->list, &ctx->pending_list);
	pthread_mutex_unlock(&ctx->list_lock);

	/* 等待信号 */
	pthread_mutex_lock(&req->mutex);
	while (!req->completed) {
		ret = pthread_cond_timedwait(&req->cond, &req->mutex, &ts);
		if (ret == ETIMEDOUT) {
			req->ret_len = -ETIMEDOUT;
			break;
		}
	}
	pthread_mutex_unlock(&req->mutex);

	/* 移除链表 */
	pthread_mutex_lock(&ctx->list_lock);
	if (!req->completed) list_del(&req->list);
	/* 如果已完成，packet_handler 那边可能没移除，这里再次确保移除是安全的 */
	else list_del(&req->list);
	pthread_mutex_unlock(&ctx->list_lock);

	return req->ret_len;
}

/**
 * @brief 解析 Family ID 的回调
 */
static int ctrl_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, CTRL_ATTR_MAX) < 0)
		return MNL_CB_OK;

	if (type == CTRL_ATTR_FAMILY_ID) {
		if (mnl_attr_validate(attr, MNL_TYPE_U16) < 0)
			return MNL_CB_ERROR;
		tb[CTRL_ATTR_FAMILY_ID] = attr;
	}
	return MNL_CB_OK;
}

/**
 * @brief 动态获取 Family ID
 */
static int resolve_family_id(struct mnl_socket *nl, const char *family_name)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct genlmsghdr *gh;
	struct nlattr *tb[CTRL_ATTR_MAX + 1] = {};
	uint32_t seq = time(NULL);
	int ret;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = GENL_ID_CTRL;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	gh = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
	gh->cmd = CTRL_CMD_GETFAMILY;
	gh->version = 1;

	mnl_attr_put_strz(nlh, CTRL_ATTR_FAMILY_NAME, family_name);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0)
		return -1;

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	if (ret < 0) return -1;

	ret = mnl_cb_run(buf, ret, seq, mnl_socket_get_portid(nl), NULL, NULL);
	if (ret < 0) return -1;

	/* 重新解析获取 ID (mnl_cb_run 主要是做校验) */
	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_type == NLMSG_ERROR) return -1;

	gh = mnl_nlmsg_get_payload(nlh);
	mnl_attr_parse(nlh, sizeof(*gh), ctrl_attr_cb, tb);

	if (tb[CTRL_ATTR_FAMILY_ID])
		return mnl_attr_get_u16(tb[CTRL_ATTR_FAMILY_ID]);

	return -1;
}

/**
 * @brief 线程安全的发送函数
 */
static int thread_safe_send(myipc_ctx_t *ctx, struct nlmsghdr *nlh)
{
	int ret;
	pthread_mutex_lock(&ctx->send_lock);
	nlh->nlmsg_seq = ++ctx->seq_counter;
	ret = mnl_socket_sendto(ctx->nl, nlh, nlh->nlmsg_len);
	pthread_mutex_unlock(&ctx->send_lock);
	return ret;
}

/**
 * @brief 发送 RPC 响应
 */
static int send_reply(myipc_ctx_t *ctx, uint32_t target_pid, uint32_t req_seq,
		      int status, int sys_err, void *data, size_t len)
{
	char buf[RCV_BUF_SIZE];
	struct nlmsghdr *nlh;
	struct genlmsghdr *gh;
	struct reply_header *rh;
	//struct nlattr *nest;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = ctx->family_id;
	nlh->nlmsg_flags = NLM_F_REQUEST; /* Reply is also a request to kernel */
	/* 关键：必须回填请求的 seq，但这里我们无法改变 nlh->seq (由 send_safe控制) 
	 * 实际上 Netlink 的全双工异步模型中，Reply 的 seq 并不强制要求等于 Request seq，
	 * 除非我们在应用层协议头里带上 seq。但标准做法是让 reply 作为一个新消息。
	 * 不过为了让接收端能匹配，我们在负载头里可以设计 echo seq，
	 * 或者利用 mnl_socket 的 portid 机制。
	 * 
	 * 在此设计中，我们暂且让 reply 作为一个独立的 SEND 消息，
	 * 接收端通过应用层协议（无）或者 seq 匹配。
	 * 
	 * 修正：为了实现同步调用的 seq 匹配，接收端需要看到的 seq 是请求时的 seq。
	 * 但这是不可能的，因为 seq 由发送端内核分配或发送端库维护。
	 * 
	 * 解决方案：在 Pending List 中匹配的是 "Request Seq"。
	 * Server 回复时，应该将 Request Seq 放在 ATTR_DATA 的头部，或者作为 ATTR_COOKIE。
	 * 简单起见，我们这里让 Reply 的 seq 等于 Request Seq，但这需要 bypass thread_safe_send 的 seq 覆盖。
	 */
	
	/* Hack: 直接加锁发送，手动控制 seq */
	pthread_mutex_lock(&ctx->send_lock);
	nlh->nlmsg_seq = req_seq; /* 强行设置为请求的 Seq */
	
	gh = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
	gh->cmd = MYIPC_CMD_SEND;
	gh->version = 1;

	mnl_attr_put_u32(nlh, MYIPC_ATTR_TARGET_ID, target_pid);
	
	/* 构造 ATTR_DATA: [Reply Header] + [Body] */
	//nest = mnl_attr_nest_start(nlh, MYIPC_ATTR_DATA);
	
	/* 这里为了内存连续，我们直接操作缓冲区，也可以拆分 attribute */
	/* 但 libmnl 的 nest 机制不方便直接 append raw data。
	 * 我们把 Reply Header 和 Body 拼成一块内存放入 DATA 属性。
	 */
	size_t payload_len = sizeof(*rh) + len;
	void *payload = malloc(payload_len);
	if (!payload) {
		pthread_mutex_unlock(&ctx->send_lock);
		return -ENOMEM;
	}
	
	rh = (struct reply_header *)payload;
	rh->status = status;
	rh->sys_errno = sys_err;
	if (len > 0)
		memcpy((char *)payload + sizeof(*rh), data, len);

	mnl_attr_put(nlh, MYIPC_ATTR_DATA, payload_len, payload);
	free(payload);
	
	//mnl_attr_nest_end(nlh, nest);

	int ret = mnl_socket_sendto(ctx->nl, nlh, nlh->nlmsg_len);
	pthread_mutex_unlock(&ctx->send_lock);
	return ret;
}

/* --- 接收处理逻辑 --- */

static int parse_rpc_request(myipc_ctx_t *ctx, struct nlattr **attrs, uint32_t src_pid, uint32_t seq)
{
	void *data;
	size_t len;
	struct rpc_header *rpc;
	struct method_entry *m;
	char resp_buf[2048];
	int ret_len;
	
	if (!attrs[MYIPC_ATTR_DATA]) return MNL_CB_ERROR;
	
	data = mnl_attr_get_payload(attrs[MYIPC_ATTR_DATA]);
	len = mnl_attr_get_payload_len(attrs[MYIPC_ATTR_DATA]);
	
	if (len < sizeof(struct rpc_header)) return MNL_CB_ERROR;
	rpc = (struct rpc_header *)data;
	
	/* 查找方法 */
	pthread_rwlock_rdlock(&ctx->method_lock);
	for (m = ctx->methods; m; m = m->next) {
		if (strncmp(m->name, rpc->method, MAX_METHOD_NAME) == 0) {
			/* 执行回调 */
			ret_len = m->handler((char *)data + sizeof(*rpc), 
					     len - sizeof(*rpc),
					     resp_buf, sizeof(resp_buf));
			pthread_rwlock_unlock(&ctx->method_lock);
			
			/* 发送回复 */
			if (ret_len >= 0) {
				send_reply(ctx, src_pid, seq, 0, 0, resp_buf, ret_len);
			} else {
				send_reply(ctx, src_pid, seq, 1, -ret_len, NULL, 0);
			}
			return MNL_CB_OK;
		}
	}
	pthread_rwlock_unlock(&ctx->method_lock);
	
	/* 方法未找到 */
	fprintf(stderr, "MyIPC Log: Method '%s' not found\n", rpc->method);
	send_reply(ctx, src_pid, seq, 1, ENOENT, NULL, 0);
	
	return MNL_CB_OK;
}

static int parse_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, MYIPC_ATTR_MAX) < 0)
		return MNL_CB_OK;
	tb[type] = attr;
	return MNL_CB_OK;
}

/**
 * @brief 数据包处理主入口
 */
static int packet_handler(const struct nlmsghdr *nlh, void *data)
{
	myipc_ctx_t *ctx = (myipc_ctx_t *)data;
	struct genlmsghdr *gh = mnl_nlmsg_get_payload(nlh);
	struct nlattr *attrs[MYIPC_ATTR_MAX + 1] = {};
	struct pending_call *req, *tmp;
	struct reply_header *rh;
	uint32_t src_pid = 0;

	/* 解析 Attributes */
	mnl_attr_parse(nlh, sizeof(*gh), parse_attr_cb, attrs);
	
	/* 尝试获取 Source PID (如果内核透传了) */
	if (attrs[MYIPC_ATTR_TARGET_ID])
		src_pid = mnl_attr_get_u32(attrs[MYIPC_ATTR_TARGET_ID]);

	/* --- 场景 1: 这是一个 RPC 回复 (Reply) --- */
	/* 检查 Pending List 中是否有等待此 Seq 的请求 */
	pthread_mutex_lock(&ctx->list_lock);
	list_for_each_entry_safe(req, tmp, &ctx->pending_list, list) {
		if (req->seq == nlh->nlmsg_seq) {
			/* 找到了！ */
			if (attrs[MYIPC_ATTR_DATA]) {
				void *p = mnl_attr_get_payload(attrs[MYIPC_ATTR_DATA]);
				int l = mnl_attr_get_payload_len(attrs[MYIPC_ATTR_DATA]);
				
				if (l >= sizeof(struct reply_header)) {
					rh = (struct reply_header *)p;
					/* 如果是同步调用 */
					if (!req->async_cb) {
						int copy_len = l - sizeof(*rh);
						if (copy_len > req->max_len) copy_len = req->max_len;
						
						if (rh->status == 0) {
							memcpy(req->resp_buf, (char*)p + sizeof(*rh), copy_len);
							req->ret_len = copy_len;
						} else {
							req->ret_len = -rh->sys_errno;
						}
						
						pthread_mutex_lock(&req->mutex);
						req->completed = 1;
						pthread_cond_signal(&req->cond);
						pthread_mutex_unlock(&req->mutex);
					} 
					/* 如果是异步调用 */
					else {
						/* 在 IO 线程直接执行回调 (注意不要阻塞太久) */
						int st = (rh->status == 0) ? 0 : -rh->sys_errno;
                        if (req->async_cb) {
						    req->async_cb(st, (char*)p + sizeof(*rh), 
							      l - sizeof(*rh), req->async_priv);
                        }
						
						/* 异步请求处理完直接移除 */
						list_del(&req->list);
						free(req);
					}
				}
			}
			pthread_mutex_unlock(&ctx->list_lock);
			return MNL_CB_OK; 
		}
	}
	pthread_mutex_unlock(&ctx->list_lock);

	/* --- 场景 2: 这是一个 RPC 请求 (Request) --- */
	if (gh->cmd == MYIPC_CMD_SEND) {
		return parse_rpc_request(ctx, attrs, src_pid, nlh->nlmsg_seq);
	}

	/* --- 场景 3: 这是一个发布事件 (Publish) --- */
	if (gh->cmd == MYIPC_CMD_PUBLISH && attrs[MYIPC_ATTR_DATA]) {
		struct event_header *eh;
		void *p = mnl_attr_get_payload(attrs[MYIPC_ATTR_DATA]);
		int l = mnl_attr_get_payload_len(attrs[MYIPC_ATTR_DATA]);
		
		if (l >= sizeof(struct event_header) && ctx->event_cb) {
			eh = (struct event_header *)p;
			ctx->event_cb(eh->topic, (char*)p + sizeof(*eh), l - sizeof(*eh));
		}
		return MNL_CB_OK;
	}

	return MNL_CB_OK;
}

/**
 * @brief I/O 接收线程
 */
static void *rx_thread_func(void *arg)
{
	myipc_ctx_t *ctx = (myipc_ctx_t *)arg;
	char buf[RCV_BUF_SIZE];
	int ret;

	while (ctx->running) {
		ret = mnl_socket_recvfrom(ctx->nl, buf, sizeof(buf));
		if (ret > 0) {
            fprintf(stderr, "MyIPC Debug: Received %d bytes\n", ret);
			/* 使用自定义的 packet_handler 处理 */
			/* 注意：这里我们不使用 mnl_cb_run 的默认检查，因为我们需要处理 async logic */
			struct nlmsghdr *nlh = (struct nlmsghdr *)buf;
			
			/* 基本校验 */
			if (ret < sizeof(struct nlmsghdr)) continue;
			
			/* 遍历消息批处理 */
			while (mnl_nlmsg_ok(nlh, ret)) {
				if (nlh->nlmsg_type == NLMSG_ERROR) {
					/* 处理内核级错误 */
					struct nlmsgerr *err = mnl_nlmsg_get_payload(nlh);
					fprintf(stderr, "MyIPC Log: Netlink Error %d\n", err->error);
				} else if (nlh->nlmsg_type == ctx->family_id) {
					packet_handler(nlh, ctx);
				}
				nlh = mnl_nlmsg_next(nlh, &ret);
			}
		} else if (ret == 0) {
			break;
		} else {
			if (errno == EINTR) continue;
			fprintf(stderr, "MyIPC Log: Recv error %s\n", strerror(errno));
			break;
		}
	}
	return NULL;
}

/* --- API 实现 --- */

myipc_ctx_t *myipc_init(void)
{
	myipc_ctx_t *ctx;
	int ret;

	ctx = calloc(1, sizeof(*ctx));
	if (!ctx) return NULL;

	ctx->nl = mnl_socket_open(NETLINK_GENERIC);
	if (!ctx->nl) {
		fprintf(stderr, "MyIPC Log: Failed to open socket\n");
		goto err_free;
	}

	if (mnl_socket_bind(ctx->nl, 0, MNL_SOCKET_AUTOPID) < 0) {
		fprintf(stderr, "MyIPC Log: Failed to bind socket\n");
		goto err_close;
	}

	ctx->local_pid = mnl_socket_get_portid(ctx->nl);

    #if 1
    if (resolve_family_info(ctx->nl, MYIPC_FAMILY_NAME, 
                            &ctx->family_id, &ctx->mcast_group_id) < 0) {
        fprintf(stderr, "MyIPC Log: Kernel module not loaded or resolve failed\n");
        goto err_close;
    }
    
    if (ctx->mcast_group_id == 0) {
        fprintf(stderr, "MyIPC Log: Warning: 'event' multicast group not found\n");
    } else {
        printf("MyIPC Debug: Resolved Family ID %u, Mcast Group ID %u\n", 
               ctx->family_id, ctx->mcast_group_id);
    }
    #else
	ctx->family_id = resolve_family_id(ctx->nl, MYIPC_FAMILY_NAME);
	
	if (ctx->family_id == 0) {
		fprintf(stderr, "MyIPC Log: Kernel module not loaded?\n");
		goto err_close;
	}
    #endif


	/* 初始化锁和链表 */
	pthread_mutex_init(&ctx->send_lock, NULL);
	pthread_mutex_init(&ctx->list_lock, NULL);
	pthread_rwlock_init(&ctx->method_lock, NULL);
	INIT_LIST_HEAD(&ctx->pending_list);

	/* 启动接收线程 */
	ctx->running = 1;
	ret = pthread_create(&ctx->rx_thread, NULL, rx_thread_func, ctx);
	if (ret != 0) {
		fprintf(stderr, "MyIPC Log: Failed to create thread\n");
		goto err_mutex;
	}

	return ctx;

err_mutex:
	pthread_mutex_destroy(&ctx->send_lock);
	pthread_mutex_destroy(&ctx->list_lock);
	pthread_rwlock_destroy(&ctx->method_lock);
err_close:
	mnl_socket_close(ctx->nl);
err_free:
	free(ctx);
	return NULL;
}

void myipc_cleanup(myipc_ctx_t *ctx)
{
	if (!ctx) return;

	ctx->running = 0;
	pthread_cancel(ctx->rx_thread); /* 强制退出 */
	pthread_join(ctx->rx_thread, NULL);

	mnl_socket_close(ctx->nl);
	
	/* 清理 pending list */
	struct pending_call *req, *tmp;
	pthread_mutex_lock(&ctx->list_lock);
	list_for_each_entry_safe(req, tmp, &ctx->pending_list, list) {
		list_del(&req->list);
		if (req->async_cb) free(req);
		/* 同步调用在线程栈上，不用 free */
	}
	pthread_mutex_unlock(&ctx->list_lock);

	/* 清理 methods */
	struct method_entry *m, *tm;
	pthread_rwlock_wrlock(&ctx->method_lock);
	m = ctx->methods;
	while(m) {
		tm = m->next;
		free(m);
		m = tm;
	}
	pthread_rwlock_unlock(&ctx->method_lock);

	pthread_mutex_destroy(&ctx->send_lock);
	pthread_mutex_destroy(&ctx->list_lock);
	pthread_rwlock_destroy(&ctx->method_lock);
	free(ctx);
}

int myipc_register_service(myipc_ctx_t *ctx, const char *service_name)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct genlmsghdr *gh;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = ctx->family_id;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;

	gh = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
	gh->cmd = MYIPC_CMD_REGISTER;
	gh->version = 1;

	mnl_attr_put_strz(nlh, MYIPC_ATTR_NAME, service_name);

	/* Register 也是一个同步操作，需要等 ACK */
	/* 简化起见，这里假设发送成功即注册成功，或者可以在 recv 中处理 NLM_F_ACK */
	return thread_safe_send(ctx, nlh) > 0 ? 0 : -1;
}

int myipc_add_method(myipc_ctx_t *ctx, const char *method_name, 
                     myipc_method_handler_t handler)
{
	struct method_entry *e = malloc(sizeof(*e));
	if (!e) return -ENOMEM;
	
	strncpy(e->name, method_name, MAX_METHOD_NAME);
	e->handler = handler;
	
	pthread_rwlock_wrlock(&ctx->method_lock);
	e->next = ctx->methods;
	ctx->methods = e;
	pthread_rwlock_unlock(&ctx->method_lock);
	return 0;
}

int32_t myipc_lookup_service(myipc_ctx_t *ctx, const char *service_name)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct genlmsghdr *gh;
	struct pending_call req;
	uint32_t result_pid = 0;
	int ret;

	/* 1. 初始化同步请求上下文 */
	pthread_cond_init(&req.cond, NULL);
	pthread_mutex_init(&req.mutex, NULL);
	req.resp_buf = &result_pid; /* 结果将直接写入这个变量 */
	req.max_len = sizeof(result_pid);
	req.completed = 0;
	req.async_cb = NULL;

	/* 2. 构造 Lookup 消息 */
	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = ctx->family_id;
	nlh->nlmsg_flags = NLM_F_REQUEST; /* 此时不需要 NLM_F_ACK，因为我们要等 Reply */

	gh = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
	gh->cmd = MYIPC_CMD_LOOKUP;
	gh->version = 1;

	mnl_attr_put_strz(nlh, MYIPC_ATTR_NAME, service_name);

	/* 3. 发送并获取 Seq */
	/* 注意：必须先发，拿到 seq 后再挂链表等待，防止 seq 错乱 */
	pthread_mutex_lock(&ctx->send_lock);
	nlh->nlmsg_seq = ++ctx->seq_counter;
	req.seq = nlh->nlmsg_seq;
	
	if (mnl_socket_sendto(ctx->nl, nlh, nlh->nlmsg_len) < 0) {
		pthread_mutex_unlock(&ctx->send_lock);
		pthread_mutex_destroy(&req.mutex);
		pthread_cond_destroy(&req.cond);
		return -EIO;
	}
	pthread_mutex_unlock(&ctx->send_lock);

	/* 4. 通用等待 (超时设为 1秒) */
	ret = __wait_for_reply(ctx, &req, 1000);

	pthread_cond_destroy(&req.cond);
	pthread_mutex_destroy(&req.mutex);

	/* 5. 处理结果 */
	if (ret < 0) {
		/* 可能是超时(-ETIMEDOUT) 或 服务端返回错误(-ENOENT) */
		return ret; 
	}
	
	if (ret != sizeof(uint32_t)) {
		/* 数据长度不对 */
		return -EPROTO; 
	}

	return (int32_t)result_pid;
}

/* 通用调用构造器 */
static int build_call_req(myipc_ctx_t *ctx, struct nlmsghdr *nlh,
			  int32_t target_pid, const char *method, 
			  const void *arg, size_t arg_len)
{
	struct genlmsghdr *gh;
	struct rpc_header *rh;
	//struct nlattr *nest;
	
	nlh->nlmsg_type = ctx->family_id;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	
	gh = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
	gh->cmd = MYIPC_CMD_SEND;
	gh->version = 1;
	
	mnl_attr_put_u32(nlh, MYIPC_ATTR_TARGET_ID, target_pid);
	
	//nest = mnl_attr_nest_start(nlh, MYIPC_ATTR_DATA);
	
	size_t payload_len = sizeof(*rh) + arg_len;
	void *payload = malloc(payload_len);
	if (!payload) return -ENOMEM;
	
	rh = (struct rpc_header *)payload;
	strncpy(rh->method, method, MAX_METHOD_NAME);
	if (arg_len > 0)
		memcpy((char*)payload + sizeof(*rh), arg, arg_len);
		
	mnl_attr_put(nlh, MYIPC_ATTR_DATA, payload_len, payload);
	free(payload);
	
	//mnl_attr_nest_end(nlh, nest);
	return 0;
}

int myipc_call_sync(myipc_ctx_t *ctx, int32_t target_pid, const char *method,
                    const void *arg, size_t arg_len,
                    void *ret_buf, size_t max_ret_len, int timeout_ms)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct pending_call req;

	pthread_cond_init(&req.cond, NULL);
	pthread_mutex_init(&req.mutex, NULL);
	req.resp_buf = ret_buf;
	req.max_len = max_ret_len;
	req.completed = 0;
	req.async_cb = NULL;

	nlh = mnl_nlmsg_put_header(buf);
	if (build_call_req(ctx, nlh, target_pid, method, arg, arg_len) < 0) {
		pthread_cond_destroy(&req.cond);
		pthread_mutex_destroy(&req.mutex);
		return -ENOMEM;
	}

	pthread_mutex_lock(&ctx->send_lock);
	nlh->nlmsg_seq = ++ctx->seq_counter;
	req.seq = nlh->nlmsg_seq;
	
	if (mnl_socket_sendto(ctx->nl, nlh, nlh->nlmsg_len) < 0) {
		pthread_mutex_unlock(&ctx->send_lock);
		pthread_cond_destroy(&req.cond);
		pthread_mutex_destroy(&req.mutex);
		return -EIO;
	}
	pthread_mutex_unlock(&ctx->send_lock);

	int ret = __wait_for_reply(ctx, &req, timeout_ms);

	pthread_cond_destroy(&req.cond);
	pthread_mutex_destroy(&req.mutex);

	return ret;
}

int myipc_call_async(myipc_ctx_t *ctx, int32_t target_pid, const char *method,
                     const void *arg, size_t arg_len,
                     myipc_async_cb_t cb, void *priv)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct pending_call *req;

	req = calloc(1, sizeof(*req));
	if (!req) return -ENOMEM;
	
	req->async_cb = cb;
	req->async_priv = priv;
	
	nlh = mnl_nlmsg_put_header(buf);
	if (build_call_req(ctx, nlh, target_pid, method, arg, arg_len) < 0) {
		free(req);
		return -ENOMEM;
	}

	pthread_mutex_lock(&ctx->list_lock);
	pthread_mutex_lock(&ctx->send_lock);
	
	nlh->nlmsg_seq = ++ctx->seq_counter;
	req->seq = nlh->nlmsg_seq;
	list_add(&req->list, &ctx->pending_list);
	
	if (mnl_socket_sendto(ctx->nl, nlh, nlh->nlmsg_len) < 0) {
		list_del(&req->list);
		pthread_mutex_unlock(&ctx->send_lock);
		pthread_mutex_unlock(&ctx->list_lock);
		free(req);
		return -EIO;
	}
	
	pthread_mutex_unlock(&ctx->send_lock);
	pthread_mutex_unlock(&ctx->list_lock);
	
	return 0;
}

/* 解析单个组播组的属性 (Name & ID) */
static int parse_mcast_grp_cb(const struct nlattr *attr, void *data)
{
	struct family_info *info = data;
	const struct nlattr **tb = (const struct nlattr **)data; // 这里的 data 指针转换需要技巧，见下文调用处
	
	/* 注意：这里我们不能直接用 tb，因为 libmnl 的回调 data 传递限制。
	 * 我们使用更原始的方式解析嵌套属性。
	 */
	int type = mnl_attr_get_type(attr);

	if (type == CTRL_ATTR_MCAST_GRP_NAME) {
		if (mnl_attr_validate(attr, MNL_TYPE_STRING) < 0) return MNL_CB_ERROR;
		const char *name = mnl_attr_get_str(attr);
		/* 检查是否是我们想要的组名 ("event") */
		if (strcmp(name, info->mcast_name) == 0) {
			/* 标记找到了名字，等待 ID */
			// 实际解析时，我们无法保证属性顺序，所以通常需要先全部解析存起来
			// 但这里为了简化，我们假设 Name 和 ID 都在这个 nest 里面
		}
	} else if (type == CTRL_ATTR_MCAST_GRP_ID) {
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) return MNL_CB_ERROR;
		// info->mcast_id = mnl_attr_get_u32(attr); // 暂时不能赋值，需确认名字匹配
	}
	return MNL_CB_OK;
}



/* 解析单个组播组内部属性 */
static int mcast_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);
	
	if (mnl_attr_type_valid(attr, CTRL_ATTR_MCAST_GRP_MAX) < 0)
		return MNL_CB_OK;
		
	tb[type] = attr;
	return MNL_CB_OK;
}

/* 遍历所有组播组 */
static int parse_mcast_grps_cb(const struct nlattr *attr, void *data)
{
	struct mcast_parse_ctx *ctx = data;
	struct nlattr *tb[CTRL_ATTR_MCAST_GRP_MAX + 1] = {};

	/* attr 本身是一个容器，包含 Name 和 ID */
	mnl_attr_parse_nested(attr, mcast_attr_cb, tb);

	if (tb[CTRL_ATTR_MCAST_GRP_NAME] && tb[CTRL_ATTR_MCAST_GRP_ID]) {
		const char *name = mnl_attr_get_str(tb[CTRL_ATTR_MCAST_GRP_NAME]);
		if (strcmp(name, ctx->target_name) == 0) {
			ctx->found_id = mnl_attr_get_u32(tb[CTRL_ATTR_MCAST_GRP_ID]);
			return MNL_CB_STOP; /* 找到了，停止遍历 */
		}
	}
	return MNL_CB_OK;
}

/* 顶层 Family 属性解析 */
static int _ctrl_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = data;
	int type = mnl_attr_get_type(attr);

	if (mnl_attr_type_valid(attr, CTRL_ATTR_MAX) < 0)
		return MNL_CB_OK;
	tb[type] = attr;
	return MNL_CB_OK;
}

/**
 * @brief 完整解析 Family ID 和 "event" 组播 ID
 */
static int resolve_family_info(struct mnl_socket *nl, const char *family_name, 
			       uint16_t *fam_id, uint32_t *mcast_id)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct genlmsghdr *gh;
	struct nlattr *tb[CTRL_ATTR_MAX + 1] = {};
	uint32_t seq = time(NULL);
	int ret;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = GENL_ID_CTRL;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK;
	nlh->nlmsg_seq = seq;

	gh = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
	gh->cmd = CTRL_CMD_GETFAMILY;
	gh->version = 1;

	mnl_attr_put_strz(nlh, CTRL_ATTR_FAMILY_NAME, family_name);

	if (mnl_socket_sendto(nl, nlh, nlh->nlmsg_len) < 0) return -1;

	ret = mnl_socket_recvfrom(nl, buf, sizeof(buf));
	if (ret < 0) return -1;
	
	/* 1. 基础解析校验 */
	if (mnl_cb_run(buf, ret, seq, mnl_socket_get_portid(nl), NULL, NULL) < 0) return -1;

	nlh = (struct nlmsghdr *)buf;
	if (nlh->nlmsg_type == NLMSG_ERROR) return -1;

	/* 2. 解析顶层属性 */
	gh = mnl_nlmsg_get_payload(nlh);
	mnl_attr_parse(nlh, sizeof(*gh), _ctrl_attr_cb, tb);

	if (!tb[CTRL_ATTR_FAMILY_ID]) return -1;
	*fam_id = mnl_attr_get_u16(tb[CTRL_ATTR_FAMILY_ID]);

	/* 3. 解析组播组 ID */
	if (tb[CTRL_ATTR_MCAST_GROUPS]) {
		struct mcast_parse_ctx mctx = { .target_name = "event", .found_id = 0 };
		mnl_attr_parse_nested(tb[CTRL_ATTR_MCAST_GROUPS], parse_mcast_grps_cb, &mctx);
		*mcast_id = mctx.found_id;
	} else {
		*mcast_id = 0;
	}

	return 0;
}

int myipc_subscribe(myipc_ctx_t *ctx, myipc_event_cb_t cb)
{
    if (ctx->mcast_group_id == 0) {
        fprintf(stderr, "MyIPC Log: Multicast group ID invalid\n");
        return -ENOENT;
    }
    
    /* 使用解析到的正确 ID */
    int group = ctx->mcast_group_id;
    
    if (mnl_socket_setsockopt(ctx->nl, NETLINK_ADD_MEMBERSHIP, &group, sizeof(int)) < 0) {
        fprintf(stderr, "MyIPC Log: Setsockopt failed: %s\n", strerror(errno));
        return -errno;
    }
	
    ctx->event_cb = cb;
    return 0;
}

int myipc_publish(myipc_ctx_t *ctx, const char *topic, const void *data, size_t len)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct genlmsghdr *gh;
	struct event_header *eh;
	//struct nlattr *nest;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = ctx->family_id;
	nlh->nlmsg_flags = NLM_F_REQUEST;

	gh = mnl_nlmsg_put_extra_header(nlh, sizeof(struct genlmsghdr));
	gh->cmd = MYIPC_CMD_PUBLISH;
	gh->version = 1;

	//nest = mnl_attr_nest_start(nlh, MYIPC_ATTR_DATA);
	
	size_t p_len = sizeof(*eh) + len;
	void *p = malloc(p_len);
	if (!p) return -ENOMEM;
	
	eh = (struct event_header *)p;
	strncpy(eh->topic, topic, MAX_TOPIC_NAME);
	if (len > 0) memcpy((char*)p + sizeof(*eh), data, len);
	
	mnl_attr_put(nlh, MYIPC_ATTR_DATA, p_len, p);
	free(p);
	
	//mnl_attr_nest_end(nlh, nest);

	return thread_safe_send(ctx, nlh);
}