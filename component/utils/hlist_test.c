#include <stdio.h>
#include <stdlib.h>
#include "hlist.h"

// 示例数据结构
struct user {
    int id;
    char name[32];
    struct hlist_node hash_node;
};

#define HASHTABLE_SIZE 8
struct hlist_head user_table[HASHTABLE_SIZE];

// 简单哈希函数
static inline unsigned int user_hash(int id)
{
    return id % HASHTABLE_SIZE;
}

// 添加用户到哈希表
void user_add(struct user *u)
{
    unsigned int hash = user_hash(u->id);
    hlist_add_head(&u->hash_node, &user_table[hash]);
}

// 查找用户
struct user *user_find(int id)
{
    unsigned int hash = user_hash(id);
    struct hlist_node *node;
    struct user *u;
    
    hlist_for_each_entry(u, node, &user_table[hash], hash_node) {
        if (u->id == id)
            return u;
    }
    return NULL;
}

int main()
{
    // 初始化哈希表
    for (int i = 0; i < HASHTABLE_SIZE; i++)
        INIT_HLIST_HEAD(&user_table[i]);

    // 添加测试用户
    struct user *u1 = malloc(sizeof(*u1));
    u1->id = 1001;
    snprintf(u1->name, sizeof(u1->name), "Alice");
    user_add(u1);

    struct user *u2 = malloc(sizeof(*u2));
    u2->id = 1002;
    snprintf(u2->name, sizeof(u2->name), "Bob");
    user_add(u2);

    // 查找测试
    struct user *found = user_find(1002);
    if (found)
        printf("Found user: %s\n", found->name);

    // 安全删除所有节点
    struct hlist_node *n, *tmp;
    for (int i = 0; i < HASHTABLE_SIZE; i++) {
        hlist_for_each_safe(n, tmp, &user_table[i]) {
            struct user *u = hlist_entry(n, struct user, hash_node);
            hlist_del(n);
            free(u);
        }
    }

    return 0;
}