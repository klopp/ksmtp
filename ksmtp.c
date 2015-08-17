/*
 * ksmtp.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 00:49
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "ksmtp.h"
#include "addr.h"
#include "mime.h"
#include <limits.h>

static void delTextPart( void * ptr )
{
    TextPart part = (TextPart)ptr;
    Free( part->body );
    Free( part->ctype );
    Free( part );
}

static void delAddr( void * ptr )
{
    Addr addr = (Addr)ptr;
    Free( addr->name );
    Free( addr->email );
    Free( addr );
}

static void delHeader( void * ptr )
{
    Header header = (Header)ptr;
    Free( header->title );
    Free( header->value );
    Free( header );
}

static void delAFile( void *ptr )
{
    AFile file = (AFile)ptr;
    Free( file->name );
    Free( file->ctype );
    Free( file );
}

static void delEFile( void *ptr )
{
    EFile file = (EFile)ptr;
    Free( file->name );
    Free( file->ctype );
    Free( file );
}

Smtp smtpCreate( KsmtpFlags flags )
{
    Smtp smtp = (Smtp)Calloc( sizeof(struct _Smtp), 1 );
    if( !smtp ) return NULL;

    smtp->flags = flags;
    smtp->port = 25;
    smtp->sd.timeout = 10;
    smtp->sd.sock = -1;
    if( gethostname( smtp->nodename, sizeof(smtp->nodename) - 1 ) < 0 )
    {
        strncpy( smtp->nodename, "localhost", sizeof(smtp->nodename) - 1 );
    }
    smtp->parts = lcreate( delTextPart );
    smtp->afiles = lcreate( delAFile );
    smtp->efiles = lcreate( delEFile );
    smtp->bcc = lcreate( delAddr );
    smtp->cc = lcreate( delAddr );
    smtp->to = lcreate( delAddr );
    smtp->headers = lcreate( delHeader );

    smtp->replyto = Calloc( sizeof(struct _Addr), 1 );
    smtp->from = Calloc( sizeof(struct _Addr), 1 );

    smtp->error = snew();
    smtp->current = snew();

    if( smtp->from && smtp->replyto && smtp->headers && smtp->to && smtp->cc
            && smtp->bcc && smtp->parts && smtp->afiles && smtp->efiles
            && smtp->error && smtp->current )
    {
        smtpSetCharset( smtp, KSMTP_DEFAULT_CHARSET );
        return smtp;
    }

    smtpDestroy( smtp, 0 );
    return NULL;
}

int smtpDestroy( Smtp smtp, int sig )
{
    knet_disconnect( &smtp->sd );

    ldestroy( smtp->parts );
    ldestroy( smtp->afiles );
    ldestroy( smtp->efiles );
    ldestroy( smtp->headers );
    ldestroy( smtp->to );
    ldestroy( smtp->cc );
    ldestroy( smtp->bcc );

    delAddr( smtp->from );
    delAddr( smtp->replyto );

    sdel( smtp->error );
    sdel( smtp->current );

    Free( smtp->subject );
    Free( smtp->xmailer );
    Free( smtp->host );
    Free( smtp->smtp_user );
    Free( smtp->smtp_password );

    Free( smtp );
    return sig;
}

int smtpAddTextPart( Smtp smtp, const char * body, const char *ctype,
        const char * charset )
{
    TextPart part = (TextPart)lfirst( smtp->parts );
    while( part )
    {
        if( !strcasecmp( part->ctype, ctype ) )
        {
            // TODO check if part with ctype (and/or charset?) exists?
        }
        part = (TextPart)lnext( smtp->parts );
    }

    part = Malloc( sizeof(struct _TextPart) );
    if( !part ) return 0;
    part->body = Strdup( body );
    if( !part->body )
    {
        Free( part );
        return 0;
    }
    part->ctype = Strdup( ctype );
    if( !part->ctype )
    {
        delTextPart( part );
        return 0;
    }
    strncpy( part->charset, charset ? charset : smtp->charset,
            sizeof(part->charset) - 1 );
    if( !isUsAsciiCs( part->charset ) ) snprintf( part->cprefix,
            sizeof(part->cprefix) - 1, "=?%s?B?", part->charset );
    else *part->cprefix = 0;
    if( !ladd( smtp->parts, part ) )
    {
        delTextPart( part );
        return 0;
    }
    return 1;
}

int smtpAddDefTextPart( Smtp smtp, const char * body, const char *ctype )
{
    return smtpAddTextPart( smtp, body, ctype, NULL );
}

int smtpAddUtfTextPart( Smtp smtp, const char * body, const char *ctype )
{
    static char utf8[] = "UTF-8";
    return smtpAddTextPart( smtp, body, ctype, utf8 );
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
    if( addr )
    {
        if( !ladd( smtp->cc, addr ) )
        {
            delAddr( addr );
            return 0;
        }
    }
    return 1;
}
void smtpClearCc( Smtp smtp )
{
    lclear( smtp->cc );
}
void smtpClearTo( Smtp smtp )
{
    lclear( smtp->to );
}
int smtpAddBcc( Smtp smtp, const char * bcc )
{
    Addr addr = createAddr( bcc );
    if( addr )
    {
        if( !ladd( smtp->bcc, addr ) )
        {
            delAddr( addr );
            return 0;
        }
    }
    return 1;
}
void smtpClearBcc( Smtp smtp )
{
    lclear( smtp->bcc );
}

int smtpAddTo( Smtp smtp, const char * to )
{
    Addr addr = createAddr( to );
    if( addr )
    {
        if( !ladd( smtp->to, addr ) )
        {
            Free( addr );
            return 0;
        }
    }
    return 1;
}

int smtpAddHeader( Smtp smtp, const char * key, const char * value )
{
    Header header = Calloc( sizeof(struct _Header), 1 );
    if( !header ) return 0;
    header->title = Strdup( key );
    header->value = Strdup( value );
    if( !header->value || !header->title )
    {
        delHeader( header );
        return 0;
    }
    if( !ladd( smtp->headers, header ) )
    {
        delHeader( header );
        return 0;
    }
    return 1;
}

int smtpAddXMailer( Smtp smtp, const char * xmailer )
{
    return smtpAddHeader( smtp, "X-Mailer", xmailer );
}

void smtpClearHeaders( Smtp smtp )
{
    lclear( smtp->headers );
}

const char * smtpEmbedFile( Smtp smtp, const char * name, const char * ctype )
{
    EFile file = Calloc( sizeof(struct _EFile), 1 );
    if( !file ) return NULL;
    file->name = Strdup( name );
    if( !file->name )
    {
        Free( file );
        return NULL;
    }
    if( ctype )
    {
        file->ctype = Strdup( ctype );
        if( !file->ctype )
        {
            delEFile( file );
            return NULL;
        }
    }
    smtp->lastid++;
    sprintf( file->cid, "%s%zu", KFILE_CONTENT_ID, smtp->lastid );
    if( !ladd( smtp->efiles, file ) )
    {
        delEFile( file );
        return NULL;
    }
    return file->cid;
}

int smtpAttachFile( Smtp smtp, const char * name, const char * ctype )
{
    AFile file = Calloc( sizeof(struct _AFile), 1 );
    if( !file ) return 0;
    file->name = Strdup( name );
    if( !file->name )
    {
        Free( file );
        return 0;
    }
    if( ctype )
    {
        file->ctype = Strdup( ctype );
        if( !file->ctype )
        {
            delAFile( file );
            return 0;
        }
    }
    smtp->lastid++;
    if( !ladd( smtp->afiles, file ) )
    {
        delAFile( file );
        return 0;
    }
    return 1;
}

void smtpClearAFiles( Smtp smtp )
{
    lclear( smtp->afiles );
}

void smtpClearEFiles( Smtp smtp )
{
    lclear( smtp->efiles );
}

int smtpSetSubject( Smtp smtp, const char * subj )
{
    char * s = Strdup( subj );
    if( s )
    {
        Free( smtp->subject );
        smtp->subject = s;
        return 0;
    }
    return 1;
}

void smtpSetNodename( Smtp smtp, const char * node )
{
    strncpy( smtp->nodename, node, sizeof(smtp->nodename) - 1 );
}

void smtpSetCharset( Smtp smtp, const char * charset )
{
    strncpy( smtp->charset, charset, sizeof(smtp->charset) - 1 );
    if( !isUsAsciiCs( charset ) ) snprintf( smtp->cprefix,
            sizeof(smtp->cprefix) - 1, "=?%s?B?", charset );
    else *smtp->cprefix = 0;
}

int smtpSetXmailer( Smtp smtp, const char * xmailer )
{
    char * x = Strdup( xmailer );
    if( x )
    {
        Free( smtp->xmailer );
        smtp->xmailer = x;
        return 1;
    }
    return 0;
}

int smtpSetLogin( Smtp smtp, const char * login )
{
    char * s = Strdup( login );
    if( s )
    {
        Free( smtp->smtp_user );
        smtp->smtp_user = s;
        return 1;
    }
    return 0;
}

int smtpSetPassword( Smtp smtp, const char * password )
{
    char * s = Strdup( password );
    if( s )
    {
        Free( smtp->smtp_password );
        smtp->smtp_password = s;
        return 1;
    }
    return 0;
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

int smtpSetHost( Smtp smtp, const char * server )
{
    char * h = Strdup( server );
    if( h )
    {
        Free( smtp->host );
        smtp->host = h;
        return 1;
    }
    return 0;
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
        smtp->sd.timeout = timeout;
        return 1;
    }
    return 0;
}

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
