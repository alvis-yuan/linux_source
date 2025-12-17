#ifndef __INSTRUCT_PROC_H__
#define __INSTRUCT_PROC_H__

#include <pthread.h>
#include <stdint.h>

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern void ASC_Proc(uint8_t code);
extern void CodePage_Proc(uint8_t code);
extern void Unicode_Proc(uint32_t unicode);
extern void Cmd_Start(void);
extern void Cmd_Proc(uint32_t data);
extern void SkipNBytes(uint32_t n);
extern void ResetNextFun(void);
extern int ReplyToHost(const void *data, uint32_t len, int chn_id);

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern pthread_mutex_t Cmd_Mutex;
extern void (*pNextFun)(void);
extern uint16_t Cmd_BufferId, Cmd_BufferOffset;
extern uint32_t Cmd_Data, Cmd_Len;
extern uint32_t Cmd_List0, Cmd_List1, Cmd_List2, Cmd_List3, Cmd_List4, Cmd_List5;
extern uint8_t Cmd_Bytes[16];
extern int Cmd_ChnId, Cmd_DontClearBuffer;

#endif // __INSTRUCT_PROC_H__
