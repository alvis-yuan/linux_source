#ifndef _USER_LIST_H
#define _USER_LIST_H

#include <stddef.h>
#include <stdbool.h>

/*
 * 简单双向链表实现
 * 移除了所有内核特定功能，适用于用户态程序
 */

struct list_head {
    struct list_head *next, *prev;
};

#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

/**
 * INIT_LIST_HEAD - 初始化list_head结构
 * @list: 要初始化的list_head结构
 *
 * 将list_head初始化为指向自身。如果是列表头，结果是一个空列表。
 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/*
 * 在两个已知连续条目之间插入新条目
 * 仅用于内部列表操作，我们已经知道prev/next条目！
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
 * list_add - 添加新条目
 * @new: 要添加的新条目
 * @head: 要添加到的列表头（之后）
 *
 * 在指定头部后插入新条目。适用于实现栈。
 */
static inline void list_add(struct list_head *new, struct list_head *head)
{
    __list_add(new, head, head->next);
}

/**
 * list_add_tail - 添加新条目到尾部
 * @new: 要添加的新条目
 * @head: 要添加到的列表头（之前）
 *
 * 在指定头部前插入新条目。适用于实现队列。
 */
static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
    __list_add(new, head->prev, head);
}

/*
 * 通过使prev/next条目相互指向来删除列表条目
 * 仅用于内部列表操作，我们已经知道prev/next条目！
 */
static inline void __list_del(struct list_head *prev, struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

static inline void __list_del_entry(struct list_head *entry)
{
    __list_del(entry->prev, entry->next);
}

/**
 * list_del - 从列表中删除条目
 * @entry: 要从列表中删除的元素
 * 注意：此后在条目上调用list_empty()不会返回true，条目处于未定义状态。
 */
static inline void list_del(struct list_head *entry)
{
    __list_del_entry(entry);
    entry->next = NULL;
    entry->prev = NULL;
}

/**
 * list_replace - 用新条目替换旧条目
 * @old: 要被替换的元素
 * @new: 要插入的新元素
 */
static inline void list_replace(struct list_head *old,
                struct list_head *new)
{
    new->next = old->next;
    new->next->prev = new;
    new->prev = old->prev;
    new->prev->next = new;
}

/**
 * list_replace_init - 用新条目替换旧条目并初始化旧条目
 * @old: 要被替换的元素
 * @new: 要插入的新元素
 */
static inline void list_replace_init(struct list_head *old,
                     struct list_head *new)
{
    list_replace(old, new);
    INIT_LIST_HEAD(old);
}

/**
 * list_del_init - 从列表中删除条目并重新初始化它
 * @entry: 要从列表中删除的元素
 */
static inline void list_del_init(struct list_head *entry)
{
    __list_del_entry(entry);
    INIT_LIST_HEAD(entry);
}

/**
 * list_move - 从一个列表中删除并添加为另一个列表的头部
 * @list: 要移动的条目
 * @head: 将位于我们条目之前的头部
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
    __list_del_entry(list);
    list_add(list, head);
}

/**
 * list_move_tail - 从一个列表中删除并添加为另一个列表的尾部
 * @list: 要移动的条目
 * @head: 将跟随我们条目的头部
 */
static inline void list_move_tail(struct list_head *list,
                  struct list_head *head)
{
    __list_del_entry(list);
    list_add_tail(list, head);
}

/**
 * list_is_first - 测试@list是否是列表@head中的第一个条目
 * @list: 要测试的条目
 * @head: 列表的头部
 */
static inline int list_is_first(const struct list_head *list,
                    const struct list_head *head)
{
    return list->prev == head;
}

/**
 * list_is_last - 测试@list是否是列表@head中的最后一个条目
 * @list: 要测试的条目
 * @head: 列表的头部
 */
static inline int list_is_last(const struct list_head *list,
                const struct list_head *head)
{
    return list->next == head;
}

/**
 * list_empty - 测试列表是否为空
 * @head: 要测试的列表
 */
static inline int list_empty(const struct list_head *head)
{
    return head->next == head;
}

/**
 * list_is_singular - 测试列表是否只有一个条目
 * @head: 要测试的列表
 */
static inline int list_is_singular(const struct list_head *head)
{
    return !list_empty(head) && (head->next == head->prev);
}

static inline void __list_splice(const struct list_head *list,
                 struct list_head *prev,
                 struct list_head *next)
{
    struct list_head *first = list->next;
    struct list_head *last = list->prev;

    first->prev = prev;
    prev->next = first;

    last->next = next;
    next->prev = last;
}

/**
 * list_splice - 连接两个列表，适用于栈
 * @list: 要添加的新列表
 * @head: 在第一个列表中添加的位置
 */
static inline void list_splice(const struct list_head *list,
                struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head, head->next);
}

/**
 * list_splice_tail - 连接两个列表，每个列表都是一个队列
 * @list: 要添加的新列表
 * @head: 在第一个列表中添加的位置
 */
static inline void list_splice_tail(struct list_head *list,
                struct list_head *head)
{
    if (!list_empty(list))
        __list_splice(list, head->prev, head);
}

/**
 * list_splice_init - 连接两个列表并重新初始化空列表
 * @list: 要添加的新列表
 * @head: 在第一个列表中添加的位置
 * @list处的列表被重新初始化
 */
static inline void list_splice_init(struct list_head *list,
                    struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head, head->next);
        INIT_LIST_HEAD(list);
    }
}

/**
 * list_splice_tail_init - 连接两个列表并重新初始化空列表
 * @list: 要添加的新列表
 * @head: 在第一个列表中添加的位置
 * @list处的列表被重新初始化
 */
static inline void list_splice_tail_init(struct list_head *list,
                     struct list_head *head)
{
    if (!list_empty(list)) {
        __list_splice(list, head->prev, head);
        INIT_LIST_HEAD(list);
    }
}

#define container_of(ptr, type, member) ({ \
    const typeof( ((type *)0)->member ) *__mptr = (ptr); \
    (type *)( (char *)__mptr - offsetof(type,member) ); \
})

/**
 * list_entry - 获取此条目的结构体
 * @ptr:    &struct list_head指针
 * @type:   此结构体嵌入的类型
 * @member: 结构体中list_head的名称
 */
#if 0
#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))
#else
#define list_entry(ptr, type, member) container_of(ptr, type, member)
#endif

/**
 * list_first_entry - 从列表中获取第一个元素
 * @ptr:    要从中获取元素的列表头
 * @type:   此结构体嵌入的类型
 * @member: 结构体中list_head的名称
 * 注意：列表预期不为空
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/**
 * list_last_entry - 从列表中获取最后一个元素
 * @ptr:    要从中获取元素的列表头
 * @type:   此结构体嵌入的类型
 * @member: 结构体中list_head的名称
 * 注意：列表预期不为空
 */
#define list_last_entry(ptr, type, member) \
    list_entry((ptr)->prev, type, member)

/**
 * list_first_entry_or_null - 从列表中获取第一个元素
 * @ptr:    要从中获取元素的列表头
 * @type:   此结构体嵌入的类型
 * @member: 结构体中list_head的名称
 * 注意：如果列表为空，则返回NULL
 */
#define list_first_entry_or_null(ptr, type, member) ({ \
    struct list_head *head__ = (ptr); \
    struct list_head *pos__ = head__->next; \
    pos__ != head__ ? list_entry(pos__, type, member) : NULL; \
})

/**
 * list_next_entry - 获取列表中的下一个元素
 * @pos:    类型*光标
 * @member: 结构体中list_head的名称
 */
#define list_next_entry(pos, member) \
    list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * list_prev_entry - 获取列表中的上一个元素
 * @pos:    类型*光标
 * @member: 结构体中list_head的名称
 */
#define list_prev_entry(pos, member) \
    list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * list_for_each - 遍历列表
 * @pos:    &struct list_head用作循环光标
 * @head:   列表的头部
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_prev - 反向遍历列表
 * @pos:    &struct list_head用作循环光标
 * @head:   列表的头部
 */
#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * list_for_each_safe - 安全地遍历列表，防止列表条目被移除
 * @pos:    &struct list_head用作循环光标
 * @n:      另一个&struct list_head用作临时存储
 * @head:   列表的头部
 */
#define list_for_each_safe(pos, n, head) \
    for (pos = (head)->next, n = pos->next; pos != (head); \
        pos = n, n = pos->next)

/**
 * list_for_each_prev_safe - 安全地反向遍历列表，防止列表条目被移除
 * @pos:    &struct list_head用作循环光标
 * @n:      另一个&struct list_head用作临时存储
 * @head:   列表的头部
 */
#define list_for_each_prev_safe(pos, n, head) \
    for (pos = (head)->prev, n = pos->prev; \
         pos != (head); \
         pos = n, n = pos->prev)

/**
 * list_entry_is_head - 测试条目是否指向列表头部
 * @pos:    类型*光标
 * @head:   列表的头部
 * @member: 结构体中list_head的名称
 */
#define list_entry_is_head(pos, head, member) \
    (&(pos)->member == (head))

/**
 * list_for_each_entry - 遍历给定类型的列表
 * @pos:    类型*用作循环光标
 * @head:   列表的头部
 * @member: 结构体中list_head的名称
 */
#define list_for_each_entry(pos, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_reverse - 反向遍历给定类型的列表
 * @pos:    类型*用作循环光标
 * @head:   列表的头部
 * @member: 结构体中list_head的名称
 */
#define list_for_each_entry_reverse(pos, head, member) \
    for (pos = list_last_entry(head, typeof(*pos), member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_prev_entry(pos, member))

/**
 * list_for_each_entry_continue - 继续遍历给定类型的列表
 * @pos:    类型*用作循环光标
 * @head:   列表的头部
 * @member: 结构体中list_head的名称
 * 继续遍历给定类型的列表，从当前位置之后继续
 */
#define list_for_each_entry_continue(pos, head, member) \
    for (pos = list_next_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = list_next_entry(pos, member))

/**
 * list_for_each_entry_safe - 安全地遍历给定类型的列表，防止列表条目被移除
 * @pos:    类型*用作循环光标
 * @n:      另一个类型*用作临时存储
 * @head:   列表的头部
 * @member: 结构体中list_head的名称
 */
#define list_for_each_entry_safe(pos, n, head, member) \
    for (pos = list_first_entry(head, typeof(*pos), member), \
        n = list_next_entry(pos, member); \
         !list_entry_is_head(pos, head, member); \
         pos = n, n = list_next_entry(n, member))

#endif /* _USER_LIST_H */