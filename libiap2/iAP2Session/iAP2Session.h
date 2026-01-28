/*
 * File: iAP2Session.h
 * Package: iAP2Session
 * Abstract: iAP2会话层接口定义
 *
 * 会话层职责：
 * - 管理control session和EA session
 * - 处理认证和识别流程
 * - 路由会话数据到应用层
 *
 * 架构说明：
 * - Session层完全事件驱动，不需要Process循环
 * - 所有处理都在Transport的RunLoop线程回调中完成
 * - CP操作通过回调机制异步通知，不需要轮询
 */

#ifndef __IAP2_SESSION_H__
#define __IAP2_SESSION_H__

#include <stdint.h>
#include <iAP2Defines.h>
#include "iAP2Packet.h"
#include "iAP2Transport.h"
#include "iAP2SessionTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 会话类型定义
 */
typedef enum {
    kIAP2SessionTypeControl = 0,  /* 控制会话（含认证） */
    kIAP2SessionTypeBuffer  = 1,  /* 缓冲会话（如图片、健身数据） */
    kIAP2SessionTypeEA      = 2,  /* EA外部附件会话 */
    kIAP2SessionTypeCount
} iAP2SessionType_t;

/*
 * 控制会话ID和版本
 */
#define kIAP2CtrlSessionId      0x0a
#define kIAP2CtrlSessionVersion 0x01
#define kIAP2InvalidSessionId      0xFF


/*
 * 会话管理器句柄（不透明类型）
 */
typedef struct iAP2Session_st iAP2Session_t;

/*
 * 会话数据回调类型
 */
typedef BOOL (*iAP2SessionDataCB_t)(const uint8_t *data,
                                    uint32_t dataLen,
                                    void *context);
/*
 * 认证完成回调
 */
typedef void (*iAP2SessionAuthCompleteCB_t)(BOOL success, void *context);

/*
 * 识别完成回调
 */
typedef void (*iAP2SessionIdentifyCompleteCB_t)(BOOL success, void *context);

/*
 * 会话配置
 */
typedef struct {
    /* 传输层 */
    iAP2Transport_t                    *transport;          /* 传输层实例 */

    /* 回调函数 */
    iAP2SessionDataCB_t                 sessionDataCB;      /* 会话数据回调 */
    iAP2SessionAuthCompleteCB_t         authCompleteCB;     /* 认证完成回调 */
    iAP2SessionIdentifyCompleteCB_t    identifyCompleteCB; /* 识别完成回调 */
    void                               *context;            /* 用户上下文 */
} iAP2SessionConfig_t;

/*
 ****************************************************************
 * API函数声明
 ****************************************************************
 */

/*
 * iAP2SessionCreate
 * 创建会话管理器实例
 *
 * Input:
 *   config: 配置参数
 *   type: 传输类型（USB或蓝牙）
 *
 * Return:
 *   成功返回句柄，失败返回NULL
 */
iAP2Session_t *iAP2SessionCreate(const iAP2SessionConfig_t *config,
                                 uint8_t type);

/*
 * iAP2SessionDestroy
 * 销毁会话管理器实例
 *
 * Input:
 *   session: 会话管理器句柄
 */
void iAP2SessionDestroy(iAP2Session_t *session);

/*
 * iAP2SessionStart
 * 启动会话管理器
 * 在传输层连接建立后调用
 *
 * Input:
 *   session: 会话管理器句柄
 *
 * Return:
 *   成功返回TRUE
 */
BOOL iAP2SessionStart(iAP2Session_t *session);

/*
 * iAP2SessionStop
 * 停止会话管理器
 *
 * Input:
 *   session: 会话管理器句柄
 */
void iAP2SessionStop(iAP2Session_t *session);


/*
 * iAP2SessionSendData
 * 发送会话数据
 *
 * Input:
 *   session: 会话管理器句柄
 *   sessionID: 会话ID
 *   data: 数据
 *   dataLen: 数据长度
 *
 * Return:
 *   成功返回TRUE
 */
BOOL iAP2SessionSendData(iAP2Session_t *session,
                         uint8_t sessionID,
                         const uint8_t *data,
                         uint32_t dataLen);

/*
 * iAP2SessionIsAuthenticated
 * 检查是否已完成认证
 *
 * Input:
 *   session: 会话管理器句柄
 *
 * Return:
 *   已认证返回TRUE
 */
BOOL iAP2SessionIsAuthenticated(iAP2Session_t *session);

/*
 * iAP2SessionIsIdentified
 * 检查是否已完成识别
 *
 * Input:
 *   session: 会话管理器句柄
 *
 * Return:
 *   已识别返回TRUE
 */
BOOL iAP2SessionIsIdentified(iAP2Session_t *session);

/*
 * iAP2SessionIsReady
 * 检查会话管理器是否就绪（认证和识别都完成）
 *
 * Input:
 *   session: 会话管理器句柄
 *
 * Return:
 *   就绪返回TRUE
 */
BOOL iAP2SessionIsReady(iAP2Session_t *session);

/*
 * iAP2SessionGetStateString
 * 获取当前会话状态字符串（用于调试）
 *
 * Input:
 *   session: 会话管理器句柄
 *
 * Return:
 *   状态字符串
 */
const char *iAP2SessionGetStateString(iAP2Session_t *session);

/*
 ****************************************************************
 * 会话注册辅助函数
 ****************************************************************
 */

/*
 * iAP2SessionReg
 * 注册会话到SYN参数
 *
 * Input:
 *   p_syn_data: SYN参数结构
 *   id: 会话ID
 *   type: 会话类型
 *   version: 会话版本
 *
 * Return:
 *   成功返回0
 */
int iAP2SessionReg(iAP2PacketSYNData_t *p_syn_data, uint8_t id, uint8_t type,
                   uint8_t version);

/*
 * iAP2SessionRegCtrl
 * 注册控制会话
 *
 * Input:
 *   p_syn_data: SYN参数结构
 *
 * Return:
 *   成功返回0
 */
static inline int iAP2SessionRegCtrl(iAP2PacketSYNData_t *p_syn_data)
{
    return iAP2SessionReg(p_syn_data, kIAP2CtrlSessionId, kIAP2SessionTypeControl,
                          kIAP2CtrlSessionVersion);
}

/*
 * iAP2SessionRegEA
 * 注册EA数据会话
 *
 * Input:
 *   eaSessionID: EA会话ID
 *   context: 会话上下文
 *
 * Return:
 *   成功返回0
 */
int iAP2SessionRegEA(uint8_t eaSessionID, void *context);



#ifdef __cplusplus
}
#endif

#endif /* __IAP2_SESSION_H__ */

