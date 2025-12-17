#ifndef __PRINT_QUEUE_H__
#define __PRINT_QUEUE_H__

#define MQ_TSPL "/tspl_mqueue"

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern void print_queue_run(void);
extern void print_queue_exit(void);
extern int print_queue_init(void);

#endif /* __PRINT_QUEUE_H__ */
