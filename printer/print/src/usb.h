#ifndef __USB_H__
#define __USB_H__

#include <stdint.h>

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern int usb_send_data(const void *data, uint32_t len);
extern int usb_init(void);

#endif /* __USB_H__ */
