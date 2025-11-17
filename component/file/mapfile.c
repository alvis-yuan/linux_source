#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>  // mmap相关头文件
#include <sys/stat.h>  // 文件状态
#include <fcntl.h>     // 文件控制选项
#include <unistd.h>    // POSIX API
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "用法: %s <文件名>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    
    // 1. 打开文件
    int fd = open(filename, O_RDWR);  // 需要读写权限
    if (fd == -1) {
        perror("open failed");
        return EXIT_FAILURE;
    }

    // 2. 获取文件大小
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat failed");
        close(fd);
        return EXIT_FAILURE;
    }
    size_t file_size = sb.st_size;
    printf("文件大小: %zu 字节\n", file_size);

    // 3. 将文件映射到内存, 注意：mmap的大小不能为零，所以需要确保文件大小大于0，例如使用ftruncate扩展文件大小
    char *mapped = mmap(NULL, file_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        perror("mmap failed");
        close(fd);
        return EXIT_FAILURE;
    }

    // 4. 现在可以像操作内存一样操作文件内容
    printf("\n文件前100字节内容:\n");
    for (int i = 0; i < 100 && i < file_size; i++) {
        putchar(mapped[i]);
    }
    printf("\n");

    // 示例：修改文件内容 (将前10字节转换为大写)
    printf("\n将前10个字母转换为大写...\n");
    for (int i = 0; i < 10 && i < file_size; i++) {
        if (mapped[i] >= 'a' && mapped[i] <= 'z') {
            mapped[i] = mapped[i] - 'a' + 'A';
        }
    }

    // 5. 同步修改到磁盘 (可选)
    if (msync(mapped, file_size, MS_SYNC) == -1) {
        perror("msync failed");
    }

    // 6. 解除映射
    if (munmap(mapped, file_size) == -1) {
        perror("munmap failed");
    }

    // 7. 关闭文件
    close(fd);

    printf("操作完成。文件 '%s' 已被修改。\n", filename);
    return EXIT_SUCCESS;
}
