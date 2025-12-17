#ifndef __HOST_CMD_H__
#define __HOST_CMD_H__

#include <stdint.h>

enum {
	HOST_CMD_SRC_USB = 0,
	HOST_CMD_SRC_TCP,
	HOST_CMD_SRC_MAX
};

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern int host_cmd_handler(const uint8_t *data, uint32_t len, int src);

#endif /* __HOST_CMD_H__ */
