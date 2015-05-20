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

    if( isUsAscii( value ) )
    {
        if( !scatc( msg, value ) ) return 0;
    }
    else
    {
        string b64 = base64_sencode( value, smtp->cprefix );
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
            string b64 = base64_sencode( a->name, smtp->cprefix );
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
            string b64 = base64_sencode( part->body, part->cprefix );
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
    return 1;
}
/*
static int attachFiles( Smtp mopts )
{
    int retval = 0;
    File next_file = NULL;
    dstrbuf * out;

    if( retval == _ERROR ) return retval;

    while( (next_file = (File)dlGetNext( mopts->files )) != NULL )
    {
        dstrbuf *file_type;
        dstrbuf *file_name;
        FILE *current = fopen( next_file->name, "rb" );
        if( !current )
        {
            smtpFormatError( mopts, "Could not open attachment: %s",
                    next_file->name );
            retval = _ERROR;
            break;
        }
        // If the user specified an absolute path, just get the file name
        file_type = mimeFiletype( next_file->name, next_file->ctype );
        file_name = mimeFilename( next_file->name );
        out = DSB_NEW;
        dsbPrintf( out, "\r\n--%s\r\n", mopts->boundary->str );
        dsbPrintf( out, "Content-Transfer-Encoding: base64\r\n" );
        dsbPrintf( out, "Content-Type: %s; name=\"%s\"\r\n", file_type->str,
                file_name->str );
        dsbPrintf( out, "Content-Disposition: attachment; filename=\"%s\"\r\n",
                file_name->str );
        dsbPrintf( out, "\r\n" );

        mimeB64EncodeFile( current, out );
        retval = smtpSendData( mopts, out->str, out->size );
        dsbDestroy( out );
        dsbDestroy( file_type );
        dsbDestroy( file_name );
        if( retval == _ERROR )
        {
            break;
        }
    }
    return retval;
}

 */
int processMessage( Smtp smtp, string msg )
{
    FILE * fout = fopen( "/home/klopp/tmp/ksmtp.log", "w" );
    if( !fout ) return 0;
    fprintf( fout, "%s\n", sstr( msg ) );

    if( smtp->files && smtp->files->size )
    {
        if( !attachFiles( smtp, fout ) ) return 0;
    }

    if( smtp->boundary )
    {
        fprintf( fout, "\r\n--%s--\r\n", smtp->boundary );
    }

    fclose( fout );
    return 1;
}
/*
 int processMessage( Smtp mopts, dstrbuf *msg )
 {
 int retval = 0, bytes;
 char *ptr = msg->str;
 Addr next = NULL;

 retval = smtpSetMailFrom( mopts, mopts->from->email );

 if( retval == _ERROR )
 {
 goto end;
 }

 while( (next = (Addr)dlGetNext( mopts->to )) != NULL )
 {
 retval = smtpSetRcpt( mopts, next->email );
 if( retval == _ERROR )
 {
 goto end;
 }
 }
 while( (next = (Addr)dlGetNext( mopts->cc )) != NULL )
 {
 retval = smtpSetRcpt( mopts, next->email );
 if( retval == _ERROR )
 {
 goto end;
 }
 }
 while( (next = (Addr)dlGetNext( mopts->bcc )) != NULL )
 {
 retval = smtpSetRcpt( mopts, next->email );
 if( retval == _ERROR )
 {
 goto end;
 }
 }

 retval = smtpStartData( mopts );
 if( retval == _ERROR )
 {
 goto end;
 }
 while( *ptr != '\0' )
 {
 bytes = strlen( ptr );
 if( bytes > CHUNK_BYTES )
 {
 bytes = CHUNK_BYTES;
 }
 retval = smtpSendData( mopts, ptr, bytes );
 if( retval == _ERROR )
 {
 goto end;
 }
 ptr += bytes;
 }

 if( mopts->files && mopts->files->size )
 {
 retval = attachFiles( mopts );
 if( retval == _ERROR )
 {
 goto end;
 }
 }

 if( mopts->boundary )
 {
 if( smtpSendData( mopts, "\r\n--", sizeof("\r\n--") - 1 ) == _ERROR
 || smtpSendData( mopts, mopts->boundary->str,
 mopts->boundary->len ) == _ERROR
 || smtpSendData( mopts, "--\r\n", sizeof("--\r\n") - 1 )
 == _ERROR )
 {
 retval = _ERROR;
 goto end;
 }
 }
 retval = smtpEndData( mopts );

 end: return retval != _ERROR;
 }
 */

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

