/*
 * File: iAP2Identification.c
 * Package: iAP2Session
 * Abstract: iAP2 Identification Module Implementation
 *
 * 实现iAP2协议的识别流程
 * 流程: Device发送StartIdentification -> Accessory发送IdentificationInformation -> 结果
 */

#include <stdio.h>
#include <string.h>

#include "iAP2Identification.h"
#include "iAP2ControlMessage.h"
#include "iAP2ControlCodec.h"
#include <iAP2Log.h>
#include <iAP2Transport.h>

/* 消息缓冲区 */
#define ID_MSG_BUFFER_SIZE 2048
static uint8_t g_msgBuffer[ID_MSG_BUFFER_SIZE];

/* 模块状态 */
static iAP2IdState_t g_idState = kIAP2IdStateIdle;
static iAP2IdConfig_t g_idConfig;
static BOOL g_initialized = FALSE;

/*
 ****************************************************************
 * 内置配置信息（适配层无需传递）
 ****************************************************************
 */

/* PPID (utf8) : 16字符 */
static const char *g_PPID = "0123456789ABCDEF";
#define g_eaProtocalName "com.sunmi.iap2"

/* 配件基本信息 */
static iAP2AccessoryInfo_t g_accessoryInfo = {
    .name = "CloudPrinter",
    .modelIdentifier = "NT311",
    .manufacturer = "SUNMI",
    .serialNumber = "SN123456789",
    .firmwareVersion = "1.0.0",
    .hardwareVersion = "1.0",
    .currentLanguage = "en",
    .supportedLanguages = "en,zh",
    .MaximumCurrentDrawnFromDevice = 0,
    .PowerProvidingCapability = 0, /* No Power Providing */
};

/* EA协议定义 */
static iAP2EAProtocol_t g_eaProtocols[] = {
    {
        .protocolIdentifier = 1,
        .protocolName = g_eaProtocalName,
        .matchAction = 0  /* NoAction */
    }
};

/* 蓝牙传输信息 */
static iAP2BTTransportInfo_t g_btTransport = {
    .transportIdentifier = 1,
    .transportName = "iAP2Bluetooth",
    .macAddress = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}
};

/* USB Device 传输组件信息 */
static iAP2USBHostTransportInfo_t g_usbTransport = {
    .transportIdentifier = 1,
    .transportName = "iAP2-Printer",
};

/* 消息能力声明 - 配件发送的消息 */
static uint16_t g_messagesSent[] = {
#if 0
    /* Authentication */
    0xAA01,  /* AuthenticationCertificate */
    0xAA03,  /* AuthenticationResponse */
    0xAA06,  /* AccessoryAuthenticationSerialNumber */

    /* Identification */
    0x1D01,  /* IdentificationInformation */
    0x1D06,  /* IdentificationInformationUpdate */

    /* App Launch */
    0xEA02,  /* RequestAppLaunch */
#endif
    /* App Discovery */
    0xAD00,  /* StartAppDiscoveryUpdates */
    0xAD02,  /* StopAppDiscoveryUpdates */

    /* External Accessory Protocol */
    0xEA03   /* EAPSessionStatus */
};

/* 消息能力声明 - 配件接收的消息 */
static uint16_t g_messagesReceived[] = {
#if 0
    /* Authentication */
    0xAA00,  /* RequestAuthenticationCertificate */
    0xAA02,  /* RequestAuthenticationChallengeResponse */
    0xAA04,  /* AuthenticationFailed */
    0xAA05,  /* AuthenticationSucceeded */

    /* Identification */
    0x1D00,  /* StartIdentification */
    0x1D02,  /* IdentificationAccepted */
    0x1D03,  /* IdentificationRejected */
#endif
    /* App Discovery */
    0xAD01,  /* AppDiscoveryUpdate */

    /* External Accessory Protocol */
    0xEA00,  /* EAPStartSession */
    0xEA01   /* EAPStopSession */
};



/*
 ****************************************************************
 * 内部函数
 ****************************************************************
 */

/*
 * 添加EA协议组参数
 */
static int _AddEAProtocolGroup(ctrlSessBuilder *builder,
                               const iAP2EAProtocol_t *protocol)
{
    ctrlSessGroupBuilder groupBuilder;

    if (ctrlSess_BeginGroup(&groupBuilder, builder, kiAP2IdParamExtProtocol) != 0) {
        return -1;
    }

    /* Protocol Identifier (uint8) */
    ctrlSess_AddUint8(builder, kiAP2GrpEapProtocolId, protocol->protocolIdentifier);
    /* Protocol Name (utf8) */
    ctrlSess_AddString(builder, kiAP2GrpEapProtocolName, protocol->protocolName);
    /* Match Action (enum) */
    ctrlSess_AddEnum(builder, kiAP2GrpEapMatchAction, protocol->matchAction);

    if (ctrlSess_EndGroup(&groupBuilder) != 0) {
        return -1;
    }

    return 0;
}

/*
 * 添加蓝牙传输组件组参数
 */
static int _AddBTTransportGroup(ctrlSessBuilder *builder,
                                const iAP2BTTransportInfo_t *btInfo)
{
    ctrlSessGroupBuilder groupBuilder;

    if (ctrlSess_BeginGroup(&groupBuilder, builder, kiAP2IdParamBtTransComp) != 0) {
        return -1;
    }

    /* Transport Identifier (uint16) */
    ctrlSess_AddUint16(builder, kiAP2GrpBtTransId, btInfo->transportIdentifier);

    /* Transport Name (utf8) */
    if (btInfo->transportName[0] != '\0') {
        ctrlSess_AddString(builder, kiAP2GrpBtTransName, btInfo->transportName);

    } else {
        ctrlSess_AddString(builder, kiAP2GrpBtTransName, "iAP2-Bluetooth");
    }

    /* iAP2 Connection (none - presence indicates support) */
    ctrlSess_AddNone(builder, kiAP2GrpBtTransIap2Conn);
    /* MAC Address (blob, 6 bytes) */
    ctrlSess_AddBlob(builder, kiAP2GrpBtTransMacAddr, btInfo->macAddress, 6);

    if (ctrlSess_EndGroup(&groupBuilder) != 0) {
        return -1;
    }

    return 0;
}

/* 添加USB Host传输组件组参数 */
static int _AddUSBHostTransportGroup(ctrlSessBuilder *builder,
                                     const iAP2USBHostTransportInfo_t *usbInfo)
{
    ctrlSessGroupBuilder groupBuilder;

    if (ctrlSess_BeginGroup(&groupBuilder, builder,
                            kiAP2IdParamUsbHostTransComp) != 0) {
        return -1;
    }

    /* Transport Identifier (uint16) */
    ctrlSess_AddUint16(builder, kiAP2GrpTransDevId, usbInfo->transportIdentifier);

    /* Transport Name (utf8) */
    if (usbInfo->transportName[0] != '\0') {
        ctrlSess_AddString(builder, kiAP2GrpTransDevName, usbInfo->transportName);

    } else {
        ctrlSess_AddString(builder, kiAP2GrpTransDevName, "iAP2-USB");
    }

    /* iAP2 Connection (none - presence indicates support) */
    ctrlSess_AddNone(builder, kiAP2GrpTransDevIap2Conn);

    if (ctrlSess_EndGroup(&groupBuilder) != 0) {
        return -1;
    }

    return 0;
}


/*
 * 发送IdentificationInformation消息 (0x1D01)
 */
static BOOL _SendIdentificationInfo(void)
{
    if (!g_idConfig.accessoryInfo) {
        iAP2LogError("[Id] No accessory info configured");
        return FALSE;
    }

    const iAP2AccessoryInfo_t *info = g_idConfig.accessoryInfo;
    iAP2LogDbg("[Id] ========================================");
    iAP2LogDbg("[Id] Sending IdentificationInformation");
    iAP2LogDbg("[Id] ========================================");
    iAP2LogDbg("[Id] Name: %s", info->name);
    iAP2LogDbg("[Id] ModelIdentifier: %s", info->modelIdentifier);
    iAP2LogDbg("[Id] Manufacturer: %s", info->manufacturer);
    iAP2LogDbg("[Id] SerialNumber: %s", info->serialNumber);
    iAP2LogDbg("[Id] FirmwareVersion: %s", info->firmwareVersion);
    iAP2LogDbg("[Id] HardwareVersion: %s", info->hardwareVersion);
    /* 构建IdentificationInformation消息 */
    ctrlSessBuilder builder;
    ctrlSess_BuilderInit(&builder, g_msgBuffer, ID_MSG_BUFFER_SIZE, kiAP2IdMsgInfo);
    /* 必需参数 */
    ctrlSess_AddString(&builder, kiAP2IdParamName, info->name);
    ctrlSess_AddString(&builder, kiAP2IdParamModelId, info->modelIdentifier);
    ctrlSess_AddString(&builder, kiAP2IdParamManufacturer, info->manufacturer);
    ctrlSess_AddString(&builder, kiAP2IdParamSerialNumber, info->serialNumber);
    ctrlSess_AddString(&builder, kiAP2IdParamFwVer, info->firmwareVersion);
    ctrlSess_AddString(&builder, kiAP2IdParamHwVer, info->hardwareVersion);

    /* 消息能力声明 */
    if (g_idConfig.messagesSentByAccessory && g_idConfig.messagesSentCount > 0) {
        iAP2LogDbg("[Id] MessagesSentByAccessory: %u messages",
                   g_idConfig.messagesSentCount);

        for (uint16_t i = 0; i < g_idConfig.messagesSentCount && i < 10; i++) {
            iAP2LogDbg("[Id]   - 0x%04X", g_idConfig.messagesSentByAccessory[i]);
        }

        if (g_idConfig.messagesSentCount > 10) {
            iAP2LogDbg("[Id]   - ... and %u more", g_idConfig.messagesSentCount - 10);
        }

        ctrlSess_AddUint16Array(&builder, kiAP2IdParamMsgSent,
                                g_idConfig.messagesSentByAccessory,
                                g_idConfig.messagesSentCount);
    }

    if (g_idConfig.messagesReceivedByAccessory
        && g_idConfig.messagesReceivedCount > 0) {
        iAP2LogDbg("[Id] MessagesReceivedByAccessory: %u messages",
                   g_idConfig.messagesReceivedCount);

        for (uint16_t i = 0; i < g_idConfig.messagesReceivedCount && i < 10; i++) {
            iAP2LogDbg("[Id]   - 0x%04X", g_idConfig.messagesReceivedByAccessory[i]);
        }

        if (g_idConfig.messagesReceivedCount > 10) {
            iAP2LogDbg("[Id]   - ... and %u more", g_idConfig.messagesReceivedCount - 10);
        }

        ctrlSess_AddUint16Array(&builder, kiAP2IdParamMsgRecv,
                                g_idConfig.messagesReceivedByAccessory,
                                g_idConfig.messagesReceivedCount);
    }

    /* 供电能力 (可选) */
    iAP2LogDbg("[Id] PowerProvidingCapability: %u", info->PowerProvidingCapability);
    ctrlSess_AddEnum(&builder, kiAP2IdParamPowerProviding,
                     info->PowerProvidingCapability);
    /* 最大电流 (可选) */
    iAP2LogDbg("[Id] MaximumCurrentDrawnFromDevice: %u mA",
               info->MaximumCurrentDrawnFromDevice);
    ctrlSess_AddUint16(&builder, kiAP2IdParamMaxCurrDrawn,
                       info->MaximumCurrentDrawnFromDevice);

    /* EA协议 (可选) */
    if (g_idConfig.eaProtocolCount > 0) {
        iAP2LogDbg("[Id] ExternalAccessoryProtocols: %u", g_idConfig.eaProtocolCount);
    }

    for (uint8_t i = 0; i < g_idConfig.eaProtocolCount; i++) {
        iAP2LogDbg("[Id]   - Protocol %u: ID=%u, Name='%s', MatchAction=%u",
                   i,
                   g_idConfig.eaProtocols[i].protocolIdentifier,
                   g_idConfig.eaProtocols[i].protocolName,
                   g_idConfig.eaProtocols[i].matchAction);
        _AddEAProtocolGroup(&builder, &g_idConfig.eaProtocols[i]);
    }

    /* 当前语言 (可选) */
    if (info->currentLanguage) {
        iAP2LogDbg("[Id] CurrentLanguage: %s", info->currentLanguage);
        ctrlSess_AddString(&builder, kiAP2IdParamCurrentLang, info->currentLanguage);
    }

    /* 支持的语言 (可选) */
    if (info->supportedLanguages) {
        iAP2LogDbg("[Id] SupportedLanguages: %s", info->supportedLanguages);
        ctrlSess_AddString(&builder, kiAP2IdParamSupportLang, info->supportedLanguages);
    }

    /* 蓝牙传输组件 (可选) */
    if (g_idConfig.btTransport) {
        iAP2LogDbg("[Id] BluetoothTransportComponent:");
        iAP2LogDbg("[Id]   - ID: %u", g_idConfig.btTransport->transportIdentifier);
        iAP2LogDbg("[Id]   - Name: %s", g_idConfig.btTransport->transportName);
        iAP2LogDbg("[Id]   - MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                   g_idConfig.btTransport->macAddress[0],
                   g_idConfig.btTransport->macAddress[1],
                   g_idConfig.btTransport->macAddress[2],
                   g_idConfig.btTransport->macAddress[3],
                   g_idConfig.btTransport->macAddress[4],
                   g_idConfig.btTransport->macAddress[5]);
        _AddBTTransportGroup(&builder, g_idConfig.btTransport);
    }

    /* USB Host传输组件 (可选) */
    if (g_idConfig.usbTransport) {
        iAP2LogDbg("[Id] USBHostTransportComponent:");
        iAP2LogDbg("[Id]   - ID: %u", g_idConfig.usbTransport->transportIdentifier);
        iAP2LogDbg("[Id]   - Name: %s", g_idConfig.usbTransport->transportName);
        _AddUSBHostTransportGroup(&builder, g_idConfig.usbTransport);
    }

    /* PPID (utf8) */
    ctrlSess_AddString(&builder, kiAP2IdParamProductPlanUid, g_PPID);
    uint16_t msgLen = ctrlSess_BuilderFinish(&builder);
    iAP2LogDbg("[Id] Total message length: %u bytes", msgLen);
    iAP2LogDbg("[Id] ========================================");

    /* 发送消息 */
    if (g_idConfig.sendMsgCallback) {
        return g_idConfig.sendMsgCallback(g_msgBuffer, msgLen, g_idConfig.context);
    }

    return FALSE;
}

/*
 * 处理StartIdentification消息 (0x1D00)
 */
static BOOL _HandleStartIdentification(const uint8_t *data, uint32_t len)
{
    (void)data;
    (void)len;
    iAP2LogDbg("Received StartIdentification");

    /* 发送识别信息 */
    if (!_SendIdentificationInfo()) {
        g_idState = kIAP2IdStateRejected;
        return FALSE;
    }

    g_idState = kIAP2IdStateWaitResult;
    iAP2LogDbg("Id state -> WaitResult");
    return TRUE;
}

/*
 * 处理IdentificationAccepted消息 (0x1D02)
 */
static BOOL _HandleIdentificationAccepted(const uint8_t *data, uint32_t len)
{
    (void)data;
    (void)len;
    iAP2LogDbg("Identification ACCEPTED");
    g_idState = kIAP2IdStateIdentified;

    /* 通知上层 */
    if (g_idConfig.resultCallback) {
        g_idConfig.resultCallback(TRUE, 0, g_idConfig.context);
    }

    return TRUE;
}

/*
 * 处理IdentificationRejected消息 (0x1D03)
 *
 * 设备拒绝识别时，会返回与 IdentificationInformation 相同结构的消息，
 * 但只包含被拒绝的参数。需要解析所有参数来了解拒绝原因。
 */
static BOOL _HandleIdentificationRejected(const uint8_t *data, uint32_t len)
{
    iAP2LogError("[Id] ========================================");
    iAP2LogError("[Id] Identification REJECTED by device");
    iAP2LogError("[Id] ========================================");
    /* 解析消息 */
    ctrlSessMessage msg;

    if (ctrlSess_DecodeMessageHeader(data, len, &msg) != 0) {
        iAP2LogError("[Id] Failed to decode IdentificationRejected message");
        g_idState = kIAP2IdStateRejected;

        if (g_idConfig.resultCallback) {
            g_idConfig.resultCallback(FALSE, 0xFF, g_idConfig.context);
        }

        return TRUE;
    }

    /* 解析所有被拒绝的参数 */
    const uint8_t *paramPtr = msg.paramStart;
    size_t remaining = msg.totalLength - 6;
    BOOL hasRejectedParams = FALSE;
    uint8_t rejectCount = 0;

    while (remaining > 0) {
        ctrlSessParameter param;
        int consumed = ctrlSess_GetNextParameter(paramPtr, remaining, &param);

        if (consumed <= 0) {
            break;
        }

        hasRejectedParams = TRUE;
        rejectCount++;

        /* 解析每个被拒绝的参数 */
        switch (param.id) {
            case kiAP2IdRejectParamName:
                iAP2LogError("[Id] Rejected: Name");
                break;

            case kiAP2IdRejectParamModelId:
                iAP2LogError("[Id] Rejected: ModelIdentifier");
                break;

            case kiAP2IdRejectParamManufacturer:
                iAP2LogError("[Id] Rejected: Manufacturer");
                break;

            case kiAP2IdRejectParamSerialNumber:
                iAP2LogError("[Id] Rejected: SerialNumber");
                break;

            case kiAP2IdRejectParamFwVer:
                iAP2LogError("[Id] Rejected: FirmwareVersion");
                break;

            case kiAP2IdRejectParamHwVer:
                iAP2LogError("[Id] Rejected: HardwareVersion");
                break;

            case kiAP2IdRejectParamMsgSent: {
                size_t arrayLen = 0;
                const uint8_t *arrayData = ctrlSess_ParamGetBlob(&param, &arrayLen);
                uint16_t msgCount = arrayLen / 2;
                iAP2LogError("[Id] Rejected: MessagesSentByAccessory (count=%u)", msgCount);

                /* 打印被拒绝的消息ID */
                for (uint16_t i = 0; i < msgCount && i < 10; i++) {
                    uint16_t msgId = READ_U16(arrayData + i * 2);
                    iAP2LogError("[Id]   - Message ID: 0x%04X", msgId);
                }

                if (msgCount > 10) {
                    iAP2LogError("[Id]   - ... and %u more", msgCount - 10);
                }

                break;
            }

            case kiAP2IdRejectParamMsgRecv: {
                size_t arrayLen = 0;
                const uint8_t *arrayData = ctrlSess_ParamGetBlob(&param, &arrayLen);
                uint16_t msgCount = arrayLen / 2;
                iAP2LogError("[Id] Rejected: MessagesReceivedByAccessory (count=%u)", msgCount);

                /* 打印被拒绝的消息ID */
                for (uint16_t i = 0; i < msgCount && i < 10; i++) {
                    uint16_t msgId = READ_U16(arrayData + i * 2);
                    iAP2LogError("[Id]   - Message ID: 0x%04X", msgId);
                }

                if (msgCount > 10) {
                    iAP2LogError("[Id]   - ... and %u more", msgCount - 10);
                }

                break;
            }

            case kiAP2IdRejectParamPowerProviding: {
                iAP2LogError("[Id] Rejected: PowerProvidingCapability");
                break;
            }

            case kiAP2IdRejectParamMaxCurrDrawn:
                iAP2LogError("[Id] Rejected: MaximumCurrentDrawnFromDevice");
                break;

            case kiAP2IdRejectParamExtProtocol:
                iAP2LogError("[Id] Rejected: ExternalAccessoryProtocol (group)");
                break;

            case kiAP2IdRejectParamAppMatchTeamId:
                iAP2LogError("[Id] Rejected: AppMatchTeamID");
                break;

            case kiAP2IdRejectParamCurrentLang:
                iAP2LogError("[Id] Rejected: CurrentLanguage");
                break;

            case kiAP2IdRejectParamSupportLang:
                iAP2LogError("[Id] Rejected: SupportedLanguage");
                break;

            case kiAP2IdRejectParamUartTransComp:
                iAP2LogError("[Id] Rejected: UARTTransportComponent (group)");
                break;

            case kiAP2IdRejectParamUsbDevTransComp:
                iAP2LogError("[Id] Rejected: USBDeviceTransportComponent (group)");
                break;

            case kiAP2IdRejectParamUsbHostTransComp:
                iAP2LogError("[Id] Rejected: USBHostTransportComponent (group)");
                break;

            case kiAP2IdRejectParamBtTransComp:
                iAP2LogError("[Id] Rejected: BluetoothTransportComponent (group)");
                break;

            case kiAP2IdRejectParamIap2HidComp:
                iAP2LogError("[Id] Rejected: iAP2HIDComponent (group)");
                break;

            case kiAP2IdRejectParamLocInfoComp:
                iAP2LogError("[Id] Rejected: LocationInformationComponent (group)");
                break;

            case kiAP2IdRejectParamUsbHostHidComp:
                iAP2LogError("[Id] Rejected: USBHostHIDComponent (group)");
                break;

            case kiAP2IdRejectParamBtHidComp:
                iAP2LogError("[Id] Rejected: BluetoothHIDComponent (group)");
                break;

            case kiAP2IdRejectParamProductPlanUid:
                iAP2LogError("[Id] Rejected: ProductPlanUUID");
                break;

            default:
                iAP2LogError("[Id] Rejected: Unknown parameter ID %u", param.id);
                break;
        }

        paramPtr += consumed;
        remaining -= consumed;
    }

    if (!hasRejectedParams) {
        iAP2LogError("[Id] No specific rejection parameters provided");
        iAP2LogError("[Id] Possible reasons:");
        iAP2LogError("[Id]   - Device does not support iAP2");
        iAP2LogError("[Id]   - Authentication failed");
        iAP2LogError("[Id]   - Device firmware issue");

    } else {
        iAP2LogError("[Id] Total rejected parameters: %u", rejectCount);
    }

    iAP2LogError("[Id] ========================================");
    g_idState = kIAP2IdStateRejected;

    /* 通知上层 - 使用拒绝参数数量作为原因码 */
    if (g_idConfig.resultCallback) {
        g_idConfig.resultCallback(FALSE, rejectCount, g_idConfig.context);
    }

    return TRUE;
}

/*
 ****************************************************************
 * API实现
 ****************************************************************
 */

int iAP2IdInit(const iAP2IdConfig_t *config, uint8_t type)
{
    if (!config || !config->sendMsgCallback) {
        iAP2LogError("[Id] Invalid config: missing send callback");
        return -1;
    }

    /* 复制用户提供的回调配置 */
    memcpy(&g_idConfig, config, sizeof(iAP2IdConfig_t));
    /* 使用内置的配件信息和消息能力 */
    g_idConfig.accessoryInfo = &g_accessoryInfo;
    g_idConfig.eaProtocols = g_eaProtocols;
    g_idConfig.eaProtocolCount = sizeof(g_eaProtocols) / sizeof(g_eaProtocols[0]);

    if (type == kIAP2TransportTypeUSB) {
        g_idConfig.usbTransport = &g_usbTransport;

    } else {
        g_idConfig.usbTransport = NULL;
    }

    if (type == kIAP2TransportTypeBluetooth) {
        g_idConfig.btTransport = &g_btTransport;

    } else {
        g_idConfig.btTransport = NULL;
    }

    g_idConfig.messagesSentByAccessory = g_messagesSent;
    g_idConfig.messagesSentCount = sizeof(g_messagesSent) / sizeof(
                                       g_messagesSent[0]);
    g_idConfig.messagesReceivedByAccessory = g_messagesReceived;
    g_idConfig.messagesReceivedCount = sizeof(g_messagesReceived) / sizeof(
                                           g_messagesReceived[0]);
    g_idState = kIAP2IdStateIdle;
    g_initialized = TRUE;
    iAP2LogDbg("[Id] Identification module initialized with built-in config");
    iAP2LogDbg("[Id] Accessory: %s (%s)", g_accessoryInfo.name,
               g_accessoryInfo.modelIdentifier);
    iAP2LogDbg("[Id] Messages sent: %u, received: %u",
               g_idConfig.messagesSentCount, g_idConfig.messagesReceivedCount);
    iAP2LogDbg("[Id] EA protocols: %u", g_idConfig.eaProtocolCount);
    return 0;
}

void iAP2IdDeinit(void)
{
    g_initialized = FALSE;
    g_idState = kIAP2IdStateIdle;
    memset(&g_idConfig, 0, sizeof(g_idConfig));
    iAP2LogDbg("Identification module deinitialized");
}

void iAP2IdReset(void)
{
    g_idState = kIAP2IdStateIdle;
    iAP2LogDbg("Identification module reset");
}

iAP2IdState_t iAP2IdGetState(void)
{
    return g_idState;
}

void iAP2IdOnAuthComplete(BOOL success)
{
    if (!g_initialized) {
        return;
    }

    if (success) {
        g_idState = kIAP2IdStateWaitStart;
        iAP2LogDbg("Id state -> WaitStart (auth succeeded)");

    } else {
        g_idState = kIAP2IdStateIdle;
        iAP2LogDbg("Id state -> Idle (auth failed)");
    }
}

BOOL iAP2IdHandleMessage(uint16_t msgId, const uint8_t *data, uint32_t len)
{
    if (!g_initialized || !data || len == 0) {
        return FALSE;
    }

    switch (msgId) {
        case kiAP2IdMsgStartId:
            if (g_idState == kIAP2IdStateWaitStart) {
                return _HandleStartIdentification(data, len);
            }

            break;

        case kiAP2IdMsgAccepted:
            if (g_idState == kIAP2IdStateWaitResult) {
                return _HandleIdentificationAccepted(data, len);
            }

            break;

        case kiAP2IdMsgRejected:
            if (g_idState == kIAP2IdStateWaitResult) {
                return _HandleIdentificationRejected(data, len);
            }

            break;

        default:
            break;
    }

    return FALSE;
}

int iAP2IdSendUpdate(const iAP2AccessoryInfo_t *updatedInfo)
{
    if (!g_initialized || !updatedInfo) {
        return -1;
    }

    if (g_idState != kIAP2IdStateIdentified) {
        iAP2LogError("Cannot send update: not identified");
        return -1;
    }

    /* 构建IdentificationInformationUpdate消息 */
    ctrlSessBuilder builder;
    ctrlSess_BuilderInit(&builder, g_msgBuffer, ID_MSG_BUFFER_SIZE,
                         kiAP2IdMsgUpdate);

    /* 只添加非空的更新字段 */
    if (updatedInfo->name) {
        ctrlSess_AddString(&builder, kiAP2IdParamName, updatedInfo->name);
    }

    if (updatedInfo->firmwareVersion) {
        ctrlSess_AddString(&builder, kiAP2IdParamFwVer, updatedInfo->firmwareVersion);
    }

    if (updatedInfo->currentLanguage) {
        ctrlSess_AddString(&builder, kiAP2IdParamCurrentLang,
                           updatedInfo->currentLanguage);
    }

    uint16_t msgLen = ctrlSess_BuilderFinish(&builder);
    iAP2LogDbg("Sending IdentificationInformationUpdate: %u bytes", msgLen);

    if (g_idConfig.sendMsgCallback) {
        if (g_idConfig.sendMsgCallback(g_msgBuffer, msgLen, g_idConfig.context)) {
            return 0;
        }
    }

    return -1;
}

BOOL iAP2IdIsComplete(void)
{
    return (g_idState == kIAP2IdStateIdentified ||
            g_idState == kIAP2IdStateRejected);
}

BOOL iAP2IdIsSuccess(void)
{
    return (g_idState == kIAP2IdStateIdentified);
}

/*
 * iAP2IdSetAccessoryInfo
 * 设置配件信息（可选，用于覆盖默认配置）
 *
 * 注意：必须在 iAP2IdInit() 之后、识别开始之前调用
 */
int iAP2IdSetAccessoryInfo(const char *name, const char *modelId,
                           const char *manufacturer, const char *serialNumber,
                           const char *fwVersion, const char *hwVersion)
{
    if (!g_initialized) {
        iAP2LogError("[Id] Module not initialized");
        return -1;
    }

    if (g_idState != kIAP2IdStateIdle && g_idState != kIAP2IdStateWaitStart) {
        iAP2LogError("[Id] Cannot change accessory info in current state: %d",
                     g_idState);
        return -1;
    }

    if (name) g_accessoryInfo.name = name;

    if (modelId) g_accessoryInfo.modelIdentifier = modelId;

    if (manufacturer) g_accessoryInfo.manufacturer = manufacturer;

    if (serialNumber) g_accessoryInfo.serialNumber = serialNumber;

    if (fwVersion) g_accessoryInfo.firmwareVersion = fwVersion;

    if (hwVersion) g_accessoryInfo.hardwareVersion = hwVersion;

    iAP2LogDbg("[Id] Accessory info updated: %s (%s)",
               g_accessoryInfo.name, g_accessoryInfo.modelIdentifier);
    return 0;
}

/*
 * iAP2IdSetBluetoothMAC
 * 设置蓝牙 MAC 地址（可选）
 */
int iAP2IdSetBluetoothMACName(const uint8_t *macAddress, const char *name)
{
    if (!g_initialized) {
        iAP2LogError("[Id] Module not initialized");
        return -1;
    }

    if (!macAddress) {
        iAP2LogError("[Id] Invalid MAC address");
        return -1;
    }

    if (g_idState != kIAP2IdStateIdle && g_idState != kIAP2IdStateWaitStart) {
        iAP2LogError("[Id] Cannot change MAC address in current state: %d", g_idState);
        return -1;
    }

    memcpy(g_btTransport.macAddress, macAddress, 6);

    if (name) {
        snprintf(g_btTransport.transportName, sizeof(g_btTransport.transportName), "%s",
                 name);
    }

    iAP2LogDbg("[Id] Bluetooth MAC updated: %02X:%02X:%02X:%02X:%02X:%02X",
               macAddress[0], macAddress[1], macAddress[2],
               macAddress[3], macAddress[4], macAddress[5]);
    return 0;
}
