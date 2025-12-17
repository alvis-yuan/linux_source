#ifndef __SERIAL_H__
#define __SERIAL_H__

typedef struct {
	int baudrate;
	int databits;	/* 7,8 */
	int stopbits;	/* 1,2 */
	int parity;		/* 0: None; 1: Odd; 2: Even */
} serial_settings_t;

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern void serial_settings_changed(void);
extern int serial_init(void);

/*----------------------------------------------*
 | GLOBAL VARIABLES                             |
 *----------------------------------------------*/
extern serial_settings_t serial_settings;

#endif /* __SERIAL_H__ */
