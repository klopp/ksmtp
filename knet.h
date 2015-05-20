/*
 * knet.h, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 02:41
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#ifndef KNET_H_
#define KNET_H_

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

#ifndef __WINDOWS__
#include <sys/utsname.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/rand.h>
#include <netdb.h>
#else
#include <winsock2.h>
#include <process.h>
#include "openssl/ssl.h"
#include "openssl/rand.h"
#if defined(_MSC_VER)
# include <io.h>
#endif
#endif

typedef struct sockaddr SA;
typedef struct sockaddr_in SIN;

enum
{
    /*_ERROR = -6, _SUCCESS = 1, */_SOCKET_ERROR = 0x01, _SOCKET_EOF = 0x02
};

#define MAXSOCKBUF  4096

typedef struct _ksocket
{
    int eom;
    int sock;
    int flags;
    int error;
    int avail;
    char *bufptr;
    char *buf;
    SSL_CTX *ctx;
    SSL *ssl;
}*ksocket;

ksocket knet_connect( const char *host, int port );
int knet_read( ksocket sd, char *buf, size_t size );
int knet_write( ksocket sd, const char *buf, size_t size );
int knet_getc( ksocket sd );
int knet_verify_sert( ksocket sd );
void knet_close( ksocket sd );
int knet_use_tls( ksocket sd );
int knet_error( ksocket sd );
char * knet_error_msg( ksocket sd );
int knet_eof( ksocket sd );

#endif /* KNET_H_ */
