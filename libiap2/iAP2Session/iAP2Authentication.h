/*
 * File: iAP2Authentication.h
 * Package: iAP2Session
 * Abstract: iAP2 Authentication Module Interface (事件驱动版本)
 *
 * 实现iAP2协议的认证流程，与MFi协处理器交互完成证书和挑战响应
 * 使用事件驱动模式，CP操作完成后通过回调直接发送消息
 * 不需要轮询，完全异步
 */

#ifndef IAP2_AUTHENTICATION_H
#define IAP2_AUTHENTICATION_H

#include <stdint.h>
#include <iAP2Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 认证状态
 */
typedef enum {
    kIAP2AuthStateIdle = 0,           /* 空闲状态 */
    kIAP2AuthStateWaitCertRequest,    /* 等待证书请求 */
    kIAP2AuthStateReadingCert,        /* 正在读取证书（异步） */
    kIAP2AuthStateWaitChallenge,      /* 等待挑战 */
    kIAP2AuthStateSigningChallenge,   /* 正在签名挑战（异步） */
    kIAP2AuthStateWaitResult,         /* 等待认证结果 */
    kIAP2AuthStateAuthenticated,      /* 认证成功 */
    kIAP2AuthStateFailed              /* 认证失败 */
} iAP2AuthState_t;

/*
 * 认证结果回调
 */
typedef void (*iAP2AuthResultCB_t)(BOOL success, void *context);

/*
 * 发送控制消息回调
 */
typedef BOOL (*iAP2AuthSendMsgCB_t)(const uint8_t *data, uint32_t len,
                                    void *context);

/*
 * 认证模块配置
 */
typedef struct {
    iAP2AuthResultCB_t          resultCallback;     /* 认证结果回调 */
    iAP2AuthSendMsgCB_t         sendMsgCallback;    /* 发送消息回调 */
    void                       *context;            /* 用户上下文 */
    BOOL                        useAsyncCP;         /* 是否使用异步CP操作 */
    uint8_t                     controlSessionVersion; /* 控制会话版本 */
} iAP2AuthConfig_t;

/*
 ****************************************************************
 * API函数声明
 ****************************************************************
 */

/*
 * iAP2AuthInit
 * 初始化认证模块
 */
int iAP2AuthInit(const iAP2AuthConfig_t *config);

/*
 * iAP2AuthDeinit
 * 反初始化认证模块
 */
void iAP2AuthDeinit(void);

/*
 * iAP2AuthReset
 * 重置认证模块状态
 */
void iAP2AuthReset(void);

/*
 * iAP2AuthGetState
 * 获取当前认证状态
 */
iAP2AuthState_t iAP2AuthGetState(void);

/*
 * iAP2AuthOnLinkConnected
 * 链路连接通知
 */
void iAP2AuthOnLinkConnected(void);

/*
 * iAP2AuthSetControlSessionVersion
 * 设置控制会话版本（在Link连接后调用）
 */
void iAP2AuthSetControlSessionVersion(uint8_t version);

/*
 * iAP2AuthHandleMessage
 * 处理认证相关的控制消息
 */
BOOL iAP2AuthHandleMessage(uint16_t msgId, const uint8_t *data, uint32_t len);

/*
 * iAP2AuthIsComplete
 * 检查认证是否完成
 */
BOOL iAP2AuthIsComplete(void);

/*
 * iAP2AuthIsSuccess
 * 检查认证是否成功
 */
BOOL iAP2AuthIsSuccess(void);

#ifdef __cplusplus
}
#endif

#endif /* IAP2_AUTHENTICATION_H */
