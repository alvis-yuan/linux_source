/*
 * File: iAP2Adapt.h
 * Package: iAP2Adapt
 * Abstract: iAP2 传输层适配 - 头文件
 */

#ifndef IAP2_ADAPT_H
#define IAP2_ADAPT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    kIAP2TransportTypeBluetooth = 0,
    kIAP2TransportTypeUSB,
};

/**
 * @brief 处理iAP2数据
 * @param data 数据指针
 * @param len 数据长度
 * @return 处理的字节数
 */
int iAP2AdaptDataHandler(unsigned char *data, unsigned int len);

/*
 * 会话数据回调类型
 */
typedef int8_t (*iAP2SessionDataCB_t)(const uint8_t *data,
                                      uint32_t dataLen,
                                      void *context);

/* 物理层数据发送回调 - 用户实现，将数据通过物理层发送 */
typedef int (*iAP2TransportSendDataCB_t)(const uint8_t *data,
        uint32_t dataLen,
        void *context);

/**
 * @brief iAP2连接建立时调用
 *
 * @param type 传输类型（蓝牙/USB）
 * @param sessionDataCB 会话数据回调
 * @param sendDataCB 发送数据回调
 * @param context 用户上下文指针

 * @return 0 on success, -1 on failure
 */
int iAP2AdaptOnConnected(int type, iAP2SessionDataCB_t sessionDataCB,
                         iAP2TransportSendDataCB_t sendDataCB, void *context);

/**
 * @brief iAP2 BSA连接断开时调用
 */
void iAP2AdaptOnDisconnected(void);

#ifdef __cplusplus
}
#endif

#endif /* IAP2_ADAPT_H */
