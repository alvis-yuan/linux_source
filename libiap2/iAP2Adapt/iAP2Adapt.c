/*
 * File: iAP2Adapt.c
 * Package: iAP2Adapt
 * Abstract: iAP2 适配层
 *
 * ============================================================
 * 适配层只需关注 3 件事：
 * ============================================================
 * 1. usb/bt连接/断开 → 通知iAP2
 * 2. usb/bt接收数据 → 交给iAP2
 * 3. iAP2发送数据 → 通过蓝牙/USB发送
 *
 * ============================================================
 * 以下由iAP2内部自动处理（传输层无需关心）：
 * ============================================================
 * - Link层协商（SYN/ACK/DETECT）
 * - 认证流程（Certificate/Challenge/Response）
 * - 识别流程（StartIdentification/IdentificationInfo）
 * - 会话管理（Control/EA/Buffer sessions）
 * - 数据重传、流控、分包重组
 *
 * ============================================================
 * 线程模型（完全自动，无需管理）：
 * ============================================================
 *
 * 主线程（Transport线程）
 *   ├─ 初始化iAP2
 *   ├─ 通知连接/断开
 *   ├─ 传递接收数据
 *   └─ 发送数据回调
 *
 * RunLoop线程（Transport内部，Connect时自动启动）
 *   ├─ 处理Link层协议
 *   ├─ 接收数据 → 回调Session
 *   ├─ Session处理认证和识别
 *   ├─ 发送队列中的数据
 *   └─ 处理超时和重传
 *
 * CP线程（CPTask内部，按需启动）
 *   ├─ 等待CP任务请求
 *   ├─ 执行I2C操作（读证书、签名）
 *   └─ 完成后回调 → 自动发送消息
 *
 * ============================================================
 * 使用方式：
 * ============================================================
 * 1. 连接时：iAP2AdaptOnConnected(conn_id, bd_addr)
 * 2. 收数据：iAP2AdaptDataHandler(conn_id, data, len)
 * 3. 断开时：iAP2AdaptOnDisconnected(conn_id)
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <stdbool.h>
#include "global_var.h"

/* iAP2核心模块 */
#include "iAP2Transport.h"
#include "iAP2Session.h"
#include "iAP2Identification.h"
#include "iAP2Log.h"

/*
 ****************************************************************
 * 全局变量
 ****************************************************************
 */

/* iAP2核心实例 */
static iAP2Transport_t *g_transport = NULL;
static iAP2Session_t   *g_session = NULL;

/* RunLoop线程 */
static pthread_t g_runLoopThread = 0;


/*
 ****************************************************************
 * 会话层回调 - 接收业务数据（iAP2 → 业务层）
 ****************************************************************
 */

/*
 * 会话数据回调
 *
 * iAP2内部会自动处理：
 * - Control数据：认证消息、识别消息等（内部消化，不上报）
 * - EA数据：业务数据（通过此回调上报给业务层）
 * - Buffer数据：图片、健身数据等（如需要可上报）
 *
 * BSA层只需关心EA数据，转发给业务层即可
 */
static BOOL _OnEASessionData(uint8_t sessionID,
                             iAP2SessionType_t sessionType,
                             const uint8_t *data,
                             uint32_t dataLen,
                             void *context)
{
    (void)context;
    (void)sessionID;

    /* 只处理EA会话数据，转发给业务层 */
    if (sessionType == kIAP2SessionTypeEA) {
        /* 转发给业务层（打印通道） */
        //print_channel_send_data(PRINT_CHN_ID_IAP2, data, dataLen, 0);
        return TRUE;
    }

    /* Control和Buffer数据由Session层内部处理，无需上报 */
    return TRUE;
}

/*
 * 认证完成回调（可选）
 *
 * 认证成功后，iAP2会自动进入识别流程
 */
static void _OnAuthComplete(BOOL success, void *context)
{
    (void)context;

    if (success) {
        iAP2LogDbg("[iAP2] ✓ Authentication completed");

    } else {
        iAP2LogError("[iAP2] ✗ Authentication failed");
    }
}

/*
 * 识别完成回调（可选）
 *
 * 识别成功后，iAP2进入就绪状态，可以收发EA数据
 */
static void _OnIdentifyComplete(BOOL success, void *context)
{
    (void)context;

    if (success) {
        iAP2LogDbg("[iAP2] ✓ Identification completed - Ready for data");

    } else {
        iAP2LogError("[iAP2] ✗ Identification failed");
    }
}

/*
 ****************************************************************
 * RunLoop线程 - 处理iAP2内部任务（BSA层无需关心）
 ****************************************************************
 */

/*
 * RunLoop线程
 *
 * 周期性处理iAP2内部状态机：
 * - Transport层：Link协商、数据重传、超时检测
 * - Session层：认证流程、识别流程、会话管理
 *
 * 适配层只需启动这个线程，无需关心内部细节
 */
static void *_RunLoopThread(void *arg)
{
    (void)arg;
    pthread_detach(pthread_self());
    iAP2LogDbg("[iAP2] RunLoop started");
    iAP2TransportProcess(g_transport);
    iAP2LogDbg("[iAP2] RunLoop stopped");
    return NULL;
}


/*
 * 初始化iAP2
 *
 * 调用时机：程序启动时
 *
 * 内部会自动：
 * 1. 打开MFi芯片
 * 2. 创建Transport层和Session层
 *
 * 注意：不需要创建额外的线程
 * - Transport内部的RunLoop线程会在Connect时自动启动
 * - Session层完全事件驱动，不需要Process循环
 *
 * 适配层只需调用一次，无需关心内部细节
 */
static int iAP2AdaptInit(int type, iAP2SessionDataCB_t sessionDataCB,
                         iAP2TransportSendDataCB_t sendDataCB, void *context)
{
    if (g_transport || g_session) {
        iAP2LogDbg("[iAP2] Already initialized");
        return 0;
    }

    iAP2LogEnable(kiAP2LogTypeLog);
    iAP2LogEnable(kiAP2LogTypeLogDbg);
    //iAP2LogEnable(kiAP2LogTypeData);
    iAP2LogDbg("[iAP2] Initializing...");
    /* 1. 创建传输层 */
    iAP2TransportConfig_t transportConfig;
    memset(&transportConfig, 0, sizeof(transportConfig));
    transportConfig.type = type;
    transportConfig.sendDataCB = sendDataCB;
    transportConfig.userContext = context;
    transportConfig.maxPacketSize = 128;
    transportConfig.maxOutstanding = 1;
    transportConfig.retransmitTimeout = 1000;
    g_transport = iAP2TransportCreate(&transportConfig);

    if (!g_transport) {
        iAP2LogError("[iAP2] Failed to create transport");
        return -1;
    }

    /* 2. 创建会话层 */
    iAP2SessionConfig_t sessionConfig;
    memset(&sessionConfig, 0, sizeof(sessionConfig));
    sessionConfig.transport = g_transport;
    sessionConfig.sessionDataCB = sessionDataCB;
    sessionConfig.context = context;
    sessionConfig.authCompleteCB = _OnAuthComplete;
    sessionConfig.identifyCompleteCB = _OnIdentifyComplete;
    g_session = iAP2SessionCreate(&sessionConfig, type);

    if (!g_session) {
        iAP2LogError("[iAP2] Failed to create session");
        iAP2TransportDestroy(g_transport);
        g_transport = NULL;
        return -1;
    }

    iAP2IdSetAccessoryInfo("sunmiPrinter", sys_global_var()->model,
                           "sunmi", sys_global_var()->sn, APP0_VERSION,
                           sys_global_var()->hw_ver);

    /* 启动会话（准备就绪，等待连接） */
    if (!iAP2SessionStart(g_session)) {
        iAP2LogError("[iAP2] Failed to start session");
        iAP2SessionDestroy(g_session);
        g_session = NULL;
        iAP2TransportDestroy(g_transport);
        g_transport = NULL;
        return -1;
    }

    /* 3. 启动RunLoop线程
     *
     * 设计说明：
     * - RunLoop 线程在 Init 时创建，而不是在 Connect 时
     * - 线程会进入等待状态，直到收到 Attach 事件
     * - 这样可以避免竞争条件，确保线程在连接前就绪
     */
    if (pthread_create(&g_runLoopThread, NULL, _RunLoopThread, NULL) != 0) {
        iAP2LogError("[iAP2] Failed to create RunLoop thread");
        iAP2SessionDestroy(g_session);
        g_session = NULL;
        iAP2TransportDestroy(g_transport);
        g_transport = NULL;
        return -1;
    }

    /* 等待线程启动 */
    usleep(50000);  /* 50ms */
    iAP2LogDbg("[iAP2] Initialized successfully");
    iAP2LogDbg("[iAP2] Waiting for Bluetooth connection...");
    return 0;
}

/*
 * 清理iAP2
 *
 * 调用时机：程序退出时
 *
 * 内部会自动：
 * 1. 停止RunLoop线程
 * 2. 销毁Session层和Transport层
 * 3. 关闭MFi芯片
 *
 * 适配层只需调用一次，无需关心内部细节
 */
static void iAP2AdaptCleanup(void)
{
    iAP2LogDbg("[iAP2] Cleaning up...");

    /* 销毁会话层 */
    if (g_session) {
        iAP2SessionDestroy(g_session);
        g_session = NULL;
    }

    /* 销毁传输层 */
    if (g_transport) {
        iAP2TransportDestroy(g_transport);
        g_transport = NULL;
        g_runLoopThread = 0;
    }

    iAP2LogDbg("[iAP2] Cleaned up");
}

/*
 * bt/usb连接事件 - 已连接
 *
 * 参数：
 *
 * 内部会自动触发：
 * 1. Link层协商（发送SYN包）
 * 2. 认证流程（等待设备请求证书）
 * 3. 识别流程（发送配件信息）
 * 4. 进入就绪状态
 *
 * 适配层只需调用此函数通知连接，无需关心后续流程
 */
int iAP2AdaptOnConnected(int type, iAP2SessionDataCB_t sessionDataCB,
                         iAP2TransportSendDataCB_t sendDataCB, void *context)
{
    if (sendDataCB == NULL) {
        iAP2LogError("[iAP2] sendDataCB is NULL");
        return -1;
    }

    if (sessionDataCB == NULL) {
        iAP2LogError("[iAP2] sessionDataCB is NULL");
        return -1;
    }

    if (iAP2AdaptInit(type, sessionDataCB, sendDataCB, context) < 0) {
        iAP2LogError("[iAP2] Failed to initialize BSA");
        return -1;
    }

    if (!g_transport) {
        iAP2LogError("[iAP2] Not initialized");
        return -1;
    }

    /* 通知iAP2传输层：物理连接已建立
     *
     * 这会触发 Link 层的 Attach 事件，RunLoop 线程会开始处理
     */
    if (!iAP2TransportConnect(g_transport)) {
        iAP2LogError("[iAP2] Failed to connect transport");
        return -1;
    }

    return 0;
    /*
     * ============================================================
     * 后续流程由iAP2内部自动处理，BSA层无需关心：
     * ============================================================
     *
     * 【RunLoop线程】（Transport内部，自动运行）
     *   - 处理Link层协议（SYN/ACK/重传/超时）
     *   - 接收数据 → 回调Session
     *   - Session处理认证和识别
     *   - 发送队列中的数据
     *
     * 【CP线程】（CPTask内部，按需启动）
     *   - 等待CP任务请求
     *   - 执行I2C操作（读证书、签名）
     *   - 完成后回调 → 自动发送消息
     *
     * 1. Link层协商（RunLoop线程）
     *    - 发送SYN包（包含会话信息、参数）
     *    - 接收SYNACK包
     *    - 发送ACK包
     *    - Link连接建立
     *
     * 2. 认证流程（RunLoop线程 + CP线程）
     *    - 接收RequestAuthenticationCertificate
     *    - 启动CP线程读取证书（异步）
     *    - CP完成后自动发送AuthenticationCertificate
     *    - 接收RequestAuthenticationChallengeResponse
     *    - 启动CP线程签名挑战（异步）
     *    - CP完成后自动发送AuthenticationResponse
     *    - 接收AuthenticationSucceeded
     *    - 认证完成（调用_OnAuthComplete）
     *
     * 3. 识别流程（RunLoop线程）
     *    - 接收StartIdentification
     *    - 发送IdentificationInformation（配件信息）
     *    - 接收IdentificationAccepted
     *    - 识别完成（调用_OnIdentifyComplete）
     *
     * 4. 就绪状态
     *    - 可以收发EA数据
     *    - EA数据通过_OnSessionData回调上报
     */
}

/*
 * 传输层断开事件
 *
 * 调用时机：BSA DG连接断开时
 *
 * 参数：
 *
 * 内部会自动：
 * 1. 重置Link层状态
 * 2. 重置Session层状态
 * 3. 重置认证/识别状态
 *
 * 适配层只需调用此函数通知断开，无需关心后续清理
 */
void iAP2AdaptOnDisconnected(void)
{
    if (!g_transport) {
        return;
    }

    /* 通知iAP2传输层：物理连接已断开 */
    iAP2TransportDisconnect(g_transport);
    iAP2AdaptCleanup();
    /*
     * ============================================================
     * 后续清理由iAP2内部自动处理，适配层无需关心：
     * ============================================================
     *
     * 1. Link层重置
     *    - 清空发送队列
     *    - 清空接收缓冲
     *    - 重置序列号
     *
     * 2. Session层重置
     *    - 重置会话状态
     *    - 清空会话数据
     *
     * 3. 认证/识别状态重置
     *    - 重置认证状态为Idle
     *    - 重置识别状态为Idle
     *    - 下次连接时重新认证和识别
     */
}

/*
 * usb/bt数据接收 - 收到数据
 *
 * 调用时机：usb/bt收到数据时
 *
 * 参数：
 *   data: 接收到的数据
 *   len: 数据长度
 *
 * 返回：
 *   处理的字节数
 *
 * 内部会自动：
 * 1. Link层解包、重组
 * 2. 会话层分发（Control/EA/Buffer）
 * 3. Control数据：认证/识别消息处理
 * 4. EA数据：通过_OnSessionData回调上报
 *
 * 适配层只需调用此函数传递数据，无需关心数据内容
 */
int iAP2AdaptDataHandler(unsigned char *data, unsigned int len)
{
    iAP2LogDbg("[iAP2] DataHandler called: len=%u", len);

    if (!g_transport) {
        iAP2LogError("[iAP2] Not initialized");
        return -1;
    }

    iAP2LogDbg("[iAP2] Received %u bytes", len);
    fprintf(stderr, "===========iAP2 Rx Raw Data: =============\n");

    for (int i = 0; i < len; i++) {
        fprintf(stderr, "%02X ", data[i]);

        if ((i + 1) % 16 == 0) {
            fprintf(stderr, "\n");
        }
    }

    fprintf(stderr, "\n");
    /* 将数据交给iAP2传输层处理 */
    uint32_t processed = iAP2TransportReceiveData(g_transport, data, len);
    /*
     * ============================================================
     * 后续处理由iAP2内部自动完成，BSA层无需关心：
     * ============================================================
     *
     * 1. Link层处理
     *    - 解析包头（SOF/Length/Control/Seq/Ack/SessionID/Checksum）
     *    - 验证校验和
     *    - 处理ACK/RESET/SUSPEND等控制包
     *    - 重组分片数据
     *    - 发送ACK确认
     *
     * 2. 会话层分发
     *    - 根据SessionID分发到不同会话
     *    - Control Session (0x0A): 认证/识别消息
     *    - EA Session: 业务数据
     *    - Buffer Session: 图片/健身数据
     *
     * 3. Control消息处理（内部）
     *    - 认证消息：RequestAuthCert/RequestChallenge等
     *    - 识别消息：StartIdentification等
     *    - 自动响应，无需BSA层参与
     *
     * 4. EA数据上报（外部）
     *    - 通过_OnSessionData回调
     *    - 转发给业务层（print_channel_send_data）
     */
    return (int)processed;
}
