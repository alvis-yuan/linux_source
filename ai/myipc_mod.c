/*
 * myipc_mod.c - Lightweight IPC based on Generic Netlink
 *
 * Implements: Register, Lookup, List(Dump), RPC Relay, Pub/Sub, Auto-cleanup.
 * Adheres to Linux Kernel Coding Style.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <net/genetlink.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mutex.h>
#include <linux/netlink.h>

/* --- Protocol Definitions --- */

#define MYIPC_FAMILY_NAME "MYIPC"
#define MYIPC_VERSION 1

enum {
	MYIPC_ATTR_UNSPEC,
	MYIPC_ATTR_NAME,      /* String: Service Name */
	MYIPC_ATTR_TARGET_ID, /* U32: Port ID */
	MYIPC_ATTR_DATA,      /* Binary: Payload */
	__MYIPC_ATTR_MAX,
};
#define MYIPC_ATTR_MAX (__MYIPC_ATTR_MAX - 1)

enum {
	MYIPC_CMD_UNSPEC,
	MYIPC_CMD_REGISTER,
	MYIPC_CMD_UNREGISTER, /* Explicit unregister */
	MYIPC_CMD_LOOKUP,
	MYIPC_CMD_LIST,       /* Dump all services */
	MYIPC_CMD_SEND,
	MYIPC_CMD_PUBLISH,
	__MYIPC_CMD_MAX,
};
#define MYIPC_CMD_MAX (__MYIPC_CMD_MAX - 1)

enum {
	MYIPC_MCAST_GROUP_EVENT,
};

/* --- Data Structures --- */

struct ipc_service_entry {
	struct list_head list;
	char name[64];
	u32 portid;
};

/* 在 myipc_mod.c 头部添加响应头定义，确保与用户态一致 */
struct ipc_reply_header {
	s32 status;    /* 0: OK, other: Error */
	s32 sys_errno; /* Linux errno */
};


static struct genl_family myipc_family;
/* Global registry list */
static LIST_HEAD(service_list);

/*
 * Locking:
 * Protects the service_list.
 * We use a mutex because allocation (kzalloc) and netlink sending
 * may sleep.
 */
static DEFINE_MUTEX(ipc_lock);

/* Attribute validation policy */
static const struct nla_policy myipc_policy[MYIPC_ATTR_MAX + 1] = {
	[MYIPC_ATTR_NAME]      = { .type = NLA_NUL_STRING, .len = 63 },
	[MYIPC_ATTR_TARGET_ID] = { .type = NLA_U32 },
	[MYIPC_ATTR_DATA]      = { .type = NLA_BINARY },
};

/* --- Helper Functions --- */

static struct ipc_service_entry *find_service_by_name(const char *name)
{
	struct ipc_service_entry *entry;

	list_for_each_entry(entry, &service_list, list) {
		if (strcmp(entry->name, name) == 0)
			return entry;
	}
	return NULL;
}

/* Remove all services registered by a specific Port ID */
static void cleanup_services_by_portid(u32 portid)
{
	struct ipc_service_entry *entry, *tmp;
	int count = 0;

	mutex_lock(&ipc_lock);
	list_for_each_entry_safe(entry, tmp, &service_list, list) {
		if (entry->portid == portid) {
			list_del(&entry->list);
			kfree(entry);
			count++;
		}
	}
	mutex_unlock(&ipc_lock);

	if (count > 0)
		pr_info("MYIPC: Auto-cleaned %d services for PID %u\n",
			count, portid);
}

/* --- Command Handlers (.doit) --- */

static int myipc_register(struct sk_buff *skb, struct genl_info *info)
{
	struct ipc_service_entry *entry;
	char *name;

	if (!info->attrs[MYIPC_ATTR_NAME])
		return -EINVAL;

	name = nla_data(info->attrs[MYIPC_ATTR_NAME]);

	mutex_lock(&ipc_lock);
	
	if (find_service_by_name(name)) {
		mutex_unlock(&ipc_lock);
		return -EEXIST;
	}

	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry) {
		mutex_unlock(&ipc_lock);
		return -ENOMEM;
	}

	strscpy(entry->name, name, sizeof(entry->name));
	entry->portid = info->snd_portid;
	list_add_tail(&entry->list, &service_list);

	mutex_unlock(&ipc_lock);

	pr_info("MYIPC: Registered '%s' (PID: %u)\n", name, entry->portid);
	return 0;
}

static int myipc_unregister(struct sk_buff *skb, struct genl_info *info)
{
	struct ipc_service_entry *entry, *tmp;
	char *name;
	int found = 0;

	if (!info->attrs[MYIPC_ATTR_NAME])
		return -EINVAL;

	name = nla_data(info->attrs[MYIPC_ATTR_NAME]);

	mutex_lock(&ipc_lock);
	list_for_each_entry_safe(entry, tmp, &service_list, list) {
		/* Only allow unregister if name matches AND caller owns it */
		if (strcmp(entry->name, name) == 0 &&
		    entry->portid == info->snd_portid) {
			list_del(&entry->list);
			kfree(entry);
			found = 1;
			break;
		}
	}
	mutex_unlock(&ipc_lock);

	return found ? 0 : -ENOENT;
}

static int myipc_lookup(struct sk_buff *skb, struct genl_info *info)
{
	struct ipc_service_entry *entry;
	struct sk_buff *msg;
	void *hdr;
	char *name;
	struct ipc_reply_header *rh;
	void *payload;
	size_t payload_len;
	u32 found_portid = 0;
	int found = 0;

	if (!info->attrs[MYIPC_ATTR_NAME])
		return -EINVAL;

	name = nla_data(info->attrs[MYIPC_ATTR_NAME]);

	/* 1. 查找服务 */
	mutex_lock(&ipc_lock);
	entry = find_service_by_name(name);
	if (entry) {
		found_portid = entry->portid;
		found = 1;
	}
	mutex_unlock(&ipc_lock);

	/* 2. 准备回包 */
	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	/* 使用 genlmsg_put_reply 自动处理 seq 和 pid */
	hdr = genlmsg_put_reply(msg, info, &myipc_family, 0, MYIPC_CMD_LOOKUP);
	if (!hdr) {
		nlmsg_free(msg);
		return -EMSGSIZE;
	}

	/* 3. 构造统一的 Reply Payload: [Header] + [Data] */
	/* 如果找到，Data 就是 u32 portid；如果没找到，Data 为空 */
	payload_len = sizeof(*rh) + (found ? sizeof(u32) : 0);
	
	payload = kzalloc(payload_len, GFP_KERNEL);
	if (!payload) {
		nlmsg_free(msg);
		return -ENOMEM;
	}

	rh = (struct ipc_reply_header *)payload;
	if (found) {
		rh->status = 0;
		rh->sys_errno = 0;
		/* 将 portid 紧跟在 header 后面 */
		*(u32 *)((char *)payload + sizeof(*rh)) = found_portid;
	} else {
		rh->status = 1;
		rh->sys_errno = ENOENT; /* 明确告知用户态没找到 */
	}

	/* 4. 将 Payload 放入 ATTR_DATA */
	if (nla_put(msg, MYIPC_ATTR_DATA, payload_len, payload)) {
		kfree(payload);
		nlmsg_free(msg);
		return -EMSGSIZE;
	}

	kfree(payload);
	genlmsg_end(msg, hdr);
	
	return genlmsg_reply(msg, info);
}

/* Relay: User A -> Kernel -> User B */
static int myipc_send(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff *msg;
	void *hdr;
	u32 target_pid;
	void *data;
	int len;

    pr_info("MYIPC: Send handler invoked\n");

	if (!info->attrs[MYIPC_ATTR_TARGET_ID] ||
	    !info->attrs[MYIPC_ATTR_DATA])
		return -EINVAL;

	target_pid = nla_get_u32(info->attrs[MYIPC_ATTR_TARGET_ID]);
	data = nla_data(info->attrs[MYIPC_ATTR_DATA]);
	len = nla_len(info->attrs[MYIPC_ATTR_DATA]);

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	/* Forwarding: Sender PID becomes "snd_portid" for receiver */
	hdr = genlmsg_put(msg, target_pid, info->snd_seq, &myipc_family, 0,
			  MYIPC_CMD_SEND);
	if (!hdr) {
		nlmsg_free(msg);
		return -EMSGSIZE;
	}

	/* Pack the original sender's ID so receiver knows who sent it */
	if (nla_put_u32(msg, MYIPC_ATTR_TARGET_ID, info->snd_portid)) {
		nlmsg_free(msg);
		return -EMSGSIZE;
	}

	if (nla_put(msg, MYIPC_ATTR_DATA, len, data)) {
		nlmsg_free(msg);
		return -EMSGSIZE;
	}

	genlmsg_end(msg, hdr);
	return genlmsg_unicast(genl_info_net(info), msg, target_pid);
}

static int myipc_publish(struct sk_buff *skb, struct genl_info *info)
{
	struct sk_buff *msg;
	void *hdr;
	void *data;
	int len;

    pr_info("MYIPC: Publish handler invoked\n");

	if (!info->attrs[MYIPC_ATTR_DATA])
		return -EINVAL;

	data = nla_data(info->attrs[MYIPC_ATTR_DATA]);
	len = nla_len(info->attrs[MYIPC_ATTR_DATA]);

	msg = nlmsg_new(NLMSG_DEFAULT_SIZE, GFP_KERNEL);
	if (!msg)
		return -ENOMEM;

	hdr = genlmsg_put(msg, 0, info->snd_seq, &myipc_family, 0,
			  MYIPC_CMD_PUBLISH);
	if (!hdr) {
		nlmsg_free(msg);
		return -EMSGSIZE;
	}

	if (nla_put(msg, MYIPC_ATTR_DATA, len, data)) {
		nlmsg_free(msg);
		return -EMSGSIZE;
	}

	genlmsg_end(msg, hdr);
	return genlmsg_multicast_netns(&myipc_family, genl_info_net(info), msg, 0, 0, GFP_KERNEL);
	//return genlmsg_multicast(&myipc_family, msg, 0, 0, GFP_KERNEL);
}

/* --- Dump Handlers (.dumpit) --- */

/*
 * List all services.
 * Implements "ubus list".
 */
static int myipc_dump_services(struct sk_buff *skb, struct netlink_callback *cb)
{
	struct ipc_service_entry *entry;
	void *hdr;
	int idx = 0;
	int skip = cb->args[0]; /* Used for resuming dump */

	mutex_lock(&ipc_lock);

	list_for_each_entry(entry, &service_list, list) {
		if (idx++ < skip)
			continue;

		hdr = genlmsg_put(skb, NETLINK_CB(cb->skb).portid,
				  cb->nlh->nlmsg_seq, &myipc_family,
				  NLM_F_MULTI, MYIPC_CMD_LIST);
		
		if (!hdr)
			break; /* Buffer full, stop and return */

		if (nla_put_string(skb, MYIPC_ATTR_NAME, entry->name) ||
		    nla_put_u32(skb, MYIPC_ATTR_TARGET_ID, entry->portid)) {
			genlmsg_cancel(skb, hdr);
			break;
		}

		genlmsg_end(skb, hdr);
	}

	mutex_unlock(&ipc_lock);

	cb->args[0] = idx; /* Save position */
	return skb->len;
}

/* --- Netlink Notification Handler (Cleanup) --- */

/*
 * Listener for NETLINK_URELEASE events.
 * This is called when any userspace netlink socket is closed/released.
 */
static int myipc_nl_notifier(struct notifier_block *nb, unsigned long event,
			     void *ptr)
{
	struct netlink_notify *n = ptr;

	/* We only care if a socket is released (closed) */
	if (event != NETLINK_URELEASE)
		return NOTIFY_DONE;

	/* We only care about Generic Netlink protocol sockets */
	if (n->protocol != NETLINK_GENERIC)
		return NOTIFY_DONE;

	/* Clean up any services registered by this Port ID */
	if (n->portid)
		cleanup_services_by_portid(n->portid);

	return NOTIFY_OK;
}

static struct notifier_block myipc_nb = {
	.notifier_call = myipc_nl_notifier,
};

/* --- Genl Family Definition --- */

static const struct genl_ops myipc_ops[] = {
	{
		.cmd = MYIPC_CMD_REGISTER,
		//.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.doit = myipc_register,
	},
	{
		.cmd = MYIPC_CMD_UNREGISTER,
		//.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.doit = myipc_unregister,
	},
	{
		.cmd = MYIPC_CMD_LOOKUP,
		//.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.doit = myipc_lookup,
	},
	{
		.cmd = MYIPC_CMD_LIST,
		//.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.dumpit = myipc_dump_services, /* Use dumpit for lists */
	},
	{
		.cmd = MYIPC_CMD_SEND,
		//.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.doit = myipc_send,
	},
	{
		.cmd = MYIPC_CMD_PUBLISH,
		//.validate = GENL_DONT_VALIDATE_STRICT | GENL_DONT_VALIDATE_DUMP,
		.doit = myipc_publish,
	},
};

static const struct genl_multicast_group myipc_mcgrps[] = {
	[MYIPC_MCAST_GROUP_EVENT] = { .name = "event", },
};

static struct genl_family myipc_family = {
	.name     = MYIPC_FAMILY_NAME,
	.version  = MYIPC_VERSION,
	.maxattr  = MYIPC_ATTR_MAX,
	.policy   = myipc_policy,
	.module   = THIS_MODULE,
	.ops      = myipc_ops,
	.n_ops    = ARRAY_SIZE(myipc_ops),
	.mcgrps   = myipc_mcgrps,
	.n_mcgrps = ARRAY_SIZE(myipc_mcgrps),
};

/* --- Init/Exit --- */

static int __init myipc_init(void)
{
	int ret;

	pr_info("MYIPC: Module loading...\n");

	ret = genl_register_family(&myipc_family);
	if (ret) {
		pr_err("MYIPC: Failed to register family\n");
		return ret;
	}

	/* Register notification block to detect socket close */
	netlink_register_notifier(&myipc_nb);

	return 0;
}

static void __exit myipc_exit(void)
{
	struct ipc_service_entry *entry, *tmp;

	/* Unregister notifier first to stop incoming cleanup events */
	netlink_unregister_notifier(&myipc_nb);

	genl_unregister_family(&myipc_family);

	mutex_lock(&ipc_lock);
	list_for_each_entry_safe(entry, tmp, &service_list, list) {
		list_del(&entry->list);
		kfree(entry);
	}
	mutex_unlock(&ipc_lock);

	pr_info("MYIPC: Module unloaded\n");
}

module_init(myipc_init);
module_exit(myipc_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Assistant");
MODULE_DESCRIPTION("Generic Netlink IPC Relay with Auto-cleanup");