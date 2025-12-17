#include <mqtt.h>
#include <libcommon.h>

/** 
 * @file 
 * @brief Implements @ref mqtt_pal_sendall and @ref mqtt_pal_recvall and 
 *        any platform-specific helpers you'd like.
 * @cond Doxygen_Suppress
 */


#ifdef __unix__
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>

#ifdef TLS_OPENSSL
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

int open_nb_socket_tls(BIO** bio, SSL_CTX** ssl_ctx, const char* addr,
        const char* port, const char* ca_file, const char* ca_path)
{
    *ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    SSL* ssl;

    /* load certificate */
    if (!SSL_CTX_load_verify_locations(*ssl_ctx, ca_file, ca_path)) {
    	SSL_CTX_free(*ssl_ctx);
        printf("error: failed to load certificate\n");
        return -1;
    }

    /* open BIO socket */
    *bio = BIO_new_ssl_connect(*ssl_ctx);
    BIO_get_ssl(*bio, &ssl);
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    BIO_set_conn_hostname(*bio, addr);
    BIO_set_nbio(*bio, 1);
    BIO_set_conn_port(*bio, port);

    /* wait for connect with 10 second timeout */
    int start_time = time(NULL);
    while(BIO_do_connect(*bio) <= 0 && (int)time(NULL) - start_time < 20);
    if (BIO_do_connect(*bio) <= 0) {
        printf("bio connect error: %s\n", ERR_reason_error_string(ERR_get_error()));
        BIO_free_all(*bio);
        SSL_CTX_free(*ssl_ctx);
        *bio = NULL;
        *ssl_ctx=NULL;
        return -2;
    }

    /* verify certificate */
    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        /* Handle the failed verification */
        printf("error: x509 certificate verification failed\n");
        BIO_free_all(*bio);
        SSL_CTX_free(*ssl_ctx);
        *bio = NULL;
        *ssl_ctx=NULL;
        return -3;
    }
    return 0;
}

ssize_t mqtt_pal_sendall_tls(mqtt_pal_socket_handle fd, const void* buf, size_t len, int flags) {
    size_t sent = 0;
    while(sent < len) {
        int tmp = BIO_write(fd, buf + sent, len - sent);
        if (tmp > 0) {
            sent += (size_t) tmp;
        } else if (tmp <= 0 && !BIO_should_retry(fd)) {
            return MQTT_ERROR_SOCKET_ERROR;
        }
    }
    
    return sent;
}


ssize_t mqtt_pal_recvall_tls(mqtt_pal_socket_handle fd, void* buf, size_t bufsz, int flags) {
    const void const *start = buf;
    int rv;
    do {
        rv = BIO_read(fd, buf, bufsz);
        if (rv > 0) {
            /* successfully read bytes from the socket */
            buf += rv;
            bufsz -= rv;
        } 
        else if(rv==0 && (!BIO_should_read(fd)))
        {
            break;
        }
        else if (!BIO_should_retry(fd)) {
            /* an error occurred that wasn't "nothing to read". */
            LogError("BIO_read rv:%d, error:%d-%s", rv, errno, strerror(errno));
            return MQTT_ERROR_SOCKET_ERROR;
        }
    } while (!BIO_should_read(fd));

    return (ssize_t)(buf - start);
}

#endif

static int open_nb_socket(const char* addr, const char* port)
{
    struct addrinfo hints = {0};

    hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM; /* Must be TCP */
    int sockfd = -1;
    int rv;
    struct addrinfo *p, *servinfo;

    //printf("%s, create socket\n", __func__);
    /* get address information */
    rv = getaddrinfo(addr, port, &hints, &servinfo);
    if(rv != 0) {
        printf("Failed to open socket (getaddrinfo): %s\n", gai_strerror(rv));
        return -1;
    }

    /* open the first possible socket */
    for(p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        /* connect to server */
        rv = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
        if(rv == -1)
        {
            close(sockfd);
            continue;
        }
        break;
    }

    /* free servinfo */
    freeaddrinfo(servinfo);

    /* make non-blocking */
    if (sockfd != -1) fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);

    /* return the new socket fd */
    return sockfd;
}


ssize_t mqtt_pal_sendall(int fd, const void* buf, size_t len, int flags) {
    size_t sent = 0;
    while(sent < len) {
        ssize_t tmp = send(fd, buf + sent, len - sent, flags);
        if (tmp < 1) {
            return MQTT_ERROR_SOCKET_ERROR;
        }
        sent += (size_t) tmp;
    }
    return sent;
}

ssize_t mqtt_pal_recvall(int fd, void* buf, size_t bufsz, int flags) {
    const void const *start = buf;
    ssize_t rv;
    do {
        rv = recv(fd, buf, bufsz, flags);
        if (rv > 0) {
            /* successfully read bytes from the socket */
            buf += rv;
            bufsz -= rv;
        } else if (rv < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
            /* an error occurred that wasn't "nothing to read". */
            return MQTT_ERROR_SOCKET_ERROR;
        }
    } while (rv > 0);

    return buf - start;
}

//#endif
int mqtt_pal_close_socket(mqtt_pal_socket_handle  handle, enum TLSVersion version)
{
	printf("%s mqtt close socket %d\n",__func__, version);
	if(version == TLS0)
	{
		int *fb = (int *)handle;
		close(*fb);
		*fb = -1;
	}
#ifdef TLS_OPENSSL
	else
	{
		struct tls_context *bio_hd = (struct tls_context *)handle;
		//printf("%s  pal tls %p %p\n", __func__, bio_hd->bio, bio_hd->ssl_ctx);
	    if(bio_hd->bio)
	    {
	    	//BIO_reset(bio_hd->bio);
	    	BIO_free_all(bio_hd->bio);
	    	bio_hd->bio = 0;
	    }
	    if(bio_hd->ssl_ctx)
	    {
	        SSL_CTX_free(bio_hd->ssl_ctx);
	        bio_hd->ssl_ctx=0;
	    }
		ERR_free_strings();
		EVP_cleanup();
    }
#endif
    if(handle)
    {
    	free(handle);
    	handle = 0;
    }
    return 0;
}

mqtt_pal_socket_handle *mqtt_open_ssl_socket(const char *addr,        const char *port,const char *ca_file)
{
	struct tls_context *context = NULL;
	SSL* ssl = NULL;
	SSL_CTX *ssl_ctx = NULL;
	BIO *bio = NULL;

    SSL_load_error_strings();
    ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();
    SSL_library_init();
    context = (struct tls_context *)malloc(sizeof(struct tls_context));
	if( !context ) {
        LogError("malloc failed");
        goto errout;
	}
	memset(context,0,sizeof(*context));

	ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    if (!SSL_CTX_load_verify_locations(ssl_ctx, ca_file, NULL)) {
        LogError("failed to load certificate");
        goto errout;
    }

    /* open BIO socket */
    bio = BIO_new_ssl_connect(ssl_ctx);
	if( !bio ) {
        LogError("BIO_new_ssl_connect failed");
        goto errout;
	}
    BIO_get_ssl(bio, &ssl);
	if( !ssl ) {
        LogError("BIO_get_ssl failed");
        goto errout;
	}
    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    BIO_set_conn_hostname(bio, addr);
    BIO_set_nbio(bio, 1);
    BIO_set_conn_port(bio, port);

	time_t start = time(0);
	long rv = 0;
	while( time(0)-start<20 && (rv=BIO_do_connect(bio))<=0 ) util_msleep(10);
	if( rv<=0 ) {
        LogError("BIO_do_connect failed");
        goto errout;
	}
    if (SSL_get_verify_result(ssl) != X509_V_OK) {
        /* Handle the failed verification */
        LogError("x509 certificate verification failed");
        goto errout;
    }
	LogInfo("Open SSL Socket success");
	context->bio = bio;
	context->ssl_ctx = ssl_ctx;
    return (mqtt_pal_socket_handle)context;
errout:
	if( context ) free(context);
	if( ssl_ctx ) SSL_CTX_free(ssl_ctx);
	if( bio ) BIO_free_all(bio);
	ERR_free_strings();
	EVP_cleanup();
	return NULL;
}

int  mqtt_pal_open_socket(mqtt_pal_socket_handle * handle, enum TLSVersion version, const char* addr,
        const char* port, const char* ca_file, const char* ca_path)
{
	int ret = 0;
	printf("%s start ssl version %d\n", __func__, version);
    if(version  == TLS0)
    {
    	int *fb;
    	ret = open_nb_socket(addr, port);
        if(ret<0)
        {
            printf("mqtt pal open socket error:%d,%s\n",ret,strerror(errno));
            return MQTT_ERROR_SOCKET_ERROR;
        }
        fb = (int *)malloc(sizeof(int));
        *fb = ret;
        *handle = (mqtt_pal_socket_handle *)fb;
        //printf("%s open socket http %d\n ", __func__, *fb);
    }
#ifdef TLS_OPENSSL
    else
    {
        SSL_load_error_strings();
        ERR_load_BIO_strings();
        OpenSSL_add_all_algorithms();
        SSL_library_init();
        struct tls_context *context;
        context= (struct tls_context *)malloc(sizeof(struct tls_context));
        //printf("%s start !!!!!!!!!!!!!!!!\n", __func__);

    	ret = open_nb_socket_tls(&(context->bio), &(context->ssl_ctx), addr, port,ca_file,ca_path);
        if(ret<0)
        {
            printf("mqtt pal open  tls socket error:%d,%s\n",ret,strerror(errno));
            free(context);
            context = 0;
    		ERR_free_strings();
    		EVP_cleanup();
            return MQTT_ERROR_SOCKET_ERROR;
        }
        *handle = (mqtt_pal_socket_handle *)context;
        //printf("%s openssl %p %p\n", __func__, context, context->ssl_ctx);
        //printf("%s  pal tls %p %p\n", __func__, context->bio, context->ssl_ctx);
    }
#else
    *handle  = NULL;
    return MQTT_ERROR_SOCKET_ERROR;
#endif
    //printf("%s end \n", __func__);
    return ret;
}

ssize_t mqtt_pal_recv(enum TLSVersion version, mqtt_pal_socket_handle fd, void* buf, size_t bufsz, int flags)
{
	ssize_t ret = 0;
	//printf("%s mqtt recv \n", __func__);
	if(fd == NULL) return MQTT_ERROR_SOCKET_ERROR;

    if(version  == TLS0)
    {
    	int fb = *(int *)fd;
    	ret = mqtt_pal_recvall(fb, buf, bufsz, flags);
    }
#ifdef TLS_OPENSSL
    else
    {
    	struct tls_context * handle = (struct tls_context *)fd;
    	//printf("%s openssl %d %d\n", __func__, handle, handle->ssl_ctx);
    	ret = mqtt_pal_recvall_tls(handle->bio, buf, bufsz, flags);
    }
#endif
    return ret;
}



ssize_t mqtt_pal_send(enum TLSVersion version, mqtt_pal_socket_handle fd, const void* buf, size_t len, int flags)
{
	ssize_t ret = 0;
	//printf("%s mqtt send \n", __func__);
	if(fd == NULL) return MQTT_ERROR_SOCKET_ERROR;
    if(version  == TLS0)
    {
    	int fb = *(int *)fd;
    	ret = mqtt_pal_sendall(fb, buf, len, flags);
    }
#ifdef TLS_OPENSSL
    else
    {
    	struct tls_context * handle = (struct tls_context *)fd;
    	//printf("%s mqtt %p %p \n", __func__, handle->bio);
    	ret = mqtt_pal_sendall_tls(handle->bio, buf, len, flags);
    }
#endif
    return ret;
}
#endif

/** @endcond */