#include "libcommon.h"
#include "glbvar.h"

pthread_attr_t g_pthread_attr;
pthread_mutexattr_t g_mutex_attr;
int g_process_running;

int glbvar_init(void)
{
	if (pthread_attr_init(&g_pthread_attr)) {
		LogFatal("pthread_attr_init() failed.");
		return -1;
	}
	if (pthread_attr_setdetachstate(&g_pthread_attr, PTHREAD_CREATE_DETACHED)) {
		LogFatal("pthread_attr_setdetachstate() failed.");
		return -1;
	}

	if (pthread_mutexattr_init(&g_mutex_attr)) {
		LogError("pthread_mutexattr_init() failed.");
		return -1;
	}
	if (pthread_mutexattr_settype(&g_mutex_attr, PTHREAD_MUTEX_RECURSIVE)) {
		LogError("pthread_mutexattr_settype() failed.");
		return -1;
	}

	g_process_running = 1;

	return 0;
}
