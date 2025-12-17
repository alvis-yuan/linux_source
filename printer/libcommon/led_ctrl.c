#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include <libcommon.h>

#define LED_B_NODE  	"/sys/class/sunmi/base/gpio_led_net"
#define LED_R_NODE  	"/sys/class/sunmi/base/gpio_led_error"
#define LED_ALARM_NODE	"/sys/class/sunmi/base/gpio_led_alarm"

#define LED_TAPE_ON system("tinymix 3 3")
#define LED_TAPE_OFF system("tinymix 3 1")
#define LED_TAPE_ON_1600E system("echo 1 > /sys/class/sunmi/base/gpio_led_alarm ")
#define LED_TAPE_OFF_1600E system("echo 0 > /sys/class/sunmi/base/gpio_led_alarm")


static pthread_t g_led_tape_tid = 0;
static pthread_mutex_t g_led_tape_mutex = PTHREAD_MUTEX_INITIALIZER;

static void *task_tape_blink(void *arg)
{
	pthread_detach(pthread_self());

	int op = 1;
	while(1) {
		if(strcmp(CPU_TYPE, SUNMI_CPU_TYPE_X1600) == 0){
			op?LED_TAPE_ON_1600E:LED_TAPE_OFF_1600E;
			usleep(100*1000);
			op = !op;
		}else{
			op?LED_TAPE_ON:LED_TAPE_OFF;
			usleep(100*1000);
			op = !op;
		}
	}
	return NULL;
}

static void inline led_tape_blink(int en)
{
	pthread_mutex_lock(&g_led_tape_mutex);
	if( en && g_led_tape_tid<=0 ) {
		pthread_create(&g_led_tape_tid,0,task_tape_blink,NULL);
	} else if( !en && g_led_tape_tid>0 ) {
		pthread_cancel(g_led_tape_tid);
		g_led_tape_tid = 0;
	}
	pthread_mutex_unlock(&g_led_tape_mutex);
}

int led_ctrl(int led,LED_OPERATE_E op)
{
	char cmd[128]={0};
	const char *node = NULL;
	int op_value[LED_OP_MAX] = {0,1,3};

	if( op<0 || op>=LED_OP_MAX) return -1;
	if( led<0 || led>=LED_ID_MAX) return -1;
	switch(led) {
		case LED_ID_NET:
		case LED_ID_ERROR:
			node = (led==LED_ID_NET?LED_B_NODE:LED_R_NODE);
			snprintf(cmd,sizeof(cmd),"echo %d 50 > %s",op_value[op],node);
			system(cmd);
			break;
		case LED_ID_TAPE:
			if( op==LED_OP_BLINK ) {
				led_tape_blink(1);
			} else {
				led_tape_blink(0);
				if(strcmp(CPU_TYPE, SUNMI_CPU_TYPE_X1600) == 0){
					(op==LED_OP_ON)?LED_TAPE_ON_1600E:LED_TAPE_OFF_1600E;
				}else{
					(op==LED_OP_ON)?LED_TAPE_ON:LED_TAPE_OFF;
				}
				
			}
			break;
	}
	return 0;
}

/* --------------------------------------------------------------------------
 * FUN:           读取LED报警灯带状态
 * RETURN:        <0 fail;  0 灭； 1 亮
 * ------------------------------------------------------------------------*/
int led_alarm_tape_get( ) 
{
	int ret = -1;
	char buf[512] = {0};
	
	ret = SystemWithResult("tinymix 3", buf, sizeof(buf));
	if( ret > 0 )
	{
		if(NULL != strstr(buf, ">ENABLE"))
		{
			ret = 1;
			//LogDbg("led alarm on");
		}
		else
		{
			ret = 0;
			//LogDbg("led alarm off, buf:%s", buf);
		}
	}
	else
	{
		//LogDbg("led alarm error, ret=%d, buf:%s", ret, buf);
	}
	return ret;
}



