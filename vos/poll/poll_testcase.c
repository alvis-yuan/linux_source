#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <pthread.h>

#include "pollable.h"
#include "vos_timer.h"
#include "vos_signal.h"

// 测试数据结构
struct test_context {
    int event_count;
    int fd;
    uint32_t events;
    void *user_data;
};

#if 0
// 事件处理函数
static void test_event_handler(int fd, uint32_t events, void *user_data)
{
    struct test_context *ctx = (struct test_context *)user_data;
    if (ctx) {
        ctx->event_count++;
        ctx->fd = fd;
        ctx->events = events;
        printf("Event handler called: fd=%d, events=0x%x, count=%d\n", 
               fd, events, ctx->event_count);
    }
}

// 测试1: 创建和销毁epoll实例
void test_epoll_instance_creation()
{
    printf("=== Test 1: Epoll Instance Creation and Destruction ===\n");
    
    // 测试创建
    int ret = epoll_instance_create();
    assert(ret == 0);
    printf("Epoll instance created successfully\n");
    
    // 测试销毁
    epoll_instance_destroy();
    printf("Epoll instance destroyed successfully\n");
    
    // 测试重复销毁
    epoll_instance_destroy(); // 应该不会崩溃
    printf("Duplicate destroy handled safely\n");
    
    printf("Test 1 PASSED\n\n");
}

// 测试2: 添加和删除事件
void test_epoll_add_remove_events()
{
    printf("=== Test 2: Epoll Add and Remove Events ===\n");
    
    // 创建epoll实例
    assert(epoll_instance_create() == 0);
    
    // 创建测试文件描述符
    int pipe_fds[2];
    assert(pipe(pipe_fds) == 0);
    
    struct test_context ctx = {0};
    
    // 测试添加事件
    int ret = epoll_add_event(pipe_fds[0], EVENT_TYPE_SOCKET, 
                            EPOLLIN, test_event_handler, &ctx);
    assert(ret == 0);
    printf("Event added successfully\n");
    
    // 测试重复添加
    ret = epoll_add_event(pipe_fds[0], EVENT_TYPE_SOCKET, 
                        EPOLLIN, test_event_handler, &ctx);
    assert(ret == -1); // 应该失败
    printf("Duplicate add handled correctly\n");
    
    // 测试删除事件
    ret = epoll_remove_event(pipe_fds[0]);
    assert(ret == 0);
    printf("Event removed successfully\n");
    
    // 测试删除不存在的文件描述符
    ret = epoll_remove_event(999); // 不存在的fd
    assert(ret == -1); // 应该失败
    printf("Remove non-existent fd handled correctly\n");
    
    // 清理
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    epoll_instance_destroy();
    printf("Test 2 PASSED\n\n");
}

// 测试3: 修改事件
void test_epoll_modify_events()
{
    printf("=== Test 3: Epoll Modify Events ===\n");
    
    // 创建epoll实例
    assert(epoll_instance_create() == 0);
    
    // 创建测试文件描述符
    int pipe_fds[2];
    assert(pipe(pipe_fds) == 0);
    
    struct test_context ctx = {0};
    
    // 添加事件
    assert(epoll_add_event(pipe_fds[0], EVENT_TYPE_SOCKET, 
                              EPOLLIN, test_event_handler, &ctx) == 0);
    
    // 测试修改事件
    int ret = epoll_modify_event(pipe_fds[0], EPOLLIN | EPOLLOUT);
    assert(ret == 0);
    printf("Event modified successfully\n");
    
    // 测试修改不存在的文件描述符
    ret = epoll_modify_event(999, EPOLLIN); // 不存在的fd
    assert(ret == -1); // 应该失败
    printf("Modify non-existent fd handled correctly\n");
    
    // 清理
    epoll_remove_event(pipe_fds[0]);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    epoll_instance_destroy();
    printf("Test 3 PASSED\n\n");
}

static void timer_cb(vos_timer_t *timer)
{
    struct test_context *ctx = (struct test_context *)vos_timer_get_user_data(timer);
    if (ctx) {
        ctx->event_count++;
        printf("Timer callback called: count=%d\n", ctx->event_count);
    }
}

// 测试4: 定时器事件
void test_epoll_timer_events()
{
    printf("=== Test 4: Epoll Timer Events ===\n");
    
    // 创建epoll实例
    assert(epoll_instance_create() == 0);
    
    // 创建定时器
    struct test_context ctx = {0};
    timer_t *timer = vos_timer_create(timer_cb, 1000, &ctx);
    assert(timer != NULL);
    
    // 启动定时器
    //vos_timer_set_repeat_count(timer, 1); // 触发2次

    vos_timer_set_auto_delete(timer, 1);
    
    // 运行epoll事件循环（短暂运行）
    printf("Running epoll for short time to catch timer events...\n");

    vos_timer_start(timer);
    
    // 注意：这里我们不会真正运行事件循环，因为这会阻塞测试
    // 在实际测试中，您可能需要使用超时或信号来控制事件循环
    epoll_run(); // 运行100ms
    
    // 清理
    //epoll_remove_event(vos_timer_get_fd(timer));
    //vos_timer_delete(timer);
    epoll_instance_destroy();
    printf("Test 4 PASSED\n\n");
}

// 测试5: 边界条件测试
void test_epoll_edge_cases()
{
    printf("=== Test 5: Epoll Edge Cases ===\n");
    
    // 测试无效参数
    int ret = epoll_add_event(-1, EVENT_TYPE_SOCKET, EPOLLIN, NULL, NULL);
    assert(ret == -1); // 应该失败
    printf("Invalid fd handled correctly\n");
    
    ret = epoll_add_event(0, EVENT_TYPE_SOCKET, EPOLLIN, NULL, NULL);
    assert(ret == -1); // 应该失败
    printf("NULL handler handled correctly\n");
    
    // 测试在没有epoll实例的情况下操作
    ret = epoll_add_event(1, EVENT_TYPE_SOCKET, EPOLLIN, test_event_handler, NULL);
    assert(ret == -1); // 应该失败
    printf("Operation without instance handled correctly\n");
    
    ret = epoll_remove_event(1);
    assert(ret == -1); // 应该失败
    printf("Remove without instance handled correctly\n");
    
    ret = epoll_modify_event(1, EPOLLIN);
    assert(ret == -1); // 应该失败
    printf("Modify without instance handled correctly\n");
    
    printf("Test 5 PASSED\n\n");
}

// 测试6: 运行和停止事件循环
void test_epoll_run_stop()
{
    printf("=== Test 6: Epoll Run and Stop ===\n");
    
    // 创建epoll实例
    assert(epoll_instance_create() == 0);
    
    // 测试停止（在没有运行的情况下）
    epoll_stop(); // 应该不会崩溃
    printf("Stop without running handled safely\n");
    
    // 注意：实际运行事件循环的测试需要更复杂的设置
    // 因为epoll_run()会阻塞，这里我们只测试接口调用
    
    epoll_instance_destroy();
    printf("Test 6 PASSED\n\n");
}

#endif
static void _timer_cb(vos_timer_t *timer)
{
    struct test_context *ctx = (struct test_context *)vos_timer_get_user_data(timer);
    if (ctx) {
        ctx->event_count++;
        printf("Timer callback called: count=%d\n", ctx->event_count);
    }
}

static int chg_data = 0;

static void __timer_cb(vos_timer_t *timer)
{
    chg_data++;
    printf("Timer callback called: chg_data=%d\n", chg_data);
}

static void get_sig_mask(void) {
    sigset_t thread_mask;
    
    LogInfo("tid: %ld", pthread_self());

    // 获取当前线程的信号掩码
    if (pthread_sigmask(SIG_BLOCK, NULL, &thread_mask) == -1) {
        perror("pthread_sigmask");
        return ;
    }
    
    printf("Thread signal mask: ");
    for (int i = 1; i < NSIG; i++) {
        LogInfo("signo: %d", i);
        if (sigismember(&thread_mask, i)) {
            printf("%d ", i);
        }
    }
    printf("\n");
    
}


static void *epoll_test(void *arg)
{
    int cnt = 0;
    vos_timer_t *timer = (vos_timer_t *)arg;

    while (1) {
        sleep(1);
        #if 1
        cnt++;
        if (cnt == 5) {
            vos_timer_stop(timer);
            //vos_timer_delete(timer);
        } else if (cnt == 10) {
            vos_timer_set_period(timer, 2000);
            //vos_timer_set_repeat_count(timer, 3);
            vos_timer_set_user_data(timer, &chg_data);
            vos_timer_set_cb(timer, __timer_cb);
            //vos_timer_set_auto_delete(timer, 1);
            vos_timer_resume(timer);
        }
        #endif
    }
}

static void signal_cb(const struct signalfd_siginfo *info, void *user_data)
{
    vos_timer_t *timer = (vos_timer_t *)user_data;
    LogInfo("Signal callback called: signo=%d from pid:%d, timer=%p", info->ssi_signo, info->ssi_pid, timer);

    if (info->ssi_signo == SIGINT || info->ssi_signo == SIGTERM) {
        LogInfo("Signal %d received, exiting test", info->ssi_signo);
        vos_timer_delete(timer);
        
        // 销毁信号处理器
        vos_signal_destroy();

        epoll_instance_destroy();        
        exit(0);
    }
}

// 主测试函数
int main()
{
#if 0    
    printf("Starting Epoll Framework Test Suite\n\n");
    
    test_epoll_instance_creation();
    test_epoll_add_remove_events();
    test_epoll_modify_events();
    test_epoll_timer_events();
    test_epoll_edge_cases();
    test_epoll_run_stop();
    
    printf("All epoll framework tests PASSED! ✓\n");
    printf("Epoll framework implementation is working correctly.\n");
#else
    int ret = 0;
    pthread_t tid;
    struct test_context ctx = {0};

    // 初始化信号处理器
    ret = vos_signal_init();
    assert(ret == 0);





    ret = epoll_instance_create();
    assert(ret == 0);

    // 创建定时器
    vos_timer_t *timer = vos_timer_create(_timer_cb, 1000, &ctx);
    assert(timer != NULL);

    // 安装信号处理器
    ret = vos_signal_install(SIGRTMIN + 10, signal_cb, timer);
    vos_signal_install(SIGINT, signal_cb, timer);
    vos_signal_install(SIGTERM, signal_cb, timer);    
    vos_signal_start();

    ret = pthread_create(&tid, NULL, epoll_test, timer);
    assert(ret == 0);

    // 启动定时器
    vos_timer_start(timer);

    epoll_run();

    vos_timer_delete(timer);
    
    // 销毁信号处理器
    vos_signal_destroy();

    epoll_instance_destroy();
#endif
    return 0;
}
