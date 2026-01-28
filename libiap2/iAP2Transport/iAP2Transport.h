/*
 * File: iAP2Transport.h
 * Package: iAP2Transport
 * Abstract: iAP2通用传输层接口定义
 *
 * 传输层职责：
 * - 封装物理传输（蓝牙、USB等）
 * - 管理Link层（数据包、可靠传输）
 * - 向上层提供数据收发接口
 * 
 * 注意：传输层不处理会话逻辑，会话管理由iAP2Session层负责
 */

#ifndef IAP2_TRANSPORT_H
#define IAP2_TRANSPORT_H

#include <stdint.h>
#include <iAP2Defines.h>
#include <iAP2Link.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 传输层类型
 */
typedef enum {
    kIAP2TransportTypeBluetooth = 0,
    kIAP2TransportTypeUSB,
    kIAP2TransportTypeUART,
    kIAP2TransportTypeCount
} iAP2TransportType_t;

/*
 * 传输层状态
 */
typedef enum {
    kIAP2TransportStateDisconnected = 0,
    kIAP2TransportStateConnecting,
    kIAP2TransportStateConnected,
    kIAP2TransportStateDisconnecting
} iAP2TransportState_t;

/*
 * 前向声明
 */
typedef struct iAP2Transport_st iAP2Transport_t;

/*
 ****************************************************************
 * 传输层回调函数类型
 ****************************************************************
 */

/* 物理层数据发送回调 - 用户实现，将数据通过物理层发送 */
typedef int (*iAP2TransportSendDataCB_t)(const uint8_t* data, 
                                          uint32_t dataLen, 
                                          void* context);

/* 物理层连接状态变化回调 - 用户实现 */
typedef void (*iAP2TransportConnectionChangeCB_t)(BOOL connected, void* context);

/* Link层数据接收回调 - Session层实现 */
typedef BOOL (*iAP2TransportDataReadyCB_t)(uint8_t* data,
                                            uint32_t dataLen,
                                            uint8_t sessionID,
                                            void* context);

/* Link层连接状态回调 - Session层实现 */
typedef void (*iAP2TransportLinkConnectedCB_t)(BOOL connected, void* context);

/*
 * 传输层配置
 */
typedef struct {
    iAP2TransportType_t                 type;               /* 传输类型 */
    iAP2TransportSendDataCB_t           sendDataCB;         /* 物理层发送回调 */
    iAP2TransportConnectionChangeCB_t   connectionChangeCB; /* 物理层连接回调 */
    iAP2TransportDataReadyCB_t          dataReadyCB;        /* Link层数据回调 */
    iAP2TransportLinkConnectedCB_t      linkConnectedCB;    /* Link层连接回调 */
    void*                               callbackContext;    /* 回调上下文 */
    void*                               userContext;        /* 用户上下文 */
    uint16_t                            maxPacketSize;      /* 最大包大小 */
    uint8_t                             maxOutstanding;     /* 最大未确认包数 */
    uint16_t                            retransmitTimeout;  /* 重传超时(ms) */
} iAP2TransportConfig_t;

/*
 ****************************************************************
 * 通用传输层API
 ****************************************************************
 */

/*
 * iAP2TransportCreate
 * 创建传输层实例
 *
 * Input:
 *   config: 配置参数
 *
 * Return:
 *   成功返回句柄，失败返回NULL
 */
iAP2Transport_t* iAP2TransportCreate(const iAP2TransportConfig_t* config);

/*
 * iAP2TransportDestroy
 * 销毁传输层实例
 *
 * Input:
 *   transport: 传输层句柄
 */
void iAP2TransportDestroy(iAP2Transport_t* transport);

/*
 * iAP2TransportConnect
 * 通知传输层物理连接已建立
 *
 * Input:
 *   transport: 传输层句柄
 *
 * Return:
 *   成功返回TRUE
 */
BOOL iAP2TransportConnect(iAP2Transport_t* transport);

/*
 * iAP2TransportDisconnect
 * 通知传输层物理连接已断开
 *
 * Input:
 *   transport: 传输层句柄
 */
void iAP2TransportDisconnect(iAP2Transport_t* transport);

/*
 * iAP2TransportReceiveData
 * 接收物理层数据
 * 当从物理层收到数据时调用此函数
 *
 * Input:
 *   transport: 传输层句柄
 *   data: 接收到的数据
 *   dataLen: 数据长度
 *
 * Return:
 *   处理的字节数
 */
uint32_t iAP2TransportReceiveData(iAP2Transport_t* transport,
                                   const uint8_t* data,
                                   uint32_t dataLen);

/*
 * iAP2TransportSendData
 * 发送数据到指定会话
 * Session层调用此函数发送数据
 *
 * Input:
 *   transport: 传输层句柄
 *   sessionID: 会话ID
 *   data: 要发送的数据
 *   dataLen: 数据长度
 *
 * Return:
 *   成功返回TRUE
 */
BOOL iAP2TransportSendData(iAP2Transport_t* transport,
                            uint8_t sessionID,
                            const uint8_t* data,
                            uint32_t dataLen);

/*
 * iAP2TransportProcess
 * 处理传输层任务
 * 需要在主循环中周期性调用
 *
 * Input:
 *   transport: 传输层句柄
 *
 * Return:
 *   如果还有待处理任务返回TRUE
 */
BOOL iAP2TransportProcess(iAP2Transport_t* transport);

/*
 * iAP2TransportGetState
 * 获取当前连接状态
 *
 * Input:
 *   transport: 传输层句柄
 *
 * Return:
 *   当前状态
 */
iAP2TransportState_t iAP2TransportGetState(iAP2Transport_t* transport);

/*
 * iAP2TransportIsConnected
 * 检查iAP2链路是否已建立
 *
 * Input:
 *   transport: 传输层句柄
 *
 * Return:
 *   已连接返回TRUE
 */
BOOL iAP2TransportIsConnected(iAP2Transport_t* transport);

/*
 * iAP2TransportGetType
 * 获取传输层类型
 *
 * Input:
 *   transport: 传输层句柄
 *
 * Return:
 *   传输层类型
 */
iAP2TransportType_t iAP2TransportGetType(iAP2Transport_t* transport);

/*
 * iAP2TransportGetLink
 * 获取底层Link对象（供Session层使用）
 *
 * Input:
 *   transport: 传输层句柄
 *
 * Return:
 *   Link对象指针
 */
iAP2Link_t* iAP2TransportGetLink(iAP2Transport_t* transport);

/*
 * iAP2TransportSetCallbacks
 * 设置传输层回调函数
 * 用于Session层注册数据接收和连接状态回调
 *
 * Input:
 *   transport: 传输层句柄
 *   dataReadyCB: 数据接收回调
 *   linkConnectedCB: Link连接状态回调
 *   context: 回调上下文
 */
void iAP2TransportSetCallbacks(iAP2Transport_t* transport,
                                 iAP2TransportDataReadyCB_t dataReadyCB,
                                 iAP2TransportLinkConnectedCB_t linkConnectedCB,
                                 void* context);

#ifdef __cplusplus
}
#endif

#endif /* IAP2_TRANSPORT_H */
