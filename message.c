/*
 * message.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 15:11
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "message.h"
#include "mime.h"

static int makeEncodedHeader( Smtp smtp, const char * title, const char * value,
        string msg )
{
    if( !scatc( msg, title ) || !scatc( msg, ": " ) ) return 0;

    if( isUsAsciiCs( value ) )
    {
        if( !scatc( msg, value ) ) return 0;
    }
    else
    {
        string b64 = base64_sencode( value, smtp->charset );
        if( !b64 ) return 0;
        if( !scat( msg, b64 ) )
        {
            sdel( b64 );
            return 0;
        }
        sdel( b64 );
    }

    return scatc( msg, "\r\n" ) != NULL;
}

static string makeEmail( Smtp smtp, Addr a )
{
    string buf = snew();
    if( !buf ) return NULL;

    if( a->name )
    {
        if( isUsAsciiCs( a->name ) )
        {
            if( !sprint( buf, "%s <%s>", a->name, a->email ) )
            {
                sdel( buf );
                return NULL;
            }
        }
        else
        {
            string b64 = base64_sencode( a->name, smtp->charset );
            if( !b64 )
            {
                sdel( buf );
                return NULL;
            }
            if( !sprint( buf, "%s <%s>", sstr( b64 ), a->email ) )
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
        if( !scatc( buf, a->email ) )
        {
            sdel( buf );
            return 0;
        }
    }
    if( !scatc( buf, "\r\n" ) )
    {
        sdel( buf );
        return 0;
    }
    return buf;
}

static int makePartContentType( Smtp smtp, TextPart part, string msg )
{
    string ct = snew();
    if( !ct ) return 0;

    if( !isUsAscii( part->charset ) )
    {
        if( !sprint( ct, "Content-Type: text/%s; charset=%s\r\n"
                "Content-Disposition: inline\r\n"
                "Content-Transfer-Encoding: base64\r\n", part->ctype,
                part->charset ) )
        {
            sdel( ct );
            return 0;
        }
    }
    else
    {
        if( !sprint( ct, "Content-Type: text/%s; charset=%s\r\n", part->ctype,
                part->charset ) )
        {
            sdel( ct );
            return 0;
        }
    }
    if( !scat( msg, ct ) )
    {
        sdel( ct );
        return 0;
    }
    sdel( ct );
    return 1;

}

static int makeOneAddr( Smtp smtp, const char * title, Addr a, string out )
{
    if( a )
    {
        string buf = makeEmail( smtp, a );
        if( buf )
        {
            if( scatc( out, title ) || scatc( out, ": " ) || !scat( out, buf ) )
            {
                sdel( buf );
                return 1;
            }
        }
        return 0;
    }
    return 1;
}

static int makeAddr( Smtp smtp, Addr a, string out )
{
    if( a )
    {
        string buf = makeEmail( smtp, a );
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

static int makeAddrList( Smtp smtp, const char * title, List list, string msg )
{
    Addr a;

    if( !list || !list->size ) return 1;
    if( !scatc( msg, title ) || !scatc( msg, ": " ) ) return 0;

    a = (Addr)lfirst( list );
    while( a )
    {
        if( !makeAddr( smtp, a, msg ) ) return 0;
        a = (Addr)lnext( list );
        if( !scatc( msg, a ? ", " : "\r\n" ) ) return 0;
    }
    return 1;
}

static int makeDateHeader( string msg )
{
    time_t set_time;
    struct tm *lt;
    char buf[128];

    set_time = time( &set_time );
#ifdef USE_GMT
    lt = gmtime(&set_time);
#else
    lt = localtime( &set_time );
#endif
    strftime( buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", lt );

    return scatc( msg, "Date: " ) && scatc( msg, buf ) && scatc( msg, "\r\n" );
}

static int makeExtraHeaders( Smtp smtp, string msg )
{
    Header header = lfirst( smtp->headers );
    while( header )
    {
        if( makeEncodedHeader( smtp, header->title, header->value, msg ) ) return 0;
        header = lnext( smtp->headers );
    }
    return 1;
}

static int createHeaders( Smtp smtp, string msg )
{
    if( !makeEncodedHeader( smtp, "Subject", smtp->subject, msg ) ) return 0;

    if( !makeOneAddr( smtp, "From", smtp->from, msg ) ) return 0;
    if( !makeOneAddr( smtp, "Reply-To", smtp->replyto, msg ) ) return 0;

    if( !makeAddrList( smtp, "To", smtp->to, msg ) ) return 0;
    if( !makeAddrList( smtp, "Cc", smtp->cc, msg ) ) return 0;
    if( !makeAddrList( smtp, "Bcc", smtp->bcc, msg ) ) return 0;

    if( !makeDateHeader( msg ) ) return 0;
    if( !makeExtraHeaders( smtp, msg ) ) return 0;

    if( !scatc( msg, "Mime-Version: 1.0\r\n" ) ) return 0;

    if( smtp->files && smtp->files->size )
    {
        if( !scatc( msg, "Content-Type: multipart/mixed; boundary=\"" )
                || !scatc( msg, smtp->boundary ) || !scatc( msg, "\"\r\n" ) ) return 0;
    }
    else if( smtp->parts && smtp->parts->size > 1 )
    {
        if( !scatc( msg, "Content-Type: multipart/alternative; boundary=\"" )
                || !scatc( msg, smtp->boundary ) || !scatc( msg, "\"\r\n" ) ) return 0;
    }
    else
    {
        TextPart part = (TextPart)lfirst( smtp->parts );
        if( part && !makePartContentType( smtp, part, msg ) ) return 0;
    }

    return scatc( msg, "\r\n" ) != NULL;
}

string createMessage( Smtp smtp )
{
    string msg = snew();

    if( (smtp->files && smtp->files->size)
            || (smtp->parts && smtp->parts->size > 1) )
    {
        smtp->boundary = mimeMakeBoundary();
    }

    if( !createHeaders( smtp, msg ) )
    {
        sdel( msg );
        return NULL;
    }
    /*
     if( mopts->parts && mopts->parts->size && makeMessage( mopts, buf ) < 0 )
     {
     dsbDestroy( buf );
     buf = NULL;
     }
     */
    return msg;
}

