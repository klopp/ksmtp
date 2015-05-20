/*
 * message.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 15:11
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "message.h"

static int printOneAddr( const char * title, Addr a, string out )
{
    if( a )
    {
        string buf = snew();
        if( buf )
        {
            if( sprint( buf, "%s: %s <%s>\r\n", title, a->name ? a->name : "",
                    a->email ) )
            {
                if( scat( out, buf ) )
                {
                    sdel( buf );
                    return 1;
                }
            }
            sdel( buf );
        }
        return 0;
    }
    return 1;
}

static int printAddr( Addr a, string out )
{
    if( a )
    {
        string buf = snew();
        if( buf )
        {
            if( sprint( buf, "%s <%s>\r\n", a->name ? a->name : "", a->email ) )
            {
                if( scat( out, buf ) )
                {
                    sdel( buf );
                    return 1;
                }
            }
            sdel( buf );
        }
        return 0;
    }
    return 1;
}

static int printAddrList( const char * title, List list, string msg )
{
    Addr a;

    if( !list || !list->size ) return 1;
    if( !scatc( msg, title ) || !scatc( msg, ": " ) ) return 0;

    a = (Addr)lfirst( list );
    while( a )
    {
        if( !printAddr( a, msg ) ) return 0;
        a = (Addr)lnext( list );
        if( !scatc( msg, a ? ", " : "\r\n" ) ) return 0;
    }
    return 1;
}

static int printDateHeader( string msg )
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

static int printExtraHeaders( List headers, string msg )
{
    char * header = lfirst( headers );
    while( header )
    {
        if( !scatc( msg, header ) || !scatc( msg, "\r\n" ) ) return 0;
        header = lnext( headers );
    }
    return 1;
}

static int createHeaders( Smtp smtp, string msg )
{
    if( !printEncodedHeader( "Subject", smtp->subject, msg ) ) return 0;

    if( !printOneAddr( "From", smtp->from, msg ) ) return 0;
    if( !printOneAddr( "Reply-To", smtp->replyto, msg ) ) return 0;

    if( !printAddrList( "To", smtp->to, msg ) ) return 0;
    if( !printAddrList( "Cc", smtp->cc, msg ) ) return 0;
    if( !printAddrList( "Bcc", smtp->bcc, msg ) ) return 0;

    if( !printDateHeader( msg ) ) return 0;
    if( !printExtraHeaders( smtp->headers, msg ) ) return 0;

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
        if( part && !printPartContentType( part, msg ) ) return 0;
    }

    return scatc( msg, "\r\n" ) != NULL;
}

string createMessage( Smtp smtp )
{
    string msg = snew();

    if( (smtp->files && smtp->files->size)
            || (smtp->parts && smtp->parts->size > 1) )
    {
        mimeMakeBoundary( smtp->boundary );
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

