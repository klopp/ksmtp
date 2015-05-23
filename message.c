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
    return buf;
}

static string makeTextParts( Smtp smtp )
{
    TextPart part = lfirst( smtp->parts );
    char boundary[36];
    int rc = 1;
    string parts = snew();
    if( !parts ) return NULL;

    if( smtp->parts->size > 1 )
    {
        mimeMakeBoundary( boundary );
        if( !scpyc( parts, "Content-Type: multipart/alternative; boundary=\"" )
                || !scatc( parts, boundary ) || !scatc( parts, "\"\r\n\r\n" ) )
        {
            sdel( parts );
            return NULL;
        }
    }

    while( part )
    {
        if( smtp->parts->size > 1 )
        {
            if( !scatc( parts, "--" ) || !scatc( parts, boundary )
                    || !scatc( parts, "\r\nContent-ID: text@part\r\n" ) )
            {
                rc = 0;
                break;
            }
        }
        if( *part->cprefix )
        {
            string b64 = base64_sencode( part->body );
            if( !b64 || !scatc( parts, "Content-Type: text/" )
                    || !scatc( parts, part->ctype )
                    || !scatc( parts, "; charset=" )
                    || !scatc( parts, part->charset )
                    || !scatc( parts, "\r\nContent-Disposition: inline\r\n"
                            "Content-Transfer-Encoding: base64\r\n\r\n" )
                    || !scatc( parts, sstr( b64 ) )
                    || !scatc( parts, "\r\n\r\n" ) )
            {
                sdel( b64 );
                rc = 0;
                break;
            }
            sdel( b64 );
        }
        else
        {
            if( !scatc( parts, "Content-Type: text/" )
                    || !scatc( parts, part->ctype )
                    || !scatc( parts, "; charset=" )
                    || !scatc( parts, part->charset )
                    || !scatc( parts, "\r\n\r\n" )
                    || !scatc( parts, part->body )
                    || !scatc( parts, "\r\n\r\n" ) )
            {
                rc = 0;
                break;
            }
        }
        part = lnext( smtp->parts );
    }
    if( rc )
    {
        if( smtp->parts->size > 1 )
        {
            if( !scatc( parts, "--" ) || !scatc( parts, boundary )
                    || !scatc( parts, "--\r\n" ) )
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
        if( !scatc( msg, a ? "," : "\r\n" ) ) return 0;
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

static int ksmtp_write( Smtp smtp, const char * buf, size_t len )
{
    if( smtp->flags & KSMTP_VERBOSE_MSG ) fwrite( buf, len, 1, stderr );
    return knet_write( &smtp->sd, buf, len );
}

static int insertOneFile( Smtp smtp, const char * boundary, const char * name,
        const char * ctype, const char * disposition, const char * cid )
{
    FILE * f;
    string mime_name;
    string b64;
    string out = snew();
    const char * mime_type = getMimeType( name, ctype );

    if( !out )
    {
        smtpFormatError( smtp, "attachFile(\"%s\"), internal error 1", name );
        return 0;
    }
    f = fopen( name, "rb" );
    if( !f )
    {
        smtpFormatError( smtp, "attachFile(\"%s\") : %s", name,
                strerror( errno ) );
        return 0;
    }
    mime_name = mimeFileName( name, *smtp->cprefix ? smtp->charset : NULL );
    if( !mime_name )
    {
        fclose( f );
        smtpFormatError( smtp, "attachFile(\"%s\"), internal error 2", name );
        return 0;
    }
    if( !sprint( out, "\r\n--%s\r\n"
            "Content-Transfer-Encoding: base64\r\n"
            "Content-Type: %s; name=\"%s\"\r\n"
            "Content-Disposition: %s; filename=\"%s\"\r\n"
            "%s%s%s"
            "\r\n", boundary, mime_type, sstr( mime_name ), disposition,
            sstr( mime_name ), cid ? "Content-ID: <" : "", cid ? cid : "",
            cid ? ">\r\n" : "" ) )

    /*
     if( !scpyc( out, "\r\n--" ) || !scatc( out, boundary )
     || !scatc( out,
     "\r\nContent-Transfer-Encoding: base64\r\nContent-Type: " )
     || !scatc( out, mime_type ) || !scatc( out, "; name=\"" )
     || !scatc( out, sstr( mime_name ) )
     || !scatc( out, "\"\r\nContent-Disposition: " )
     || !scatc( out, disposition ) || !scatc( out, "; filename=\"" )
     || !scatc( out, sstr( mime_name ) ) || !scatc( out, "\"\r\n" )
     || !scatc( out, cid ? "Content-ID: <" : "" )
     || !scatc( out, cid ? cid : "" )
     || !scatc( out, cid ? ">\r\n" : "" ) || !scatc( out, "\r\n" ) )
     */
    {
        fclose( f );
        sdel( mime_name );
        smtpFormatError( smtp, "attachFile(\"%s\"), internal error 3", name );
        return 0;
    }

    b64 = base64_fencode( f );
    if( !b64 )
    {
        fclose( f );
        sdel( mime_name );
        sdel( out );
        smtpFormatError( smtp, "attachFile(\"%s\"), internal error 4", name );
        return 0;
    }

    if( !ksmtp_write( smtp, sstr( out ), slen( out ) )
            || !ksmtp_write( smtp, sstr( b64 ), slen( b64 ) )
            || !ksmtp_write( smtp, "\r\n", 2 ) )
    {
        fclose( f );
        sdel( mime_name );
        sdel( b64 );
        sdel( out );
        smtpFormatError( smtp, "attachFile(\"%s\"), internal error 5", name );
        return 0;
    }

    sdel( mime_name );
    sdel( b64 );
    sdel( out );
    return 1;
}

static int embedFiles( Smtp smtp, const char * boundary )
{
    EFile file = lfirst( smtp->efiles );
    while( file )
    {
        if( !insertOneFile( smtp, boundary, file->name, file->ctype, "inline",
                file->cid ) ) return 0;
        file = lnext( smtp->efiles );
    }
    return 1;
}

static int attachFiles( Smtp smtp, const char * boundary )
{
    AFile file = lfirst( smtp->afiles );
    while( file )
    {
        if( !insertOneFile( smtp, boundary, file->name, file->ctype,
                "attachment", NULL ) ) return 0;
        file = lnext( smtp->afiles );
    }
    return 1;
}

static int write_boundary_end( Smtp smtp, const char * boundary )
{
    return ksmtp_write( smtp, "--", 2 )
            && ksmtp_write( smtp, boundary, strlen( boundary ) )
            && ksmtp_write( smtp, "--\r\n", 4 );
}

static int write_boundary( Smtp smtp, const char * boundary )
{
    return ksmtp_write( smtp, "--", 2 )
            && ksmtp_write( smtp, boundary, strlen( boundary ) )
            && ksmtp_write( smtp, "\r\n", 2 );
}

string createHeaders( Smtp smtp )
{
    string headers = snew();

    if( !headers
            || !makeEncodedHeader( smtp, "Subject", smtp->subject, headers )
            || !makeOneAddr( smtp, "From", smtp->from, headers )
            || !makeOneAddr( smtp, "Reply-To", smtp->replyto, headers )
            || !makeAddrList( smtp, "To", smtp->to, headers )
            || !makeAddrList( smtp, "Cc", smtp->cc, headers )
            || !makeAddrList( smtp, "Bcc", smtp->bcc, headers )
            || !makeDateHeader( headers )
            || !makeExtraHeaders( smtp, headers ) )
    {
        sdel( headers );
        return NULL;
    }
    return headers;
}

int processMessage( Smtp smtp, string headers )
{
    Addr addr;
    char mp_boundary[36];
    char r_boundary[36];
    string textparts = NULL;
    string related = NULL;
    string multipart = NULL;
    int rc = 1;

    textparts = makeTextParts( smtp );
    if( !textparts )
    {
        rc = 0;
        goto pmend;
    }

    if( !smtp_mail_from( smtp, smtp->from->email ) )
    {
        rc = 0;
        goto pmend;
    }

    addr = lfirst( smtp->to );
    while( addr )
    {
        if( !smtp_rcpt_to( smtp, addr->email ) )
        {
            rc = 0;
            goto pmend;
        }
        addr = lnext( smtp->to );
    }
    addr = lfirst( smtp->cc );
    while( addr )
    {
        if( !smtp_rcpt_to( smtp, addr->email ) )
        {
            rc = 0;
            goto pmend;
        }
        addr = lnext( smtp->cc );
    }
    addr = lfirst( smtp->bcc );
    while( addr )
    {
        if( !smtp_rcpt_to( smtp, addr->email ) )
        {
            rc = 0;
            goto pmend;
        }
        addr = lnext( smtp->bcc );
    }

    if( !smtp_data( smtp )
            || !ksmtp_write( smtp, sstr( headers ), slen( headers ) ) )
    {
        rc = 0;
        goto pmend;
    }

    if( smtp->efiles->size )
    {
        mimeMakeBoundary( r_boundary );
        related = sfromchar( "Content-Type: multipart/related; boundary=\"" );
        if( !related || !scatc( related, r_boundary )
                || !scatc( related, "\"\r\n\r\n" ) )
        {
            rc = 0;
            goto pmend;
        }
    }

    if( smtp->afiles->size )
    {
        mimeMakeBoundary( mp_boundary );
        multipart = sfromchar( "Content-Type: multipart/mixed; boundary=\"" );
        if( !multipart || !scatc( multipart, mp_boundary )
                || !scatc( multipart, "\"\r\n\r\n" ) )
        {
            rc = 0;
            goto pmend;
        }
        if( !ksmtp_write( smtp, sstr( multipart ), slen( multipart ) ) )
        {
            rc = 0;
            goto pmend;
        }
        if( slen(textparts) && !write_boundary( smtp, mp_boundary ) )
        {
            rc = 0;
            goto pmend;
        }

        if( related )
        {
            if( !ksmtp_write( smtp, sstr( related ), slen( related ) )
                    || !write_boundary( smtp, r_boundary )
                    || !ksmtp_write( smtp, sstr( textparts ),
                            slen( textparts ) )
                    || !embedFiles( smtp, r_boundary )
                    || !write_boundary_end( smtp, r_boundary ) )
            {
                rc = 0;
                goto pmend;
            }
        }

        if( !attachFiles( smtp, mp_boundary )
                || !write_boundary_end( smtp, mp_boundary ) )
        {
            rc = 0;
            goto pmend;
        }
    }
    else if( smtp->efiles->size )
    {
        if( !ksmtp_write( smtp, sstr( related ), slen( related ) )
                || !write_boundary( smtp, r_boundary )
                || !ksmtp_write( smtp, sstr( textparts ), slen( textparts ) )
                || !embedFiles( smtp, r_boundary )
                || !write_boundary_end( smtp, r_boundary ) )
        {
            rc = 0;
            goto pmend;
        }
    }
    else
    {
        if( slen( textparts )
                && !ksmtp_write( smtp, sstr( textparts ), slen( textparts ) ) )
        {
            rc = 0;
            goto pmend;
        }
    }
    pmend: sdel( textparts );
    sdel( multipart );
    sdel( related );
    return rc ? smtp_end_data( smtp ) : 0;
}
