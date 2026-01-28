/*
 * File: iAP2CPTask.c
 * Package: iAP2Session
 * Abstract: MFi CP协处理器后台任务实现
 *
 * 支持两种模式：
 * 1. 轮询模式（无RTOS）- 在主循环中执行CP操作
 * 2. 线程模式（RTOS）- 在独立线程中执行CP操作
 * 
 * CP操作完成后通过回调直接通知，不需要轮询
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include "iAP2CPTask.h"
#include "../driver/mfiI2c.h"
#include <iAP2Log.h>

/*
 ****************************************************************
 * 数据结构
 ****************************************************************
 */

/* CP任务请求 */
typedef struct {
    iAP2CPMsgType_t type;
    uint8_t         challengeData[64];
    uint32_t        challengeLen;
} CPRequest_t;

/* CP任务状态 */
typedef enum {
    kCPStateIdle = 0,
    kCPStatePending,
    kCPStateComplete,
    kCPStateFailed
} CPState_t;

/* CP任务上下文 */
typedef struct {
    /* 配置 */
    iAP2CPTaskConfig_t  config;
    BOOL                initialized;
    
    /* 请求和状态 */
    volatile CPRequest_t    currentRequest;
    volatile CPState_t      state;
    
    /* 结果缓冲区 */
    uint8_t             certBuffer[1024];
    uint16_t            certLen;
    uint8_t             responseBuffer[128];
    uint16_t            responseLen;
    uint8_t             serialBuffer[32];
    uint16_t            serialLen;
    
    /* 线程模式 */
    pthread_t           thread;
    pthread_mutex_t     mutex;
    pthread_cond_t      cond;
    BOOL                threadRunning;
    BOOL                threadExit;
} CPTaskContext_t;

static CPTaskContext_t g_cpContext;

/*
 ****************************************************************
 * 内部函数 - CP操作
 ****************************************************************
 */

/*
 * 执行读取证书操作
 */
static BOOL _ExecuteReadCert(uint8_t* certBuffer, uint16_t* certLen)
{
    int ret = mfiAuthenticationCertificate(certBuffer, certLen);
    if (ret >= 0 && *certLen > 0) {
        iAP2LogDbg("[CP Task] Certificate read OK: %u bytes\n", *certLen);
        return TRUE;
    } else {
        iAP2LogError("[CP Task] Certificate read FAILED\n");
        return FALSE;
    }
}

/*
 * 执行读取证书序列号操作
 */
static BOOL _ExecuteReadSerial(uint8_t* serialBuffer, uint16_t* serialLen)
{
    int ret = mfiGetDeviceCertificateSerialNumber(serialBuffer);
    if (ret >= 0) {
        *serialLen = 32;  /* 序列号固定32字节 */
        iAP2LogDbg("[CP Task] Serial number read OK: %u bytes\n", *serialLen);
        return TRUE;
    } else {
        iAP2LogError("[CP Task] Serial number read FAILED\n");
        return FALSE;
    }
}

/*
 * 执行签名挑战操作
 */
static BOOL _ExecuteSignChallenge(uint8_t* challengeData, 
                                   uint16_t challengeLen,
                                   uint8_t* responseBuffer, 
                                   uint16_t* responseLen)
{
    /* 打印挑战数据 */
    fprintf(stderr, "Challenge Data: %u bytes\n", challengeLen);
    for (size_t i = 0; i < challengeLen; i++) {
        fprintf(stderr, "%02X", challengeData[i]);
    }
    fprintf(stderr, "\n");

    int ret = mfiAuthenticationResponse((uint8_t*)challengeData, challengeLen,
                                         responseBuffer, 
                                         responseLen);
    iAP2LogDbg("ret=%d, *responseLen=%u", ret, *responseLen);
    if (ret >= 0 && *responseLen > 0) {
        iAP2LogDbg("[CP Task] Challenge sign OK: %u bytes\n", *responseLen);
        return TRUE;
    } else {
        iAP2LogError("[CP Task] Challenge sign FAILED\n");
        return FALSE;
    }
}

/*
 * 处理CP请求
 */
static void _ProcessRequest(void)
{
    BOOL success = FALSE;
    
    switch (g_cpContext.currentRequest.type) {
        case kCPMsgReadCert:
            iAP2LogDbg("[CP Task] Processing: Read Certificate\n");
            success = _ExecuteReadCert(g_cpContext.certBuffer, 
                                        &g_cpContext.certLen);
            break;
            
        case kCPMsgReadSerial:
            iAP2LogDbg("[CP Task] Processing: Read Serial Number\n");
            success = _ExecuteReadSerial(g_cpContext.serialBuffer,
                                          &g_cpContext.serialLen);
            break;
            
        case kCPMsgSignChallenge:
            iAP2LogDbg("[CP Task] Processing: Sign Challenge\n");
            success = _ExecuteSignChallenge((uint8_t*)g_cpContext.currentRequest.challengeData,
                                             g_cpContext.currentRequest.challengeLen,
                                             g_cpContext.responseBuffer,
                                             &g_cpContext.responseLen);
            break;
            
        default:
            iAP2LogError("[CP Task] Unknown request type: %d\n", 
                        g_cpContext.currentRequest.type);
            break;
    }
    
    g_cpContext.state = success ? kCPStateComplete : kCPStateFailed;
}

/*
 * 通知结果
 * 直接通过回调通知，不再调用内部接口
 */
static void _NotifyResult(void)
{
    BOOL success = (g_cpContext.state == kCPStateComplete);
    const uint8_t* data = NULL;
    uint16_t dataLen = 0;
    
    switch (g_cpContext.currentRequest.type) {
        case kCPMsgReadCert:
            data = g_cpContext.certBuffer;
            dataLen = g_cpContext.certLen;
            break;
            
        case kCPMsgReadSerial:
            data = g_cpContext.serialBuffer;
            dataLen = g_cpContext.serialLen;
            break;
            
        case kCPMsgSignChallenge:
            data = g_cpContext.responseBuffer;
            dataLen = g_cpContext.responseLen;
            break;
            
        default:
            break;
    }
    
    /* 通过回调通知结果 */
    if (g_cpContext.config.resultCallback) {
        g_cpContext.config.resultCallback(g_cpContext.currentRequest.type,
                                           data, dataLen, success,
                                           g_cpContext.config.context);
    }
    
    /* 重置状态 */
    g_cpContext.currentRequest.type = kCPMsgNone;
    g_cpContext.state = kCPStateIdle;
}

/*
 * CP任务线程函数
 */
static void* _CPTaskThread(void* arg)
{
    (void)arg;
    
    iAP2LogDbg("[CP Task] Thread started\n");
    
    while (!g_cpContext.threadExit) {
        pthread_mutex_lock(&g_cpContext.mutex);
        
        /* 等待请求 */
        while (g_cpContext.state == kCPStateIdle && !g_cpContext.threadExit) {
            pthread_cond_wait(&g_cpContext.cond, &g_cpContext.mutex);
        }
        
        if (g_cpContext.threadExit) {
            pthread_mutex_unlock(&g_cpContext.mutex);
            break;
        }
        
        /* 处理请求 */
        if (g_cpContext.state == kCPStatePending) {
            pthread_mutex_unlock(&g_cpContext.mutex);
            
            /* 执行CP操作（不持有锁，避免阻塞） */
            _ProcessRequest();
            
            pthread_mutex_lock(&g_cpContext.mutex);
            
            /* 通知结果 */
            _NotifyResult();
        }
        
        pthread_mutex_unlock(&g_cpContext.mutex);
    }
    
    iAP2LogDbg("[CP Task] Thread exited\n");
    return NULL;
}

/*
 * 初始化线程模式
 */
static int _InitThreadMode(void)
{
    pthread_mutex_init(&g_cpContext.mutex, NULL);
    pthread_cond_init(&g_cpContext.cond, NULL);
    
    g_cpContext.threadExit = FALSE;
    g_cpContext.threadRunning = FALSE;
    
    /* 创建线程 */
    int ret = pthread_create(&g_cpContext.thread, NULL, _CPTaskThread, NULL);
    if (ret != 0) {
        iAP2LogError("[CP Task] Failed to create thread: %d\n", ret);
        pthread_mutex_destroy(&g_cpContext.mutex);
        pthread_cond_destroy(&g_cpContext.cond);
        return -1;
    }
    
    g_cpContext.threadRunning = TRUE;
    iAP2LogDbg("[CP Task] Thread mode initialized\n");
    
    return 0;
}

/*
 * 反初始化线程模式
 */
static void _DeinitThreadMode(void)
{
    if (!g_cpContext.threadRunning) {
        return;
    }
    
    /* 通知线程退出 */
    pthread_mutex_lock(&g_cpContext.mutex);
    g_cpContext.threadExit = TRUE;
    pthread_cond_signal(&g_cpContext.cond);
    pthread_mutex_unlock(&g_cpContext.mutex);
    
    /* 等待线程结束 */
    pthread_join(g_cpContext.thread, NULL);
    
    /* 清理资源 */
    pthread_mutex_destroy(&g_cpContext.mutex);
    pthread_cond_destroy(&g_cpContext.cond);
    
    g_cpContext.threadRunning = FALSE;
    iAP2LogDbg("[CP Task] Thread mode deinitialized\n");
}

/*
 * 线程模式下提交请求
 */
static BOOL _SubmitRequestThreadMode(iAP2CPMsgType_t type, 
                                      const uint8_t* challengeData,
                                      uint32_t challengeLen)
{
    pthread_mutex_lock(&g_cpContext.mutex);
    
    if (g_cpContext.state != kCPStateIdle) {
        pthread_mutex_unlock(&g_cpContext.mutex);
        iAP2LogError("[CP Task] Busy, cannot submit request\n");
        return FALSE;
    }
    
    /* 设置请求 */
    g_cpContext.currentRequest.type = type;
    if (type == kCPMsgSignChallenge && challengeData && challengeLen > 0) {
        if (challengeLen > sizeof(g_cpContext.currentRequest.challengeData)) {
            challengeLen = sizeof(g_cpContext.currentRequest.challengeData);
        }
        memcpy((void*)g_cpContext.currentRequest.challengeData, challengeData, challengeLen);
        g_cpContext.currentRequest.challengeLen = challengeLen;
        iAP2LogDbg("[CP Task] Sign Challenge: %u bytes\n", challengeLen);
    }
    
    g_cpContext.state = kCPStatePending;
    
    /* 唤醒线程 */
    pthread_cond_signal(&g_cpContext.cond);
    pthread_mutex_unlock(&g_cpContext.mutex);
    
    return TRUE;
}

/*
 ****************************************************************
 * API实现
 ****************************************************************
 */

/*
 * 初始化CP任务
 */
int iAP2CPTaskInit(const iAP2CPTaskConfig_t* config)
{
    if (!config) {
        iAP2LogError("[CP Task] Invalid config\n");
        return -1;
    }
    
    if (g_cpContext.initialized) {
        iAP2LogError("[CP Task] Already initialized\n");
        return -1;
    }
    
    memset(&g_cpContext, 0, sizeof(g_cpContext));
    memcpy(&g_cpContext.config, config, sizeof(iAP2CPTaskConfig_t));
    
    g_cpContext.state = kCPStateIdle;
    g_cpContext.currentRequest.type = kCPMsgNone;
    
    /* 打开MFi芯片 */
    if (mfiOpen() < 0) {
        iAP2LogError("[CP Task] Failed to open MFi chip\n");
        return -1;
    }
    
    /* 根据配置选择模式 */
    if (config->useThread) {
        if (_InitThreadMode() < 0) {
            mfiClose();
            return -1;
        }
    } else {
        iAP2LogDbg("[CP Task] Polling mode initialized\n");
    }
    
    g_cpContext.initialized = TRUE;
    
    iAP2LogDbg("[CP Task] Initialized (useThread=%d)\n", config->useThread);
    
    return 0;
}

/*
 * 反初始化CP任务
 */
void iAP2CPTaskDeinit(void)
{
    if (!g_cpContext.initialized) {
        return;
    }
    
    if (g_cpContext.config.useThread) {
        _DeinitThreadMode();
    }
    
    mfiClose();
    
    memset(&g_cpContext, 0, sizeof(g_cpContext));
    
    iAP2LogDbg("[CP Task] Deinitialized\n");
}

/*
 * 请求读取证书
 */
BOOL iAP2CPTaskRequestReadCert(void)
{
    if (!g_cpContext.initialized) {
        return FALSE;
    }
    
    if (g_cpContext.config.useThread) {
        return _SubmitRequestThreadMode(kCPMsgReadCert, NULL, 0);
    }
    
    /* 轮询模式 */
    if (g_cpContext.state != kCPStateIdle) {
        iAP2LogError("[CP Task] Busy, cannot request read cert\n");
        return FALSE;
    }
    
    g_cpContext.currentRequest.type = kCPMsgReadCert;
    g_cpContext.state = kCPStatePending;
    g_cpContext.certLen = 0;
    
    iAP2LogDbg("[CP Task] Read cert requested (polling mode)\n");
    return TRUE;
}

/*
 * 请求签名挑战
 */
BOOL iAP2CPTaskRequestSignChallenge(const uint8_t* challengeData, uint32_t challengeLen)
{
    if (!g_cpContext.initialized || !challengeData || challengeLen == 0) {
        return FALSE;
    }
    
    if (g_cpContext.config.useThread) {
        return _SubmitRequestThreadMode(kCPMsgSignChallenge, challengeData, challengeLen);
    }
    
    /* 轮询模式 */
    if (g_cpContext.state != kCPStateIdle) {
        iAP2LogError("[CP Task] Busy, cannot request sign\n");
        return FALSE;
    }
    
    /* 保存挑战数据 */
    if (challengeLen > sizeof(g_cpContext.currentRequest.challengeData)) {
        challengeLen = sizeof(g_cpContext.currentRequest.challengeData);
    }
    memcpy((void*)g_cpContext.currentRequest.challengeData, challengeData, challengeLen);
    g_cpContext.currentRequest.challengeLen = challengeLen;
    
    g_cpContext.currentRequest.type = kCPMsgSignChallenge;
    g_cpContext.state = kCPStatePending;
    g_cpContext.responseLen = 0;
    
    iAP2LogDbg("[CP Task] Sign challenge requested (polling mode)\n");
    return TRUE;
}

/*
 * 请求读取证书序列号
 */
BOOL iAP2CPTaskRequestReadSerial(void)
{
    if (!g_cpContext.initialized) {
        return FALSE;
    }
    
    if (g_cpContext.config.useThread) {
        return _SubmitRequestThreadMode(kCPMsgReadSerial, NULL, 0);
    }
    
    /* 轮询模式 */
    if (g_cpContext.state != kCPStateIdle) {
        iAP2LogError("[CP Task] Busy, cannot request read serial\n");
        return FALSE;
    }
    
    g_cpContext.currentRequest.type = kCPMsgReadSerial;
    g_cpContext.state = kCPStatePending;
    g_cpContext.serialLen = 0;
    
    iAP2LogDbg("[CP Task] Read serial requested (polling mode)\n");
    return TRUE;
}

/*
 * 处理CP任务（轮询模式）
 */
BOOL iAP2CPTaskProcess(void)
{
    if (!g_cpContext.initialized) {
        return FALSE;
    }
    
    /* 线程模式不需要调用Process */
    if (g_cpContext.config.useThread) {
        return FALSE;
    }
    
    /* 轮询模式处理 */
    switch (g_cpContext.state) {
        case kCPStatePending:
            /* 执行CP操作 */
            _ProcessRequest();
            return TRUE;
            
        case kCPStateComplete:
        case kCPStateFailed:
            /* 通知结果 */
            _NotifyResult();
            return TRUE;
            
        default:
            return FALSE;
    }
}
