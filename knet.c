/*
 * knet.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 02:40
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "knet.h"

int knet_error( ksocket sd )
{
    return sd->flags & _SOCKET_ERROR;
}

int knet_eof( ksocket sd )
{
    return sd->flags & _SOCKET_EOF;
}

char * knet_error_msg( ksocket sd )
{
    return strerror( sd->error );
}

static void _rand_seed( void )
{
    struct
    {
        time_t tt;
        pid_t pid;
#ifndef __WINDOWS__
        uid_t uid;
        uid_t euid;
        gid_t gid;
        gid_t egid;
#endif
    } data;

    data.tt = time( 0 );
    data.pid = getpid();
#ifndef __WINDOWS__
    data.uid = getuid();
    data.euid = geteuid();
    data.gid = getgid();
    data.egid = getegid();
#endif
    RAND_seed( &data, sizeof(data) );
}

int knet_init( void )
{
#if defined(__WINDOWS__)
    WSADATA wsaData;
    WORD wVer = MAKEWORD(2,2);
    if (WSAStartup(wVer,&wsaData) != NO_ERROR)
    {
        return 0;
    }
    if (LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 2 )
    {
        WSACleanup();
        return 0;
    }
#endif
    return 1;
}

void knet_down( void )
{
#if defined(__WINDOWS__)
    WSACleanup();
#endif
}

int knet_connect( ksocket sd, const char * host, int port )
{
    struct sockaddr_in sa;
    struct hostent * he = gethostbyname( host );
    if( !he ) return 1;

    sd->sock = socket( PF_INET, SOCK_STREAM, 0 );
    if( sd->sock > 0 )
    {
        memset( &sa, 0, sizeof(sa) );
        sa.sin_family = PF_INET;
        sa.sin_port = htons( port );
        memcpy( &sa.sin_addr.s_addr, he->h_addr_list[0],
                sizeof(sa.sin_addr.s_addr) );
        if( connect( sd->sock, (struct sockaddr *)&sa, sizeof(sa) ) >= 0 )
        {
            return 1;
        }
#if defined(_MSC_VER)
        closeksocket( sd->sock );
#else
        close( sd->sock );
#endif
        sd->sock = -1;
    }
    return 0;
}

int knet_use_tls( ksocket sd )
{
    _rand_seed();
    SSL_load_error_strings();
    if( SSL_library_init() == -1 ) return 0;
#ifndef __WINDOWS__
    sd->ctx = SSL_CTX_new( TLSv1_client_method() );
#else
    sd->ctx = SSL_CTX_new (SSLv23_client_method());
#endif
    if( !sd->ctx ) return 0;
    sd->ssl = SSL_new( sd->ctx );
    if( !sd->ssl )
    {
        SSL_CTX_free( sd->ctx );
        sd->ctx = NULL;
        return 0;
    }
    SSL_set_fd( sd->ssl, sd->sock );
    SSL_set_mode( sd->ssl, SSL_MODE_AUTO_RETRY );
    if( SSL_connect( sd->ssl ) == -1 )
    {
        SSL_CTX_free( sd->ctx );
        SSL_free( sd->ssl );
        sd->ssl = NULL;
        sd->ctx = NULL;
        return 0;
    }
    return 1;
}

int knet_verify_sert( ksocket sd )
{
    X509 *cert = SSL_get_peer_certificate( sd->ssl );
    if( !cert ) return 0;
    X509_free( cert );
    return 1;
}

void knet_close( ksocket sd )
{
    if( sd->sock >= 0 )
    {
#if defined(_MSC_VER)
        closeksocket( sd->sock );
#else
        close( sd->sock );
#endif
    }
    if( sd->ssl )
    {
        SSL_shutdown( sd->ssl );
        SSL_free( sd->ssl );
        if( sd->ctx ) SSL_CTX_free( sd->ctx );
    }
}

static int _knet_write_socket( ksocket sd, const char * buf, size_t sz )
{
    int rc;
    size_t left = sz;
    fd_set fdwrite;
    struct timeval timeout;

    timeout.tv_sec = sd->timeout;
    timeout.tv_usec = 0;

    while( left )
    {
        FD_ZERO( &fdwrite );
        FD_SET( sd->sock, &fdwrite );

        if( (rc = select( sd->sock + 1, NULL, &fdwrite, NULL, &timeout ))
                == -1 )
        {
            sd->flags |= _SOCKET_ERROR;
            sd->error = errno;
            return -1;
        }
        if( rc == 0 )
        {
            sd->flags |= _SOCKET_ERROR;
            sd->error = EPIPE;
            break;
        }

        if( FD_ISSET( sd->sock, &fdwrite ) )
        {
            rc = send( sd->sock, buf, left, 0 );
            if( rc == -1 || rc == 0 )
            {
                sd->flags |= _SOCKET_ERROR;
                sd->error = errno;
                return -1;
            }
            left -= rc;
            buf += rc;
        }
    }
    return sz - left;
}

static int _knet_write_ssl( ksocket sd, const char * buf, size_t sz )
{
    int rc;
    size_t left = sz;
    fd_set fdwrite;
    fd_set fdread;
    struct timeval timeout;
    int write_blocked_on_read = 0;

    timeout.tv_sec = sd->timeout;
    timeout.tv_usec = 0;

    while( left )
    {
        FD_ZERO( &fdwrite );
        FD_ZERO( &fdread );
        FD_SET( sd->sock, &fdwrite );

        if( write_blocked_on_read )
        {
            FD_SET( sd->sock, &fdread );
        }

        if( (rc = select( sd->sock + 1, &fdread, &fdwrite, NULL, &timeout ))
                == -1 )
        {
            sd->flags |= _SOCKET_ERROR;
            sd->error = errno;
            return -1;
        }

        if( rc == 0 )
        {
            sd->flags |= _SOCKET_ERROR;
            sd->error = EPIPE;
            return -1;
        }

        if( FD_ISSET( sd->sock, &fdwrite )
                || (write_blocked_on_read && FD_ISSET( sd->sock, &fdread )) )
        {
            write_blocked_on_read = 0;

            rc = SSL_write( sd->ssl, buf, left );

            switch( SSL_get_error( sd->ssl, rc ) )
            {
                case SSL_ERROR_NONE:
                    left -= rc;
                    buf += rc;
                    break;

                case SSL_ERROR_WANT_WRITE:
                    break;

                case SSL_ERROR_WANT_READ:
                    write_blocked_on_read = 1;
                    break;

                default:
                    sd->flags |= _SOCKET_ERROR;
                    sd->error = errno;
                    return -1;
            }

        }
    }
    return sz - left;
}
int knet_write( ksocket sd, const char * buf, size_t len )
{
    return sd->ssl ?
            _knet_write_ssl( sd, buf, len ) : _knet_write_socket( sd, buf, len );
}

static int _knet_read_ssl( ksocket sd )
{
    int rc = 0;
    int readed = 0;
    int ssl_err;
    fd_set fdread;
    fd_set fdwrite;
    struct timeval timeout;
    int read_blocked_on_write = 0;
    int bFinish = 0;

    timeout.tv_sec = sd->timeout;
    timeout.tv_usec = 0;

    while( !bFinish )
    {
        FD_ZERO( &fdread );
        FD_ZERO( &fdwrite );
        FD_SET( sd->sock, &fdread );

        if( read_blocked_on_write )
        {
            FD_SET( sd->sock, &fdwrite );
        }

        if( (rc = select( sd->sock + 1, &fdread, &fdwrite, NULL, &timeout ))
                < 0 )
        {
            return -1;
        }

        if( FD_ISSET( sd->sock, &fdread )
                || (read_blocked_on_write && FD_ISSET( sd->sock, &fdwrite )) )
        {
            while( 1 )
            {
                read_blocked_on_write = 0;

                rc = SSL_read( sd->ssl, sd->buf + readed,
                SOCK_BUF_LEN - readed );

                ssl_err = SSL_get_error( sd->ssl, rc );
                if( ssl_err == SSL_ERROR_NONE )
                {
                    readed += rc;
                    if( !SSL_pending( sd->ssl ) )
                    {
                        bFinish = 1;
                        break;
                    }
                }
                else if( ssl_err == SSL_ERROR_ZERO_RETURN )
                {
                    bFinish = 1;
                    break;
                }
                else if( ssl_err == SSL_ERROR_WANT_READ )
                {
                    break;
                }
                else if( ssl_err == SSL_ERROR_WANT_WRITE )
                {
                    read_blocked_on_write = 1;
                    break;
                }
                else
                {
                    return -1;
                }
            }
        }
    }
    return readed;
}

static int _knet_read_socket( ksocket sd )
{
    int rc = -1;
    fd_set fdread;
    struct timeval timeout;

    timeout.tv_sec = sd->timeout;
    timeout.tv_usec = 0;

    FD_ZERO( &fdread );
    FD_SET( sd->sock, &fdread );

    if( (rc = select( sd->sock + 1, &fdread, NULL, NULL, &timeout )) < 0 ) return -1;

    if( FD_ISSET( sd->sock, &fdread ) )
    {
        rc = recv( sd->sock, sd->buf, SOCK_BUF_LEN, 0 );
    }
    return rc;
}

static int _knet_getbuf( ksocket sd )
{
    int rc;
    sd->eom = 0;
    sd->flags = 0;
    sd->inbuf = sd->cursor = 0;
    memset( sd->buf, 0, SOCK_BUF_LEN );
    rc = sd->ssl ? _knet_read_ssl( sd ) : _knet_read_socket( sd );
    if( rc == 0 )
    {
        sd->flags |= _SOCKET_EOF;
        return -1;
    }
    else if( rc == -1 )
    {
        sd->flags |= _SOCKET_ERROR;
        sd->error = errno;
        return -1;
    }
    sd->inbuf = rc;
    if( rc < SOCK_BUF_LEN )
    {
        sd->eom = 1;
    }
    else
    {
        sd->eom = 0;
    }
    return rc;
}

int knet_read( ksocket sd, char * buf, size_t sz )
{
    size_t left = sz;
    while( left )
    {
        if( sd->cursor >= sd->inbuf )
        {
            if( _knet_getbuf( sd ) == -1 ) return 0;
        }
        size_t tomove =
                sd->inbuf - sd->cursor > left ? left : sd->inbuf - sd->cursor;
        memcpy( buf, sd->buf + sd->cursor, tomove );
        buf += tomove;
        left -= tomove;
        sd->cursor += tomove;
    }
    return left ? 0 : sz;
}

int knet_getc( ksocket sd )
{
    char c;
    return knet_read( sd, &c, 1 ) ? ((int)c & 0xFF) : -1;
}

