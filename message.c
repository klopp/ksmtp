/*
 * message.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 15:11
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "message.h"
#include "smtp.h"
#include "mime.h"

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
        if( !scatc( encoded, first ) || !scatc( encoded, prefix )
                || !scat( encoded, b64 ) || !scatc( encoded, "?=" ) )
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

static int makeEncodedHeader( Smtp smtp, const char * title, const char * value,
        string msg )
{
    if( !scatc( msg, title ) || !scatc( msg, ": " ) ) return 0;

    if( isUsAscii( value ) )
    {
        if( !scatc( msg, value ) ) return 0;
    }
    else
    {
        string b64 = encodeb64( smtp->cprefix, value );
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
        if( isUsAscii( a->name ) )
        {
            if( !sprint( buf, "%s <%s>", a->name, a->email ) )
            {
                sdel( buf );
                return NULL;
            }
        }
        else
        {
            string b64 = encodeb64( smtp->cprefix, a->name );
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
    return buf;
}

static int makePartContentType( Smtp smtp, TextPart part, string msg )
{
    string ct = snew();
    if( !ct ) return 0;

    if( *part->cprefix )
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
    if( a && a->email )
    {
        string buf = makeEmail( smtp, a );
        if( !buf ) return 0;
        if( !scatc( out, title ) || !scatc( out, ": " ) || !scat( out, buf )
                || !scatc( out, "\r\n" ) )
        {
            sdel( buf );
            return 0;
        }
        sdel( buf );
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
        if( !makeEncodedHeader( smtp, header->title, header->value, msg ) ) return 0;
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
        if( !scatc( msg, "Content-Type: multipart/related; boundary=\"" )
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

static int makeMessage( Smtp smtp, string msg )
{
    TextPart part;
    char * boundary = smtp->boundary;

    part = (TextPart)lfirst( smtp->parts );
    if( smtp->parts->size > 1 && smtp->files && smtp->files->size )
    {
        boundary = mimeMakeBoundary();
        if( !boundary ) return 0;
        if( !scatc( msg, "--" ) || !scatc( msg, smtp->boundary )
                || !scatc( msg,
                        "\r\nContent-Type: multipart/alternative; boundary=\"" )
                || !scatc( msg, boundary ) || !scatc( msg, "\"\r\n" ) ) return 0;
    }

    while( part )
    {
        if( smtp->parts->size > 1 || (smtp->files && smtp->files->size) )
        {
            if( !scatc( msg, "\r\n--" ) || !scatc( msg, boundary )
                    || !scatc( msg, "\r\n" )
                    || !makePartContentType( smtp, part, msg )
                    || !scatc( msg, "Content-ID: 1\r\n\r\n" ) )
            {
                if( boundary != smtp->boundary ) free( boundary );
                return 0;
            }
        }
        if( *part->cprefix )
        {
            string b64 = base64_sencode( part->body );
            if( !b64 )
            {
                if( boundary != smtp->boundary ) free( boundary );
                return 0;
            }
            if( !scat( msg, b64 ) )
            {
                sdel( b64 );
                if( boundary != smtp->boundary ) free( boundary );
                return 0;
            }
            sdel( b64 );
        }
        else
        {
            if( !scatc( msg, part->body ) )
            {
                if( boundary != smtp->boundary ) free( boundary );
                return 0;
            }
        }
        if( !scatc( msg, "\r\n" ) )
        {
            if( boundary != smtp->boundary ) free( boundary );
            return 0;
        }
        part = (TextPart)lnext( smtp->parts );
    }

    if( boundary != smtp->boundary )
    {
        if( !scatc( msg, "\r\n--" ) || !scatc( msg, boundary )
                || !scatc( msg, "--\r\n" ) )
        {
            free( boundary );
            return 0;
        }
        else
        {
            free( boundary );
        }
    }
    return 1;
}

static int attachFiles( Smtp smtp, FILE * fout )
{
    File file = lfirst( smtp->files );

    while( file )
    {
        string mime_name;
        string b64;
        string out = snew();
        const char * mime_type = getMimeType( file->name, file->ctype );

        if( !out )
        {
            smtpFormatError( smtp, "attachFile(\"%s\"), internal error 1",
                    file->name );
            return 0;
        }
        FILE * f = fopen( file->name, "rb" );
        if( !f )
        {
            smtpFormatError( smtp, "attachFile(\"%s\") : %s", file->name,
                    strerror( errno ) );
            return 0;
        }
        mime_name = mimeFileName( file->name,
                *smtp->cprefix ? smtp->charset : NULL );
        if( !mime_name )
        {
            fclose( f );
            smtpFormatError( smtp, "attachFile(\"%s\"), internal error 2",
                    file->name );
            return 0;
        }
        if( !sprint( out, "\r\n--%s\r\n"
                "Content-Transfer-Encoding: base64\r\n"
                "Content-Type: %s; name=\"%s\"\r\n"
                "Content-Disposition: attachment; filename=\"%s\"\r\n"
                "Content-ID: <%s>\r\n"
                "\r\n", smtp->boundary, mime_type, sstr( mime_name ),
                sstr( mime_name ), file->cid ) )
        {
            fclose( f );
            sdel( mime_name );
            smtpFormatError( smtp, "attachFile(\"%s\"), internal error 3",
                    file->name );
            return 0;
        }

        b64 = base64_fencode( f );
        if( !b64 )
        {
            fclose( f );
            sdel( mime_name );
            sdel( out );
            smtpFormatError( smtp, "attachFile(\"%s\"), internal error 4",
                    file->name );
            return 0;
        }

        if( !knet_write( smtp->sd, sstr( out ), slen( out ) )
                || !knet_write( smtp->sd, sstr( b64 ), slen( b64 ) ) )
        {
            fclose( f );
            sdel( mime_name );
            sdel( b64 );
            sdel( out );
            smtpFormatError( smtp, "attachFile(\"%s\"), internal error 5",
                    file->name );
            return 0;
        }

//        fprintf( fout, "%s%s", sstr( out ), sstr( b64 ) );
        sdel( mime_name );
        sdel( b64 );
        sdel( out );
        file = lnext( smtp->files );
    }

    return 1;
}

int processMessage( Smtp smtp, string msg )
{
    Addr addr;

    if( !smtp_mail_from( smtp, smtp->from->email ) ) return 0;

    addr = lfirst( smtp->to );
    while( addr )
    {
        if( !smtp_rcpt_to( smtp, addr->email ) ) return 0;
        addr = lnext( smtp->to );
    }
    addr = lfirst( smtp->cc );
    while( addr )
    {
        if( !smtp_rcpt_to( smtp, addr->email ) ) return 0;
        addr = lnext( smtp->cc );
    }
    addr = lfirst( smtp->bcc );
    while( addr )
    {
        if( !smtp_rcpt_to( smtp, addr->email ) ) return 0;
        addr = lnext( smtp->bcc );
    }

    if( !smtp_data( smtp ) ) return 0;
    if( !knet_write( smtp->sd, sstr( msg ), slen( msg ) ) ) return 0;

    if( smtp->files && smtp->files->size )
    {
        if( !attachFiles( smtp, NULL ) ) return 0;
    }

    if( smtp->boundary )
    {
        if( !knet_write( smtp->sd, "\r\n--", sizeof("\r\n--") - 1 )
                || !knet_write( smtp->sd, smtp->boundary,
                        strlen( smtp->boundary ) )
                || !knet_write( smtp->sd, "--\r\n", sizeof("--\r\n") - 1 ) ) return 0;
    }

    return smtp_end_data( smtp );
}

string createMessage( Smtp smtp )
{
    string msg = snew();

    if( (smtp->files && smtp->files->size)
            || (smtp->parts && smtp->parts->size > 1) )
    {
        smtp->boundary = mimeMakeBoundary();
        if( !smtp->boundary )
        {
            sdel( msg );
            return NULL;
        }
    }

    if( !createHeaders( smtp, msg ) )
    {
        sdel( msg );
        return NULL;
    }

    if( smtp->parts && smtp->parts->size && !makeMessage( smtp, msg ) )
    {
        sdel( msg );
        return NULL;
    }
    return msg;
}

