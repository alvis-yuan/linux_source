#ifndef __KEY_H__
#define __KEY_H__
#include <stdint.h>
#include <stdbool.h>

#define GPIOKEY_INPUT_EVENT         "/dev/input/event0"
#define KEYBOARD_MQ                 "/mqueue"

#define PHY_KEY_FN                  0x81 //功能
#define PHY_KEY_VOLUMEUP            0x92 //音量增加键
#define PHY_KEY_VOLUMEDOWN          0x93 //音量减少键
#define PHY_KEY_SETTING             0x94 //设置键

#define VIRTUAL_KEY_TESTPAGE        0x04 //自检页键
#define VIRTUAL_KEY_WNET_INFO       0x05 //MQTT上报WNET信息
#define VIRTUAL_KEY_ALIPAY_BIND     0x06 //收款SDK绑定
#define VIRTUAL_KEY_ALIPAY_UNBIND   0x07 //收款SDK解除绑定
#define VIRTUAL_KEY_ALIPAY_BUCKS    0x08 //收款SDK入账

#define VIRTUAL_KEY_START_FOTA      0x09 //开始FOTA自检流程

typedef struct _key_info_t
{
    uint8_t index; /* 0: volume- 1: volume+ 2: function 3: setting */
    char state;    /* 1: Release 2: Press */
} key_info_t;

typedef struct _key_detail_t
{
    uint32_t pt;  /* press time       */
    uint32_t lpt; /* long press time  */
    uint32_t pst; /* press space time */
    bool isPress;
} key_detail_t;

void send_key_mqmsg(uint8_t index, char state);

#endif /* __KEY_H__ */
