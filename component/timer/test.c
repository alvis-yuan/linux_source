#include "timer.h"
#include <stdio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>

#define MAX_EVENTS 10

void my_callback(void *data)
{
    printf("Timer expired! Data: %s\n", (char *)data);
}

int main()
{
    int epoll_fd;
    struct epoll_event ev, events[MAX_EVENTS];
    char *data = "Hello, Timer!";
    int nfds, i;
    
    /* 创建epoll实例 */
    epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }
    
    /* 创建一次性定时器，1秒后触发 */
    timer_st *oneshot = timer_creat(1000, my_callback, data, false);
    timer_start(oneshot);
    
    /* 添加oneshot定时器到epoll */
    ev.events = EPOLLIN;
    ev.data.ptr = oneshot;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_get_fd(oneshot), &ev) == -1) {
        perror("epoll_ctl: oneshot");
        exit(EXIT_FAILURE);
    }
    
    /* 创建周期性定时器，每2秒触发一次 */
    timer_st *periodic = timer_creat(2000, my_callback, data, true);
    timer_start(periodic);
    
    /* 添加periodic定时器到epoll */
    ev.events = EPOLLIN;
    ev.data.ptr = periodic;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, timer_get_fd(periodic), &ev) == -1) {
        perror("epoll_ctl: periodic");
        exit(EXIT_FAILURE);
    }
    
    /* 主事件循环 */
    while (1) {
        nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            break;
        }
        
        for (i = 0; i < nfds; ++i) {
            timer_st *timer = (timer_st *)events[i].data.ptr;
            
            if (events[i].events & EPOLLIN) {
                /* 处理定时器事件 */
                int ret = timer_handle_event(timer);
                
                /* 如果是oneshot定时器且已被删除 */
                if (ret == 1) {
                    /* 不需要从epoll删除，因为fd已经自动关闭 */
                    oneshot = NULL;
                    
                    /* 检查是否还有periodic定时器 */
                    if (periodic == NULL) {
                        goto cleanup;
                    }
                }
            }
        }
    }
    
cleanup:
    /* 清理剩余定时器 */
    if (oneshot) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, timer_get_fd(oneshot), NULL);
        timer_destroy(oneshot);
    }
    if (periodic) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, timer_get_fd(periodic), NULL);
        timer_destroy(periodic);
    }
    
    close(epoll_fd);
    return 0;
}