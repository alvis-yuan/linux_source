#include <stdio.h>
#include <stdlib.h>

int main() {
    const char *input = "A 1234567890Hello World! 42OnlyLetters dynamic-string";
    char ch;
    char fixed_char[11];  // 用于%10c，需要多一位放终止符
    char str[100];
    char limited_str[11];  // 用于%10s
    char letters_only[100];
    char *dynamic_str = NULL;
    int count;

    // 1. %c - 从字符串中读取单个字符
    sscanf(input, "%c", &ch);
    printf("1. %%c 读取的字符: %c\n", ch);

    // 2. %10c - 读取固定数量的字符(10个)
    sscanf(input, "%10c", fixed_char);  // 跳过已读取的字符
    fixed_char[10] = '\0';  // 手动添加终止符
    printf("2. %%10c 读取的10个字符: %s\n", fixed_char);

    // 3. %s - 读取字符串(遇到空白字符停止)
    sscanf(input, "%s", str);  // 跳过已读取的部分
    printf("3. %%s 读取的字符串: %s\n", str);

    // 4. %10s - 读取最多10个字符的字符串
    sscanf(input, "%10s", limited_str);  // "World!"只有6字符
    printf("4. %%10s 读取的字符串(最多10字符): %s\n", limited_str);

    // 5. %[a-zA-Z] - 只读取字母
    sscanf(input, "%10[a-zA-Z0-9 ]", letters_only);
    printf("5. %%[a-zA-Z] 读取的字母字符串: %s\n", letters_only);

    // 6. %ms(%as) - GNU扩展,动态分配字符串(注意:这不是标准C)
    #ifdef __GNUC__
    sscanf(input, "%ms", &dynamic_str);
    printf("6. %%ms 动态分配的字符串: %s\n", dynamic_str);
    free(dynamic_str);
    #else
    printf("6. %%ms 动态分配字符串是GNU扩展，当前编译器不支持\n");
    #endif

    // 高级用法: 一次解析多个字段
    const char *complex_input = "Name: John Age: 25 Score: 85.5";
    char name[20];
    int age;
    float score;
    
    count = sscanf(complex_input, "Name: %s Age: %d Score: %f", name, &age, &score);
    printf("\n高级示例:\n");
    printf("成功解析了%d个字段:\n", count);
    printf("Name: %s\nAge: %d\nScore: %.1f\n", name, age, score);

    return 0;
}
