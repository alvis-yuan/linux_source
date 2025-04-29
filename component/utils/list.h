#ifndef _USERMODE_LIST_H_
#define _USERMODE_LIST_H_

/*
 * 从Linux内核提取的简化版双向链表实现
 * 适用于用户态程序，不依赖内核头文件
 */

/**
 * container_of - 通过成员地址获取包含结构体地址
 * @ptr:    成员指针
 * @type:   包含结构体类型
 * @member: 成员在结构体中的名称
 */
#define container_of(ptr, type, member) ({              \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) );})

/* 获取结构体成员偏移量 */
#define offsetof(TYPE, MEMBER) ((size_t)&((TYPE *)0)->MEMBER)

/*
 * 简单的双向链表实现
 */

struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/*
 * 在已知prev/next时插入新节点
 */
static inline void __list_add(struct list_head *new,
                  struct list_head *prev,
                  struct list_head *next)
{
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/**
 * list_add - 在链表头后添加新节点
 * @new: 新节点
 * @head: 链表头
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

/**
 * list_add_tail - 在链表尾部添加新节点
 * @new: 新节点
 * @head: 链表头
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

/*
 * 删除节点辅助函数
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

/**
 * list_del - 从链表中删除节点
 * @entry: 要删除的节点
 */
static inline void list_del(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
    entry->next = (struct list_head *)0xDEADBEEF;
    entry->prev = (struct list_head *)0xDEADBEEF;
}

/**
 * list_is_last - 测试节点是否是链表尾
 * @list: 测试节点
 * @head: 链表头
 */
static inline int list_is_last(const struct list_head *list,
                const struct list_head *head)
{
    return list->next == head;
}

/**
 * list_empty - 测试链表是否为空
 * @head: 链表头
 */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/**
 * list_entry - 获取包含链表节点的结构体
 * @ptr:    list_head指针
 * @type:   包含结构体类型
 * @member: list_head在结构体中的名称
 */
#define list_entry(ptr, type, member) \
    container_of(ptr, type, member)

/**
 * list_for_each - 正向遍历链表
 * @pos:    当前节点指针
 * @head:   链表头
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_safe - 安全版正向遍历(支持删除节点)
 * @pos:    当前节点指针
 * @n:      临时存储下一个节点
 * @head:   链表头
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)

/**
 * list_for_each_entry - 遍历包含链表节点的结构体
 * @pos:    包含结构体指针
 * @head:   链表头
 * @member: list_head在结构体中的名称
 */
#define list_for_each_entry(pos, head, member)              \
    for (pos = list_entry((head)->next, typeof(*pos), member);  \
         &pos->member != (head);                    \
         pos = list_entry(pos->member.next, typeof(*pos), member))

#endif /* _USERMODE_LIST_H_ */