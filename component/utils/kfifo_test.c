#include "kfifo.h"
#include <stdio.h>

int main() {
    struct kfifo *fifo = kfifo_alloc(1024);
    char data_in[] = "Hello kfifo!";
    char data_out[20] = {0};

    kfifo_in(fifo, data_in, strlen(data_in));  // 入队
    kfifo_out(fifo, data_out, sizeof(data_out)); // 出队

    printf("Received: %s\n", data_out);
    kfifo_free(fifo);
    return 0;
}