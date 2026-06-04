/*
 * File: iAP2Identification.h
 * Package: iAP2Session
 * Abstract: iAP2 Identification Module Interface
 *
 * 实现iAP2协议的识别流程，向Apple设备声明配件的能力和特性
 */

#ifndef IAP2_IDENTIFICATION_H
#define IAP2_IDENTIFICATION_H

#include <stdint.h>
#include <iAP2Defines.h>
#include "iAP2SessionTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 识别状态
 */
typedef enum {
    kIAP2IdStateIdle = 0,         /* 空闲状态 */
    kIAP2IdStateWaitStart,        /* 等待StartIdentification */
    kIAP2IdStateWaitResult,       /* 等待识别结果 */
    kIAP2IdStateIdentified,       /* 识别成功 */
    kIAP2IdStateRejected          /* 识别被拒绝 */
} iAP2IdState_t;

/*
 * 识别结果回调
 * accepted: TRUE表示识别被接受，FALSE表示被拒绝
 * rejectReason: 拒绝原因（仅当accepted为FALSE时有效）
 * context: 用户上下文
 */
typedef void (*iAP2IdResultCB_t)(BOOL accepted, uint8_t rejectReason,
                                 void *context);

/*
 * 发送控制消息回调
 */
typedef BOOL (*iAP2IdSendMsgCB_t)(const uint8_t *data, uint32_t len,
                                  void *context);

/*
 * 识别模块配置
 *
 * 注意：配件信息、EA协议、消息能力等已内置在模块中
 * 只需提供回调函数即可
 */
typedef struct {
    iAP2IdResultCB_t        resultCallback;     /* 识别结果回调 */
    iAP2IdSendMsgCB_t       sendMsgCallback;    /* 发送消息回调 */
    void                   *context;            /* 用户上下文 */

    /* 以下字段由模块内部管理，外部无需设置 */
    iAP2AccessoryInfo_t    *accessoryInfo;      /* 内部使用 */
    iAP2EAProtocol_t       *eaProtocols;        /* 内部使用 */
    uint8_t                 eaProtocolCount;    /* 内部使用 */
    iAP2BTTransportInfo_t  *btTransport;        /* 内部使用 */
    iAP2USBHostTransportInfo_t *usbTransport; /* 内部使用 */
    uint16_t               *messagesSentByAccessory;    /* 内部使用 */
    uint16_t                messagesSentCount;          /* 内部使用 */
    uint16_t               *messagesReceivedByAccessory;/* 内部使用 */
    uint16_t                messagesReceivedCount;      /* 内部使用 */
} iAP2IdConfig_t;

/*
 ****************************************************************
 * API函数声明
 ****************************************************************
 */

/*
 * iAP2IdInit
 * 初始化识别模块
 *
 * Input:
 *   config: 配置参数
 *   type: 传输类型（USB或蓝牙）
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2IdInit(const iAP2IdConfig_t *config, uint8_t type);

/*
 * iAP2IdDeinit
 * 反初始化识别模块
 */
void iAP2IdDeinit(void);

/*
 * iAP2IdReset
 * 重置识别模块状态
 * 在链路断开时调用
 */
void iAP2IdReset(void);

/*
 * iAP2IdGetState
 * 获取当前识别状态
 *
 * Return:
 *   当前状态
 */
iAP2IdState_t iAP2IdGetState(void);

/*
 * iAP2IdOnAuthComplete
 * 认证完成通知
 * 当认证成功后调用，开始等待识别请求
 *
 * Input:
 *   success: 认证是否成功
 */
void iAP2IdOnAuthComplete(BOOL success);

/*
 * iAP2IdHandleMessage
 * 处理识别相关的控制消息
 *
 * Input:
 *   msgId: 消息ID
 *   data: 消息数据（包含完整消息头）
 *   len: 数据长度
 *
 * Return:
 *   消息被处理返回TRUE，否则返回FALSE
 */
BOOL iAP2IdHandleMessage(uint16_t msgId, const uint8_t *data, uint32_t len);

/*
 * iAP2IdSendUpdate
 * 发送识别信息更新
 * 在识别成功后，如果配件能力发生变化，可以发送更新
 *
 * Input:
 *   updatedInfo: 更新的配件信息（仅包含变化的字段）
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2IdSendUpdate(const iAP2AccessoryInfo_t *updatedInfo);

/*
 * iAP2IdIsComplete
 * 检查识别是否完成（成功或失败）
 *
 * Return:
 *   识别完成返回TRUE
 */
BOOL iAP2IdIsComplete(void);

/*
 * iAP2IdIsSuccess
 * 检查识别是否成功
 *
 * Return:
 *   识别成功返回TRUE
 */
BOOL iAP2IdIsSuccess(void);

/*
 * iAP2IdSetAccessoryInfo
 * 设置配件信息（可选，用于覆盖默认配置）
 *
 * 必须在 iAP2IdInit() 之后、识别开始之前调用
 *
 * Input:
 *   serialNumber: 序列号（NULL表示不修改）
 *   fwVersion: 固件版本（NULL表示不修改）
 *   hwVersion: 硬件版本（NULL表示不修改）
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2IdSetAccessoryInfo(const char *serialNumber,
                           const char *fwVersion, const char *hwVersion);

/*
 * iAP2IdSetBluetoothMAC
 * 设置蓝牙 MAC 地址（可选）
 *
 * 必须在 iAP2IdInit() 之后、识别开始之前调用
 *
 * Input:
 *   macAddress: 6字节的MAC地址
 *   name: 蓝牙名称（NULL表示不修改）
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2IdSetBluetoothMACName(const uint8_t *macAddress, const char *name);

#ifdef __cplusplus
}
#endif

#endif /* IAP2_IDENTIFICATION_H */
