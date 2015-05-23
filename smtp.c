/*
 * smtp.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 03:40
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "ksmtp.h"
#include "message.h"
#include "../stringlib/b64.h"

static int smtp_answer( Smtp smtp )
{
    char buf[8];
    do
    {
        int c;
        memset( buf, 0, sizeof(buf) );
        if( knet_read( &smtp->sd, buf, 4 ) != 4 )
        {
            smtpFormatError( smtp, "smtp_answer(1): %s",
                    knet_error_msg( &smtp->sd ) );
            return 0;
        }
        scpyc( smtp->current, buf );
        while( (c = knet_getc( &smtp->sd )) != '\n' && !smtp->sd.eof/*!knet_eof( &smtp->sd )*/)
        {
            if( c == -1 )
            {
                smtpFormatError( smtp, "smtp_answer(2): %s",
                        knet_error_msg( &smtp->sd ) );
                return 0;
            }
            else
            {
                scatch( smtp->current, c );
            }
        }
        if( smtp->flags & KSMTP_VERBOSE_SMTP )
        {
            schomp( smtp->current );
            fprintf( stderr, "<<< %s\n", sstr( smtp->current ) );
        }

    } while( buf[3] != ' ' );

    return (buf[0] - '0') * 100 + (buf[1] - '0') * 10 + (buf[2] - '0');
}

static int smtp_write( Smtp smtp, const char * buf )
{
    if( smtp->flags & KSMTP_VERBOSE_SMTP )
    {
        scpyc( smtp->current, buf );
        schomp( smtp->current );
        fprintf( stderr, ">>> %s\n", sstr( smtp->current ) );
    }
    knet_write( &smtp->sd, buf, strlen( buf ) );
    if( smtp->sd.error/*knet_error( &smtp->sd )*/)
    {
        smtpFormatError( smtp, "smtp_write(): %s",
                knet_error_msg( &smtp->sd ) );
        return 0;
    }
    return 1;
}

static int smtp_cmd( Smtp smtp, const char * cmd, int ok, int ko )
{
    int rc;
    if( cmd && !smtp_write( smtp, cmd ) ) return 0;
    rc = smtp_answer( smtp );
    if( !rc ) return 0;
    if( rc == ok ) return rc;
    if( ko && rc == ko ) return rc;
    schomp( smtp->current );
    if( ko )
    {
        smtpFormatError( smtp, "smtp_cmd(): want %u or %u, got [%s]", ok, ko,
                sstr(smtp->current) );
    }
    else
    {
        smtpFormatError( smtp, "smtp_cmd(): want %u, got [%s]", ok,
                sstr(smtp->current) );
    }
    return 0;
}

static int smtp_init( Smtp smtp )
{
    return smtp_cmd( smtp, NULL, 220, 0 );
}

static int smtp_helo( Smtp smtp )
{
    int rc;
    string buf = snew();
    sprint( buf, "HELO %s\r\n", smtp->nodename );
    rc = smtp_cmd( smtp, sstr( buf ), 250, 0 );
    sdel( buf );
    return rc;
}

static int smtp_ehlo( Smtp smtp )
{
    int rc;
    string buf = snew();
    sprint( buf, "EHLO %s\r\n", smtp->nodename );
    rc = smtp_cmd( smtp, sstr( buf ), 250, 0 );
    sdel( buf );
    return rc;
}

int smtp_data( Smtp smtp )
{
    return smtp_cmd( smtp, "DATA\r\n", 354, 0 );
}

static int smtp_rset( Smtp smtp )
{
    return smtp_cmd( smtp, "RSET\r\n", 250, 0 );
}

int smtp_mail_from( Smtp smtp, const char * email )
{
    int rc;
    string buf = snew();
    sprint( buf, "MAIL FROM:<%s>\r\n", email );
    rc = smtp_cmd( smtp, sstr( buf ), 250, 0 );
    sdel( buf );
    return rc;
}

int smtp_rcpt_to( Smtp smtp, const char *email )
{
    int rc;
    string buf = snew();
    sprint( buf, "RCPT TO:<%s>\r\n", email );
    rc = smtp_cmd( smtp, sstr( buf ), 250, 251 );
    sdel( buf );
    return rc;
}

static int smtp_quit( Smtp smtp )
{
    return smtp_cmd( smtp, "QUIT\r\n", 221, 0 );
}

static int smtp_auth_login( Smtp smtp )
{
    int rc = 0;
    string buf = snew();
    string data = base64_sencode( smtp->smtp_user );
    sprint( buf, "AUTH LOGIN %s\r\n", sstr( data ) );
    rc = smtp_cmd( smtp, sstr( buf ), 334, 0 );
    if( !rc )
    {
        sdel( buf );
        sdel( data );
        return 0;
    }
    sdel( data );
    data = base64_sencode( smtp->smtp_password );
    sprint( buf, "%s\r\n", sstr( data ) );
    rc = smtp_cmd( smtp, sstr( buf ), 235, 0 );
    sdel( buf );
    sdel( data );
    return rc;
}

static int smtp_auth_plain( Smtp smtp )
{
    string buf;
    string data;
    int rc = smtp_cmd( smtp, "AUTH PLAIN\r\n", 334, 0 );
    if( !rc ) return 0;
    buf = snew();
    sprint( buf, "%c%s%c%s", '\0', smtp->smtp_user, '\0', smtp->smtp_password );
    data = base64_encode( sstr( buf ), slen( buf ) );
    sprint( buf, "%s\r\n", sstr( data ) );
    rc = smtp_cmd( smtp, sstr( data ), 235, 0 );
    sdel( data );
    sdel( buf );
    return rc;
}

static int smtpInit( Smtp smtp )
{
    int rc = smtp_init( smtp );
    if( !rc )
    {
        return rc;
    }

    rc = smtp_ehlo( smtp );
    if( !rc )
    {
        smtp_rset( smtp );
        rc = smtp_helo( smtp );
    }
    return rc;
}

static int smtp_start_tls( Smtp smtp )
{
    return smtp_cmd( smtp, "STARTTLS\r\n", 220, 0 );
}

int smtp_end_data( Smtp smtp )
{
    return smtp_cmd( smtp, "\r\n.\r\n", 250, 0 );
}

void smtpCloseSession( Smtp smtp )
{
    smtp_quit( smtp );
}

int smtpSendMail( Smtp smtp )
{
    int rc = 0;
    string headers = createHeaders( smtp );
    if( !headers ) return 0;
    rc = processMessage( smtp, headers );
    sdel( headers );
    return rc;
}

int smtpOpenSession( Smtp smtp )
{
    if( !smtp->host )
    {
        smtpSetError( smtp, "No SMTP host!" );
        return 0;
    }
    if( smtp->port < 1 )
    {
        smtpSetError( smtp, "Invalid SMTP port!" );
        return 0;
    }
    if( !smtp->to || smtp->to->size < 1 )
    {
        smtpSetError( smtp, "No To: address(es)!" );
        return 0;
    }
    if( !smtp->from || !smtp->from->email )
    {
        smtpSetError( smtp, "No From: address!" );
        return 0;
    }
    if( !smtp->parts->size && !smtp->afiles->size )
    {
        smtpSetError( smtp, "No text parts and attached files!" );
        return 0;
    }
    if( !smtp->parts->size && smtp->efiles->size )
    {
        smtpSetError( smtp, "Embedded filed detected, but no text parts!" );
        return 0;
    }

    if( smtp->smtp_auth
            && (smtp->smtp_auth != AUTH_PLAIN && smtp->smtp_auth != AUTH_LOGIN) )
    {
        smtpFormatError( smtp, "Unknown AUTH type: %d", smtp->smtp_auth );
        return 0;
    }

    if( !knet_connect( &smtp->sd, smtp->host, smtp->port ) )
    {
        smtpFormatError( smtp, "Could not connect to %s:%d", smtp->host,
                smtp->port );
        return 0;
    }

    if( !smtpInit( smtp ) ) return 0;

    if( smtp->flags & KSMTP_USE_TLS )
    {
        if( !smtp_start_tls( smtp ) || !knet_init_tls( &smtp->sd )
                || !knet_verify_sert( &smtp->sd ) || !smtp_ehlo( smtp ) )
        {
            smtpFormatError( smtp, "Could not initialize TLS for %s:%d",
                    smtp->host, smtp->port );
            return 0;
        }
    }

    if( smtp->smtp_auth )
    {
        if( smtp->smtp_auth == AUTH_LOGIN )
        {
            if( !smtp_auth_login( smtp ) )
            {
                return 0;
            }
        }
        else
        {
            if( !smtp_auth_plain( smtp ) )
            {
                return 0;
            }
        }
    }
    return 1;
}
