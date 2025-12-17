#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mqueue.h>
#include <errno.h>

#include <key.h>
#include <libcommon.h>

void send_key_mqmsg(unsigned char index, char state)
{
	key_info_t key_info;
	struct mq_attr mq_attr = {
		0,   //mq_flags
		10,  //mq_maxmsg
		256, //mq_msgsize
		0,   //mq_curmsgs
	};

	key_info.index = index;
	key_info.state = state;
	mqd_t mqId = mq_open(KEYBOARD_MQ, O_WRONLY | O_NONBLOCK, 0666, &mq_attr);
	if (mqId < 0) {
		LogInfo("mqId[%d] errorno[%d] errorstr[%s]", mqId, errno, strerror(errno));
		return;
	}

	if (mq_send(mqId, (void *)&key_info, sizeof(key_info), 0) < 0)
		LogInfo("press mq_send errorno[%d] errorstr[%s]", errno, strerror(errno));
	mq_close(mqId);
}
