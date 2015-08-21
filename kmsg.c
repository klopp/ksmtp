/*
 * kmsg.c, part of "kmail" project.
 *
 *  Created on: 20.05.2015, 00:49
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "kmsg.h"
#include "addr.h"
#include "mime.h"
#include <errno.h>
#include <time.h>

static void delTextPart( void * ptr )
{
    TextPart part = (TextPart)ptr;
    Free( part->body );
    Free( part->ctype );
    Free( part );
}

static void delEFile( void *ptr )
{
    EFile file = (EFile)ptr;
    Free( file->name );
    Free( file->ctype );
    Free( file );
}

KMsg msg_Create( void )
{
    KMsg msg = (KMsg)Calloc( sizeof(struct _KMsg), 1 );
    if( !msg ) return NULL;

    msg->parts = lcreate( delTextPart );
    msg->afiles = plcreate();
    msg->efiles = lcreate( delEFile );
    msg->bcc = plcreate();
    msg->cc = plcreate();
    msg->to = plcreate();
    msg->headers = plcreate();

    msg->replyto = Calloc( sizeof(struct _Pair), 1 );
    msg->from = Calloc( sizeof(struct _Pair), 1 );

    if( msg->from && msg->replyto && msg->headers && msg->to && msg->cc
            && msg->bcc && msg->parts && msg->afiles && msg->efiles )
    {
        msg_SetCharset( msg, KMSG_DEFAULT_CHARSET );
        return msg;
    }

    msg_Destroy( msg );
    return NULL;
}

void msg_Destroy( KMsg msg )
{
    ldestroy( msg->parts );
    ldestroy( msg->afiles );
    ldestroy( msg->efiles );
    ldestroy( msg->headers );
    ldestroy( msg->to );
    ldestroy( msg->cc );
    ldestroy( msg->bcc );

    pair_Delete( msg->from );
    pair_Delete( msg->replyto );

    Free( msg->subject );
    Free( msg->xmailer );

    Free( msg );
}

int msg_AddTextPart( KMsg msg, const char * body, const char *ctype,
        const char * charset )
{
    TextPart part = (TextPart)lfirst( msg->parts );
    while( part )
    {
        if( !strcasecmp( part->ctype, ctype ) )
        {
            // TODO check if part with ctype (and/or charset?) exists?
        }
        part = (TextPart)lnext( msg->parts );
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
    strncpy( part->charset, charset ? charset : msg->charset,
            sizeof(part->charset) - 1 );
    if( !isUsAsciiCs( part->charset ) ) snprintf( part->cprefix,
            sizeof(part->cprefix) - 1, "=?%s?B?", part->charset );
    else *part->cprefix = 0;
    if( !ladd( msg->parts, part ) )
    {
        delTextPart( part );
        return 0;
    }
    return 1;
}

int msg_AddDefTextPart( KMsg msg, const char * body, const char *ctype )
{
    return msg_AddTextPart( msg, body, ctype, NULL );
}

int msg_AddUtfTextPart( KMsg msg, const char * body, const char *ctype )
{
    static char utf8[] = "UTF-8";
    return msg_AddTextPart( msg, body, ctype, utf8 );
}

int msg_SetReplyTo( KMsg msg, const char * rto )
{
    Pair addr = createAddr( rto );
    if( addr )
    {
        pair_Delete( msg->replyto );
        msg->replyto = addr;
        return 1;
    }
    return 0;
}

int msg_SetFrom( KMsg msg, const char * from )
{
    Pair addr = createAddr( from );
    if( addr )
    {
        pair_Delete( msg->from );
        msg->from = addr;
        return 1;
    }
    return 0;
}

int msg_AddCc( KMsg msg, const char * cc )
{
    Pair addr = createAddr( cc );
    if( addr )
    {
        if( !ladd( msg->cc, addr ) )
        {
            pair_Delete( addr );
            return 0;
        }
    }
    return 1;
}
void smtpClearCc( KMsg msg )
{
    plclear( msg->cc );
}
void smtpClearTo( KMsg msg )
{
    plclear( msg->to );
}
int msg_AddBcc( KMsg msg, const char * bcc )
{
    Pair addr = createAddr( bcc );
    if( addr )
    {
        if( !ladd( msg->bcc, addr ) )
        {
            pair_Delete( addr );
            return 0;
        }
    }
    return 1;
}
void smtpClearBcc( KMsg msg )
{
    lclear( msg->bcc );
}

int msg_AddTo( KMsg msg, const char * to )
{
    Pair addr = createAddr( to );
    if( addr )
    {
        if( !ladd( msg->to, addr ) )
        {
            Free( addr );
            return 0;
        }
    }
    return 1;
}

int msg_AddHeader( KMsg msg, const char * key, const char * value )
{
    return pladd( msg->headers, key, value ) != NULL;
}

int msg_AddXMailer( KMsg msg, const char * xmailer )
{
    return msg_AddHeader( msg, "X-Mailer", xmailer );
}

void msg_ClearHeaders( KMsg msg )
{
    lclear( msg->headers );
}

const char * msg_EmbedFile( KMsg msg, const char * name, const char * ctype )
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
    msg->lastid++;
    sprintf( file->cid, "%s%zu", KFILE_CONTENT_ID, msg->lastid );
    if( !ladd( msg->efiles, file ) )
    {
        delEFile( file );
        return NULL;
    }
    return file->cid;
}

int msg_AttachFile( KMsg msg, const char * name, const char * ctype )
{
    return pladd( msg->afiles, name, ctype ) != NULL;
}

void smtpClearAFiles( KMsg msg )
{
    lclear( msg->afiles );
}

void smtpClearEFiles( KMsg msg )
{
    lclear( msg->efiles );
}

int msg_SetSubject( KMsg msg, const char * subj )
{
    char * s = Strdup( subj );
    if( s )
    {
        Free( msg->subject );
        msg->subject = s;
        return 0;
    }
    return 1;
}

void msg_SetCharset( KMsg msg, const char * charset )
{
    strncpy( msg->charset, charset, sizeof(msg->charset) - 1 );
    if( !isUsAsciiCs( charset ) ) snprintf( msg->cprefix,
            sizeof(msg->cprefix) - 1, "=?%s?B?", charset );
    else *msg->cprefix = 0;
}

int msg_SetXmailer( KMsg msg, const char * xmailer )
{
    char * x = Strdup( xmailer );
    if( x )
    {
        Free( msg->xmailer );
        msg->xmailer = x;
        return 1;
    }
    return 0;
}

#define ENCODED_BLK_SIZE    45

static string encodeb64( const char * prefix, const char * value )
{
    string encoded = snew();
    size_t size = strlen( value );
    char * first = "";
    if( !encoded ) return NULL;

    while( size )
    {
        string b64;
        size_t to_encode = size > ENCODED_BLK_SIZE ? ENCODED_BLK_SIZE : size;
        b64 = base64_encode( value, to_encode );
        if( !b64 )
        {
            sdel( b64 );
            sdel( encoded );
            return NULL;
        }
        if( !xscatc( encoded, first, prefix, sstr( b64 ), "?=", NULL ) )
        {
            sdel( b64 );
            sdel( encoded );
            return NULL;
        }
        sdel( b64 );
        first = " ";
        size -= to_encode;
        value += to_encode;
        if( size )
        {
            if( !scatc( encoded, "\r\n" ) )
            {
                sdel( b64 );
                sdel( encoded );
                return NULL;
            }
        }
    }

    return encoded;
}

static int makeEncodedHeader( KMsg msg, const char * title, const char * value,
        string headers )
{
    if( !xscatc( headers, title, ": ", NULL ) ) return 0;

    if( isUsAscii( value ) )
    {
        if( !scatc( headers, value ) ) return 0;
    }
    else
    {
        string b64 = encodeb64( msg->cprefix, value );
        if( !b64 ) return 0;
        if( !scat( headers, b64 ) )
        {
            sdel( b64 );
            return 0;
        }
        sdel( b64 );
    }

    return scatc( headers, "\r\n" ) != NULL;
}

static string makeEmail( KMsg msg, Pair a )
{
    string buf = snew();
    if( !buf ) return NULL;

    if( A_NAME(a) )
    {
        if( isUsAscii( A_NAME(a) ) )
        {
            if( !sprint( buf, "%s <%s>", A_NAME(a), A_EMAIL(a) ) )
            {
                sdel( buf );
                return NULL;
            }
        }
        else
        {
            string b64 = encodeb64( msg->cprefix, A_NAME(a) );
            if( !b64 )
            {
                sdel( buf );
                return NULL;
            }
            if( !sprint( buf, "%s <%s>", sstr( b64 ), A_EMAIL(a) ) )
            {
                sdel( buf );
                sdel( b64 );
                return NULL;
            }
            sdel( b64 );
        }
    }
    else
    {
        if( !scatc( buf, A_EMAIL(a) ) )
        {
            sdel( buf );
            return 0;
        }
    }
    return buf;
}

static int makeOneAddr( KMsg msg, const char * title, Pair a, string out )
{
    if( a && A_EMAIL(a) )
    {
        string buf = makeEmail( msg, a );
        if( !buf ) return 0;
        if( !xscatc( out, title, ": ", sstr( buf ), "\r\n", NULL ) )
        {
            sdel( buf );
            return 0;
        }
        sdel( buf );
    }
    return 1;
}

static int makeAddr( KMsg msg, Pair a, string out )
{
    if( a )
    {
        string buf = makeEmail( msg, a );
        if( buf )
        {
            if( scat( out, buf ) )
            {
                sdel( buf );
                return 1;
            }
        }
        return 0;
    }
    return 1;
}

static int makeAddrList( KMsg msg, const char * title, List list, string out )
{
    Pair a;

    if( !list || !list->size ) return 1;
    if( !xscatc( out, title, ": ", NULL ) ) return 0;

    a = lfirst( list );
    while( a )
    {
        if( !makeAddr( msg, a, out ) ) return 0;
        a = lnext( list );
        if( !scatc( out, a ? "," : "\r\n" ) ) return 0;
    }
    return 1;
}

static int makeDateHeader( string out )
{
    time_t set_time;
    struct tm *lt;
    char buf[128];

    set_time = time( &set_time );
    lt = localtime( &set_time );
    strftime( buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", lt );

    return xscatc( out, "Date: ", buf, "\r\n", NULL ) != NULL;
}

static int makeExtraHeaders( KMsg msg, string out )
{
    Pair header = lfirst( msg->headers );
    while( header )
    {
        if( !makeEncodedHeader( msg, H_NAME(header), H_VALUE(header), out ) ) return 0;
        header = lnext( msg->headers );
    }
    return 1;
}

string msg_CreateHeaders( KMsg msg )
{
    string headers = snew();

    if( !headers || !makeEncodedHeader( msg, "Subject", msg->subject, headers )
            || !makeOneAddr( msg, "From", msg->from, headers )
            || !makeOneAddr( msg, "Reply-To", msg->replyto, headers )
            || !makeAddrList( msg, "To", msg->to, headers )
            || !makeAddrList( msg, "Cc", msg->cc, headers )
            || !makeAddrList( msg, "Bcc", msg->bcc, headers )
            || !makeDateHeader( headers ) || !makeExtraHeaders( msg, headers ) )
    {
        sdel( headers );
        return NULL;
    }
    return headers;
}

string msg_CreateBody( KMsg msg )
{
    TextPart part = lfirst( msg->parts );
    char boundary[36];
    int rc = 1;
    string parts = snew();
    if( !parts ) return NULL;

    if( msg->parts->size > 1 )
    {
        mimeMakeBoundary( boundary );
        if( !scpyc( parts, "Content-Type: multipart/alternative; boundary=\"" )
                || !xscatc( parts, boundary, "\"\r\n\r\n", NULL ) )
        {
            sdel( parts );
            return NULL;
        }
    }

    while( part )
    {
        if( msg->parts->size > 1 )
        {
            if( !xscatc( parts, "--", boundary, "\r\nContent-ID: text@part\r\n",
            NULL ) )
            {
                rc = 0;
                break;
            }
        }
        if( *part->cprefix )
        {
            string b64 = base64_sencode( part->body );
            if( !b64
                    || !xscatc( parts, "Content-Type: text/", part->ctype,
                            "; charset=", part->charset,
                            "\r\nContent-Disposition: inline\r\n"
                                    "Content-Transfer-Encoding: base64\r\n\r\n",
                            sstr( b64 ), "\r\n\r\n", NULL ) )
            {
                sdel( b64 );
                rc = 0;
                break;
            }
            sdel( b64 );
        }
        else
        {
            if( !xscatc( parts, "Content-Type: text/", part->ctype,
                    "; charset=", part->charset, "\r\n\r\n", part->body,
                    "\r\n\r\n", NULL ) )
            {
                rc = 0;
                break;
            }
        }
        part = lnext( msg->parts );
    }
    if( rc )
    {
        if( msg->parts->size > 1 )
        {
            if( !xscatc( parts, "--", boundary, "--\r\n", NULL ) )
            {
                rc = 0;
            }
        }
    }
    if( !rc )
    {
        sdel( parts );
        return NULL;
    }
    return parts;
}

int msg_CreateFile( KMsg msg, MFile file, string error, const char * boundary,
        const char * name, const char * ctype, const char * disposition,
        const char * cid )
{
    FILE * f;
    string mime_name;
    const char * mime_type = getMimeType( name, ctype );

    f = fopen( name, "rb" );
    if( !f )
    {
        sprint( error, "msg_CreateFile(\"%s\") : %s", name, strerror( errno ) );
        return 0;
    }
    mime_name = mimeFileName( name, *msg->cprefix ? msg->charset : NULL );
    if( !mime_name )
    {
        fclose( f );
        sprint( error, "msg_CreateFile(\"%s\"), internal error [2]", name );
        return 0;
    }

    if( !sprint( file->headers, "\r\n--%s\r\n"
            "Content-Transfer-Encoding: base64\r\n"
            "Content-Type: %s; name=\"%s\"\r\n"
            "Content-Disposition: %s; filename=\"%s\"\r\n"
            "%s%s%s"
            "\r\n", boundary, mime_type, sstr( mime_name ), disposition,
            sstr( mime_name ), cid ? "Content-ID: <" : "", cid ? cid : "",
            cid ? ">\r\n" : "" ) )
    {
        fclose( f );
        sdel( mime_name );
        sprint( error, "msg_CreateFile(\"%s\"), internal error [3]", name );
        return 0;
    }

    sdel( file->body );
    file->body = base64_fencode( f );
    if( !file->body )
    {
        fclose( f );
        sdel( mime_name );
        sprint( error, "msg_CreateFile(\"%s\"), internal error [4]", name );
        return 0;
    }

    sdel( mime_name );
    return 1;
}
