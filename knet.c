/*
 * knet.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 02:40
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "knet.h"
#include "../stringlib/stringlib.h"

static const char * _knet_ssl_error( int err )
{
    static char _ssl_err_buf[64];
    switch( err )
    {
        case SSL_ERROR_NONE:
            return "No SSL errors.";

        case SSL_ERROR_WANT_READ:
        case SSL_ERROR_WANT_WRITE:
            return "The operation did not complete; "
                    "the same TLS/SSL I/O function should be called again later.";

        case SSL_ERROR_ZERO_RETURN:
            return "The TLS/SSL connection has been closed.";

        case SSL_ERROR_WANT_CONNECT:
        case SSL_ERROR_WANT_ACCEPT:
            return "The underlying BIO was not connected yet to the peer and "
                    "the call would block in connect()/accept(). The SSL function "
                    "should be called again when the connection is established.";

        case SSL_ERROR_WANT_X509_LOOKUP:
            return "The operation did not complete because an application callback "
                    "set by SSL_CTX_set_client_cert_cb() has asked to be called "
                    "again.  The TLS/SSL I/O function should be called again later.";

        case SSL_ERROR_SYSCALL:
            return "Some I/O error occurred.  The OpenSSL error queue may contain "
                    "more information on the error.";

        case SSL_ERROR_SSL:
            return "A failure in the SSL library occurred, usually a protocol "
                    "error. The OpenSSL error queue contains more information "
                    "on the error.";

        default:
            break;
    }
    sprintf( _ssl_err_buf, "Unknown SSL error (%d)", err );
    return _ssl_err_buf;
}

const char * knet_error( ksocket sd )
{
#if defined(__WINDOWS__)
    static char m[1024];
#endif
    if( sd->ssl && sd->ssl_error != SSL_ERROR_NONE ) return _knet_ssl_error(
            sd->ssl_error );
#if defined(__WINDOWS__)
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, sd->error,
            MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
            m, sizeof(m) - 1, NULL);
    chomp(m);
    return m;
#else
    return strerror( sd->error );
#endif
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

static int _knet_init( void )
{
#if defined(__WINDOWS__)
    WSADATA wsaData;
    WORD wVer = MAKEWORD(2, 2);
    if (WSAStartup(wVer, &wsaData) != NO_ERROR)
    {
        return 0;
    }
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
    {
        WSACleanup();
        return 0;
    }
#endif
    return 1;
}

static void _knet_down( void )
{
#if defined(__WINDOWS__)
    WSACleanup();
#endif
}

int knet_connect( ksocket sd, const char * host, int port )
{
    struct sockaddr_in sa;
    struct hostent * he;
    /*
     struct timeval timeout;
     fd_set fdwrite, fdexcept;
     int rc;
     */
    unsigned long nonblock = 1;

    if( sd->sock >= 0 ) return 1;
    sd->ssl_error = SSL_ERROR_NONE;
    if( !_knet_init() ) return 0;

    he = gethostbyname( host );
    if( !he )
    {
        sd->error = WSAGetLastError();
        return 0;
    }

    /*
     timeout.tv_sec = sd->timeout;
     timeout.tv_usec = 0;
     */

    memset( &sa, 0, sizeof(sa) );
    sa.sin_family = PF_INET;
    sa.sin_port = htons( port );
    memcpy( &sa.sin_addr.s_addr, he->h_addr_list[0],
            sizeof(sa.sin_addr.s_addr) );

    sd->sock = socket( PF_INET, SOCK_STREAM, 0 );
    if( sd->sock < 0 )
    {
        sd->error = WSAGetLastError();
        return 0;
    }

    if( ioctlsocket( sd->sock, FIONBIO, &nonblock ) == SOCKET_ERROR )
    {
        sd->error = WSAGetLastError();
        closesocket( sd->sock );
        return 0;
    }

    if( connect( sd->sock, (struct sockaddr*)&sa, sizeof(sa) ) == SOCKET_ERROR )
    {
        if( WSAGetLastError() != WSAEWOULDBLOCK )
        {
            sd->error = WSAGetLastError();
            closesocket( sd->sock );
            return 0;
        }
    }
    /*
     while( 1 )
     {
     FD_ZERO( &fdwrite );
     FD_ZERO( &fdexcept );

     FD_SET( sd->sock, &fdwrite );
     FD_SET( sd->sock, &fdexcept );

     if( (rc = select( sd->sock + 1, NULL, &fdwrite, &fdexcept, &timeout ))
     == SOCKET_ERROR )
     {
     sd->error = WSAGetLastError();
     closesocket( sd->sock );
     return 0;
     }
     if( !rc )
     {
     sd->error = ETIME;
     closesocket( sd->sock );
     return 0;
     }
     if( rc && FD_ISSET( sd->sock, &fdwrite ) ) break;
     if( rc && FD_ISSET( sd->sock, &fdexcept ) )
     {
     sd->error = SOCKET_ERROR;
     closesocket( sd->sock );
     return 0;
     }
     }
     FD_CLR( sd->sock, &fdwrite );
     FD_CLR( sd->sock, &fdexcept );
     */

    return 1;
}

int knet_init_tls( ksocket sd )
{
    struct timeval timeout;
    fd_set fdread, fdwrite;
    int rc;

    sd->ssl_error = SSL_ERROR_NONE;
    _rand_seed();
    SSL_load_error_strings();
    if( SSL_library_init() == -1 )
    {
        sd->ssl_error = SSL_ERROR_SSL;
        return 0;
    }
#ifndef __WINDOWS__
    sd->ctx = SSL_CTX_new( TLSv1_client_method() );
#else
    sd->ctx = SSL_CTX_new(SSLv23_client_method());
#endif
    if( !sd->ctx ) return 0;
    sd->ssl = SSL_new( sd->ctx );
    if( !sd->ssl )
    {
        SSL_CTX_free( sd->ctx );
        sd->ctx = NULL;
        sd->ssl_error = SSL_ERROR_SSL;
        return 0;
    }
    SSL_set_fd( sd->ssl, sd->sock );
    SSL_set_mode( sd->ssl, SSL_MODE_AUTO_RETRY );

    timeout.tv_sec = sd->timeout;
    timeout.tv_usec = 0;

    while( 1 )
    {
        FD_ZERO( &fdwrite );
        FD_ZERO( &fdread );
        FD_SET( sd->sock, &fdwrite );
        FD_SET( sd->sock, &fdread );

        if( (rc = select( sd->sock + 1, NULL, &fdread, &fdwrite, &timeout ))
                < 0 )
        {
            sd->error = WSAGetLastError();
            break;
        }
        if( rc == 0 ) continue;
        if( !FD_ISSET( sd->sock, &fdwrite ) && !FD_ISSET( sd->sock, &fdread ) ) continue;
        rc = SSL_connect( sd->ssl );

        switch( (rc = SSL_get_error( sd->ssl, rc )) )
        {
            case SSL_ERROR_NONE:
                return 1;

            case SSL_ERROR_WANT_READ:
                break;
            case SSL_ERROR_WANT_WRITE:
                break;

            default:
                SSL_shutdown( sd->ssl );
                SSL_free( sd->ssl );
                SSL_CTX_free( sd->ctx );
                sd->ctx = NULL;
                sd->ssl = NULL;
                sd->ssl_error = rc;
                return 0;
        }
    }
    return 0;
}

int knet_verify_sert( ksocket sd )
{
    X509 *cert = SSL_get_peer_certificate( sd->ssl );
    if( !cert ) return 0;
    X509_free( cert );
    return 1;
}

void knet_disconnect( ksocket sd )
{
    if( sd->sock >= 0 )
    {
        closesocket( sd->sock );
    }
    if( sd->ssl )
    {
        SSL_shutdown( sd->ssl );
        SSL_free( sd->ssl );
    }
    if( sd->ctx ) SSL_CTX_free( sd->ctx );
    sd->sock = -1;
    sd->ssl = NULL;
    sd->ctx = NULL;
    _knet_down();
}

static int _knet_write_socket( ksocket sd, const char * buf, size_t sz )
{
    int rc;
    size_t left = sz;
    fd_set fdwrite;
    struct timeval time;

    time.tv_sec = sd->timeout;
    time.tv_usec = 0;

    while( left )
    {
        FD_ZERO( &fdwrite );
        FD_SET( sd->sock, &fdwrite );

        if( (rc = select( sd->sock + 1, NULL, &fdwrite, NULL, &time ))
                == SOCKET_ERROR )
        {
            sd->error = WSAGetLastError();
            return -1;
        }
        if( !rc )
        {
            sd->error = WSAETIMEDOUT;
            return -1;
        }

        if( rc && FD_ISSET( sd->sock, &fdwrite ) )
        {
            rc = send( sd->sock, buf, left, 0 );
            if( rc == SOCKET_ERROR || rc == 0 )
            {
                sd->error = WSAGetLastError();
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
    int ssl_err = 0;

    timeout.tv_sec = sd->timeout;
    timeout.tv_usec = 0;

    sd->ssl_error = SSL_ERROR_NONE;
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
                == SOCKET_ERROR )
        {
            sd->error = WSAGetLastError();
            return -1;
        }
        if( rc == 0 )
        {
            continue;
        }

        if( FD_ISSET( sd->sock, &fdwrite )
                || (write_blocked_on_read && FD_ISSET( sd->sock, &fdread )) )
        {
            write_blocked_on_read = 0;

            rc = SSL_write( sd->ssl, buf, left );

            switch( (ssl_err = SSL_get_error( sd->ssl, rc )) )
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
                    sd->error = WSAGetLastError();
                    sd->ssl_error = ssl_err;
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
    sd->ssl_error = SSL_ERROR_NONE;

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
                == SOCKET_ERROR )
        {
            sd->error = WSAGetLastError();
            return -1;
        }
        if( rc == 0 ) break;

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
                    sd->error = WSAGetLastError();
                    sd->ssl_error = ssl_err;
                    return -1;
                }
            }
        }
    }
    return readed;
}

static int _knet_read_socket( ksocket sd )
{
    int rc = 0;
    fd_set fdread;
    struct timeval time;

    time.tv_sec = sd->timeout;
    time.tv_usec = 0;

    FD_ZERO( &fdread );
    FD_SET( sd->sock, &fdread );

    if( (rc = select( sd->sock + 1, &fdread, NULL, NULL, &time ))
            == SOCKET_ERROR )
    {
        sd->error = WSAGetLastError();
        return 0;
    }
    if( !rc )
    {
        sd->error = WSAETIMEDOUT;
        return 0;
    }

    if( FD_ISSET( sd->sock, &fdread ) )
    {
        rc = recv( sd->sock, sd->buf, SOCK_BUF_LEN, 0 );
        if( rc == SOCKET_ERROR )
        {
            sd->error = WSAGetLastError();
            return 0;
        }
    }

    return rc;
}

static int _knet_getbuf( ksocket sd )
{
    int rc;
    sd->eof = 0;
    sd->error = 0;
    sd->inbuf = sd->cursor = 0;
    memset( sd->buf, 0, SOCK_BUF_LEN );
    rc = sd->ssl ? _knet_read_ssl( sd ) : _knet_read_socket( sd );
    if( rc == 0 )
    {
        sd->eof = 1;
        return -1;
    }
    else if( rc == -1 )
    {
        //        sd->error = errno;
        return -1;
    }
    sd->inbuf = rc;
    return rc;
}

int knet_read( ksocket sd, char * buf, size_t sz )
{
    size_t readed = 0;
    while( readed < sz )
    {
        size_t tomove;
        if( sd->cursor >= sd->inbuf )
        {
            if( _knet_getbuf( sd ) == -1 ) return 0;
        }
        tomove = sz - readed;
        if( tomove > (size_t)(sd->inbuf - sd->cursor) ) tomove = sd->inbuf - sd->cursor;
        memcpy( buf, sd->buf + sd->cursor, tomove );
        buf += tomove;
        readed += tomove;
        sd->cursor += tomove;
    }
    return readed;
}

int knet_getc( ksocket sd )
{
    char c;
    return knet_read( sd, &c, 1 ) ? ((int)c & 0xFF) : -1;
}

