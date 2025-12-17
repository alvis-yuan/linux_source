#ifndef __TCPSERVER_H__
#define __TCPSERVER_H__

/*----------------------------------------------*
 | PUBLIC FUNCTIONS                             |
 *----------------------------------------------*/
extern int tcp_server_send_data(int chn_id, const void *data, uint32_t len);
extern int tcp_server_init(void);

#endif /* __TCPSERVER_H__ */
