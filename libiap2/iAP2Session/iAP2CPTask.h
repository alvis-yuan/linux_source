/*
 * File: iAP2CPTask.h
 * Package: iAP2Session
 * Abstract: MFi CP协处理器后台任务接口
 *
 * 提供异步CP操作，支持两种模式：
 * 1. 轮询模式 - 在主循环中执行CP操作（会阻塞）
 * 2. 线程模式 - 在独立线程中执行CP操作（真正异步）
 */

#ifndef IAP2_CP_TASK_H
#define IAP2_CP_TASK_H

#include <stdint.h>
#include <iAP2Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * CP任务消息类型
 */
typedef enum {
    kCPMsgNone = 0,
    kCPMsgReadCert,         /* 读取证书 */
    kCPMsgSignChallenge,    /* 签名挑战 */
    kCPMsgReadSerial        /* 读取证书序列号 */
} iAP2CPMsgType_t;

/*
 * CP任务操作结果回调
 * 在CP任务完成操作后调用，通知主线程
 * 注意：此回调可能在不同线程中调用，需要线程安全
 */
typedef void (*iAP2CPResultCB_t)(iAP2CPMsgType_t type, 
                                  const uint8_t* data, 
                                  uint16_t dataLen, 
                                  BOOL success,
                                  void* context);

/*
 * CP任务配置
 */
typedef struct {
    iAP2CPResultCB_t    resultCallback;     /* 结果回调（可选，用于日志） */
    void*               context;            /* 用户上下文 */
    BOOL                useThread;          /* 是否使用独立线程 */
    int                 threadPriority;     /* 线程优先级（仅线程模式） */
} iAP2CPTaskConfig_t;

/*
 ****************************************************************
 * API函数声明
 ****************************************************************
 */

/*
 * iAP2CPTaskInit
 * 初始化CP后台任务
 *
 * Input:
 *   config: 配置参数
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2CPTaskInit(const iAP2CPTaskConfig_t* config);

/*
 * iAP2CPTaskDeinit
 * 反初始化CP后台任务
 */
void iAP2CPTaskDeinit(void);

/*
 * iAP2CPTaskRequestReadCert
 * 请求读取证书（异步）
 *
 * Return:
 *   请求成功返回TRUE
 */
BOOL iAP2CPTaskRequestReadCert(void);

/*
 * iAP2CPTaskRequestSignChallenge
 * 请求签名挑战（异步）
 *
 * Input:
 *   challengeData: 挑战数据
 *   challengeLen: 挑战数据长度
 *
 * Return:
 *   请求成功返回TRUE
 */
BOOL iAP2CPTaskRequestSignChallenge(const uint8_t* challengeData, uint32_t challengeLen);

/*
 * iAP2CPTaskRequestReadSerial
 * 请求读取证书序列号（异步）
 *
 * Return:
 *   请求成功返回TRUE
 */
BOOL iAP2CPTaskRequestReadSerial(void);

/*
 * iAP2CPTaskProcess
 * CP任务处理函数
 * 
 * 轮询模式：在主循环中调用，检查并处理完成的操作
 * 线程模式：不需要调用，结果通过回调通知
 *
 * Return:
 *   如果有待处理任务返回TRUE
 */
BOOL iAP2CPTaskProcess(void);

#ifdef __cplusplus
}
#endif

#endif /* IAP2_CP_TASK_H */
