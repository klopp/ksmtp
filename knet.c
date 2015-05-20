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

char * knet_get_error( ksocket sd )
{
    return strerror( sd->error );
}

int knet_get_ksocket( ksocket sd )
{
    return sd->sock;
}

#ifndef __WINDOWS__
static void _genRandomSeed( void )
{
    struct
    {
        struct utsname uname;
        int uname_1;
        int uname_2;
        uid_t uid;
        uid_t euid;
        gid_t gid;
        gid_t egid;
    } data;

    struct
    {
        pid_t pid;
        time_t time;
        void *stack;
    } uniq;
    data.uname_1 = uname( &data.uname );
    data.uname_2 = errno;
    data.uid = getuid();
    data.euid = geteuid();
    data.gid = getgid();
    data.egid = getegid();

    RAND_seed( &data, sizeof(data) );

    uniq.pid = getpid();
    uniq.time = time( NULL );
    uniq.stack = &uniq;
    RAND_seed( &uniq, sizeof(uniq) );
}
#endif

int knet_init( int ssl )
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
    SSL_load_error_strings();
    if( SSL_library_init() == -1 )
    {
#if defined(__WINDOWS__)
        WSACleanup();
#endif
        return 0;
    }
#ifndef __WINDOWS__
    _genRandomSeed();
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
    int ret = _ERROR;
    struct hostent *him;

    him = gethostbyname( name );
    if( him )
    {
        memcpy( hent, him, sizeof(struct hostent) );
        ret = _SUCCESS;
    }
    return ret;
}

ksocket knet_connect( const char *host, int port )
{
    SIN sin;
    struct hostent him;
    ksocket ssd = NULL;

    memset( &sin, 0, sizeof(SIN) );
    memset( &him, 0, sizeof(struct hostent) );

    if( knet_resolve_name( host, &him ) != _ERROR )
    {
        int sd;
        sin.sin_family = PF_INET;
        sin.sin_port = htons( port );
        memcpy( &sin.sin_addr.s_addr, him.h_addr_list[0],
                sizeof(sin.sin_addr.s_addr) );
        sd = socket( sin.sin_family, SOCK_STREAM, 0 );
        if( sd > 0 )
        {
            if( connect( sd, (SA *)&sin, sizeof(SIN) ) >= 0 )
            {
                ssd = calloc( sizeof(struct _ksocket), 1 );
                ssd->sock = sd;
                ssd->buf = malloc( MAXSOCKBUF );
                ssd->bufptr = ssd->buf;
            }
        }
    }
    return ssd;
}

int knet_use_tls( ksocket sd )
{
    if( sd->sock <= 0 )
    {
        return 0;
    }
#ifndef __WINDOWS__
    sd->ctx = SSL_CTX_new( TLSv1_client_method() );
    if( !sd->ctx )
    {
        return 0;
    }
    sd->ssl = SSL_new( sd->ctx );
    if( !sd->ssl )
    {
        SSL_CTX_free( sd->ctx );
        sd->ctx = NULL;
        return 0;
    }
    SSL_set_fd( sd->ssl, sd->sock );
    if( SSL_connect( sd->ssl ) == -1 )
    {
        SSL_CTX_free( sd->ctx );
        SSL_free( sd->ssl );
        sd->ssl = NULL;
        sd->ctx = NULL;
        return 0;
    }
#else
    sd->ctx = SSL_CTX_new (SSLv23_client_method());
    if(sd->ctx == NULL)
    {
        return _0;
    }
    sd->ssl = SSL_new (sd->ctx);
    if(sd->ssl == NULL)
    {
        SSL_CTX_free(sd->ctx);
        sd->ctx = NULL;
        return 0;
    }
    SSL_set_fd (sd->ssl, sd->sock);
    SSL_set_mode(sd->ssl, SSL_MODE_AUTO_RETRY);
    if( SSL_connect(sd->ssl) == -1 )
    {
        SSL_CTX_free(sd->ctx);
        SSL_free(sd->ssl);
        sd->ssl = NULL;
        sd->ctx = NULL;
        return 0;
    }
#endif /* __WINDOWS__ */

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
            if( sd->ctx )
            {
                SSL_CTX_free( sd->ctx );
            }
        }
        if( sd->buf )
        {
            free( sd->buf );
        }
        free( sd );
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
        memset( sd->buf, '\0', MAXSOCKBUF );
        if( !sd->ssl )
        {
            recval = recv( sd->sock, sd->buf, MAXSOCKBUF - 1, 0 );
        }
        else
        {
            recval = SSL_read( sd->ssl, sd->buf, MAXSOCKBUF - 1 );
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
            if( recval < MAXSOCKBUF - 1 )
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

int knet_putc( ksocket sd, int ch )
{
    int retval = _SUCCESS;

    if( !sd->ssl )
    {
        if( send( sd->sock, (char *)&ch, 1, 0 ) != 1 )
        {
            sd->flags |= _SOCKET_ERROR;
            sd->error = errno;
            retval = _ERROR;
        }
    }
    else
    {
        if( SSL_write( sd->ssl, (char *)&ch, 1 ) == -1 )
        {
            sd->flags |= _SOCKET_ERROR;
            sd->error = errno;
            retval = _ERROR;
        }
    }

    return retval;
}

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
    size_t sentLen = 0;

    if( !sd->ssl )
    {
        while( len > 0 )
        {
            const size_t blocklen = 4356;
            size_t sendSize = (len > blocklen) ? blocklen : len;
            int bytes = send( sd->sock, buf, sendSize, 0 );
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
                sentLen += bytes;
            }
        }
    }
    else
    {
        sentLen = SSL_write( sd->ssl, buf, len );
    }

    return sentLen;
}

int knet_read( ksocket sd, char *buf, size_t size )
{
    u_int i;

    for( i = 0; i < size - 1; i++ )
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

