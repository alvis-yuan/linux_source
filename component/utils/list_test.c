#include <stdio.h>
#include <stdlib.h>
#include "list.h"

// 示例数据结构
struct my_data {
    int value;
    struct list_head list;
};

int main() {
    // 初始化链表头
    LIST_HEAD(my_list);

    // 添加三个节点
    for (int i = 0; i < 3; i++) {
        struct my_data *data = malloc(sizeof(*data));
        data->value = i;
        list_add_tail(&data->list, &my_list);
    }

    // 遍历链表
    struct my_data *pos;
    list_for_each_entry(pos, &my_list, list) {
        printf("Value: %d\n", pos->value);
    }

    // 安全删除所有节点
    struct list_head *n, *tmp;
    list_for_each_safe(n, tmp, &my_list) {
        struct my_data *data = list_entry(n, struct my_data, list);
        list_del(n);
        free(data);
    }

    // 检查链表是否为空
    if (list_empty(&my_list)) {
        printf("List is now empty\n");
    }

    return 0;
}