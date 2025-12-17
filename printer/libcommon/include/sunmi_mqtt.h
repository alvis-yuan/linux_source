#ifndef __SUNMI_MQTT_H__
#define __SUNMI_MQTT_H__

#include <libcommon.h>
#include <mqtt.h>

typedef struct mqtt_client mqtt_client_t;
typedef enum MQTTErrors MQTTErrors_t;
typedef void (*mqtt_publish_recv_callback)(void** state, struct mqtt_response_publish *publish);
typedef void (*mqtt_conn_success_callback)(mqtt_client_t *client,int is_first_time);
typedef MQTTErrors_t (*mqtt_inspector_callback)(mqtt_client_t *client);
MQTTErrors_t sunmi_mqtt_publish(const char *msg);
void sunmi_mqtt_init(mqtt_publish_recv_callback recv_cb,
	mqtt_conn_success_callback conn_succ_cb,
	mqtt_inspector_callback inspector_cb );
int is_mqtt_alive();
void sunmi_mqtt_network_test();
int sunmi_mqtt_disconnect(void);

#endif
