#include <openssl/crypto.h>
#include <curl/curl.h>
#include <pthread.h>

static pthread_mutex_t g_ssl_lock[CRYPTO_NUM_LOCKS];

static void ssl_lock_callback(int mode, int type, const char *file, int line)
{
	if (mode & CRYPTO_LOCK) {
		pthread_mutex_lock(&(g_ssl_lock[type]));
	} else {
		pthread_mutex_unlock(&(g_ssl_lock[type]));
	}
}

void ssl_mutilthread_init(void)
{
	/* This function must be called at least once within a program (a program is all the code that shares a memory space)
	 * before the program calls any other function in libcurl. The environment it sets up is constant for the life of the
	 * program and is the same for every program, so multiple calls have the same effect as one call. */
	curl_global_init(CURL_GLOBAL_ALL);

	for (int i = 0; i < CRYPTO_NUM_LOCKS; i++) {
		pthread_mutex_init(&g_ssl_lock[i], NULL);
	}

	CRYPTO_set_id_callback(pthread_self);
	CRYPTO_set_locking_callback(ssl_lock_callback);
}
