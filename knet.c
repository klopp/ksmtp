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

int knet_get_ksocket( ksocket sd )
{
    return sd->sock;
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

int knet_resolve_name( const char *name, struct hostent *hent )
{
    struct hostent *him;
    him = gethostbyname( name );
    if( him )
    {
        memcpy( hent, him, sizeof(struct hostent) );
        return 1;
    }
    return 0;
}

int knet_connect( ksocket sd, const char * host, int port )
{
    struct sockaddr_in sin;
    struct hostent him;

    memset( &sin, 0, sizeof(struct sockaddr_in) );
    memset( &him, 0, sizeof(struct hostent) );

    if( knet_resolve_name( host, &him ) )
    {
        int sock;
        sin.sin_family = PF_INET;
        sin.sin_port = htons( port );
        memcpy( &sin.sin_addr.s_addr, him.h_addr_list[0],
                sizeof(sin.sin_addr.s_addr) );
        sock = socket( sin.sin_family, SOCK_STREAM, 0 );
        if( sock > 0 )
        {
            if( connect( sock, (struct sockaddr *)&sin,
                    sizeof(struct sockaddr_in) ) >= 0 )
            {
                sd->sock = sock;
                sd->bufptr = sd->buf;
                return 1;
            }
        }
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
    if( sd )
    {
        if( sd->sock )
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
}

int knet_getc( ksocket sd )
{
    int retval = 1;

    if( sd->avail <= 0 )
    {
        int recval = 0;
        sd->eom = 0;
        sd->flags = 0;
        memset( sd->buf, 0, SOCK_BUF_LEN );
        if( !sd->ssl )
        {
            recval = recv( sd->sock, sd->buf, SOCK_BUF_LEN, 0 );
        }
        else
        {
            recval = SSL_read( sd->ssl, sd->buf, SOCK_BUF_LEN );
        }
        if( recval == 0 )
        {
            sd->flags |= _SOCKET_EOF;
            retval = -1;
        }
        else if( recval == -1 )
        {
            sd->flags |= _SOCKET_ERROR;
            sd->error = errno;
            retval = -1;
        }
        else
        {
            sd->bufptr = sd->buf;
            sd->avail = recval;
            if( recval < SOCK_BUF_LEN )
            {
                sd->eom = 1;
            }
            else
            {
                sd->eom = 0;
            }
        }
    }

    if( sd->avail > 0 )
    {
        sd->avail--;
        if( sd->avail == 0 && sd->eom )
        {
            sd->flags |= _SOCKET_EOF;
        }
        retval = *sd->bufptr++;
    }
    return retval;
}

/*
 int knet_putc( ksocket sd, int ch )
 {
 //int retval = _SUCCESS;

 if( !sd->ssl )
 {
 if( send( sd->sock, (char *)&ch, 1, 0 ) != 1 )
 {
 sd->flags |= _SOCKET_ERROR;
 sd->error = errno;
 return 0;
 //retval = _ERROR;
 }
 }
 else
 {
 if( SSL_write( sd->ssl, (char *)&ch, 1 ) == -1 )
 {
 sd->flags |= _SOCKET_ERROR;
 sd->error = errno;
 return 0;
 //retval = _ERROR;
 }
 }
 return 1;
 //return retval;
 }
 */

/*
 int dnet_readln( ksocket sd, dstrbuf *buf )
 {
 int ch, size = 0;

 do
 {
 ch = knet_getc( sd );
 if( ch == -1 )
 {
 // Error
 break;
 }
 else
 {
 dsbCatChar( buf, ch );
 size++;
 }
 } while( ch != '\n' && !knet_eof( sd ) );
 return size;
 }
 */

int knet_write( ksocket sd, const char *buf, size_t len )
{
    size_t sent = 0;

    if( !sd->ssl )
    {
        while( len > 0 )
        {
            int bytes = send( sd->sock, buf,
                    (len > SOCK_BUF_LEN) ? SOCK_BUF_LEN : len, 0 );
            if( bytes == -1 )
            {
                if( errno == EAGAIN )
                {
                    continue;
                }
                else if( errno == EINTR )
                {
                    continue;
                }
                else
                {
                    sd->flags |= _SOCKET_ERROR;
                    sd->error = errno;
                    break;
                }
            }
            else if( bytes == 0 )
            {
                sd->flags |= _SOCKET_ERROR;
                sd->error = EPIPE;
                break;
            }
            else if( bytes > 0 )
            {
                buf += bytes;
                len -= bytes;
                sent += bytes;
            }
        }
    }
    else
    {
        sent = SSL_write( sd->ssl, buf, len );
    }

    return sent;
}

int knet_read( ksocket sd, char *buf, size_t size )
{
    size_t i;

    for( i = 0; i < size; i++ )
    {
        int ch = knet_getc( sd );
        if( ch == -1 )
        {
            i = 0;
            break;
        }
        *buf++ = ch;
        if( knet_eof( sd ) )
        {
            break;
        }
    }
    return i;
}

