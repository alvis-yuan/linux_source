#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main() {
    // ==================== open_memstream 示例 ====================
    // 用于动态增长的内存缓冲区写入操作
    
    char *buffer = NULL;
    size_t size = 0;
    FILE *stream = open_memstream(&buffer, &size);
    
    if (!stream) {
        perror("open_memstream failed");
        return EXIT_FAILURE;
    }

    // 像操作普通文件一样写入数据
    fprintf(stream, "Hello, open_memstream!\n");
    fprintf(stream, "Current size: %zu\n", size);
    fprintf(stream, "This data will be dynamically allocated in memory.");
    
    // 在写入完成后必须调用fflush或fclose，缓冲区才会更新
    fflush(stream);
    printf("\n1. open_memstream 结果:\nBuffer(%zu bytes): %s\n", size, buffer);
    
    // 可以继续追加内容
    fprintf(stream, "\nAdditional line after flush.");
    fclose(stream); // 关闭流也会更新缓冲区
    
    printf("\n追加后的结果:\nBuffer(%zu bytes): %s\n", size, buffer);
    
    // 记得释放缓冲区
    free(buffer);

    // ==================== fmemopen 示例 ====================
    // 用于从固定内存区域读写操作
    
    char fixed_buffer[256] = "Initial content. ";
    const char *new_data = "This is data written through fmemopen.";
    
    // 打开现有内存区域作为流 (模式 "a+" 允许读写和追加)
    stream = fmemopen(fixed_buffer, sizeof(fixed_buffer), "a+");
    if (!stream) {
        perror("fmemopen failed");
        return EXIT_FAILURE;
    }

    // 写入数据到内存缓冲区
    fprintf(stream, "%s", new_data);
    
    // 读取当前位置后的内容
    printf("\n2. fmemopen 结果:\n");
    printf("当前缓冲区内容: %s\n", fixed_buffer);
    
    // 演示读取操作
    rewind(stream); // 回到开头
    char read_buffer[256];
    fgets(read_buffer, sizeof(read_buffer), stream);
    printf("从流中读取的内容: %s\n", read_buffer);
    
    fclose(stream);

    // ==================== 组合使用示例 ====================
    // 使用fmemopen读取，open_memstream处理结果
    
    const char *input = "Name: John\nAge: 30\nOccupation: Engineer";
    char *output_buffer = NULL;
    size_t output_size = 0;
    
    // 输入流 (fmemopen)
    FILE *in_stream = fmemopen((void*)input, strlen(input), "r");
    // 输出流 (open_memstream)
    FILE *out_stream = open_memstream(&output_buffer, &output_size);
    
    // 处理数据: 转换为JSON格式
    fprintf(out_stream, "{\n");
    char line[256];
    while (fgets(line, sizeof(line), in_stream)) {
        char *colon = strchr(line, ':');
        if (colon) {
            *colon = '\0';
            char *value = colon + 1;
            // 去除value两端的空白
            while (*value == ' ') value++;
            char *end = value + strlen(value) - 1;
            while (end > value && (*end == ' ' || *end == '\n')) end--;
            *(end + 1) = '\0';
            
            fprintf(out_stream, "  \"%s\": \"%s\"%s\n", 
                   line, value, 
                   feof(in_stream) ? "" : ",");
        }
    }
    fprintf(out_stream, "}\n");
    
    fclose(in_stream);
    fclose(out_stream); // 这会更新output_buffer和output_size
    
    printf("\n3. 组合使用示例 - 格式转换结果:\n%s\n", output_buffer);
    free(output_buffer);

    return EXIT_SUCCESS;
}
