#ifndef IAP2_CONTROL_MESSAGE_H
#define IAP2_CONTROL_MESSAGE_H

/* =============================================================================
 * 101.1 Accessory Authentication Message IDs
 * ============================================================================= */
typedef enum {
    kiAP2AuthMsgReqAuthCert = 0xAA00,      /* Device -> Accessory */
    kiAP2AuthMsgAuthCert = 0xAA01,         /* Accessory -> Device */
    kiAP2AuthMsgReqAuthChallenge = 0xAA02, /* Device -> Accessory */
    kiAP2AuthMsgAuthResponse = 0xAA03,     /* Accessory -> Device */
    kiAP2AuthMsgAuthFailed = 0xAA04,       /* Device -> Accessory */
    kiAP2AuthMsgAuthSucceeded = 0xAA05,    /* Device -> Accessory */
    kiAP2AuthMsgAuthSerial = 0xAA06        /* Accessory -> Device */
} iAP2AuthMessageId_t;

/* Parameter IDs for Authentication messages */
typedef enum {
    /* RequestAuthenticationCertificate (0xAA00) */
    kiAP2AuthParamReqCertSerialNumber = 0, /* none (exist to request) */

    /* AuthenticationCertificate (0xAA01) */
    kiAP2AuthParamCertificateData = 0,     /* blob (X.509 Certificate) */

    /* RequestAuthenticationChallengeResponse (0xAA02) */
    kiAP2AuthParamChallengeData = 0,       /* blob (Random Number) */

    /* AuthenticationResponse (0xAA03) */
    kiAP2AuthParamChallengeResponse = 0,   /* blob (Computed Response) */

    /* AccessoryAuthenticationSerialNumber (0xAA06) */
    kiAP2AuthParamCertSerialNumber = 0     /* blob (Certificate Serial) */
} iAP2AuthParameterId_t;

/* =============================================================================
 * 101.2 Accessory Identification Message IDs
 * ============================================================================= */
typedef enum {
    kiAP2IdMsgStartId = 0x1D00,           /* Device -> Accessory */
    kiAP2IdMsgInfo = 0x1D01,               /* Accessory -> Device */
    kiAP2IdMsgAccepted = 0x1D02,           /* Device -> Accessory */
    kiAP2IdMsgRejected = 0x1D03,           /* Device -> Accessory */
    kiAP2IdMsgCancel = 0x1D05,             /* Accessory -> Device */
    kiAP2IdMsgUpdate = 0x1D06              /* Accessory -> Device */
} iAP2IdentificationMessageId_t;

/* IdentificationInformation (0x1D01) & Update (0x1D06) Parameter IDs */
typedef enum {
    kiAP2IdParamName = 0,                   /* utf8 */
    kiAP2IdParamModelId = 1,                /* utf8 */
    kiAP2IdParamManufacturer = 2,           /* utf8 */
    kiAP2IdParamSerialNumber = 3,                 /* utf8 */
    kiAP2IdParamFwVer = 4,                  /* utf8 */
    kiAP2IdParamHwVer = 5,                  /* utf8 */
    kiAP2IdParamMsgSent = 6,                /* uint16[] (messages sent by accessory) */
    kiAP2IdParamMsgRecv = 7,                /* uint16[] (messages received by accessory) */
    kiAP2IdParamPowerProviding = 8,         /* enum (0:None, 1:C37, 2:Advanced) */
    kiAP2IdParamMaxCurrDrawn = 9,           /* uint16 (mA) */
    kiAP2IdParamExtProtocol = 10,           /* group (see member definitions below) */
    kiAP2IdParamAppMatchTeamId = 11,        /* utf8 */
    kiAP2IdParamCurrentLang = 12,           /* utf8 */
    kiAP2IdParamSupportLang = 13,           /* utf8 (1 or more) */
    kiAP2IdParamUartTransComp = 14,         /* group */
    kiAP2IdParamUsbDevTransComp = 15,       /* group */
    kiAP2IdParamUsbHostTransComp = 16,      /* group */
    kiAP2IdParamBtTransComp = 17,           /* group */
    kiAP2IdParamIap2HidComp = 18,           /* group */
    kiAP2IdParamLocInfoComp = 22,           /* group */
    kiAP2IdParamUsbHostHidComp = 23,        /* group */
    kiAP2IdParamBtHidComp = 29,             /* group */
    kiAP2IdParamProductPlanUid = 34         /* utf8 */
} iAP2IdentificationParameterId_t;

/* IdentificationRejected (0x1D03) Parameter IDs */
typedef enum {
    kiAP2IdRejectParamName = 0,                   /* utf8 - rejected parameter name */
    kiAP2IdRejectParamModelId = 1,                /* utf8 */
    kiAP2IdRejectParamManufacturer = 2,           /* utf8 */
    kiAP2IdRejectParamSerialNumber = 3,           /* utf8 */
    kiAP2IdRejectParamFwVer = 4,                  /* utf8 */
    kiAP2IdRejectParamHwVer = 5,                  /* utf8 */
    kiAP2IdRejectParamMsgSent = 6,                /* uint16[] */
    kiAP2IdRejectParamMsgRecv = 7,                /* uint16[] */
    kiAP2IdRejectParamPowerProviding = 8,         /* enum */
    kiAP2IdRejectParamMaxCurrDrawn = 9,           /* uint16 */
    kiAP2IdRejectParamExtProtocol = 10,           /* group */
    kiAP2IdRejectParamAppMatchTeamId = 11,        /* utf8 */
    kiAP2IdRejectParamCurrentLang = 12,           /* utf8 */
    kiAP2IdRejectParamSupportLang = 13,           /* utf8 */
    kiAP2IdRejectParamUartTransComp = 14,         /* group */
    kiAP2IdRejectParamUsbDevTransComp = 15,       /* group */
    kiAP2IdRejectParamUsbHostTransComp = 16,      /* group */
    kiAP2IdRejectParamBtTransComp = 17,           /* group */
    kiAP2IdRejectParamIap2HidComp = 18,           /* group */
    kiAP2IdRejectParamLocInfoComp = 22,           /* group */
    kiAP2IdRejectParamUsbHostHidComp = 23,        /* group */
    kiAP2IdRejectParamBtHidComp = 29,             /* group */
    kiAP2IdRejectParamProductPlanUid = 34         /* utf8 */
} iAP2IdentificationRejectedParameterId_t;

/* Group member definitions (nested within group type parameter values) */
typedef enum {
    /* 101-11 ExternalAccessoryProtocol group members */
    kiAP2GrpEapProtocolId = 0,             /* uint8 */
    kiAP2GrpEapProtocolName = 1,           /* utf8 */
    kiAP2GrpEapMatchAction = 2,            /* enum (0:NoAction, 1:Optional, 2:NoAlert, 3:NoComm) */
    kiAP2GrpEapNativeTransId = 3,          /* uint16 */

    /* 101-13 USBDeviceTransportComponent group members */
    kiAP2GrpTransDevId = 0,                /* uint16 */
    kiAP2GrpTransDevName = 1,              /* utf8 */
    kiAP2GrpTransDevIap2Conn = 2,          /* none */
    kiAP2GrpTransDevSampleRates = 3,       /* enum (8k, 11k, 12k, 16k, 22k, 24k, 32k, 44k, 48k) */

    /* 101-15 USBHostTransportComponent / 101-16 UARTTransportComponent group members */
    kiAP2GrpTransHostId = 0,               /* uint16 */
    kiAP2GrpTransHostName = 1,             /* utf8 */
    kiAP2GrpTransHostIap2Conn = 2,         /* none */

    /* 101-17 BluetoothTransportComponent group members */
    kiAP2GrpBtTransId = 0,                 /* uint16 */
    kiAP2GrpBtTransName = 1,               /* utf8 */
    kiAP2GrpBtTransIap2Conn = 2,           /* none */
    kiAP2GrpBtTransMacAddr = 3,            /* uint8[6] (MAC address) */

    /* 101-18 iAP2HIDComponent group members */
    kiAP2GrpHidIap2Id = 0,                 /* uint16 */
    kiAP2GrpHidIap2Name = 1,               /* utf8 */
    kiAP2GrpHidIap2Function = 2,           /* enum (see HID Function below) */

    /* 101-19 LocationInformationComponent group members */
    kiAP2GrpLocId = 0,                     /* uint16 */
    kiAP2GrpLocName = 1,                   /* utf8 */
    kiAP2GrpLocGpggaData = 17,             /* none */
    kiAP2GrpLocGprmcData = 18,             /* none */
    kiAP2GrpLocPascdData = 20,             /* none */

    /* 101-20 USBHostHIDComponent group members */
    kiAP2GrpHidUsbHostId = 0,              /* uint16 */
    kiAP2GrpHidUsbHostName = 1,            /* utf8 */
    kiAP2GrpHidUsbHostFunction = 2,        /* enum */
    kiAP2GrpHidUsbHostTransId = 3,         /* uint16 */
    kiAP2GrpHidUsbHostIntfNum = 4,         /* uint16 */

    /* 101-22 BluetoothHIDComponent group members */
    kiAP2GrpHidBtId = 0,                   /* uint16 */
    kiAP2GrpHidBtName = 1,                 /* utf8 */
    kiAP2GrpHidBtFunction = 2,             /* enum */
    kiAP2GrpHidBtTransId = 3               /* uint16 */
} iAP2GroupMemberId_t;

/* 101-21 HIDComponentFunction Enum */
typedef enum {
    kiAP2HidFuncKeyboard = 0,
    kiAP2HidFuncMediaRemote = 1,
    kiAP2HidFuncAssistiveTouch = 2,
    kiAP2HidFuncGamepadDigital = 4,
    kiAP2HidFuncGamepadAnalog = 6,
    kiAP2HidFuncSwitchControl = 7,
    kiAP2HidFuncHeadset = 8,
    kiAP2HidFuncBraille = 10
} iAP2HidFunction_t;

/* 101-10 PowerProvidingCapability Enum */
typedef enum {
    kiAP2PowerCapNone = 0,
    kiAP2PowerCapC37Passthrough = 1,
    kiAP2PowerCapAdvanced = 2
} iAP2PowerCapability_t;

/* ==========================================================================
 * Control Message API Functions
 * ========================================================================== */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <iAP2Defines.h>

/* 发送控制消息回调类型 */
typedef BOOL (*iAP2CtrlMsgSendCB_t)(const uint8_t *data, uint32_t len,
                                    void *context);

/* App Discovery Update 回调类型 */
typedef void (*iAP2AppDiscoveryUpdateCB_t)(uint8_t listAvailable,
        uint16_t listCount, void *context);

/*
 * iAP2CtrlMsgInit
 * 初始化控制消息模块
 *
 * Input:
 *   sendCallback: 发送消息回调函数
 *   context: 用户上下文
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2CtrlMsgInit(iAP2CtrlMsgSendCB_t sendCallback, void *context);

/*
 * iAP2CtrlMsgDeinit
 * 反初始化控制消息模块
 */
void iAP2CtrlMsgDeinit(void);

/* === App Launch Functions === */

/*
 * iAP2SendRequestAppLaunch
 * 发送RequestAppLaunch消息 (0xEA02)
 *
 * Input:
 *   bundleId: 应用Bundle ID
 *   launchMethod: 启动方法（0表示不指定）
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2SendRequestAppLaunch(const char *bundleId, uint8_t launchMethod);

/* === App Discovery Functions === */

/*
 * iAP2SendStartAppDiscoveryUpdates
 * 发送StartAppDiscoveryUpdates消息 (0xAD00)
 *
 * Input:
 *   categories: 应用分类数组（可选，NULL表示不指定）
 *   categoryCount: 分类数量
 *   listMax: 列表最大数量（0表示不限制）
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2SendStartAppDiscoveryUpdates(const uint8_t *categories,
                                     uint8_t categoryCount,
                                     uint16_t listMax);

/*
 * iAP2SendStopAppDiscoveryUpdates
 * 发送StopAppDiscoveryUpdates消息 (0xAD02)
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2SendStopAppDiscoveryUpdates(void);

/*
 * iAP2HandleAppDiscoveryUpdate
 * 处理AppDiscoveryUpdate消息 (0xAD01)
 *
 * Input:
 *   data: 消息数据
 *   len: 数据长度
 *   callback: 回调函数
 *   context: 用户上下文
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2HandleAppDiscoveryUpdate(const uint8_t *data, uint32_t len,
                                 iAP2AppDiscoveryUpdateCB_t callback, void *context);

/* === External Accessory Protocol (EAP) Functions === */

/*
 * iAP2SendEAPSessionStatus
 * 发送EAPSessionStatus消息 (0xEA03)
 *
 * Input:
 *   sessionId: 会话ID
 *   status: 状态值
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2SendEAPSessionStatus(uint16_t sessionId, uint8_t status);

/*
 * iAP2HandleEAPStartSession
 * 处理EAPStartSession消息 (0xEA00)
 *
 * Input:
 *   msgId: 消息ID
 *   data: 消息数据
 *   len: 数据长度
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2HandleEAPStartSession(uint16_t msgId, const uint8_t *data,
                              uint32_t len);

/*
 * iAP2HandleEAPStopSession
 * 处理EAPStopSession消息 (0xEA01)
 *
 * Input:
 *   msgId: 消息ID
 *   data: 消息数据
 *   len: 数据长度
 *
 * Return:
 *   成功返回0，失败返回-1
 */
int iAP2HandleEAPStopSession(uint16_t msgId, const uint8_t *data, uint32_t len);

#ifdef __cplusplus
}
#endif

/* ==========================================================================
 * 101.3 & 101.4 App Launch & Discovery (Application related)
 * ========================================================================== */
typedef enum {
    /* App Launch Message IDs */
    kiAP2AppMsgLaunchReq = 0xEA02,         /* Accessory -> Device */

    /* Parameter IDs for RequestAppLaunch */
    kiAP2AppParamBundleId = 0,             /* utf8 */
    kiAP2AppParamLaunchMethod = 1,         /* enum */

    /* App Discovery Message IDs */
    kiAP2AppDisMsgStartUpdate = 0xAD00,    /* Accessory -> Device */
    kiAP2AppDisMsgUpdate = 0xAD01,         /* Device -> Accessory */
    kiAP2AppDisMsgStopUpdate = 0xAD02,     /* Accessory -> Device */

    /* Parameter IDs for StartAppDiscoveryUpdates */
    kiAP2AppDisParamCategories = 3,        /* group */
    kiAP2AppDisParamListMax = 4,           /* uint16 */

    /* Parameter IDs for AppDiscoveryUpdate */
    kiAP2AppDisParamListAvail = 3,         /* enum */
    kiAP2AppDisParamList = 4,              /* group */
    kiAP2AppDisParamListCount = 5          /* uint16 */
} iAP2AppMessageId_t;

/* ==========================================================================
 * 101.5 AssistiveTouch (Assistive Touch)
 * ========================================================================== */
typedef enum {
    kiAP2AssistiveMsgStart = 0x5400,
    kiAP2AssistiveMsgStop = 0x5401,
    kiAP2AssistiveMsgStartInfo = 0x5402,
    kiAP2AssistiveMsgInfo = 0x5403,
    kiAP2AssistiveMsgStopInfo = 0x5404,

    kiAP2AssistiveParamIsEnabled = 0       /* bool */
} iAP2AssistiveMessageId_t;

/* ==========================================================================
 * 101.6 Bluetooth Connection Status (Bluetooth Connection Status)
 * ========================================================================== */
typedef enum {
    kiAP2BtConnMsgStartUpdate = 0x4E03,
    kiAP2BtConnMsgUpdate = 0x4E04,
    kiAP2BtConnMsgStopUpdate = 0x4E05,

    kiAP2BtConnParamCompIdentifier = 0,    /* uint16 */
    kiAP2BtConnParamProfiles = 1           /* group */
} iAP2BluetoothMessageId_t;

/* ==========================================================================
 * 101.7 Communications (Communication/Call Control)
 * ========================================================================== */
typedef enum {
    kiAP2CommMsgStartCallState = 0x4154,
    kiAP2CommMsgCallStateUpdate = 0x4155,
    kiAP2CommMsgStopCallState = 0x4156,
    kiAP2CommMsgStartCommUpdate = 0x4157,
    kiAP2CommMsgCommUpdate = 0x4158,
    kiAP2CommMsgStopCommUpdate = 0x4159,
    kiAP2CommMsgInitiateCall = 0x415A,
    kiAP2CommMsgAcceptCall = 0x415B,
    kiAP2CommMsgEndCall = 0x415C,
    kiAP2CommMsgSwapCalls = 0x415D,
    kiAP2CommMsgMergeCalls = 0x415E,
    kiAP2CommMsgHoldStatusUpdate = 0x415F,
    kiAP2CommMsgMuteStatusUpdate = 0x4160,
    kiAP2CommMsgSendDtmf = 0x4161,

    /* Parameter IDs for CallStateUpdate (0x4155) */
    kiAP2CommParamCallRemoteId = 0,        /* utf8 */
    kiAP2CommParamCallDisplayName = 1,     /* utf8 */
    kiAP2CommParamCallStatus = 2,          /* enum */
    kiAP2CommParamCallDirection = 3,       /* enum */
    kiAP2CommParamCallUuid = 4,            /* utf8 */
    kiAP2CommParamCallAddrBookId = 6,      /* utf8 */
    kiAP2CommParamCallLabel = 7,           /* utf8 */
    kiAP2CommParamCallService = 8,         /* enum */
    kiAP2CommParamCallIsConferenced = 9,   /* bool */
    kiAP2CommParamCallConfGroup = 10,      /* uint8 */
    kiAP2CommParamCallDisconnReason = 11,  /* enum */
    kiAP2CommParamCallStartTs = 12          /* secs64 */
} iAP2CommunicationMessageId_t;

/* ==========================================================================
 * 101.8 Device Notifications (Device Notifications)
 * ========================================================================== */
typedef enum {
    kiAP2DevMsgInfoUpdate = 0x4E09,
    kiAP2DevMsgLangUpdate = 0x4E0A,
    kiAP2DevMsgTimeUpdate = 0x4E0B,
    kiAP2DevMsgUuidUpdate = 0x4E0C,

    kiAP2DevParamName = 0,                 /* DeviceName */
    kiAP2DevParamLang = 0,                  /* DeviceLanguage */
    kiAP2DevParamTimeSeconds = 0,          /* secs64 */
    kiAP2DevParamTimeZoneOffset = 1,       /* int16 */
    kiAP2DevParamTimeDstOffset = 2,        /* int8 */
    kiAP2DevParamUuid = 0                  /* utf8 */
} iAP2DeviceMessageId_t;

/* ==========================================================================
 * 101.9 External Accessory Protocol (External Accessory Protocol Session)
 * ========================================================================== */
typedef enum {
    kiAP2EapMsgStartSession = 0xEA00,         /* Device -> Accessory */
    kiAP2EapMsgStopSession = 0xEA01,          /* Device -> Accessory */
    kiAP2EapMsgSessionStatus = 0xEA03,       /* Accessory -> Device */

    kiAP2EapParamProtocolId = 0,           /* uint8 */
    kiAP2EapParamSessionId = 1,            /* uint16 */
    kiAP2EapParamStatus = 1                /* enum (in 0xEA03) */
} iAP2ExternalAccessoryMessageId_t;

/* ==========================================================================
 * 101.10 Human Interface Device (HID)
 * ========================================================================== */
typedef enum {
    kiAP2HidMsgStartHid = 0x6800,

    kiAP2HidParamCompIdentifier = 0,      /* uint16 */
    kiAP2HidParamVendorId = 1,            /* uint16 */
    kiAP2HidParamProductId = 2             /* uint16 */
} iAP2HidMessageId_t;

#endif /* IAP2_CONTROL_MESSAGE_H */
