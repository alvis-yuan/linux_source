/*
 * File: iAP2SessionTypes.h
 * Package: iAP2Session
 * Abstract: iAP2会话层共享类型定义
 *
 * 此文件包含会话层、认证和识别模块共享的类型定义
 * 避免重复定义和类型冲突
 */

#ifndef __IAP2_SESSION_TYPES_H__
#define __IAP2_SESSION_TYPES_H__

#include <stdint.h>
#include <iAP2Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 ****************************************************************
 * 配件信息类型定义
 ****************************************************************
 */

/*
 * 配件基本信息
 */
typedef struct {
    const char *name;               /* 配件名称 */
    const char *modelIdentifier;    /* 型号标识 */
    const char *manufacturer;       /* 制造商 */
    const char *serialNumber;       /* 序列号 */
    const char *firmwareVersion;    /* 固件版本 */
    const char *hardwareVersion;    /* 硬件版本 */
    const char *currentLanguage;    /* 当前语言 (可选) */
    const char *supportedLanguages; /* 支持的语言 (可选) */
    uint16_t    MaximumCurrentDrawnFromDevice; /* 最大电流 */
    uint8_t     PowerProvidingCapability; /* 供电能力 */
} iAP2AccessoryInfo_t;

/*
 * EA协议信息
 */
typedef struct {
    uint8_t     protocolIdentifier; /* 协议ID */
    const char *protocolName;       /* 协议名称 */
    uint8_t
    matchAction;        /* 匹配动作: 0=NoAction, 1=Optional, 2=NoAlert, 3=NoComm */
} iAP2EAProtocol_t;

/*
 * 蓝牙传输组件信息
 */
typedef struct {
    uint16_t    transportIdentifier;    /* 传输组件ID */
    char        transportName[64];          /* 传输组件名称 */
    uint8_t     macAddress[6];          /* 蓝牙MAC地址 */
} iAP2BTTransportInfo_t;

/* USB Device 传输组件信息 */
typedef struct {
    uint16_t    transportIdentifier;    /* 传输组件ID */
    char        transportName[64];          /* 传输组件名称 */
} iAP2USBHostTransportInfo_t;

#ifdef __cplusplus
}
#endif

#endif /* __IAP2_SESSION_TYPES_H__ */
