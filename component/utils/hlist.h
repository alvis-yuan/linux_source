#ifndef _USERMODE_HLIST_H_
#define _USERMODE_HLIST_H_

#include <stddef.h> /* for offsetof */

/*
 * 用户态哈希表实现（基于Linux内核hlist）
 * 移植自Linux内核的include/linux/list.h
 */

/*
 * 哈希表节点（单指针链表头节省空间）
 */
struct hlist_head {
    struct hlist_node *first;
};

struct hlist_node {
    struct hlist_node *next;
    struct hlist_node **pprev;
};

#define HLIST_HEAD_INIT { .first = NULL }
#define HLIST_HEAD(name) struct hlist_head name = {  .first = NULL }
#define INIT_HLIST_HEAD(ptr) ((ptr)->first = NULL)

static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
    h->next = NULL;
    h->pprev = NULL;
}

/*
 * 判断节点是否已添加到哈希表
 */
static inline int hlist_unhashed(const struct hlist_node *h)
{
    return !h->pprev;
}

/*
 * 判断哈希表是否为空
 */
static inline int hlist_empty(const struct hlist_head *h)
{
    return !h->first;
}

/*
 * 内部函数：实际添加节点操作
 */
static inline void __hlist_del(struct hlist_node *n)
{
    struct hlist_node *next = n->next;
    struct hlist_node **pprev = n->pprev;
    
    *pprev = next;
    if (next)
        next->pprev = pprev;
}

/*
 * 从哈希表删除节点
 */
static inline void hlist_del(struct hlist_node *n)
{
    __hlist_del(n);
    n->next = NULL;
    n->pprev = NULL;
}

/*
 * 安全删除节点（同时初始化next/pprev）
 */
static inline void hlist_del_init(struct hlist_node *n)
{
    if (!hlist_unhashed(n)) {
        __hlist_del(n);
        INIT_HLIST_NODE(n);
    }
}

/*
 * 在哈希表头添加节点
 */
static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
    struct hlist_node *first = h->first;
    n->next = first;
    if (first)
        first->pprev = &n->next;
    h->first = n;
    n->pprev = &h->first;
}

/*
 * 在指定节点前/后添加新节点
 */
static inline void hlist_add_before(struct hlist_node *n,
                    struct hlist_node *next)
{
    n->pprev = next->pprev;
    n->next = next;
    next->pprev = &n->next;
    *(n->pprev) = n;
}

static inline void hlist_add_behind(struct hlist_node *n,
                    struct hlist_node *prev)
{
    n->next = prev->next;
    prev->next = n;
    n->pprev = &prev->next;

    if (n->next)
        n->next->pprev  = &n->next;
}

/*
 * 遍历宏
 */
#define hlist_entry(ptr, type, member) container_of(ptr,type,member)

#define hlist_for_each(pos, head) \
    for (pos = (head)->first; pos ; pos = pos->next)

#define hlist_for_each_safe(pos, n, head) \
    for (pos = (head)->first; pos && ({ n = pos->next; 1; }); \
         pos = n)

#define hlist_for_each_entry(tpos, pos, head, member)        \
    for (pos = (head)->first;                \
         pos && ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = pos->next)

#define hlist_for_each_entry_safe(tpos, pos, n, head, member)    \
    for (pos = (head)->first;                \
         pos && ({ n = pos->next; 1; }) &&            \
        ({ tpos = hlist_entry(pos, typeof(*tpos), member); 1;}); \
         pos = n)

/*
 * container_of宏（与内核实现相同）
 */
#define container_of(ptr, type, member) ({            \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

#endif /* _USERMODE_HLIST_H_ */