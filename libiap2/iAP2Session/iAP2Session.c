/*
 * File: iAP2Session.c
 * Package: iAP2Session
 * Abstract: iAP2会话层实现
 *
 * 会话层职责：
 * - 管理Control Session（两阶段：Authentication → Identification）
 * - 管理EA Session
 * - 路由会话数据到应用层
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "iAP2Session.h"
#include "iAP2Authentication.h"
#include "iAP2ControlCodec.h"
#include "iAP2ControlMessage.h"
#include "iAP2Identification.h"
#include "iAP2Transport.h"
#include <iAP2Link.h>
#include <iAP2Log.h>

/*
 * 控制消息ID定义
 */
typedef enum {
    /* 认证相关 */
    kIAP2MsgIDRequestAuthenticationCertificate = 0x1D04,
    kIAP2MsgIDAuthenticationCertificate = 0x1D05,
    kIAP2MsgIDRequestAuthenticationChallengeResponse = 0x1D06,
    kIAP2MsgIDAuthenticationResponse = 0x1D07,
    kIAP2MsgIDAuthenticationFailed = 0x1D08,
    kIAP2MsgIDAuthenticationSucceeded = 0x1D09,

    /* 识别相关 */
    kIAP2MsgIDStartIdentification = 0x1D00,
    kIAP2MsgIDIdentificationInformation = 0x1D01,
    kIAP2MsgIDIdentificationAccepted = 0x1D02,
    kIAP2MsgIDIdentificationRejected = 0x1D03,
} iAP2MessageID_t;

/*
 * 会话管理器状态
 * 清晰的状态机：Idle → WaitLink → Authenticating → Identifying → Ready
 */
typedef enum {
    kSessionStateIdle = 0,              /* 空闲状态 */
    kSessionStateWaitLinkConnected,     /* 等待Link连接 */
    kSessionStateAuthenticating,        /* 认证阶段 */
    kSessionStateIdentifying,           /* 识别阶段 */
    kSessionStateReady,                 /* 就绪状态（认证和识别都完成） */
    kSessionStateFailed                 /* 失败状态 */
} iAP2SessionState_t;

/*
 * 会话管理器内部结构
 */
struct iAP2Session_st {
    /* 配置 */
    iAP2SessionConfig_t     config;

    /* 传输层 */
    iAP2Transport_t        *transport;

    /* 状态 */
    iAP2SessionState_t      state;
    BOOL                    linkConnected;
    BOOL                    authenticated;      /* 认证完成标志 */
    BOOL                    identified;         /* 识别完成标志 */

    uint16_t                 EASessionID;          /* device的会话ID */
};

/*
 * 前向声明
 */
static BOOL _HandleTransportData(uint8_t *data, uint32_t dataLen,
                                 uint8_t sessionID, void *context);
static void _HandleLinkConnected(BOOL connected, void *context);
static void _HandleAuthResult(BOOL success, void *context);
static void _HandleIdentifyResult(BOOL accepted, uint8_t rejectReason,
                                  void *context);
static BOOL _SendControlMessage(const uint8_t *data, uint32_t len,
                                void *context);

/*
 * 处理Transport层数据
 * 根据会话类型和当前状态路由数据
 */
static BOOL _HandleTransportData(uint8_t *data, uint32_t dataLen,
                                 uint8_t sessionID, void *context)
{
    iAP2Session_t *session = (iAP2Session_t *)context;

    if (!session) {
        iAP2LogError("[Session] NULL session in _HandleTransportData");
        return FALSE;
    }

    if (!data || dataLen == 0) {
        iAP2LogError("[Session] Invalid data parameters in _HandleTransportData");
        return FALSE;
    }
#if 0
    for (int i = 0; i < dataLen; i++) {
        fprintf(stderr, "%02X ", data[i]);
    }

    fprintf(stderr, "\n");
#endif
    /* 获取会话类型信息 */
    iAP2Link_t *link = iAP2TransportGetLink(session->transport);
    iAP2PacketSessionInfo_t *sessInfo = iAP2LinkGetSessionInfo(link, sessionID);
    iAP2SessionType_t sessionType = kIAP2SessionTypeControl;

    if (sessInfo) {
        sessionType = (iAP2SessionType_t)sessInfo->type;
    }

    iAP2LogDbg("%s session %d received %d bytes",
               sessionType == kIAP2SessionTypeEA ? "EA" : "Control", sessionID, dataLen);

    /* 控制会话数据 - 解析并路由到认证或识别模块 */
    if (sessionType == kIAP2SessionTypeControl) {
        /* 解析控制消息头：StartIdentifier(2) + msgLen(2) + msgID(2) + payload */
        if (dataLen < 4) {
            iAP2LogError("[Session] Control message too short");
            return TRUE;
        }

        /* 使用ControlCodec解析消息 */
        ctrlSessMessage msg;

        if (ctrlSess_DecodeMessageHeader(data, dataLen, &msg) != 0) {
            iAP2LogError("[Auth] Failed to decode message header");
            return FALSE;
        }

        uint16_t msgID = msg.msgId;
        iAP2LogDbg("[Session] Control message: ID=0x%04X, len=%u, state=%d",
                   msgID, msg.totalLength, session->state);

        /* 根据当前状态路由消息 */
        switch (session->state) {
            case kSessionStateAuthenticating:

                /* 认证阶段 - 交给认证模块处理 */
                if (iAP2AuthHandleMessage(msgID, data, dataLen)) {
                    return TRUE;
                }

                break;

            case kSessionStateIdentifying:

                /* 识别阶段 - 交给识别模块处理 */
                if (iAP2IdHandleMessage(msgID, data, dataLen)) {
                    return TRUE;
                }

                break;

            case kSessionStateReady:

                /* 就绪状态 - 可能收到识别更新或其他控制消息 */
                if (iAP2IdHandleMessage(msgID, data, dataLen)) {
                    return TRUE;
                }

                /* 处理App Launch/Discovery/EAP消息 */
                switch (msgID) {
                    case kiAP2AppDisMsgUpdate:
                        /* AppDiscoveryUpdate - 转发给用户回调 */
                        iAP2LogDbg("[Session] Received AppDiscoveryUpdate");
                        break;

                    case kiAP2EapMsgStartSession:
                        /* EAP Start Session - 转发给用户回调 */
                        iAP2HandleEAPStartSession(msgID, data, dataLen);
                        iAP2LogDbg("[Session] Received EAPStartSession");
                        break;

                    case kiAP2EapMsgStopSession:
                        /* EAP Stop Session - 转发给用户回调 */
                        iAP2LogDbg("[Session] Received EAPStopSession");
                        break;

                    default:
                        break;
                }

                break;

            default:
                break;
        }

        return TRUE;
    } else if (sessionType == kIAP2SessionTypeEA) {
        uint16_t eaSessionID = READ_U16(data);
        /* EA会话数据 - 验证EA会话ID */
        if (session->EASessionID != eaSessionID) {
            iAP2LogError("[Session] Invalid EA session ID: expected 0x%02X, got 0x%02X",
                         session->EASessionID, eaSessionID);
            return FALSE;
        }

        /* EA会话数据 - 解析并路由到应用层 */
        if (session->config.sessionDataCB) {
            /* 移除EA会话ID前缀(2字节) ref: 64.3.4.2 ExternalAccessorySession Datagram */
            return session->config.sessionDataCB(data + 2, dataLen - 2,
                                                 session->config.context);
        } else {
            iAP2LogError("[Session] No sessionDataCB registered for EA data");
            return FALSE;
        }
    } else {
        iAP2LogError("[Session] Unknown session type %d for session ID %u",
                     sessionType, sessionID);
        return FALSE;
    }

    return TRUE;
}

/*
 * 处理Link层连接状态
 * Link连接后启动认证流程
 */
static void _HandleLinkConnected(BOOL connected, void *context)
{
    iAP2Session_t *session = (iAP2Session_t *)context;

    if (!session) {
        iAP2LogError("[Session] NULL session in _HandleLinkConnected");
        return;
    }

    session->linkConnected = connected;

    if (connected) {
        iAP2LogDbg("[Session] Link connected, starting authentication phase");
        /* 获取协商后的控制会话版本 */
        iAP2Link_t *link = iAP2TransportGetLink(session->transport);

        if (link) {
            iAP2PacketSessionInfo_t *ctrlSessInfo = iAP2LinkGetSessionInfo(link,
                                                    kIAP2CtrlSessionId);

            if (ctrlSessInfo) {
                iAP2LogDbg("[Session] Negotiated control session version: %d, sessNum=%d",
                           ctrlSessInfo->version, link->param.numSessionInfo);
                iAP2AuthSetControlSessionVersion(ctrlSessInfo->version);
            } else {
                iAP2LogDbg("[Session] Control session info not found, using default version 1");
            }
        }

        /* Link连接后，进入认证阶段 */
        session->state = kSessionStateAuthenticating;
        iAP2AuthOnLinkConnected();
    } else {
        iAP2LogDbg("[Session] Link disconnected, resetting session");
        /* 重置所有状态 */
        session->state = kSessionStateIdle;
        session->authenticated = FALSE;
        session->identified = FALSE;
        /* 重置认证和识别模块 */
        iAP2AuthReset();
        iAP2IdReset();
    }
}

/*
 * 处理认证结果
 * 认证成功后进入识别阶段
 */
static void _HandleAuthResult(BOOL success, void *context)
{
    iAP2Session_t *session = (iAP2Session_t *)context;

    if (!session) {
        iAP2LogError("[Session] NULL session in _HandleAuthResult");
        return;
    }

    session->authenticated = success;

    if (success) {
        iAP2LogDbg("[Session] ✓ Authentication succeeded, entering identification phase");
        /* 认证成功，进入识别阶段 */
        session->state = kSessionStateIdentifying;
        /* 通知识别模块认证已完成 */
        iAP2IdOnAuthComplete(TRUE);
    } else {
        iAP2LogError("[Session] ✗ Authentication failed");
        session->state = kSessionStateFailed;
    }

    /* 通知用户认证结果 */
    if (session->config.authCompleteCB) {
        session->config.authCompleteCB(success, session->config.context);
    }
}

/*
 * 处理识别结果
 * 识别成功后进入就绪状态
 */
static void _HandleIdentifyResult(BOOL accepted, uint8_t rejectReason,
                                  void *context)
{
    iAP2Session_t *session = (iAP2Session_t *)context;

    if (!session) {
        iAP2LogError("[Session] NULL session in _HandleIdentifyResult");
        return;
    }

    session->identified = accepted;

    if (accepted) {
        iAP2LogDbg("[Session] ✓ Identification accepted, session ready");
        /* 识别成功，进入就绪状态 */
        session->state = kSessionStateReady;
    } else {
        iAP2LogError("[Session] ✗ Identification rejected, reason=%u", rejectReason);
        session->state = kSessionStateFailed;
    }

    /* 通知用户识别结果 */
    if (session->config.identifyCompleteCB) {
        session->config.identifyCompleteCB(accepted, session->config.context);
    }
}

/*
 * 发送控制消息（认证和识别模块使用）
 */
static BOOL _SendControlMessage(const uint8_t *data, uint32_t len,
                                void *context)
{
    iAP2Session_t *session = (iAP2Session_t *)context;

    if (!session || !session->transport) {
        iAP2LogError("[Session] Invalid session or transport in _SendControlMessage");
        return FALSE;
    }

    if (!data || len == 0) {
        iAP2LogError("[Session] Invalid data parameters in _SendControlMessage");
        return FALSE;
    }

    iAP2Link_t *link = iAP2TransportGetLink(session->transport);

    if (!link) {
        iAP2LogError("[Session] Failed to get link from transport in _SendControlMessage");
        return FALSE;
    }

    iAP2PacketSessionInfo_t *sessInfo = iAP2LinkGetSessionInfo(link,
                                        kIAP2CtrlSessionId);

    if (!sessInfo) {
        iAP2LogError("[Session] Failed to get control session info from link in _SendControlMessage");
        return FALSE;
    }

    return iAP2TransportSendData(session->transport,
                                 sessInfo->id,
                                 data, len);
}

/*
 * 创建会话管理器实例
 *
 * 架构说明：
 * 1. Transport 层创建时不包含会话信息（分层原则）
 * 2. Session 层创建后，立即注册会话信息到 Link 层
 * 3. Link 层在发送 SYN 包时使用注册的会话信息
 */
iAP2Session_t *iAP2SessionCreate(const iAP2SessionConfig_t *config,
                                 uint8_t type)
{
    if (!config || !config->transport) {
        iAP2LogError("[Session] Invalid config");
        return NULL;
    }

    iAP2Session_t *session = (iAP2Session_t *)malloc(sizeof(iAP2Session_t));

    if (!session) {
        iAP2LogError("[Session] Failed to allocate session");
        return NULL;
    }

    memset(session, 0, sizeof(iAP2Session_t));
    /* 保存配置 */
    memcpy(&session->config, config, sizeof(iAP2SessionConfig_t));
    session->transport = config->transport;
    /*
     * 关键步骤：注册控制会话到Link层
     *
     * 这是正确的分层设计：
     * - Transport 层不知道会话的概念
     * - Session 层负责定义和注册会话
     * - Link 层只是存储和传输会话信息
     */
    iAP2Link_t *link = iAP2TransportGetLink(session->transport);

    if (!link) {
        iAP2LogError("[Session] Failed to get link from transport");
        free(session);
        return NULL;
    }

    /* 注册控制会话（ID=0x0A, Type=Control, Version=1） */
    if (iAP2SessionRegCtrl(&link->initParam) < 0) {
        iAP2LogError("[Session] Failed to register control session");
        free(session);
        return NULL;
    }

    iAP2LogDbg("[Session] Registered control session to Link layer");

    // 注册EA会话（ID=0x0B, Type=EA, Version=1）
    if (iAP2SessionReg(&link->initParam, kIAP2EASessionId, kIAP2SessionTypeEA,
                       kIAP2EASessionVersion) < 0) {
        iAP2LogError("[Session] Failed to register EA session");
        free(session);
        return NULL;
    }

    iAP2LogDbg("[Session] Registered EA session to Link layer");

    /* 注册Transport层回调 */
    iAP2TransportSetCallbacks(session->transport,
                              _HandleTransportData,
                              _HandleLinkConnected,
                              session);
    /* 初始化认证模块 */
    iAP2AuthConfig_t authConfig;
    memset(&authConfig, 0, sizeof(authConfig));
    authConfig.resultCallback = _HandleAuthResult;
    authConfig.sendMsgCallback = _SendControlMessage;
    authConfig.context = session;
    authConfig.useAsyncCP = TRUE;
    authConfig.controlSessionVersion =
        1;  /* 默认版本1，Link连接后会更新 */

    if (iAP2AuthInit(&authConfig) < 0) {
        iAP2LogError("[Session] Failed to init authentication module");
        free(session);
        return NULL;
    }

    /* 初始化识别模块 */
    iAP2IdConfig_t idConfig;
    memset(&idConfig, 0, sizeof(idConfig));
    idConfig.resultCallback = _HandleIdentifyResult;
    idConfig.sendMsgCallback = _SendControlMessage;
    idConfig.context = session;

    if (iAP2IdInit(&idConfig, type) < 0) {
        iAP2LogError("[Session] Failed to init identification module");
        iAP2AuthDeinit();
        free(session);
        return NULL;
    }

    /* 初始化控制消息模块 */
    if (iAP2CtrlMsgInit(_SendControlMessage, session) < 0) {
        iAP2LogError("[Session] Failed to init control message module");
        iAP2IdDeinit();
        iAP2AuthDeinit();
        free(session);
        return NULL;
    }

    session->state = kSessionStateIdle;
    iAP2LogDbg("[Session] Session manager created");
    iAP2LogDbg("[Session] Control flow: Idle → Authentication → Identification → Ready");
    return session;
}

/*
 * 销毁会话管理器实例
 */
void iAP2SessionDestroy(iAP2Session_t *session)
{
    if (!session) {
        return;
    }

    /* 反初始化控制消息模块 */
    iAP2CtrlMsgDeinit();
    /* 反初始化识别模块 */
    iAP2IdDeinit();
    /* 反初始化认证模块 */
    iAP2AuthDeinit();
    free(session);
    iAP2LogDbg("[Session] Session manager destroyed");
}

/*
 * 启动会话管理器
 */
BOOL iAP2SessionStart(iAP2Session_t *session)
{
    if (!session) {
        return FALSE;
    }

    if (session->state != kSessionStateIdle) {
        iAP2LogError("[Session] Invalid state %d, cannot start", session->state);
        return FALSE;
    }

    session->state = kSessionStateWaitLinkConnected;
    iAP2LogDbg("[Session] Session manager started, waiting for link connection");
    return TRUE;
}

/*
 * 停止会话管理器
 */
void iAP2SessionStop(iAP2Session_t *session)
{
    if (!session) {
        return;
    }

    session->state = kSessionStateIdle;
    session->linkConnected = FALSE;
    session->authenticated = FALSE;
    session->identified = FALSE;
    /* 重置认证和识别模块 */
    iAP2AuthReset();
    iAP2IdReset();
    iAP2LogDbg("[Session] Session manager stopped");
}

/*
 * 发送会话数据
 */
BOOL iAP2SessionSendData(iAP2Session_t *session,
                         uint8_t sessionID,
                         const uint8_t *data,
                         uint32_t dataLen)
{
    if (!session || !session->transport) {
        iAP2LogError("[Session] Invalid session or transport in iAP2SessionSendData");
        return FALSE;
    }

    if (!data || dataLen == 0) {
        iAP2LogError("[Session] Invalid data parameters in iAP2SessionSendData");
        return FALSE;
    }

    return iAP2TransportSendData(session->transport, sessionID, data, dataLen);
}

/*
 * 检查是否已完成认证
 */
BOOL iAP2SessionIsAuthenticated(iAP2Session_t *session)
{
    if (!session) {
        iAP2LogError("[Session] NULL session in iAP2SessionIsAuthenticated");
        return FALSE;
    }

    return session->authenticated;
}

/*
 * 检查是否已完成识别
 */
BOOL iAP2SessionIsIdentified(iAP2Session_t *session)
{
    if (!session) {
        iAP2LogError("[Session] NULL session in iAP2SessionIsIdentified");
        return FALSE;
    }

    return session->identified;
}

/*
 * 检查会话管理器是否就绪
 */
BOOL iAP2SessionIsReady(iAP2Session_t *session)
{
    if (!session) {
        iAP2LogError("[Session] NULL session in iAP2SessionIsReady");
        return FALSE;
    }

    return (session->state == kSessionStateReady) &&
           session->authenticated &&
           session->identified;
}

/*
 * 获取当前会话状态（用于调试）
 */
const char *iAP2SessionGetStateString(iAP2Session_t *session)
{
    if (!session) {
        iAP2LogError("[Session] NULL session in iAP2SessionGetStateString");
        return "NULL";
    }

    switch (session->state) {
        case kSessionStateIdle:
            return "Idle";

        case kSessionStateWaitLinkConnected:
            return "WaitLink";

        case kSessionStateAuthenticating:
            return "Authenticating";

        case kSessionStateIdentifying:
            return "Identifying";

        case kSessionStateReady:
            return "Ready";

        case kSessionStateFailed:
            return "Failed";

        default:
            return "Unknown";
    }
}

/*
 ****************************************************************
 * 会话注册辅助函数
 ****************************************************************
 */

/*
 * 注册会话到SYN参数
 */
int iAP2SessionReg(iAP2PacketSYNData_t *p_syn_data, uint8_t id, uint8_t type,
                   uint8_t version)
{
    if (!p_syn_data) {
        iAP2LogError("[Session] NULL p_syn_data in iAP2SessionReg");
        return -1;
    }

    if (p_syn_data->numSessionInfo >= kIAP2PacketMaxSessions) {
        iAP2LogError("[Session] Too many sessions, cannot register more");
        return -1;
    }

    iAP2PacketSessionInfo_t session_info = {
        .id = id,
        .type = type,
        .version = version,
    };
    p_syn_data->sessionInfo[p_syn_data->numSessionInfo++] = session_info;
    iAP2LogDbg("[Session] Registered session: id=0x%02X, type=%d, version=%d",
               id, type, version);
    return 0;
}

/*
 * 保存EA会话ID
 */
int iAP2SessionRegEA(uint16_t eaSessionID, void *context)
{
    iAP2Session_t *session = (iAP2Session_t *)context;

    if (!session || !session->transport) {
        iAP2LogError("[Session] Invalid session or transport in iAP2SessionRegEA");
        return -1;
    }
#if 0
    iAP2Link_t *link = iAP2TransportGetLink(session->transport);

    if (!link) {
        iAP2LogError("[Session] Failed to get link from transport");
        return -1;
    }

    if (iAP2SessionReg(&link->param, eaSessionID, kIAP2SessionTypeEA, 1) < 0) {
        iAP2LogError("[Session] Failed to register EA session");
        return -1;
    }
#endif

    session->EASessionID = eaSessionID;

    return 0;
}

/*
 * 注销EA会话ID 
 */
void iAP2SessionUnregEA(void *context)
{
    iAP2Session_t *session = (iAP2Session_t *)context;

    if (!session || !session->transport) {
        iAP2LogError("[Session] Invalid session in iAP2SessionUnregEA");
        return;
    }

    if (session->EASessionID != 0) {
        iAP2LogDbg("[Session] Unregistered EA session ID: 0x%02X",
                   session->EASessionID);
        session->EASessionID = 0;
    }

}

/* 获取EA会话ID */
uint16_t iAP2SessionGetEAID(void *context)
{
    iAP2Session_t *session = (iAP2Session_t *)context;
    if (!session || !session->transport) {
        iAP2LogError("[Session] NULL session in iAP2SessionGetEAID");
        return 0;
    }

    return session->EASessionID;
}