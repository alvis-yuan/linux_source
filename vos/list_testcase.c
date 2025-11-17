#include "vos.h"

// 测试数据结构
struct test_data {
    int value;
    struct list_head list;
    char name[20];
};

// 打印列表内容
void print_list(struct list_head *head) {
    struct test_data *entry;
    printf("List: ");
    list_for_each_entry(entry, head, list) {
        printf("%d(%s) ", entry->value, entry->name);
    }
    printf("\n");
}

// 测试1: 基本初始化和空列表检查
void test_basic_init_and_empty() {
    printf("=== Test 1: Basic Init and Empty Check ===\n");
    
    LIST_HEAD(my_list);
    assert(list_empty(&my_list));
    printf("Empty list check passed\n");
    
    struct list_head dynamic_list;
    INIT_LIST_HEAD(&dynamic_list);
    assert(list_empty(&dynamic_list));
    printf("Dynamic init check passed\n");
    
    printf("Test 1 PASSED\n\n");
}

// 测试2: 添加和删除单个元素
void test_single_element_operations() {
    printf("=== Test 2: Single Element Operations ===\n");
    
    LIST_HEAD(my_list);
    struct test_data *data = malloc(sizeof(struct test_data));
    data->value = 42;
    snprintf(data->name, sizeof(data->name), "single");
    
    // 测试 list_add
    list_add(&data->list, &my_list);
    assert(!list_empty(&my_list));
    assert(list_is_first(&data->list, &my_list));
    assert(list_is_last(&data->list, &my_list));
    assert(list_is_singular(&my_list));
    printf("Single element add check passed\n");
    
    // 测试 list_del
    list_del(&data->list);
    assert(list_empty(&my_list));
    printf("Single element delete check passed\n");
    
    free(data);
    printf("Test 2 PASSED\n\n");
}

// 测试3: 多元素操作
void test_multiple_elements() {
    printf("=== Test 3: Multiple Elements Operations ===\n");
    
    LIST_HEAD(my_list);
    struct test_data *data1 = malloc(sizeof(struct test_data));
    struct test_data *data2 = malloc(sizeof(struct test_data));
    struct test_data *data3 = malloc(sizeof(struct test_data));
    
    data1->value = 1; snprintf(data1->name, sizeof(data1->name), "first");
    data2->value = 2; snprintf(data2->name, sizeof(data2->name), "second");
    data3->value = 3; snprintf(data3->name, sizeof(data3->name), "third");
    
    // 添加多个元素
    list_add_tail(&data1->list, &my_list);
    list_add_tail(&data2->list, &my_list);
    list_add_tail(&data3->list, &my_list);
    
    assert(!list_empty(&my_list));
    assert(!list_is_singular(&my_list));
    assert(list_is_first(&data1->list, &my_list));
    assert(list_is_last(&data3->list, &my_list));
    printf("Multiple elements add check passed\n");
    
    // 测试遍历
    int count = 0;
    struct test_data *entry;
    list_for_each_entry(entry, &my_list, list) {
        count++;
        assert(entry->value == count);
    }
    assert(count == 3);
    printf("Forward traversal check passed\n");
    
    // 测试反向遍历
    count = 3;
    list_for_each_entry_reverse(entry, &my_list, list) {
        assert(entry->value == count);
        count--;
    }
    assert(count == 0);
    printf("Reverse traversal check passed\n");
    
    // 清理
    list_del(&data1->list);
    list_del(&data2->list);
    list_del(&data3->list);
    free(data1);
    free(data2);
    free(data3);
    
    printf("Test 3 PASSED\n\n");
}

// 测试4: list_move 操作
void test_list_move() {
    printf("=== Test 4: List Move Operations ===\n");
    
    LIST_HEAD(list1);
    LIST_HEAD(list2);
    
    struct test_data *data1 = malloc(sizeof(struct test_data));
    struct test_data *data2 = malloc(sizeof(struct test_data));
    struct test_data *data3 = malloc(sizeof(struct test_data));
    
    data1->value = 1; snprintf(data1->name, sizeof(data1->name), "move1");
    data2->value = 2; snprintf(data2->name, sizeof(data2->name), "move2");
    data3->value = 3; snprintf(data3->name, sizeof(data3->name), "move3");
    
    // 初始化列表1
    list_add_tail(&data1->list, &list1);
    list_add_tail(&data2->list, &list1);
    list_add_tail(&data3->list, &list1);
    
    // 移动元素到列表2
    list_move(&data2->list, &list2);
    assert(list_is_singular(&list2));
    assert(!list_is_singular(&list1));
    printf("List move to head check passed\n");
    
    // 移动元素到尾部
    list_move_tail(&data1->list, &list2);
    assert(!list_empty(&list1));
    assert(!list_empty(&list2));
    printf("List move to tail check passed\n");
    
    // 清理
    list_del(&data1->list);
    list_del(&data2->list);
    list_del(&data3->list);
    free(data1);
    free(data2);
    free(data3);
    
    printf("Test 4 PASSED\n\n");
}

// 测试5: list_replace 操作
void test_list_replace() {
    printf("=== Test 5: List Replace Operations ===\n");
    
    LIST_HEAD(my_list);
    
    struct test_data *old_data = malloc(sizeof(struct test_data));
    struct test_data *new_data = malloc(sizeof(struct test_data));
    struct test_data *another_data = malloc(sizeof(struct test_data));
    
    old_data->value = 100; snprintf(old_data->name, sizeof(old_data->name), "old");
    new_data->value = 200; snprintf(new_data->name, sizeof(new_data->name), "new");
    another_data->value = 300; snprintf(another_data->name, sizeof(another_data->name), "another");
    
    // 添加两个元素，这样替换后可以更好地验证
    list_add_tail(&old_data->list, &my_list);
    list_add_tail(&another_data->list, &my_list);
    
    // 测试替换
    list_replace(&old_data->list, &new_data->list);
    
    // 检查替换是否正确
    struct test_data *entry = list_first_entry(&my_list, struct test_data, list);
    assert(entry->value == 200);
    assert(entry == new_data);
    
    // 检查列表中有两个元素
    int count = 0;
    list_for_each_entry(entry, &my_list, list) {
        count++;
    }
    assert(count == 2);
    printf("List replace check passed\n");
    
    // 测试替换并初始化
    list_replace_init(&new_data->list, &old_data->list);
    // list_replace_init 会初始化被替换的节点
    assert(list_empty(&new_data->list));
    
    entry = list_first_entry(&my_list, struct test_data, list);
    assert(entry->value == 100);
    assert(entry == old_data);
    printf("List replace init check passed\n");
    
    // 清理
    list_del(&old_data->list);
    list_del(&another_data->list);
    free(old_data);
    free(new_data);
    free(another_data);
    
    printf("Test 5 PASSED\n\n");
} 

// 测试6: 安全遍历和删除
void test_safe_traversal_and_deletion() {
    printf("=== Test 6: Safe Traversal and Deletion ===\n");
    
    LIST_HEAD(my_list);
    
    // 创建多个元素
    struct test_data *entries[5];
    for (int i = 0; i < 5; i++) {
        entries[i] = malloc(sizeof(struct test_data));
        entries[i]->value = i + 1;
        snprintf(entries[i]->name, sizeof(entries[i]->name), "entry%d", i+1);
        list_add_tail(&entries[i]->list, &my_list);
    }
    
    // 安全遍历并删除奇数元素
    struct test_data *entry, *temp;
    int count = 0;
    list_for_each_entry_safe(entry, temp, &my_list, list) {
        if (entry->value % 2 == 1) {
            list_del(&entry->list);
            free(entry);
            count++;
        }
    }
    assert(count == 3);
    printf("Safe traversal and deletion check passed\n");
    
    // 检查剩余元素
    count = 0;
    list_for_each_entry(entry, &my_list, list) {
        assert(entry->value % 2 == 0);
        count++;
    }
    assert(count == 2);
    
    // 清理剩余元素
    list_for_each_entry_safe(entry, temp, &my_list, list) {
        list_del(&entry->list);
        free(entry);
    }
    
    printf("Test 6 PASSED\n\n");
}

// 测试7: list_splice 操作
void test_list_splice() {
    printf("=== Test 7: List Splice Operations ===\n");
    
    LIST_HEAD(list1);
    LIST_HEAD(list2);
    
    // 创建两个列表
    struct test_data *data1 = malloc(sizeof(struct test_data));
    struct test_data *data2 = malloc(sizeof(struct test_data));
    struct test_data *data3 = malloc(sizeof(struct test_data));
    struct test_data *data4 = malloc(sizeof(struct test_data));
    
    data1->value = 1; snprintf(data1->name, sizeof(data1->name), "splice1");
    data2->value = 2; snprintf(data2->name, sizeof(data2->name), "splice2");
    data3->value = 3; snprintf(data3->name, sizeof(data3->name), "splice3");
    data4->value = 4; snprintf(data4->name, sizeof(data4->name), "splice4");
    
    list_add_tail(&data1->list, &list1);
    list_add_tail(&data2->list, &list1);
    list_add_tail(&data3->list, &list2);
    list_add_tail(&data4->list, &list2);
    
    printf("Before splice - List1: ");
    print_list(&list1);
    printf("Before splice - List2: ");
    print_list(&list2);
    
    // 测试拼接 - list_splice 将 list2 拼接到 list1 的头部
    list_splice(&list2, &list1);
    
    printf("After splice - List1: ");
    print_list(&list1);
    printf("After splice - List2: ");

    //(&list2);
    //return;
    
    // list_splice 将源列表拼接到目标列表的头部
    // 所以顺序应该是: list2的内容 + list1的内容
    int expected[] = {3, 4, 1, 2};
    int i = 0;
    struct test_data *entry;
    list_for_each_entry(entry, &list1, list) {
        printf("Checking: expected[%d]=%d, actual=%d\n", i, expected[i], entry->value);
        assert(entry->value == expected[i++]);
    }
    assert(i == 4);
    printf("List splice check passed\n");
    
    // 现在手动初始化 list2，因为它已经被拼接走了但未被初始化
    INIT_LIST_HEAD(&list2);
    assert(list_empty(&list2));
    
    // 测试带初始化的拼接 - 先重置 list1
    INIT_LIST_HEAD(&list1);
    list_add_tail(&data1->list, &list1);
    list_add_tail(&data2->list, &list1);
    
    LIST_HEAD(list3);
    struct test_data *data5 = malloc(sizeof(struct test_data));
    data5->value = 5; snprintf(data5->name, sizeof(data5->name), "splice5");
    list_add_tail(&data5->list, &list3);
    
    printf("Before splice_init - List1: ");
    print_list(&list1);
    printf("Before splice_init - List3: ");
    print_list(&list3);
    
    // list_splice_init 将 list3 拼接到 list1 的头部
    list_splice_init(&list3, &list1);
    
    printf("After splice_init - List1: ");
    print_list(&list1);
    printf("After splice_init - List3: ");
    print_list(&list3);
    
    assert(list_empty(&list3)); // list_splice_init 会自动初始化源列表
    
    i = 0;
    int expected2[] = {5, 1, 2};
    list_for_each_entry(entry, &list1, list) {
        printf("Checking: expected2[%d]=%d, actual=%d\n", i, expected2[i], entry->value);
        assert(entry->value == expected2[i++]);
    }
    assert(i == 3);
    printf("List splice init check passed\n");
    
    // 清理
    struct test_data *temp;
    list_for_each_entry_safe(entry, temp, &list1, list) {
        list_del(&entry->list);
        free(entry);
    }
    // 注意：data1, data2, data3, data4 已经在列表操作中被移动和释放
    // data5 需要单独释放，因为它可能不在 list1 中
    //free(data5);
    
    printf("Test 7 PASSED\n\n");
}

// 测试8: 边界条件测试
void test_edge_cases() {
    printf("=== Test 8: Edge Cases ===\n");
    
    // 测试空列表操作
    LIST_HEAD(empty_list);
    assert(list_empty(&empty_list));
    assert(!list_is_singular(&empty_list));
    printf("Empty list operations check passed\n");
    
    // 测试单元素列表
    LIST_HEAD(single_list);
    struct test_data *single = malloc(sizeof(struct test_data));
    single->value = 999;
    snprintf(single->name, sizeof(single->name), "single");
    
    list_add(&single->list, &single_list);
    assert(list_is_singular(&single_list));
    assert(list_is_first(&single->list, &single_list));
    assert(list_is_last(&single->list, &single_list));
    printf("Single element list operations check passed\n");
    
    // 测试 list_del_init
    list_del_init(&single->list);
    assert(list_empty(&single_list));
    assert(list_empty(&single->list));
    printf("List del init check passed\n");
    
    free(single);
    printf("Test 8 PASSED\n\n");
}

// 测试9: 宏功能测试
void test_macro_functions() {
    printf("=== Test 9: Macro Functions ===\n");
    
    LIST_HEAD(my_list);
    
    struct test_data *data1 = malloc(sizeof(struct test_data));
    struct test_data *data2 = malloc(sizeof(struct test_data));
    
    data1->value = 10; snprintf(data1->name, sizeof(data1->name), "macro1");
    data2->value = 20; snprintf(data2->name, sizeof(data2->name), "macro2");
    
    list_add_tail(&data1->list, &my_list);
    list_add_tail(&data2->list, &my_list);
    
    // 测试 list_entry
    struct list_head *node = &data1->list;
    struct test_data *entry = list_entry(node, struct test_data, list);
    assert(entry == data1);
    assert(entry->value == 10);
    printf("list_entry macro check passed\n");
    
    // 测试 list_first_entry
    entry = list_first_entry(&my_list, struct test_data, list);
    assert(entry == data1);
    printf("list_first_entry macro check passed\n");
    
    // 测试 list_last_entry
    entry = list_last_entry(&my_list, struct test_data, list);
    assert(entry == data2);
    printf("list_last_entry macro check passed\n");
    
    // 测试 list_next_entry
    entry = list_next_entry(data1, list);
    assert(entry == data2);
    printf("list_next_entry macro check passed\n");
    
    // 测试 list_prev_entry
    entry = list_prev_entry(data2, list);
    assert(entry == data1);
    printf("list_prev_entry macro check passed\n");
    
    // 测试 list_first_entry_or_null
    entry = list_first_entry_or_null(&my_list, struct test_data, list);
    assert(entry != NULL);
    assert(entry == data1);
    
    LIST_HEAD(empty_list);
    entry = list_first_entry_or_null(&empty_list, struct test_data, list);
    assert(entry == NULL);
    printf("list_first_entry_or_null macro check passed\n");
    
    // 清理
    list_del(&data1->list);
    list_del(&data2->list);
    free(data1);
    free(data2);
    
    printf("Test 9 PASSED\n\n");
}

int main() {
    printf("Starting Linked List Test Suite\n\n");
    
    test_basic_init_and_empty();
    test_single_element_operations();
    test_multiple_elements();
    test_list_move();
    test_list_replace();
    test_safe_traversal_and_deletion();
    test_list_splice();
    test_edge_cases();
    test_macro_functions();
    
    printf("All tests PASSED! ✓\n");
    printf("Linked List implementation is working correctly.\n");
    
    return 0;
}