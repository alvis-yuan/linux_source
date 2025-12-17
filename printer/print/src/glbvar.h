#ifndef __GLBVAR_H__
#define __GLBVAR_H__

#include <pthread.h>

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern int glbvar_init(void);

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern pthread_attr_t g_pthread_attr;
extern pthread_mutexattr_t g_mutex_attr;
extern int g_process_running;

#endif /* __GLBVAR_H__ */
