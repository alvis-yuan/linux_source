/*
 * File: iAP2Transport.c
 * Package: iAP2Transport
 * Abstract: iAP2通用传输层实现
 *
 * 传输层职责：
 * - 封装物理传输（蓝牙、USB等）
 * - 管理Link层（iAP2LinkRunLoop）
 * - 提供数据收发接口
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "iAP2Transport.h"
#include <iAP2LinkConfig.h>
#include <iAP2Link.h>
#include <iAP2LinkRunLoop.h>
#include <iAP2Packet.h>
#include <iAP2Log.h>
#include <iAP2Session.h>
#include <unistd.h>

/*
 * 内部结构定义
 */
struct iAP2Transport_st {
    /* iAP2 Link层 */
    iAP2LinkRunLoop_t*      linkRunLoop;
    uint8_t*                linkBuffer;
    
    /* 配置 */
    iAP2TransportConfig_t   config;
    
    /* 状态 */
    iAP2TransportState_t    state;
    BOOL                    linkConnected;  /* Link层是否已连接 */
    
    /* 接收缓冲 */
    iAP2Packet_t*           recvPacket;
};

/*
 * 前向声明 - iAP2Link回调函数
 */
static void _SendPacketCallback(iAP2Link_t* link, iAP2Packet_t* packet);
static BOOL _RecvDataCallback(iAP2Link_t* link, uint8_t* data, 
                               uint32_t dataLen, uint8_t session);
static void _ConnectedCallback(iAP2Link_t* link, BOOL bConnected);
static void _SendDetectCallback(iAP2Link_t* link, BOOL bBad);

/*
 * 从link获取transport句柄
 * 
 * 上下文链：link->context -> linkRunLoop -> linkRunLoop->context -> transport
 */
static iAP2Transport_t* _GetTransportFromLink(iAP2Link_t* link)
{
    if (!link || !link->context) {
        iAP2LogError("[Transport] Invalid link or context in _GetTransportFromLink");
        return NULL;
    }
    
    /* link->context 指向 linkRunLoop */
    iAP2LinkRunLoop_t* linkRunLoop = (iAP2LinkRunLoop_t*)link->context;
    if (!linkRunLoop) {
        iAP2LogError("[Transport] Invalid linkRunLoop in _GetTransportFromLink");
        return NULL;
    }
    
    /* linkRunLoop->context 指向 transport */
    iAP2Transport_t* transport = (iAP2Transport_t*)linkRunLoop->context;
    if (!transport) {
        iAP2LogError("[Transport] Invalid transport from linkRunLoop in _GetTransportFromLink");
        return NULL;
    }
    
    return transport;
}

/*
 * 发送数据包回调 - 通过物理层发送
 */
static void _SendPacketCallback(iAP2Link_t* link, iAP2Packet_t* packet)
{
    iAP2Transport_t* transport = _GetTransportFromLink(link);
    if (!transport || !packet) {
        iAP2LogError("[Transport] Invalid transport or packet in _SendPacketCallback\n");
        return;
    }
    
    /* 生成数据包缓冲区 */
    uint8_t* buffer = iAP2PacketGenerateBuffer(packet);
    if (!buffer) {
        iAP2LogError("[Transport] Failed to generate packet buffer\n");
        return;
    }
    
    /* 通过物理层发送数据 */
    if (transport->config.sendDataCB) {
        int ret = transport->config.sendDataCB(buffer, packet->packetLen, 
                                                transport->config.userContext);
        if (ret < 0) {
            iAP2LogError("[Transport] Send packet failed, ret=%d\n", ret);
        }
    } else {
        iAP2LogError("[Transport] sendDataCB is NULL in _SendPacketCallback\n");
    }
}

/*
 * 接收数据回调 - 转发给Session层
 */
static BOOL _RecvDataCallback(iAP2Link_t* link, uint8_t* data, 
                               uint32_t dataLen, uint8_t session)
{
    iAP2Transport_t* transport = _GetTransportFromLink(link);
    if (!transport) {
        iAP2LogError("[Transport] Failed to get transport in _RecvDataCallback");
        return FALSE;
    }
    
    /* 转发给Session层处理 */
    if (transport->config.dataReadyCB) {
        return transport->config.dataReadyCB(data, dataLen, session,
                                              transport->config.callbackContext);
    }
    
    /* 没有注册回调，返回TRUE表示数据已处理（丢弃） */
    iAP2LogDbg("[Transport] No dataReadyCB registered, discarding data");
    return TRUE;
}

/*
 * Link层连接状态回调
 */
static void _ConnectedCallback(iAP2Link_t* link, BOOL bConnected)
{
    iAP2Transport_t* transport = _GetTransportFromLink(link);
    if (!transport) {
        iAP2LogError("[Transport] Failed to get transport in _ConnectedCallback");
        return;
    }
    
    transport->linkConnected = bConnected;
    
    iAP2LogDbg("[Transport] Link %s\n", 
               bConnected ? "connected" : "disconnected");
    
    /* 通知Session层 */
    if (transport->config.linkConnectedCB) {
        transport->config.linkConnectedCB(bConnected, 
                                           transport->config.callbackContext);
    } else {
        iAP2LogDbg("[Transport] No linkConnectedCB registered");
    }
}

/*
 * 发送DETECT字节序列回调
 */
static void _SendDetectCallback(iAP2Link_t* link, BOOL bBad)
{
    iAP2LogDbg("[Transport] _SendDetectCallback called: link=%p, bBad=%d\n", link, bBad);
    
    iAP2Transport_t* transport = _GetTransportFromLink(link);
    if (!transport) {
        iAP2LogError("[Transport] Failed to get transport from link\n");
        return;
    }
    
    iAP2LogDbg("[Transport] Got transport: %p\n", transport);
    
    const uint8_t* detectData;
    uint32_t detectLen;
    
    if (bBad) {
        detectData = kIap2PacketDetectBadData;
        detectLen = kIap2PacketDetectBadDataLen;
    } else {
        detectData = kIap2PacketDetectData;
        detectLen = kIap2PacketDetectDataLen;
    }

    iAP2LogDbg("[Transport] Sending %s DETECT sequence (%u bytes)\n", 
               bBad ? "bad" : "good", detectLen);
    
    /* 检查回调函数指针 */
    if (!transport->config.sendDataCB) {
        iAP2LogError("[Transport] sendDataCB is NULL!\n");
        return;
    }
    
    iAP2LogDbg("[Transport] Calling sendDataCB: %p\n", transport->config.sendDataCB);
    
    /* 通过物理层发送DETECT序列 */
    int ret = transport->config.sendDataCB(detectData, detectLen, 
                                            transport->config.userContext);
    
    iAP2LogDbg("[Transport] sendDataCB returned: %d\n", ret);
}

/*
 * 创建传输层实例
 * 
 * 注意：Transport 创建时不包含会话信息
 * 会话信息由 Session 层通过 iAP2TransportGetLink() 获取 Link 对象后注册
 */
iAP2Transport_t* iAP2TransportCreate(const iAP2TransportConfig_t* config)
{
    if (!config || !config->sendDataCB) {
        iAP2LogError("[Transport] Invalid config\n");
        return NULL;
    }
    
    iAP2Transport_t* transport = (iAP2Transport_t*)malloc(sizeof(iAP2Transport_t));
    if (!transport) {
        iAP2LogError("[Transport] Failed to allocate transport\n");
        return NULL;
    }
    memset(transport, 0, sizeof(iAP2Transport_t));
    
    /* 保存配置 */
    memcpy(&transport->config, config, sizeof(iAP2TransportConfig_t));
    
    /* 设置默认值 */
    if (transport->config.maxPacketSize == 0) {
        transport->config.maxPacketSize = 1024;
    }
    if (transport->config.maxOutstanding == 0) {
        transport->config.maxOutstanding = 1;
    }
    if (transport->config.retransmitTimeout == 0) {
        transport->config.retransmitTimeout = 1000;
    }
    
    /* 
     * 配置SYN参数 - 不包含会话信息
     * 
     * 架构说明：
     * - Transport 层只负责物理传输和 Link 层协议
     * - 会话信息属于 Session 层的概念
     * - Session 层在创建后，通过 iAP2TransportGetLink() 获取 Link 对象
     * - 然后调用 iAP2SessionRegCtrl() 等函数注册会话到 link->param
     * - Link 层在发送 SYN 包时会使用 link->param 中的会话信息
     */
    iAP2PacketSYNData_t synParam;
    memset(&synParam, 0, sizeof(synParam));
    synParam.version = kiAP2LinkSynDefaultVersion;
    synParam.maxOutstandingPackets = transport->config.maxOutstanding;
    synParam.maxPacketSize = transport->config.maxPacketSize;
    synParam.retransmitTimeout = transport->config.retransmitTimeout;
    synParam.cumAckTimeout = kiAP2LinkSynValCumAckTimeoutMin;
    synParam.maxRetransmissions = kiAP2LinkSynDefaultMaxRetransmit;
    synParam.maxCumAck = kiAP2LinkSynDefaultMaxCumAck;
    synParam.numSessionInfo = 0;  /* 初始为0，由Session层注册 */

    /* 计算所需缓冲区大小 */
    uint32_t bufferSize = iAP2LinkRunLoopGetBuffSize(transport->config.maxOutstanding);
    transport->linkBuffer = (uint8_t*)malloc(bufferSize);
    if (!transport->linkBuffer) {
        iAP2LogError("[Transport] Failed to allocate link buffer\n");
        free(transport);
        return NULL;
    }

    /* 创建LinkRunLoop */
    transport->linkRunLoop = iAP2LinkRunLoopCreateAccessory(
        &synParam,
        transport,              /* context - 传递transport指针 */
        _SendPacketCallback,
        _RecvDataCallback,
        _ConnectedCallback,
        _SendDetectCallback,
        FALSE,                   /* bValidateSYN */
        transport->config.maxOutstanding,
        transport->linkBuffer
    );
    
    if (!transport->linkRunLoop) {
        iAP2LogError("[Transport] Failed to create LinkRunLoop\n");
        free(transport->linkBuffer);
        free(transport);
        return NULL;
    }
    
    transport->state = kIAP2TransportStateDisconnected;
    transport->linkConnected = FALSE;
    
    iAP2LogDbg("[Transport] Created (type=%d)\n", config->type);
    iAP2LogDbg("[Transport] Session info will be registered by Session layer\n");
    
    return transport;
}

/*
 * 销毁传输层实例
 */
void iAP2TransportDestroy(iAP2Transport_t* transport)
{
    if (!transport) {
        return;
    }
    
    /* 先断开连接 */
    if (transport->state != kIAP2TransportStateDisconnected) {
        iAP2TransportDisconnect(transport);
    }
    
    /* 删除接收包 */
    if (transport->recvPacket) {
        iAP2PacketDelete(transport->recvPacket);
        transport->recvPacket = NULL;
    }
    
    /* 删除LinkRunLoop */
    if (transport->linkRunLoop) {
        iAP2LinkRunLoopDelete(transport->linkRunLoop);
        transport->linkRunLoop = NULL;
    }
    
    /* 释放缓冲区 */
    if (transport->linkBuffer) {
        free(transport->linkBuffer);
        transport->linkBuffer = NULL;
    }
    
    free(transport);
    
    iAP2LogDbg("[Transport] Destroyed\n");
}

/*
 * 通知传输层物理连接已建立
 */
BOOL iAP2TransportConnect(iAP2Transport_t* transport)
{
    if (!transport || !transport->linkRunLoop) {
        return FALSE;
    }
    
    if (transport->state != kIAP2TransportStateDisconnected) {
        iAP2LogError("[Transport] Invalid state %d\n", transport->state);
        return FALSE;
    }
    
    transport->state = kIAP2TransportStateConnecting;
    iAP2LogDbg("[Transport] Connecting (type=%d)\n", transport->config.type);
    
    /* 通知Link层连接已建立 */
    iAP2LinkRunLoopAttached(transport->linkRunLoop);
    
    transport->state = kIAP2TransportStateConnected;
    
    iAP2LogDbg("[Transport] Connected, starting Link negotiation\n");
    
    return TRUE;
}

/*
 * 通知传输层物理连接已断开
 */
void iAP2TransportDisconnect(iAP2Transport_t* transport)
{
    if (!transport || !transport->linkRunLoop) {
        return;
    }
    
    if (transport->state == kIAP2TransportStateDisconnected) {
        return;
    }
    
    transport->state = kIAP2TransportStateDisconnecting;
    
    /* 通知Link层断开 */
    iAP2LinkRunLoopDetached(transport->linkRunLoop);
    
    transport->linkConnected = FALSE;
    transport->state = kIAP2TransportStateDisconnected;
    
    /* 清理接收包 */
    if (transport->recvPacket) {
        iAP2PacketDelete(transport->recvPacket);
        transport->recvPacket = NULL;
    }
    
    iAP2LogDbg("[Transport] Disconnected\n");
}

/*
 * 接收物理层数据
 */
uint32_t iAP2TransportReceiveData(iAP2Transport_t* transport,
                                   const uint8_t* data,
                                   uint32_t dataLen)
{
    if (!transport || !transport->linkRunLoop || !data || dataLen == 0) {
        iAP2LogError("[Transport] Invalid receive parameters\n");
        return 0;
    }
    
    if (transport->state != kIAP2TransportStateConnected) {
        iAP2LogError("[Transport] Invalid state %d\n", transport->state);
        return 0;
    }
    
    uint32_t totalParsed = 0;
    const uint8_t* parsePtr = data;
    uint32_t remaining = dataLen;
    
    while (remaining > 0) {
        /* 创建接收包（如果需要） */
        if (!transport->recvPacket) {
            transport->recvPacket = iAP2PacketCreateEmptyRecvPacket(
                transport->linkRunLoop->link);
            if (!transport->recvPacket) {
                iAP2LogError("[Transport] Failed to create recv packet\n");
                break;
            }
        }
        
        /* 解析数据 */
        BOOL bDetect = FALSE;
        uint32_t parsed = iAP2PacketParseBuffer(
            parsePtr,
            remaining,
            transport->recvPacket,
            transport->config.maxPacketSize,
            &bDetect,
            NULL,
            NULL
        );
        iAP2LogDbg("[Transport] bDetect=%d, Parsed %u bytes (remaining %u)\n", bDetect, parsed, remaining - parsed);
        
        if (parsed == 0) {
            break;
        }
        
        totalParsed += parsed;
        parsePtr += parsed;
        remaining -= parsed;
        
        /* 检查是否收到完整包 */
        if (iAP2PacketIsComplete(transport->recvPacket)) {
            /* 将完整包交给Link层处理 */
            iAP2LogDbg("[Transport] Received complete packet");
            iAP2LinkRunLoopHandleReadyPacket(transport->linkRunLoop, 
                                              transport->recvPacket);
            transport->recvPacket = NULL;  /* 包已交给Link层，不再持有 */
        }
    }
    
    return totalParsed;
}

/*
 * 发送数据到指定会话
 */
BOOL iAP2TransportSendData(iAP2Transport_t* transport,
                            uint8_t sessionID,
                            const uint8_t* data,
                            uint32_t dataLen)
{
    if (!transport || !transport->linkRunLoop || !data || dataLen == 0) {
        iAP2LogError("[Transport] Invalid parameters in iAP2TransportSendData");
        return FALSE;
    }
    
    if (!transport->linkConnected) {
        iAP2LogError("[Transport] Link not connected, cannot send data");
        return FALSE;
    }
    
    /* 队列发送数据 */
    iAP2LinkRunLoopQueueSendData(
        transport->linkRunLoop,
        data,
        dataLen,
        sessionID,
        NULL,   /* context */
        NULL    /* callback */
    );
    
    return TRUE;
}

/*
 * 处理传输层任务
 * 
 * 这是一个阻塞循环，会持续运行直到连接断开
 * 
 * 设计说明：
 * - 不检查 transport->state，因为 RunLoop 需要在连接前就启动
 * - Link 层内部会处理 Detached/Attached 状态转换
 * - 只有在 transport 被销毁时才退出循环
 */
BOOL iAP2TransportProcess(iAP2Transport_t* transport)
{
    if (!transport || !transport->linkRunLoop) {
        iAP2LogError("[Transport] Invalid transport or linkRunLoop\n");
        return FALSE;
    }
    
    iAP2LogDbg("[Transport] Processing Link tasks\n");
    
    /* 运行Link层循环处理 
     * 这个函数会阻塞，直到：
     * 1. linkRunLoop->shuttingDown 被设置为 TRUE
     * 2. 发生错误
     */
    return iAP2LinkRunLoopRun(transport->linkRunLoop);
}

/*
 * 获取当前连接状态
 */
iAP2TransportState_t iAP2TransportGetState(iAP2Transport_t* transport)
{
    if (!transport) {
        iAP2LogError("[Transport] NULL transport in iAP2TransportGetState");
        return kIAP2TransportStateDisconnected;
    }
    return transport->state;
}

/*
 * 检查Link层是否已建立
 */
BOOL iAP2TransportIsConnected(iAP2Transport_t* transport)
{
    if (!transport) {
        iAP2LogError("[Transport] NULL transport in iAP2TransportIsConnected");
        return FALSE;
    }
    return transport->linkConnected;
}

/*
 * 获取传输层类型
 */
iAP2TransportType_t iAP2TransportGetType(iAP2Transport_t* transport)
{
    if (!transport) {
        iAP2LogError("[Transport] NULL transport in iAP2TransportGetType");
        return kIAP2TransportTypeBluetooth;
    }
    return transport->config.type;
}

/*
 * 获取底层Link对象
 */
iAP2Link_t* iAP2TransportGetLink(iAP2Transport_t* transport)
{
    if (!transport || !transport->linkRunLoop) {
        iAP2LogError("[Transport] Invalid transport or linkRunLoop in iAP2TransportGetLink");
        return NULL;
    }
    return transport->linkRunLoop->link;
}

/*
 * 设置传输层回调函数
 */
void iAP2TransportSetCallbacks(iAP2Transport_t* transport,
                                 iAP2TransportDataReadyCB_t dataReadyCB,
                                 iAP2TransportLinkConnectedCB_t linkConnectedCB,
                                 void* context)
{
    if (!transport) {
        iAP2LogError("[Transport] NULL transport in iAP2TransportSetCallbacks");
        return;
    }
    
    if (dataReadyCB) {
        transport->config.dataReadyCB = dataReadyCB;
    }
    if (linkConnectedCB) {
        transport->config.linkConnectedCB = linkConnectedCB;
    }
    if (context) {
        transport->config.callbackContext = context;
    }
}
