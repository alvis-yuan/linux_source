// protocol.h
#ifndef _PROTOCOL_H_
#define _PROTOCOL_H_

#include <stdint.h>

/* 强制对齐，确保跨进程内存布局一致 */
#pragma pack(push, 1)

struct sys_status_msg {
    int32_t cpu_usage;
    int32_t mem_usage;
    char last_error[64];
};

#pragma pack(pop)

#endif /* _PROTOCOL_H_ */