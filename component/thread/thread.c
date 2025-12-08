/**
 * @file thread_ops.c
 * @brief 线程操作接口实现
 */

#include "thread.h"
#include <stdbool.h>

typedef struct _thread_arg_t {
    void *(*start_routine)(void *);
    void *arg;
    const char *name;              /**< 线程名 */
} thread_arg_t;


/**
 * @brief 互斥锁上下文结构体
 */
typedef struct _mutex_ctx {
    pthread_mutex_t mutex;         /**< 互斥锁 */
#ifdef MUTEX_DEBUG
    pthread_t owner_pid;           /**< 当前锁的持有者线程ID */
    long owner_tid;                /**< 通过sys_gettid获取的线程ID */
    char owner_name[16];           /**< 当前锁的持有者线程名 */
    int ref_cnt;                   /**< 重入计数，只有递归锁才有意义 */
    const char *file;              /**< 加锁位置的文件名 */
    const char *func;              /**< 加锁位置的函数名 */
    int line;                      /**< 加锁位置的行号 */
    bool initialized;              /**< 锁是否初始化 */
#endif
} mutex_ctx_t;

typedef struct _cond_ctx {
    pthread_cond_t cond;
    pthread_mutex_t mutex;
} cond_ctx_t;

/** 日志定义 */
#define LogError(fmt, arg...) fprintf(stderr, "[%s-%d] " fmt "\n", __func__, __LINE__, ##arg)
#define LogInfo(fmt, arg...) fprintf(stderr, "[%s-%d] " fmt "\n", __func__, __LINE__, ##arg)

/**
 * @brief 获取当前线程的系统ID
 * @return 线程系统ID
 */
static long get_current_tid(void)
{
    return syscall(SYS_gettid);
}

mutex_ctx_t *lock_init(void)
{
    int ret = -1;
    mutex_ctx_t *lck = calloc(1, sizeof(mutex_ctx_t));
    if (lck == NULL) {
        LogError("Failed to calloc mutex context");
        return NULL;
    }
    
#ifdef MUTEX_DEBUG
    pthread_mutexattr_t attr;
    
    /* 初始化互斥锁属性 */
    ret = pthread_mutexattr_init(&attr);
    if (ret != 0) {
        LogError("Failed to init mutex attribute: %d-%s", ret, strerror(ret));
        return NULL;
    }
    
    /* 设置互斥锁类型为错误检查类型: 可以检查死锁和释放不属于自己的锁 */
    ret = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
    if (ret != 0) {
        LogError("Failed to set mutex type: %d-%s", ret, strerror(ret));
        goto cleanup_attr;
    }

    /* 初始化互斥锁 */
    ret = pthread_mutex_init(&lck->mutex, &attr);
    if (ret != 0) {
        LogError("Failed to init mutex: %d-%s", ret, strerror(ret));
        goto cleanup_attr;
    }
    lck->initialized = true;
    lck->owner_pid = 0;
    lck->owner_tid = 0;
    lck->ref_cnt = 0;
    lck->file = NULL;
    lck->func = NULL;
    lck->line = 0;
    memset(lck->owner_name, 0, sizeof(lck->owner_name));
    LogInfo("Mutex initialized successfully");
    ret = 0;
    
cleanup_attr:
    pthread_mutexattr_destroy(&attr);
#else 
    ret = pthread_mutex_init(&lck->mutex, NULL);
    if (ret != 0) {
        LogError("Failed to init mutex: %d-%s", ret, strerror(ret));
        return NULL;
    }
#endif

    return lck;
}

/**
 * @brief 带调试信息的锁获取函数
 * @param lck 互斥锁上下文指针
 * @param file 文件名
 * @param func 函数名
 * @param line 行号
 * @return 成功返回0，失败返回错误码
 */
int lock_acquire(mutex_ctx_t *lck, const char *file, const char *func, int line)
{
    int ret = -1;
    
    if (lck == NULL) {
        LogError("Invalid parameter: lck is NULL");
        return -1;
    }

#ifdef MUTEX_DEBUG
    if (!lck->initialized) {
        LogError("Lock is not initialized");
        return -1;
    }
#endif
    
    ret = pthread_mutex_lock(&lck->mutex);
    if (ret != 0) {
        LogError("Failed to acquire lock: %d-%s", ret, strerror(ret));
        lock_stat(lck);
        return -1;
    }
#ifdef MUTEX_DEBUG
    
    lck->owner_pid = pthread_self();
    lck->owner_tid = get_current_tid();
    lck->ref_cnt++;
    lck->file = file;
    lck->func = func;
    lck->line = line;
    pthread_getname_np(lck->owner_pid, lck->owner_name, sizeof(lck->owner_name));
    LogInfo("Lock acquired at %s:%s:%d", file, func, line);
#endif

    return 0;
}

int lock_release(mutex_ctx_t *lck, const char *file, const char *func, int line)
{
    int ret = -1;
    
    if (lck == NULL) {
        LogError("Invalid parameter: lck is NULL");
        return -1;
    }

#ifdef MUTEX_DEBUG
    if (!lck->initialized) {
        LogError("Lock is not initialized");
        return -1;
    }
#endif
    
    ret = pthread_mutex_unlock(&lck->mutex);
    if (ret != 0) {
        LogError("Failed to release lock: %d-%s", ret, strerror(ret));
        lock_stat(lck);
        return -1;
    }
    
#ifdef MUTEX_DEBUG
    if (lck->ref_cnt <= 0) {
        LogError("Lock reference count invalid: %d", lck->ref_cnt);
        return -1;
    }
    
    lck->ref_cnt--;
    if (lck->ref_cnt == 0) {
        lck->owner_pid = 0;
        lck->owner_tid = 0;
        lck->file = NULL;
        lck->func = NULL;
        lck->line = 0;
    }
    LogInfo("Lock released successfully at %s:%s:%d", file, func, line);
#endif
    
    return 0;
}

int lock_destroy(mutex_ctx_t *lck)
{
    int ret = -1;
    
    if (lck == NULL) {
        LogError("Invalid parameter: lck is NULL");
        return -1;
    }

#ifdef MUTEX_DEBUG
    if (!lck->initialized) {
        LogError("Lock is not initialized");
        return -1;
    }
#endif
    
    ret = pthread_mutex_destroy(&lck->mutex);
    if (ret != 0) {
        LogError("Failed to destroy mutex: %d-%s", ret, strerror(ret));
        lock_stat(lck);
        return -1;
    }
#ifdef MUTEX_DEBUG
    if (lck->ref_cnt > 0) {
        LogError("Lock is still held, ref_cnt: %d", lck->ref_cnt);
        return -1;
    }
#endif
    free(lck);
    lck = NULL;

    return 0;
}

void lock_stat(mutex_ctx_t *lck)
{
    if (lck == NULL) {
        LogError("Invalid parameter: lck is NULL");
        return;
    }
    
#ifdef MUTEX_DEBUG
    LogInfo("Mutex status:");
    LogInfo("  Owner PID: %lu", lck->owner_pid);
    LogInfo("  Owner TID: %ld", lck->owner_tid);
    LogInfo("  Owner Name: %s", lck->owner_name);
    LogInfo("  Reference count: %d", lck->ref_cnt);
    if (lck->file != NULL) {
        LogInfo("  Last lock location: %s:%s:%d", 
                lck->file, lck->func, lck->line);
    } else {
        LogInfo("  Last lock location: Not locked");
    }
#endif
}

/**
 * @brief 线程启动包装函数
 */
static void* _start_routine(void *arg)
{
    thread_arg_t *thread_arg = (thread_arg_t *)arg;
    void *(*start_routine)(void *) = thread_arg->start_routine;
    void *_arg = thread_arg->arg;
    
    if (start_routine == NULL) {
        LogError("start_routine is NULL");
        free(thread_arg);
        return NULL;
    }
    
    // 设置线程名字为函数名: 前8个字符+tid
    char thread_name[16];
    snprintf(thread_name, sizeof(thread_name), "%.8s_%ld", thread_arg->name, get_current_tid());
    int ret = pthread_setname_np(pthread_self(), thread_name);
    if (ret != 0) {
        LogError("Failed to set thread name: %s", strerror(ret));
    } else {
        LogInfo("Thread name set to: %s", thread_name);
    }
    
    // 释放线程参数内存
    free(thread_arg);
    
    // 调用用户线程函数
    start_routine(_arg);
    
    return NULL;
}

/**
 * @brief 创建线程
 */
int thread_create(pthread_t *thread, const pthread_attr_t *attr,
                  void *(*start_routine)(void *), void *arg, const char *name)
{
    int ret = -1;
    thread_arg_t *thread_arg = NULL;
    
    if (thread == NULL) {
        LogError("Invalid parameter: thread is NULL");
        return -1;
    }
    
    if (start_routine == NULL) {
        LogError("Invalid parameter: start_routine is NULL");
        return -1;
    }
    
    thread_arg = malloc(sizeof(thread_arg_t));
    if (thread_arg == NULL) {
        LogError("Failed to allocate memory for thread_arg");
        return -1;
    }
    
    thread_arg->start_routine = start_routine;
    thread_arg->arg = arg;
    thread_arg->name = name;
    
    ret = pthread_create(thread, attr, _start_routine, thread_arg);
    if (ret != 0) {
        LogError("Failed to create thread: %d-%s", ret, strerror(ret));
        free(thread_arg);
        return -1;
    }
    
    return 0;
}

cond_ctx_t *cond_var_init(void)
{
    cond_ctx_t *cond = malloc(sizeof(cond_ctx_t));
    if (cond == NULL) {
        LogError("Failed to allocate memory for cond_ctx_t");
        return NULL;
    }
    
    int ret = pthread_cond_init(&cond->cond, NULL);
    if (ret != 0) {
        LogError("Failed to initialize cond: %d-%s", ret, strerror(ret));
        free(cond);
        return NULL;
    }
    
    ret = pthread_mutex_init(&cond->mutex, NULL);
    if (ret != 0) {
        LogError("Failed to initialize mutex: %d-%s", ret, strerror(ret));
        pthread_cond_destroy(&cond->cond);
        free(cond);
        return NULL;
    }
    
    return cond;
}

int cond_var_wait(cond_ctx_t *cond)
{
    int ret = -1;
    
    if (cond == NULL) {
        LogError("Invalid parameter: cond is NULL");
        return -1;
    }
    
    ret = pthread_mutex_lock(&cond->mutex);
    if (ret != 0) {
        LogError("Failed to lock mutex: %d-%s", ret, strerror(ret));
        return -1;
    }
    
    ret = pthread_cond_wait(&cond->cond, &cond->mutex);
    if (ret != 0) {
        LogError("Failed to wait cond: %d-%s", ret, strerror(ret));
        pthread_mutex_unlock(&cond->mutex);
        return -1;
    }
    
    ret = pthread_mutex_unlock(&cond->mutex);
    if (ret != 0) {
        LogError("Failed to unlock mutex: %d-%s", ret, strerror(ret));
        return -1;
    }
    
    return 0;
}

int cond_var_signal(cond_ctx_t *cond)
{
    int ret = -1;
    
    if (cond == NULL) {
        LogError("Invalid parameter: cond is NULL");
        return -1;
    }
    
    ret = pthread_mutex_lock(&cond->mutex);
    if (ret != 0) {
        LogError("Failed to lock mutex: %d-%s", ret, strerror(ret));
        return -1;
    }
    
    ret = pthread_cond_signal(&cond->cond);
    if (ret != 0) {
        LogError("Failed to signal cond: %d-%s", ret, strerror(ret));
        pthread_mutex_unlock(&cond->mutex);
        return -1;
    }
    
    ret = pthread_mutex_unlock(&cond->mutex);
    if (ret != 0) {
        LogError("Failed to unlock mutex: %d-%s", ret, strerror(ret));
        return -1;
    }
    
    return 0;
}

int cond_var_broadcast(cond_ctx_t *cond)
{
    int ret = -1;

    if (cond == NULL) {
        LogError("Invalid parameter: cond is NULL");
        return -1;
    }
    
    ret = pthread_mutex_lock(&cond->mutex);
    if (ret != 0) {
        LogError("Failed to lock mutex: %d-%s", ret, strerror(ret));
        return -1;
    }
    
    ret = pthread_cond_broadcast(&cond->cond);
    if (ret != 0) {
        LogError("Failed to broadcast cond: %d-%s", ret, strerror(ret));
        pthread_mutex_unlock(&cond->mutex);
        return -1;
    }
    
    ret = pthread_mutex_unlock(&cond->mutex);
    if (ret != 0) {
        LogError("Failed to unlock mutex: %d-%s", ret, strerror(ret));
        return -1;
    }
    
    return 0;
}

int cond_var_destroy(cond_ctx_t *cond)
{
    int ret = -1;
    
    if (cond == NULL) {
        LogError("Invalid parameter: cond is NULL");
        return -1;
    }
    
    ret = pthread_cond_destroy(&cond->cond);
    if (ret != 0) {
        LogError("Failed to destroy cond: %d-%s", ret, strerror(ret));
        return -1;
    }
    
    ret = pthread_mutex_destroy(&cond->mutex);
    if (ret != 0) {
        LogError("Failed to destroy mutex: %d-%s", ret, strerror(ret));
        return -1;
    }
    
    free(cond);
    cond = NULL;
    return 0;
}
