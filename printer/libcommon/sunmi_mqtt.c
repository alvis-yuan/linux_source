#include <pthread.h>
#include <libcommon.h>
#include <mqtt.h>
#include <mqtt_pal.h>
#include "sunmi_mqtt.h"

#define MQTT_CA_FILE "/usr/share/ca-certificates/sunmi/mqtt_ca.pem"
static uint8_t g_sndbuf[8<<10],g_rcvbuf[16<<10];
static mqtt_client_t g_client_ctx = {};
static char g_mqtt_inited = 0;
static char g_is_first_connect = 1;
static char g_cur_use_net = 0;
static mqtt_conn_success_callback g_conn_succ_cb = NULL;

static void _reconnect_cb(mqtt_client_t *client,void **unused)
{
	led_ctrl(LED_ID_NET,LED_OP_OFF);
	client->number_of_keep_alives = 0;
    LogInfo("mqtt error: %d,%s",client->error,mqtt_error_str(client->error));
	client->alives_flag = 0;
	int alive_time = 20;

	if( client->socketfd ) {
		mqtt_pal_close_socket(client->socketfd,client->tls_version);
		client->socketfd = NULL;
	}
	if( NetGetCurRoute() == NET_NONE || 
		!isTimeSynced() ||
		!sys_global_var()->sn[0] || 
		!sys_global_var()->broker_addr[0] ||
		!sys_global_var()->broker_port[0] ||
		!sys_global_var()->broker_uname[0] || 
		!sys_global_var()->broker_pwd[0] ) {
			MQTT_PAL_MUTEX_UNLOCK(&client->mutex);
			sleep(2);
			return;
	}
	mqtt_pal_socket_handle handle = mqtt_open_ssl_socket(sys_global_var()->broker_addr,sys_global_var()->broker_port,MQTT_CA_FILE);
	if( !handle ) {
		//LogError("mqtt_pal_open_socket fail");
		client->error = MQTT_ERROR_SOCKET_ERROR;
		/*mqtt_sync调用此函数前调用了MQTT_PAL_MUTEX_LOCK(&client->mutex),
			所以在次函数返回前需要做UNLOCK */
		MQTT_PAL_MUTEX_UNLOCK(&client->mutex);
		sleep(1);
		return;
	}
	mqtt_reinit(client,handle,g_sndbuf,sizeof(g_sndbuf),g_rcvbuf,sizeof(g_rcvbuf));
	g_cur_use_net = NetGetCurRoute();
	if( g_cur_use_net == NET_WNET ) alive_time = 600;
	uint8_t conn_flags = (access("/tmp/mqtt_clean",F_OK)==0?MQTT_CONNECT_CLEAN_SESSION:0);
	mqtt_connect(client,
		sys_global_var()->sn,
		NULL,NULL,0,
		sys_global_var()->broker_uname,
		sys_global_var()->broker_pwd,
		conn_flags,/* connect flag */
		alive_time /*alive time */
	);
	/* mqtt_connect函数会释放mqtt_sync中的锁,所以这里不用释放*/
	if( client->error != MQTT_OK ) {
		LogError("mqtt_connect fail(%d),%s",mqtt_error_str(client->error));
		sleep(1);
		return;
	}
}

static void _msg_recv_cb(void **unused,struct mqtt_response_publish *published)
{
	LogInfo("Topic=%.*s,Message=%.*s",
		published->topic_name_size,published->topic_name,
		published->application_message_size,published->application_message);
}

static MQTTErrors_t _inspector_cb(mqtt_client_t *client)
{
	/* 检测网络变化,如果网络切换放回MQTT_ERROR_NET_CHANGE,MQTT将自动重连 */
	int cur_net = NetGetCurRoute();
	if( cur_net != g_cur_use_net ) {
		LogInfo("MQTT will reconnect because net change %d=>%d",g_cur_use_net,cur_net);
		g_cur_use_net = cur_net;
		client->error = MQTT_ERROR_NET_CHANGE;
		return MQTT_ERROR_NET_CHANGE;
	}
	return MQTT_OK;
}

static void _netrefresher(void)
{
	uint8_t curnet = NetGetCurRoute();
	switch (curnet)
	{
		case NET_LAN:
			AudioPlayFixed(LAN_CONNECTED, 1, 0);
			break;
		case NET_WIFI:
			AudioPlayFixed(WIFI_CONNECTED, 1, 0);
			break;
		case NET_WNET:
			AudioPlayFixed(MOBILE_CONNECTED, 1, 0);
			break;
		case NET_NONE:
			LogWarn("no useful route!");
			break;
		default:
			LogError("unsupport net state!");
			break;
	}
}

static void *_refresher(void *arg)
{
	mqtt_client_t *client = (mqtt_client_t *)arg;
	pthread_detach(pthread_self());
	while(1) {
		int flag = is_mqtt_alive();
		mqtt_sync(client);
		if( is_mqtt_alive() ) {
			if( !flag ) { // mqtt 连接成功
				char topic[128];
				snprintf(topic,sizeof(topic),"/%s/%s/sub",sys_global_var()->project,sys_global_var()->sn);
				led_ctrl(LED_ID_NET,LED_OP_ON);
				_netrefresher();
				mqtt_subscribe(client, topic, MQTT_SUBACK_SUCCESS_MAX_QOS_2);
				if( g_conn_succ_cb ) g_conn_succ_cb(client,g_is_first_connect);
				g_is_first_connect = 0;
				continue;
			}
		} else {
			if ( flag ) {
				LogDbg("mqtt connect turn into disconnected!");
				AudioPlayFixed(NET_DISCONNECTED, 1, 0);
			}
		}
		usleep(50*1000);
	}
	return NULL;
}

static void _connect_success_cb(mqtt_client_t *client,int is_first_time)
{
	LogInfo("Connect success,is_first_time:%d",is_first_time);
}

int is_mqtt_alive()
{
	return mqtt_get_status(&g_client_ctx)==MQTT_OK;
}

void sunmi_mqtt_init(mqtt_publish_recv_callback recv_cb,
	mqtt_conn_success_callback conn_succ_cb,
	mqtt_inspector_callback inspector_cb )
{
	mqtt_publish_recv_callback _rcv_cb = recv_cb?recv_cb:_msg_recv_cb;
	pthread_t tid;

	if( g_mqtt_inited ) return;
	g_mqtt_inited = 1;

	mqtt_init_reconnect(&g_client_ctx,_reconnect_cb,NULL,_rcv_cb);
	g_client_ctx.tls_version = TLS1_1;
	g_client_ctx.socketfd = NULL;
	g_client_ctx.inspector_callback = inspector_cb?inspector_cb:_inspector_cb;
	g_conn_succ_cb = conn_succ_cb?conn_succ_cb:_connect_success_cb;
	pthread_create(&tid,NULL,_refresher,&g_client_ctx);
	
}

MQTTErrors_t sunmi_mqtt_publish(const char *msg)
{
	char topic[128];

	if( !is_mqtt_alive() ) {
		LogWarn("MQTT is not alive,skip msg:%s",msg);
		return MQTT_ERROR_CONNECT_NOT_CALLED;
	}
	snprintf(topic,sizeof(topic),"/%s/%s/pub",sys_global_var()->project,sys_global_var()->sn);
	return mqtt_publish(&g_client_ctx,topic,(void *)msg,strlen(msg),MQTT_PUBLISH_QOS_2);
}

void sunmi_mqtt_network_test()
{
	char topic[128],msg[]="{\"T01\":{\"mqtt_rw\":1}}";

	if( !is_mqtt_alive() ) {
		LogWarn("MQTT is not alive,skip msg:%s",msg);
		return;
	}
	snprintf(topic,sizeof(topic),"/%s/%s/sub",sys_global_var()->project,sys_global_var()->sn);
	mqtt_publish(&g_client_ctx,topic,(void *)msg,strlen(msg),MQTT_PUBLISH_QOS_2);
}

int sunmi_mqtt_disconnect(void)
{
		if( !g_mqtt_inited )
		{
			return 0;
		}

		while( is_mqtt_alive() )
		{
			MQTT_PAL_MUTEX_LOCK(&g_client_ctx.mutex);
			g_client_ctx.error = MQTT_ERROR_CONNECT_NOT_CALLED;
			MQTT_PAL_MUTEX_UNLOCK(&g_client_ctx.mutex);
			usleep(50000);
		}
		return 0;
}
