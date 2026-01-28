/*
 * File: iAP2ControlMessage.c
 * Package: iAP2Session
 * Abstract: iAP2 Control Message Implementation
 *
 * 实现App Launch、App Discovery和External Accessory Protocol功能
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "iAP2ControlMessage.h"
#include "iAP2ControlCodec.h"
#include "iAP2Session.h"
#include <iAP2Log.h>

/* 消息缓冲区 */
#define CTRL_MSG_BUFFER_SIZE 2048
static uint8_t g_msgBuffer[CTRL_MSG_BUFFER_SIZE];

/* 模块配置 */
typedef struct {
    iAP2CtrlMsgSendCB_t sendMsgCallback;
    void *context;
    BOOL initialized;
} iAP2CtrlMsgContext_t;

static iAP2CtrlMsgContext_t g_ctrlMsgCtx = {0};

/*
 ****************************************************************
 * 初始化和配置
 ****************************************************************
 */

int iAP2CtrlMsgInit(iAP2CtrlMsgSendCB_t sendCallback, void *context)
{
    if (!sendCallback) {
        iAP2LogError("[CtrlMsg] Invalid send callback");
        return -1;
    }

    g_ctrlMsgCtx.sendMsgCallback = sendCallback;
    g_ctrlMsgCtx.context = context;
    g_ctrlMsgCtx.initialized = TRUE;
    iAP2LogDbg("[CtrlMsg] Control message module initialized");
    return 0;
}

void iAP2CtrlMsgDeinit(void)
{
    memset(&g_ctrlMsgCtx, 0, sizeof(g_ctrlMsgCtx));
    iAP2LogDbg("[CtrlMsg] Control message module deinitialized");
}

/*
 ****************************************************************
 * App Launch 功能实现
 ****************************************************************
 */

int iAP2SendRequestAppLaunch(const char *bundleId, uint8_t launchMethod)
{
    if (!g_ctrlMsgCtx.initialized) {
        iAP2LogError("[CtrlMsg] Module not initialized");
        return -1;
    }

    if (!bundleId) {
        iAP2LogError("[CtrlMsg] Invalid bundle ID");
        return -1;
    }

    /* 构建RequestAppLaunch消息 (0xEA02) */
    ctrlSessBuilder builder;
    ctrlSess_BuilderInit(&builder, g_msgBuffer, CTRL_MSG_BUFFER_SIZE,
                         kiAP2AppMsgLaunchReq);

    /* 添加BundleID参数 */
    if (ctrlSess_AddString(&builder, kiAP2AppParamBundleId, bundleId) != 0) {
        iAP2LogError("[CtrlMsg] Failed to add bundle ID parameter");
        return -1;
    }

    /* 添加LaunchMethod参数（可选） */
    if (launchMethod > 0) {
        if (ctrlSess_AddEnum(&builder, kiAP2AppParamLaunchMethod, launchMethod) != 0) {
            iAP2LogError("[CtrlMsg] Failed to add launch method parameter");
            return -1;
        }
    }

    uint16_t msgLen = ctrlSess_BuilderFinish(&builder);
    iAP2LogDbg("[CtrlMsg] Sending RequestAppLaunch: bundleId=%s, method=%u, len=%u",
               bundleId, launchMethod, msgLen);

    /* 发送消息 */
    if (g_ctrlMsgCtx.sendMsgCallback) {
        if (!g_ctrlMsgCtx.sendMsgCallback(g_msgBuffer, msgLen, g_ctrlMsgCtx.context)) {
            iAP2LogError("[CtrlMsg] Failed to send RequestAppLaunch message");
            return -1;
        }
    }

    return 0;
}

/*
 ****************************************************************
 * App Discovery 功能实现
 ****************************************************************
 */

int iAP2SendStartAppDiscoveryUpdates(const uint8_t *categories,
                                     uint8_t categoryCount,
                                     uint16_t listMax)
{
    if (!g_ctrlMsgCtx.initialized) {
        iAP2LogError("[CtrlMsg] Module not initialized");
        return -1;
    }

    /* 构建StartAppDiscoveryUpdates消息 (0xAD00) */
    ctrlSessBuilder builder;
    ctrlSess_BuilderInit(&builder, g_msgBuffer, CTRL_MSG_BUFFER_SIZE,
                         kiAP2AppDisMsgStartUpdate);

    /* 添加Categories参数（可选） */
    if (categories && categoryCount > 0) {
        ctrlSessGroupBuilder groupBuilder;

        if (ctrlSess_BeginGroup(&groupBuilder, &builder,
                                kiAP2AppDisParamCategories) != 0) {
            iAP2LogError("[CtrlMsg] Failed to begin categories group");
            return -1;
        }

        /* 添加每个category */
        for (uint8_t i = 0; i < categoryCount; i++) {
            if (ctrlSess_AddEnum(&builder, 0, categories[i]) != 0) {
                iAP2LogError("[CtrlMsg] Failed to add category %u", i);
                return -1;
            }
        }

        if (ctrlSess_EndGroup(&groupBuilder) != 0) {
            iAP2LogError("[CtrlMsg] Failed to end categories group");
            return -1;
        }
    }

    /* 添加ListMax参数（可选） */
    if (listMax > 0) {
        if (ctrlSess_AddUint16(&builder, kiAP2AppDisParamListMax, listMax) != 0) {
            iAP2LogError("[CtrlMsg] Failed to add list max parameter");
            return -1;
        }
    }

    uint16_t msgLen = ctrlSess_BuilderFinish(&builder);
    iAP2LogDbg("[CtrlMsg] Sending StartAppDiscoveryUpdates: categoryCount=%u, listMax=%u, len=%u",
               categoryCount, listMax, msgLen);

    /* 发送消息 */
    if (g_ctrlMsgCtx.sendMsgCallback) {
        if (!g_ctrlMsgCtx.sendMsgCallback(g_msgBuffer, msgLen, g_ctrlMsgCtx.context)) {
            iAP2LogError("[CtrlMsg] Failed to send StartAppDiscoveryUpdates message");
            return -1;
        }
    }

    return 0;
}

int iAP2SendStopAppDiscoveryUpdates(void)
{
    if (!g_ctrlMsgCtx.initialized) {
        iAP2LogError("[CtrlMsg] Module not initialized");
        return -1;
    }

    /* 构建StopAppDiscoveryUpdates消息 (0xAD02) - 无参数 */
    ctrlSessBuilder builder;
    ctrlSess_BuilderInit(&builder, g_msgBuffer, CTRL_MSG_BUFFER_SIZE,
                         kiAP2AppDisMsgStopUpdate);
    uint16_t msgLen = ctrlSess_BuilderFinish(&builder);
    iAP2LogDbg("[CtrlMsg] Sending StopAppDiscoveryUpdates: len=%u", msgLen);

    /* 发送消息 */
    if (g_ctrlMsgCtx.sendMsgCallback) {
        if (!g_ctrlMsgCtx.sendMsgCallback(g_msgBuffer, msgLen, g_ctrlMsgCtx.context)) {
            iAP2LogError("[CtrlMsg] Failed to send StopAppDiscoveryUpdates message");
            return -1;
        }
    }

    return 0;
}

int iAP2HandleAppDiscoveryUpdate(const uint8_t *data, uint32_t len,
                                 iAP2AppDiscoveryUpdateCB_t callback, void *context)
{
    if (!data || len == 0 || !callback) {
        iAP2LogError("[CtrlMsg] Invalid parameters for AppDiscoveryUpdate");
        return -1;
    }

    /* 解析消息 */
    ctrlSessMessage msg;

    if (ctrlSess_DecodeMessageHeader(data, len, &msg) != 0) {
        iAP2LogError("[CtrlMsg] Failed to decode AppDiscoveryUpdate message");
        return -1;
    }

    if (msg.msgId != kiAP2AppDisMsgUpdate) {
        iAP2LogError("[CtrlMsg] Invalid message ID: expected 0x%04X, got 0x%04X",
                     kiAP2AppDisMsgUpdate, msg.msgId);
        return -1;
    }

    /* 解析参数 */
    const uint8_t *paramPtr = msg.paramStart;
    size_t remaining = msg.totalLength - 6;
    uint8_t listAvailable = 0;
    uint16_t listCount = 0;
    //BOOL hasListAvailable = FALSE;
    //BOOL hasListCount = FALSE;

    while (remaining > 0) {
        ctrlSessParameter param;
        int consumed = ctrlSess_GetNextParameter(paramPtr, remaining, &param);

        if (consumed <= 0) {
            break;
        }

        switch (param.id) {
            case kiAP2AppDisParamListAvail:
                if (ctrlSess_ParamGetEnum(&param, &listAvailable) == 0) {
                    //hasListAvailable = TRUE;
                }

                break;

            case kiAP2AppDisParamList:
                /* 这是一个group，包含应用列表信息 */
                /* 实际应用中需要进一步解析group内容 */
                iAP2LogDbg("[CtrlMsg] Received app list group");
                break;

            case kiAP2AppDisParamListCount:
                listCount = ctrlSess_ParamGetUint16(&param);
                //hasListCount = TRUE;
                break;

            default:
                iAP2LogDbg("[CtrlMsg] Unknown parameter ID: %u", param.id);
                break;
        }

        paramPtr += consumed;
        remaining -= consumed;
    }

    iAP2LogDbg("[CtrlMsg] AppDiscoveryUpdate: listAvailable=%u, listCount=%u",
               listAvailable, listCount);
    /* 调用回调 */
    callback(listAvailable, listCount, context);
    return 0;
}

/*
 ****************************************************************
 * External Accessory Protocol (EAP) 功能实现
 ****************************************************************
 */

int iAP2SendEAPSessionStatus(uint16_t sessionId, uint8_t status)
{
    if (!g_ctrlMsgCtx.initialized) {
        iAP2LogError("[CtrlMsg] Module not initialized");
        return -1;
    }

    /* 构建EAPSessionStatus消息 (0xEA03) */
    ctrlSessBuilder builder;
    ctrlSess_BuilderInit(&builder, g_msgBuffer, CTRL_MSG_BUFFER_SIZE,
                         kiAP2EapMsgSessionStatus);

    /* 添加SessionID参数 */
    if (ctrlSess_AddUint16(&builder, kiAP2EapParamSessionId, sessionId) != 0) {
        iAP2LogError("[CtrlMsg] Failed to add session ID parameter");
        return -1;
    }

    /* 添加Status参数 */
    if (ctrlSess_AddEnum(&builder, kiAP2EapParamStatus, status) != 0) {
        iAP2LogError("[CtrlMsg] Failed to add status parameter");
        return -1;
    }

    uint16_t msgLen = ctrlSess_BuilderFinish(&builder);
    iAP2LogDbg("[CtrlMsg] Sending EAPSessionStatus: sessionId=%u, status=%u, len=%u",
               sessionId, status, msgLen);

    /* 发送消息 */
    if (g_ctrlMsgCtx.sendMsgCallback) {
        if (!g_ctrlMsgCtx.sendMsgCallback(g_msgBuffer, msgLen, g_ctrlMsgCtx.context)) {
            iAP2LogError("[CtrlMsg] Failed to send EAPSessionStatus message");
            return -1;
        }
    }

    return 0;
}

static int iAP2HandleEAPStartSessionInternal(uint16_t sessionId,
        uint8_t protocolId)
{
    if (!g_ctrlMsgCtx.initialized) {
        iAP2LogError("[CtrlMsg] Module not initialized");
        return -1;
    }

    iAP2LogDbg("[CtrlMsg] Handling EAPStartSession: sessionId=%u, protocolId=%u",
               sessionId, protocolId);

    // TODO 处理EAPStartSession逻辑
    // 1. 验证协议ID是否在identification阶段声明的协议列表中
    // 2. 注册EA会话ID
    if (iAP2SessionRegEA(sessionId, g_ctrlMsgCtx.context) != 0) {
        iAP2LogError("[CtrlMsg] Failed to register EA session");
        return -1;
    }

    // 3. 如果有效，创建EAP数据会话
    return 0;
}

int iAP2HandleEAPStartSession(uint16_t msgId, const uint8_t *data, uint32_t len)
{
    if (!data || len == 0) {
        iAP2LogError("[CtrlMsg] Invalid parameters for EAPStartSession");
        return -1;
    }

    if (msgId != kiAP2EapMsgStartSession) {
        iAP2LogError("[CtrlMsg] Mismatched message ID: expected 0x%04X, got 0x%04X",
                     kiAP2EapMsgStartSession, msgId);
        return -1;
    }

    /* 解析消息 */
    ctrlSessMessage msg;

    if (ctrlSess_DecodeMessageHeader(data, len, &msg) != 0) {
        iAP2LogError("[CtrlMsg] Failed to decode EAPStartSession message");
        return -1;
    }

    if (msg.msgId != kiAP2EapMsgStartSession) {
        iAP2LogError("[CtrlMsg] Invalid message ID: expected 0x%04X, got 0x%04X",
                     kiAP2EapMsgStartSession, msg.msgId);
        return -1;
    }

    /* 解析参数 */
    const uint8_t *paramPtr = msg.paramStart;
    size_t remaining = msg.totalLength - 6;
    uint8_t protocolId = 0;
    uint16_t sessionId = 0;
    BOOL hasProtocolId = FALSE;
    BOOL hasSessionId = FALSE;

    while (remaining > 0) {
        ctrlSessParameter param;
        int consumed = ctrlSess_GetNextParameter(paramPtr, remaining, &param);

        if (consumed <= 0) {
            break;
        }

        switch (param.id) {
            case kiAP2EapParamProtocolId:
                protocolId = ctrlSess_ParamGetUint8(&param);
                hasProtocolId = TRUE;
                break;

            case kiAP2EapParamSessionId:
                sessionId = ctrlSess_ParamGetUint16(&param);
                hasSessionId = TRUE;
                break;

            default:
                iAP2LogDbg("[CtrlMsg] Unknown parameter ID: %u", param.id);
                break;
        }

        paramPtr += consumed;
        remaining -= consumed;
    }

    if (!hasProtocolId || !hasSessionId) {
        iAP2LogError("[CtrlMsg] Missing required parameters in EAPStartSession");
        return -1;
    }

    iAP2LogDbg("[CtrlMsg] EAPStartSession: protocolId=%u, sessionId=%u",
               protocolId, sessionId);
    iAP2HandleEAPStartSessionInternal(sessionId, protocolId);
    return 0;
}

int iAP2HandleEAPStopSession(uint16_t msgId, const uint8_t *data, uint32_t len)
{
    if (!data || len == 0) {
        iAP2LogError("[CtrlMsg] Invalid parameters for EAPStopSession");
        return -1;
    }

    if (msgId != kiAP2EapMsgStopSession) {
        iAP2LogError("[CtrlMsg] Mismatched message ID: expected 0x%04X, got 0x%04X",
                     kiAP2EapMsgStopSession, msgId);
        return -1;
    }

    /* 解析消息 */
    ctrlSessMessage msg;

    if (ctrlSess_DecodeMessageHeader(data, len, &msg) != 0) {
        iAP2LogError("[CtrlMsg] Failed to decode EAPStopSession message");
        return -1;
    }

    if (msg.msgId != kiAP2EapMsgStopSession) {
        iAP2LogError("[CtrlMsg] Invalid message ID: expected 0x%04X, got 0x%04X",
                     kiAP2EapMsgStopSession, msg.msgId);
        return -1;
    }

    /* 解析参数 */
    const uint8_t *paramPtr = msg.paramStart;
    size_t remaining = msg.totalLength - 6;
    uint16_t sessionId = 0;
    BOOL hasSessionId = FALSE;

    while (remaining > 0) {
        ctrlSessParameter param;
        int consumed = ctrlSess_GetNextParameter(paramPtr, remaining, &param);

        if (consumed <= 0) {
            break;
        }

        switch (param.id) {
            case kiAP2EapParamSessionId:
                sessionId = ctrlSess_ParamGetUint16(&param);
                hasSessionId = TRUE;
                break;

            default:
                iAP2LogDbg("[CtrlMsg] Unknown parameter ID: %u", param.id);
                break;
        }

        paramPtr += consumed;
        remaining -= consumed;
    }

    if (!hasSessionId) {
        iAP2LogError("[CtrlMsg] Missing required parameters in EAPStopSession");
        return -1;
    }

    iAP2LogDbg("[CtrlMsg] EAPStopSession: sessionId=%u", sessionId);
    // 1. 验证会话ID是否有效
    // 2. 清理会话状态，释放资源等
    return 0;
}
