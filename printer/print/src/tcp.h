#ifndef __TCP_H__
#define __TCP_H__

#include <stdint.h>

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern int tcp_send_data(const void *data, uint32_t len);
extern int tcp_init(void);

#endif /* __TCP_H__ */
