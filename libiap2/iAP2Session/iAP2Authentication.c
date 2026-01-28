/*
 * File: iAP2Authentication.c
 * Package: iAP2Session
 * Abstract: iAP2认证模块实现
 *
 * 使用ControlCodec进行消息编解码
 * 使用iAP2CPTask进行异步CP操作
 *
 * 认证流程:
 * 1. 收到RequestAuthenticationCertificate -> 读取证书 -> 发送证书
 * 2. 收到RequestAuthenticationChallengeResponse -> 签名挑战 -> 发送响应
 * 3. 收到AuthenticationSucceeded/Failed -> 通知结果
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "iAP2Authentication.h"
#include "iAP2ControlMessage.h"
#include "iAP2ControlCodec.h"
#include "iAP2CPTask.h"
#include "../driver/mfiI2c.h"
#include <iAP2Log.h>

/*
 ****************************************************************
 * 模块状态
 ****************************************************************
 */

static struct {
    iAP2AuthConfig_t    config;
    iAP2AuthState_t     state;
    BOOL                initialized;
    BOOL                requestSerialNumber;
    uint8_t             controlSessionVersion;  /* 控制会话版本 */

    /* 消息缓冲区 */
    uint8_t             msgBuffer[1024];

    /* 挑战数据（需要保存用于CP操作） */
    uint8_t             challengeBuffer[64];
    uint32_t            challengeLen;
} g_authContext;

/*
 ****************************************************************
 * 内部函数 - 消息发送
 ****************************************************************
 */

/*
 * 发送AuthenticationCertificate消息 (0xAA01)
 */
static BOOL _SendAuthCertificate(const uint8_t *certData, uint16_t certLen)
{
    if (!certData || certLen == 0) {
        iAP2LogError("[Auth] No certificate data\n");
        return FALSE;
    }

    iAP2LogDbg("[Auth] Sending certificate: %u bytes\n", certLen);
    /* 使用ControlCodec构建消息 */
    ctrlSessBuilder builder;
    ctrlSess_BuilderInit(&builder, g_authContext.msgBuffer,
                         sizeof(g_authContext.msgBuffer),
                         kiAP2AuthMsgAuthCert);

    if (ctrlSess_AddBlob(&builder, kiAP2AuthParamCertificateData,
                         certData, certLen) != 0) {
        iAP2LogError("[Auth] Failed to add certificate parameter\n");
        return FALSE;
    }

    uint16_t msgLen = ctrlSess_BuilderFinish(&builder);

    if (g_authContext.config.sendMsgCallback) {
        return g_authContext.config.sendMsgCallback(g_authContext.msgBuffer, msgLen,
                g_authContext.config.context);
    }

    return FALSE;
}

/*
 * 发送AccessoryAuthenticationSerialNumber消息 (0xAA06)
 * 注意：此函数现在由CP回调调用，序列号数据由CP任务提供
 */
static BOOL _SendAuthSerialNumber(const uint8_t *serialNumber,
                                  uint16_t serialLen)
{
    if (!serialNumber || serialLen != 32) {
        iAP2LogError("[Auth] Invalid serial number data\n");
        return FALSE;
    }

    iAP2LogDbg("[Auth] Sending certificate serial number\n");
    ctrlSessBuilder builder;
    ctrlSess_BuilderInit(&builder, g_authContext.msgBuffer,
                         sizeof(g_authContext.msgBuffer),
                         kiAP2AuthMsgAuthSerial);

    if (ctrlSess_AddBlob(&builder, kiAP2AuthParamCertSerialNumber,
                         serialNumber, serialLen) != 0) {
        return FALSE;
    }

    uint16_t msgLen = ctrlSess_BuilderFinish(&builder);

    if (g_authContext.config.sendMsgCallback) {
        return g_authContext.config.sendMsgCallback(g_authContext.msgBuffer, msgLen,
                g_authContext.config.context);
    }

    return FALSE;
}

/*
 * 发送AuthenticationResponse消息 (0xAA03)
 */
static BOOL _SendAuthResponse(const uint8_t *responseData, uint16_t responseLen)
{
    if (!responseData || responseLen == 0) {
        iAP2LogError("[Auth] No response data\n");
        return FALSE;
    }

    iAP2LogDbg("[Auth] Sending response: %u bytes\n", responseLen);
    ctrlSessBuilder builder;
    ctrlSess_BuilderInit(&builder, g_authContext.msgBuffer,
                         sizeof(g_authContext.msgBuffer),
                         kiAP2AuthMsgAuthResponse);

    if (ctrlSess_AddBlob(&builder, kiAP2AuthParamChallengeResponse,
                         responseData, responseLen) != 0) {
        iAP2LogError("[Auth] Failed to add response parameter\n");
        return FALSE;
    }

    uint16_t msgLen = ctrlSess_BuilderFinish(&builder);

    if (g_authContext.config.sendMsgCallback) {
        return g_authContext.config.sendMsgCallback(g_authContext.msgBuffer, msgLen,
                g_authContext.config.context);
    }

    return FALSE;
}

/*
 ****************************************************************
 * 内部函数 - CP操作回调（事件驱动，不需要轮询）
 ****************************************************************
 */

/*
 * CP操作完成回调
 * 注意：此回调可能在CP线程中调用，但iAP2TransportSendData是线程安全的
 */
static void _CPResultCallback(iAP2CPMsgType_t type,
                              const uint8_t *data,
                              uint16_t dataLen,
                              BOOL success,
                              void *context)
{
    (void)context;

    switch (type) {
        case kCPMsgReadSerial:
            iAP2LogDbg("[Auth] CP: Serial number read %s (%u bytes)\n",
                       success ? "OK" : "FAILED", dataLen);

            if (success && data && dataLen == 32) {
                /* 发送序列号 */
                if (_SendAuthSerialNumber(data, dataLen)) {
                    iAP2LogDbg("[Auth] Serial number sent, now reading certificate\n");

                    /* 序列号发送成功，继续读取证书 */
                    if (!iAP2CPTaskRequestReadCert()) {
                        iAP2LogError("[Auth] Failed to request read cert after serial\n");
                        g_authContext.state = kIAP2AuthStateFailed;

                        if (g_authContext.config.resultCallback) {
                            g_authContext.config.resultCallback(FALSE, g_authContext.config.context);
                        }
                    }

                } else {
                    iAP2LogError("[Auth] Failed to send serial number\n");
                    g_authContext.state = kIAP2AuthStateFailed;

                    if (g_authContext.config.resultCallback) {
                        g_authContext.config.resultCallback(FALSE, g_authContext.config.context);
                    }
                }

            } else {
                iAP2LogError("[Auth] Serial number read failed\n");
                g_authContext.state = kIAP2AuthStateFailed;

                if (g_authContext.config.resultCallback) {
                    g_authContext.config.resultCallback(FALSE, g_authContext.config.context);
                }
            }

            break;

        case kCPMsgReadCert:
            iAP2LogDbg("[Auth] CP: Certificate read %s (%u bytes)\n",
                       success ? "OK" : "FAILED", dataLen);

            if (success && data && dataLen > 0) {
                /* 直接发送证书，不需要保存和轮询 */
                if (_SendAuthCertificate(data, dataLen)) {
                    g_authContext.state = kIAP2AuthStateWaitChallenge;
                    iAP2LogDbg("[Auth] State -> WaitChallenge\n");

                } else {
                    g_authContext.state = kIAP2AuthStateFailed;

                    if (g_authContext.config.resultCallback) {
                        g_authContext.config.resultCallback(FALSE, g_authContext.config.context);
                    }
                }

            } else {
                iAP2LogError("[Auth] Certificate read failed\n");
                g_authContext.state = kIAP2AuthStateFailed;

                if (g_authContext.config.resultCallback) {
                    g_authContext.config.resultCallback(FALSE, g_authContext.config.context);
                }
            }

            break;

        case kCPMsgSignChallenge:
            iAP2LogDbg("[Auth] CP: Challenge sign %s (%u bytes)\n",
                       success ? "OK" : "FAILED", dataLen);

            if (success && data && dataLen > 0) {
                /* 直接发送响应，不需要保存和轮询 */
                if (_SendAuthResponse(data, dataLen)) {
                    g_authContext.state = kIAP2AuthStateWaitResult;
                    iAP2LogDbg("[Auth] State -> WaitResult\n");

                } else {
                    g_authContext.state = kIAP2AuthStateFailed;

                    if (g_authContext.config.resultCallback) {
                        g_authContext.config.resultCallback(FALSE, g_authContext.config.context);
                    }
                }

            } else {
                iAP2LogError("[Auth] Challenge signing failed\n");
                g_authContext.state = kIAP2AuthStateFailed;

                if (g_authContext.config.resultCallback) {
                    g_authContext.config.resultCallback(FALSE, g_authContext.config.context);
                }
            }

            break;

        default:
            break;
    }
}

/*
 ****************************************************************
 * 内部函数 - 消息处理
 ****************************************************************
 */

/*
 * 处理RequestAuthenticationCertificate消息 (0xAA00)
 */
static BOOL _HandleReqAuthCert(const uint8_t *data, uint32_t len)
{
    iAP2LogDbg("[Auth] Received RequestAuthenticationCertificate\n");
    /* 使用ControlCodec解析消息 */
    ctrlSessMessage msg;

    if (ctrlSess_DecodeMessageHeader(data, len, &msg) != 0) {
        iAP2LogError("[Auth] Failed to decode message header\n");
        return FALSE;
    }

    /* 检查是否请求序列号 */
    BOOL deviceRequestsSerialNumber = FALSE;
    const uint8_t *paramPtr = msg.paramStart;
    size_t remaining = msg.totalLength - 6;

    while (remaining > 0) {
        ctrlSessParameter param;
        int consumed = ctrlSess_GetNextParameter(paramPtr, remaining, &param);

        if (consumed <= 0) break;

        if (param.id == kiAP2AuthParamReqCertSerialNumber) {
            deviceRequestsSerialNumber = TRUE;
            iAP2LogDbg("[Auth] Device requests certificate serial number\n");
        }

        paramPtr += consumed;
        remaining -= consumed;
    }

    /*
     * 根据控制会话版本决定是否发送序列号：
     * - 版本1：忽略序列号请求，只发送证书
     * - 版本2：如果device请求了序列号，则先发送序列号，再发送证书
     */
    uint8_t ctrlSessionVersion = g_authContext.controlSessionVersion;

    if (deviceRequestsSerialNumber && ctrlSessionVersion >= 2) {
        /* 版本2及以上：需要先读取并发送序列号 */
        g_authContext.requestSerialNumber = TRUE;
        iAP2LogDbg("[Auth] Control session v%d: Will send serial number first\n",
                   ctrlSessionVersion);
        /* 启动CP任务读取序列号（异步，完成后会自动读取证书） */
        g_authContext.state = kIAP2AuthStateReadingCert;

        if (!iAP2CPTaskRequestReadSerial()) {
            iAP2LogError("[Auth] Failed to request read serial\n");
            g_authContext.state = kIAP2AuthStateFailed;
            return FALSE;
        }

        iAP2LogDbg("[Auth] CP task started: ReadingSerial (async)\n");

    } else {
        /* 版本1或未请求：直接读取证书 */
        g_authContext.requestSerialNumber = FALSE;

        if (deviceRequestsSerialNumber) {
            iAP2LogDbg("[Auth] Control session v%d: Ignoring serial number request\n",
                       ctrlSessionVersion);
        }

        /* 启动CP任务读取证书（异步，完成后通过回调发送） */
        g_authContext.state = kIAP2AuthStateReadingCert;

        if (!iAP2CPTaskRequestReadCert()) {
            iAP2LogError("[Auth] Failed to request read cert\n");
            g_authContext.state = kIAP2AuthStateFailed;
            return FALSE;
        }

        iAP2LogDbg("[Auth] CP task started: ReadingCert (async)\n");
    }

    return TRUE;
}

/*
 * 处理RequestAuthenticationChallengeResponse消息 (0xAA02)
 */
static BOOL _HandleReqAuthChallenge(const uint8_t *data, uint32_t len)
{
    iAP2LogDbg("[Auth] Received RequestAuthenticationChallengeResponse\n");
    /* 使用ControlCodec解析消息 */
    ctrlSessMessage msg;

    if (ctrlSess_DecodeMessageHeader(data, len, &msg) != 0) {
        iAP2LogError("[Auth] Failed to decode message header\n");
        return FALSE;
    }

    /* 提取挑战数据 */
    const uint8_t *challengeData = NULL;
    size_t challengeLen = 0;
    const uint8_t *paramPtr = msg.paramStart;
    size_t remaining = msg.totalLength - 6;

    while (remaining > 0) {
        ctrlSessParameter param;
        int consumed = ctrlSess_GetNextParameter(paramPtr, remaining, &param);

        if (consumed <= 0) break;

        if (param.id == kiAP2AuthParamChallengeData) {
            challengeData = ctrlSess_ParamGetBlob(&param, &challengeLen);
        }

        paramPtr += consumed;
        remaining -= consumed;
    }

    if (!challengeData || challengeLen == 0) {
        iAP2LogError("[Auth] No challenge data in message\n");
        g_authContext.state = kIAP2AuthStateFailed;
        return FALSE;
    }

    /* 保存挑战数据 */
    if (challengeLen > sizeof(g_authContext.challengeBuffer)) {
        challengeLen = sizeof(g_authContext.challengeBuffer);
    }

    fprintf(stderr, "Challenge Data: \n");

    for (size_t i = 0; i < challengeLen; i++) {
        fprintf(stderr, "%02X", challengeData[i]);
    }

    fprintf(stderr, "\n");
    memcpy(g_authContext.challengeBuffer, challengeData, challengeLen);
    g_authContext.challengeLen = (uint32_t)challengeLen;
    iAP2LogDbg("[Auth] Challenge data: %u bytes\n", g_authContext.challengeLen);
    /* 启动CP任务签名挑战（异步，完成后通过回调发送） */
    g_authContext.state = kIAP2AuthStateSigningChallenge;

    if (!iAP2CPTaskRequestSignChallenge(g_authContext.challengeBuffer,
                                        g_authContext.challengeLen)) {
        iAP2LogError("[Auth] Failed to request sign challenge\n");
        g_authContext.state = kIAP2AuthStateFailed;
        return FALSE;
    }

    iAP2LogDbg("[Auth] CP task started: SigningChallenge (async)\n");
    return TRUE;
}

/*
 * 处理AuthenticationSucceeded消息 (0xAA05)
 */
static BOOL _HandleAuthSucceeded(const uint8_t *data, uint32_t len)
{
    (void)data;
    (void)len;
    iAP2LogDbg("[Auth] ✓ Authentication SUCCEEDED\n");
    g_authContext.state = kIAP2AuthStateAuthenticated;

    if (g_authContext.config.resultCallback) {
        g_authContext.config.resultCallback(TRUE, g_authContext.config.context);
    }

    return TRUE;
}

/*
 * 处理AuthenticationFailed消息 (0xAA04)
 */
static BOOL _HandleAuthFailed(const uint8_t *data, uint32_t len)
{
    (void)data;
    (void)len;
    iAP2LogError("[Auth] ✗ Authentication FAILED\n");
    g_authContext.state = kIAP2AuthStateFailed;

    if (g_authContext.config.resultCallback) {
        g_authContext.config.resultCallback(FALSE, g_authContext.config.context);
    }

    return TRUE;
}

/*
 ****************************************************************
 * API实现
 ****************************************************************
 */

/*
 * 初始化认证模块
 */
int iAP2AuthInit(const iAP2AuthConfig_t *config)
{
    if (!config || !config->sendMsgCallback) {
        iAP2LogError("[Auth] Invalid config\n");
        return -1;
    }

    if (g_authContext.initialized) {
        iAP2LogError("[Auth] Already initialized\n");
        return -1;
    }

    memset(&g_authContext, 0, sizeof(g_authContext));
    memcpy(&g_authContext.config, config, sizeof(iAP2AuthConfig_t));
    g_authContext.state = kIAP2AuthStateIdle;
    g_authContext.controlSessionVersion = config->controlSessionVersion;
    /* 初始化CP任务 */
    iAP2CPTaskConfig_t cpConfig = {
        .resultCallback = _CPResultCallback,
        .context = NULL,
        .useThread = config->useAsyncCP,  /* 根据配置选择模式 */
        .threadPriority = 5
    };

    if (iAP2CPTaskInit(&cpConfig) < 0) {
        iAP2LogError("[Auth] Failed to init CP task\n");
        return -1;
    }

    g_authContext.initialized = TRUE;
    iAP2LogDbg("[Auth] Authentication module initialized (async=%d, ctrlVersion=%d)\n",
               config->useAsyncCP, g_authContext.controlSessionVersion);
    return 0;
}

/*
 * 反初始化认证模块
 */
void iAP2AuthDeinit(void)
{
    if (!g_authContext.initialized) {
        return;
    }

    /* 反初始化CP任务 */
    iAP2CPTaskDeinit();
    memset(&g_authContext, 0, sizeof(g_authContext));
    iAP2LogDbg("[Auth] Authentication module deinitialized\n");
}

/*
 * 重置认证模块状态
 */
void iAP2AuthReset(void)
{
    if (!g_authContext.initialized) {
        return;
    }

    g_authContext.state = kIAP2AuthStateIdle;
    g_authContext.challengeLen = 0;
    iAP2LogDbg("[Auth] Authentication module reset\n");
}

/*
 * 获取当前认证状态
 */
iAP2AuthState_t iAP2AuthGetState(void)
{
    return g_authContext.state;
}

/*
 * Link连接通知
 */
void iAP2AuthOnLinkConnected(void)
{
    if (!g_authContext.initialized) {
        return;
    }

    g_authContext.state = kIAP2AuthStateWaitCertRequest;
    iAP2LogDbg("[Auth] State -> WaitCertRequest\n");
}

/*
 * 设置控制会话版本
 */
void iAP2AuthSetControlSessionVersion(uint8_t version)
{
    if (!g_authContext.initialized) {
        return;
    }

    g_authContext.controlSessionVersion = version;
    iAP2LogDbg("[Auth] Control session version set to %d\n", version);
}

/*
 * 处理认证相关的控制消息
 */
BOOL iAP2AuthHandleMessage(uint16_t msgId, const uint8_t *data, uint32_t len)
{
    if (!g_authContext.initialized || !data || len == 0) {
        return FALSE;
    }

    switch (msgId) {
        case kiAP2AuthMsgReqAuthCert:
            if (g_authContext.state == kIAP2AuthStateWaitCertRequest) {
                return _HandleReqAuthCert(data, len);
            }

            break;

        case kiAP2AuthMsgReqAuthChallenge:
            if (g_authContext.state == kIAP2AuthStateWaitChallenge) {
                return _HandleReqAuthChallenge(data, len);
            }

            break;

        case kiAP2AuthMsgAuthSucceeded:
            if (g_authContext.state == kIAP2AuthStateWaitResult) {
                return _HandleAuthSucceeded(data, len);
            }

            break;

        case kiAP2AuthMsgAuthFailed:
            if (g_authContext.state == kIAP2AuthStateWaitResult) {
                return _HandleAuthFailed(data, len);
            }

            break;

        default:
            break;
    }

    return FALSE;
}

/*
 * 检查认证是否完成
 */
BOOL iAP2AuthIsComplete(void)
{
    return (g_authContext.state == kIAP2AuthStateAuthenticated ||
            g_authContext.state == kIAP2AuthStateFailed);
}

/*
 * 检查认证是否成功
 */
BOOL iAP2AuthIsSuccess(void)
{
    return (g_authContext.state == kIAP2AuthStateAuthenticated);
}
