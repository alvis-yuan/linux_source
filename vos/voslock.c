#include "vos.h"
#include <sys/time.h>

/**
 * @brief 初始化对象锁
 * @param lock 指向OBJLOCK结构的指针
 * @return 成功返回0，失败返回-1
 */
int objlock_init(OBJLOCK *lock)
{
    int ret = 0;

    if (lock == NULL) {
        LOG_ERROR("Lock pointer is NULL");
        ret = -1;
        goto out;
    }

    ret = pthread_mutex_init(&lock->lock, NULL);
    if (ret != 0) {
        LOG_ERROR("Failed to initialize mutex");
        ret = -1;
        goto out;
    }

    lock->tid = 0;
    lock->cnt = 0;
    LOG_INFO("Object lock initialized successfully");

out:
    return ret;
}

/**
 * @brief 加锁或解锁对象锁
 * @param lock 指向OBJLOCK结构的指针
 * @param tid 线程ID，如果为0则使用当前线程ID
 * @param lk 操作类型：1表示加锁，0表示解锁
 * @return 成功返回0，失败返回错误码
 */
int objlock_lock(OBJLOCK *lock, unsigned long tid, char lk)
{
    int ret = 0;
    extern __thread unsigned long threadID;

    if (lock == NULL) {
        LOG_ERROR("Lock pointer is NULL");
        ret = -1;
        goto out;
    }

    /* 如果tid为0，使用当前线程ID */
    if (tid == 0) {
        if (threadID) {
            tid = threadID;
        } else {
            tid = (unsigned long)pthread_self();
        }
    }

    if (lk) {
        /* 加锁操作 */
        if (lock->tid == 0 || lock->tid != tid) {
            ret = pthread_mutex_lock(&lock->lock);
            if (ret != 0) {
                LOG_ERROR("Failed to lock mutex");
                ret = -1;
                goto out;
            }

            if (lock->tid != 0 && lock->tid != tid) {
                LOG_ERROR("Object lock state error, count: %lu", lock->cnt);
            }

            lock->tid = tid;
            lock->cnt = 1;
            LOG_INFO("Lock acquired by thread %lu", tid);
        } else {
            lock->cnt++;
            LOG_INFO("Lock count increased to %lu by thread %lu", lock->cnt, tid);
        }
    } else {
        /* 解锁操作 */
        if (lock->tid == tid) {
            lock->cnt--;
            if (lock->cnt == 0) {
                lock->tid = 0;
                ret = pthread_mutex_unlock(&lock->lock);
                if (ret != 0) {
                    LOG_ERROR("Failed to unlock mutex");
                    ret = -1;
                    goto out;
                }
                LOG_INFO("Lock released by thread %lu", tid);
            } else {
                LOG_INFO("Lock count decreased to %lu by thread %lu", lock->cnt, tid);
            }
        } else {
            LOG_ERROR("Unlock failed: thread ID mismatch");
            ret = -1;
            goto out;
        }
    }

out:
    return ret;
}

/**
 * @brief 带超时的加锁或解锁对象锁
 * @param lock 指向OBJLOCK结构的指针
 * @param tid 线程ID，如果为0则使用当前线程ID
 * @param msecs 超时时间（毫秒）
 * @param lk 操作类型：1表示加锁，0表示解锁
 * @return 成功返回0，失败返回错误码
 */
int objlock_locktm(OBJLOCK *lock, unsigned long tid, unsigned long msecs, char lk)
{
    int ret = 0;
    extern __thread unsigned long threadID;
    struct timespec mtime;
    struct timeval now;

    if (lock == NULL) {
        LOG_ERROR("Lock pointer is NULL");
        ret = -1;
        goto out;
    }

    /* 如果tid为0，使用当前线程ID */
    if (tid == 0) {
        if (threadID) {
            tid = threadID;
        } else {
            tid = (unsigned long)pthread_self();
        }
    }

    if (lk) {
        /* 加锁操作 */
        if (lock->tid == 0 || lock->tid != tid) {
            if (msecs) {
                /* 计算超时时间 */
                gettimeofday(&now, NULL);
                mtime.tv_sec = now.tv_sec + msecs / 1000;
                mtime.tv_nsec = (now.tv_usec + (msecs % 1000) * 1000) * 1000;
                ret = pthread_mutex_timedlock(&lock->lock, &mtime);
            } else {
                ret = pthread_mutex_lock(&lock->lock);
            }

            if (ret != 0) {
                LOG_ERROR("Failed to acquire lock with timeout");
                ret = -1;
                goto out;
            }

            if (lock->tid != 0 && lock->tid != tid) {
                LOG_ERROR("Object lock state error, count: %lu", lock->cnt);
            }

            lock->tid = tid;
            lock->cnt = 1;
            LOG_INFO("Lock acquired by thread %lu with timeout %lu ms", tid, msecs);
        } else {
            lock->cnt++;
            LOG_INFO("Lock count increased to %lu by thread %lu", lock->cnt, tid);
        }
    } else {
        /* 解锁操作 */
        if (lock->tid == tid) {
            lock->cnt--;
            if (lock->cnt == 0) {
                lock->tid = 0;
                ret = pthread_mutex_unlock(&lock->lock);
                if (ret != 0) {
                    LOG_ERROR("Failed to unlock mutex");
                    ret = -1;
                    goto out;
                }
                LOG_INFO("Lock released by thread %lu", tid);
            } else {
                LOG_INFO("Lock count decreased to %lu by thread %lu", lock->cnt, tid);
            }
        } else {
            LOG_ERROR("Unlock failed: thread ID mismatch");
            ret = -1;
            goto out;
        }
    }

out:
    return ret;
}