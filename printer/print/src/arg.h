#ifndef __ARG_H__
#define __ARG_H__

#include <stdbool.h>

typedef struct {
	bool daemon;
} arg_param_t;

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern void arg_parse(int argc, char *argv[]);

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern arg_param_t arg_param;

#endif /* __ARG_H__ */
