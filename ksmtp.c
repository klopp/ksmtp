/*
 * ksmtp.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 00:49
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "ksmtp.h"
#include "addr.h"
#include <limits.h>

static void delTextPart( void * ptr )
{
    TextPart part = (TextPart)ptr;
    free( part->body );
    free( part->ctype );
    free( part );
}

static void delAddr( void * ptr )
{
    Addr addr = (Addr)ptr;
    free( addr->name );
    free( addr->email );
    free( addr );
}

static void delFile( void *ptr )
{
    File file = (File)ptr;
    free( file->name );
    free( file->ctype );
    free( file );
}

Smtp smtpCreate( void )
{
    Smtp smtp = (Smtp)calloc( sizeof(struct _Smtp), 1 );
    if( !smtp ) return NULL;

    smtp->port = 25;
    smtp->timeout = 10;
    if( gethostname( smtp->nodename, sizeof(smtp->nodename) - 1 ) < 0 )
    {
        strcpy( smtp->nodename, "localhost" );
    }
    smtp->parts = lcreate( delTextPart );
    smtp->files = lcreate( delFile );
    smtp->bcc = lcreate( delAddr );
    smtp->cc = lcreate( delAddr );
    smtp->to = lcreate( delAddr );
    smtp->headers = lcreate( free );

    smtp->replyto = calloc( sizeof(struct _Addr), 1 );
    smtp->from = calloc( sizeof(struct _Addr), 1 );

    smtp->error = snew();

    if( smtp->from && smtp->replyto && smtp->headers && smtp->to && smtp->cc
            && smtp->bcc && smtp->parts && smtp->files && smtp->error )
    {
        return smtp;
    }

    return (Smtp)smtpDestroy( smtp, 0 );
}

int smtpDestroy( Smtp smtp, int sig )
{
//    dnetClose( smtp->sd );

    ldestroy( smtp->parts );
    ldestroy( smtp->files );
    ldestroy( smtp->headers );
    ldestroy( smtp->to );
    ldestroy( smtp->cc );
    ldestroy( smtp->bcc );

    delAddr( smtp->from );
    delAddr( smtp->replyto );

    sdel( smtp->error );

    free( smtp->subject );
    free( smtp->xmailer );
    free( smtp->host );
    free( smtp->smtp_user );
    free( smtp->smtp_password );

    free( smtp );
    return sig;
}

int smtpAddTextPart( Smtp smtp, const char * body, const char *ctype )
{
    TextPart part = malloc( sizeof(struct _TextPart) );
    if( !part ) return 0;
    part->body = strdup( body );
    if( !part->body )
    {
        free( part );
        return 0;
    }
    part->ctype = strdup( ctype );
    if( !part->ctype )
    {
        free( part->body );
        free( part );
        return 0;
    }
    //part->cs = getCharSet( (u_char *)body );
    return ladd( smtp->parts, part ) != NULL;
}

const char *
smtpGetError( Smtp smtp )
{
    return smtp->error->len ? smtp->error->str : NULL;
}

Smtp smtpSetError( Smtp smtp, const char * error )
{
    scpyc( smtp->error, error );
    return smtp;
}

Smtp smtpFormatError( Smtp smtp, const char *fmt, ... )
{
    /*
     va_list ap;
     int actualLen = 0;

     while( true )
     {
     size_t size = smtp->error->size - smtp->error->len;
     va_start( ap, fmt );
     actualLen = vsnprintf( smtp->error->str, size, fmt, ap );
     va_end( ap );
     if( actualLen > -1 && (size_t)actualLen < size )
     {
     break;
     }
     else if( actualLen > -1 )
     {
     size = smtp->error->size + (actualLen - size);
     dsbResize( smtp->error, size + 1 );
     }
     else
     {
     dsbResize( smtp->error, smtp->error->size * 2 );
     }
     }
     smtp->error->len = actualLen;
     */
    return smtp;
}

int smtpSetReplyTo( Smtp smtp, const char * rto )
{
    Addr addr = createAddr( rto );
    if( addr )
    {
        delAddr( smtp->replyto );
        smtp->replyto = addr;
        return 1;
    }
    return 0;
}

int smtpSetFrom( Smtp smtp, const char * from )
{
    Addr addr = createAddr( from );
    if( addr )
    {
        delAddr( smtp->from );
        smtp->from = addr;
        return 1;
    }
    return 0;
}

int smtpAddCc( Smtp smtp, const char * cc )
{
    Addr addr = createAddr( cc );
    return ladd( smtp->cc, addr );
}
Smtp smtpClearCc( Smtp smtp )
{
    lclear( smtp->cc );
    return smtp;
}
Smtp smtpClearTo( Smtp smtp )
{
    lclear( smtp->to );
    return smtp;
}
int smtpAddBcc( Smtp smtp, const char * bcc )
{
    Addr addr = createAddr( bcc );
    return ladd( smtp->bcc, addr );
}
Smtp smtpClearBcc( Smtp smtp )
{
    lclear( smtp->bcc );
    return smtp;
}

int smtpAddTo( Smtp smtp, const char * to )
{
    Addr addr = createAddr( to );
    return ladd( smtp->to, addr );
}

Smtp smtpAddHeader( Smtp smtp, const char * hdr )
{
    ladd( smtp->headers, strdup( hdr ) );
    return smtp;
}
/*
 Smtp smtpAddHeaderPair( Smtp smtp, const char * key, const char * value )
 {
 dstrbuf * hdr = DSB_NEW;
 dsbPrintf( hdr, "%s: %s", key, value );
 smtpAddHeader( smtp, hdr->str );
 dsbDestroy( hdr );
 return smtp;
 }
 */
Smtp smtpClearHeaders( Smtp smtp )
{
    lclear( smtp->headers );
    return smtp;
}

Smtp smtpAddFile( Smtp smtp, const char * name, const char * ctype )
{
    File file = calloc( sizeof(struct _File), 1 );
    if( !file ) return 0;
    file->name = strdup( name );
    if( !file->name )
    {
        free( file );
        return NULL;
    }
    if( ctype )
    {
        file->ctype = strdup( ctype );
        if( !file->ctype )
        {
            free( file->name );
            free( file );
            return NULL;
        }
    }
    ladd( smtp->files, file );
    return smtp;
}
Smtp smtpClearFiles( Smtp smtp )
{
    lclear( smtp->files );
    return smtp;
}

Smtp smtpSetSubject( Smtp smtp, const char * subj )
{
    free( smtp->subject );
    smtp->subject = strdup( subj );
    return smtp;
}

Smtp smtpSetNodename( Smtp smtp, const char * node )
{
    strncpy( smtp->nodename, node, sizeof(smtp->nodename)-1 );
    return smtp;
}

Smtp smtpSetXmailer( Smtp smtp, const char * xmailer )
{
    free( smtp->xmailer );
    smtp->xmailer = strdup( xmailer );
    return smtp;
}

Smtp smtpSetLogin( Smtp smtp, const char * login )
{
    free( smtp->smtp_user );
    smtp->smtp_user = strdup( login );
    return smtp;
}

Smtp smtpSetPassword( Smtp smtp, const char * password )
{
    free( smtp->smtp_password );
    smtp->smtp_password = strdup( password );
    return smtp;
}

int smtpSetSMTP( Smtp smtp, const char * host, int port )
{
    if( smtpSetPort( smtp, port ) )
    {
        smtpSetHost( smtp, host );
        return 1;
    }
    return 0;
}

Smtp smtpSetHost( Smtp smtp, const char * server )
{
    free( smtp->host );
    smtp->host = strdup( server );
    return smtp;
}

int smtpSetAuth( Smtp smtp, AuthType auth )
{
    if( auth == AUTH_LOGIN || auth == AUTH_PLAIN )
    {
        smtp->smtp_auth = auth;
        return 1;
    }
    return 0;
}

int smtpSetPort( Smtp smtp, int port )
{
    if( port > 0 && port < INT_MAX )
    {
        smtp->port = port;
        return 1;
    }
    return 0;
}

int smtpSetTimeout( Smtp smtp, int timeout )
{
    if( timeout > 0 && timeout < INT_MAX )
    {
        smtp->timeout = timeout;
        return 1;
    }
    return 0;
}

/*
 static int smtpSetInt( int * dest, const char * str )
 {
 char * end;
 long value = strtol(str, &end, 10);
 if( value >= INT_MAX || value <= 0 )
 {
 return 0;
 }
 *dest = (int)value;
 return 1;
 }

 static int smtpSetCharPort( Smtp smtp, const char * port )
 {
 return smtpSetInt(&smtp->port, port);
 }

 static int smtpSetCharTimeout( Smtp smtp, const char * timeout )
 {
 return smtpSetInt(&smtp->timeout, timeout);
 }
 */

/*
int smtpSendOneMail( Smtp smtp )
{
    if( smtpOpenSession( smtp ) )
    {
        if( smtpSendMail( smtp ) )
        {
            smtpCloseSession( smtp );
            return 1;
        }
    }
    return 0;
}

*/
