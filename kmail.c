/*
 * kmail.c, part of "ksmtp" project.
 *
 *  Created on: 18.08.2015, 23:38
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "kmail.h"
#include "mime.h"

KMail mail_Create( KmailFlags flags, const char * node, int timeout )
{
    KMail mail = (KMail)Calloc( sizeof(struct _KMail), 1 );
    if( !mail ) return NULL;

    mail->smtp = smtp_Create( timeout, node, flags & KMAIL_VERBOSE_SMTP );

    mail->flags = flags;

    mail->error = snew();
    mail->login = snew();
    mail->password = snew();
    mail->host = snew();
    mail->port = 25;

    if( !mail->error || !mail->login || !mail->password || !mail->port )
    {
        mail_Destroy( mail );
        mail = NULL;
    }
    return mail;
}

void mail_Destroy( KMail mail )
{
    sdel( mail->error );
    sdel( mail->login );
    sdel( mail->password );
    sdel( mail->host );
    smtp_Destroy( mail->smtp );
    Free( mail );
}

int mail_SetLogin( KMail mail, const char * login )
{
    return scpyc( mail->login, login ) != NULL;
}

int mail_SetPassword( KMail mail, const char * password )
{
    return scpyc( mail->password, password ) != NULL;
}

int mail_SetSMTP( KMail mail, const char * host, int port )
{
    if( mail_SetPort( mail, port ) )
    {
        mail_SetHost( mail, host );
        return 1;
    }
    return 0;
}

int mail_SetHost( KMail mail, const char * host )
{
    return scpyc( mail->host, host ) != NULL;
}

int mail_SetPort( KMail mail, int port )
{
    if( port > 0 && port < INT_MAX )
    {
        mail->port = port;
        return 1;
    }
    return 0;
}

static int mail_set_SMTP_error( KMail mail )
{
    scpy( mail->error, mail->smtp->error );
    return 0;
}

int mail_OpenSession( KMail mail, int tls, AuthType auth )
{
    if( !smtp_OpenSession( mail->smtp, sstr( mail->host ), mail->port, tls ) )
    {
        return mail_set_SMTP_error( mail );
    }

    if( auth == AUTH_PLAIN )
    {
        if( !smtp_AUTH_PLAIN( mail->smtp, sstr( mail->login ),
                sstr( mail->password ) ) ) return mail_set_SMTP_error( mail );
    }
    else if( auth == AUTH_LOGIN )
    {
        if( !smtp_AUTH_LOGIN( mail->smtp, sstr( mail->login ),
                sstr( mail->password ) ) ) return mail_set_SMTP_error( mail );
    }
    else
    {
        mail_FormatError( mail, "Unknown AUTH type: %d", auth );
        return 0;
    }

    return 1;
}

void mail_CloseSession( KMail mail )
{
    smtp_CloseSession( mail->smtp );
}

static int delMFile( MFile file )
{
    sdel( file->body );
    sdel( file->headers );
    return 0;
}

static int mail_EmbedFiles( KMail mail, KMsg msg, const char * boundary )
{
    struct _MFile file =
    { NULL, NULL };
    EFile efile = lfirst( msg->efiles );
    file.headers = snew();

    while( efile )
    {
        if( !msg_CreateFile( msg, &file, mail->error, boundary, efile->name,
                efile->ctype, "inline", efile->cid ) )
        {
            return delMFile( &file );
        }
        if( !smtp_write( mail->smtp, sstr( file.headers ))
                || !smtp_write( mail->smtp, sstr( file.body ))
                || !smtp_write( mail->smtp, "\r\n" ) )
        {
            mail_FormatError( mail, "mail_EmbedFiles(\"%s\"), internal error",
                    efile->name );
            return delMFile( &file );
        }
        efile = lnext( msg->efiles );
    }
    delMFile( &file );
    return 1;
}

static int mail_AttachFiles( KMail mail, KMsg msg, const char * boundary )
{
    struct _MFile file =
    { NULL, NULL };
    AFile afile = lfirst( msg->afiles );
    file.headers = snew();

    while( afile )
    {
        if( !msg_CreateFile( msg, &file, mail->error, boundary, afile->name,
                afile->ctype, "attachment", NULL ) )
        {
            return delMFile( &file );
        }
        if( !smtp_write( mail->smtp, sstr( file.headers ))
                || !smtp_write( mail->smtp, sstr( file.body ))
                || !smtp_write( mail->smtp, "\r\n") )
        {
            mail_FormatError( mail, "mail_AttachFiles(\"%s\"), internal error",
                    afile->name );
            return delMFile( &file );
        }
        afile = lnext( msg->afiles );
    }
    delMFile( &file );
    return 1;
}

static int write_boundary_end( KSmtp smtp, const char * boundary )
{
    return smtp_write( smtp, "--" )
            && smtp_write( smtp, boundary )
            && smtp_write( smtp, "--\r\n" );
}

static int write_boundary( KSmtp smtp, const char * boundary )
{
    return smtp_write( smtp, "--" )
            && smtp_write( smtp, boundary )
            && smtp_write( smtp, "\r\n" );
}

int mail_SendMessage( KMail mail, KMsg msg )
{
    Addr addr;
    int rc = 1;
    string out = NULL;
    string related = NULL;
    string multipart = NULL;
    char mp_boundary[36];
    char r_boundary[36];

    if( !smtp_MAIL_FROM( mail->smtp, msg->from->email ) )
    {
        rc = 0;
        goto pmend;
    }

    addr = lfirst( msg->to );
    while( addr )
    {
        if( !smtp_RCPT_TO( mail->smtp, addr->email ) )
        {
            rc = 0;
            goto pmend;
        }
        addr = lnext( msg->to );
    }
    addr = lfirst( msg->cc );
    while( addr )
    {
        if( !smtp_RCPT_TO( mail->smtp, addr->email ) )
        {
            rc = 0;
            goto pmend;
        }
        addr = lnext( msg->cc );
    }

    addr = lfirst( msg->bcc );
    while( addr )
    {
        if( !smtp_RCPT_TO( mail->smtp, addr->email ) )
        {
            rc = 0;
            goto pmend;
        }
        addr = lnext( msg->bcc );
    }

    out = msg_CreateHeaders( msg );
    if( !out )
    {
        rc = 0;
        goto pmend;
    }

    if( !smtp_DATA( mail->smtp )
            || !smtp_write( mail->smtp, sstr( out ) ) )
    {
        rc = 0;
        goto pmend;
    }

    sdel( out );
    out = msg_CreateBody( msg );
    if( !out )
    {
        rc = 0;
        goto pmend;
    }

    if( msg->efiles->size )
    {
        mimeMakeBoundary( r_boundary );
        related = sfromchar( "Content-Type: multipart/related; boundary=\"" );
        if( !related || !xscatc( related, r_boundary, "\"\r\n\r\n", NULL ) )
        {
            rc = 0;
            goto pmend;
        }
    }

    if( msg->afiles->size )
    {
        mimeMakeBoundary( mp_boundary );
        multipart = sfromchar( "Content-Type: multipart/mixed; boundary=\"" );
        if( !multipart
                || !xscatc( multipart, mp_boundary, "\"\r\n\r\n", NULL ) )
        {
            rc = 0;
            goto pmend;
        }
        if( !smtp_write( mail->smtp, sstr( multipart ) ) )
        {
            rc = 0;
            goto pmend;
        }
        if( slen(out) && !write_boundary( mail->smtp, mp_boundary ) )
        {
            rc = 0;
            goto pmend;
        }

        if( related )
        {
            if( !smtp_write( mail->smtp, sstr( related ) )
                    || !write_boundary( mail->smtp, r_boundary )
                    || !smtp_write( mail->smtp, sstr( out ) )
                    || !mail_EmbedFiles( mail, msg, r_boundary )
                    || !write_boundary_end( mail->smtp, r_boundary ) )
            {
                rc = 0;
                goto pmend;
            }
        }

        if( !mail_AttachFiles( mail, msg, mp_boundary )
                || !write_boundary_end( mail->smtp, mp_boundary ) )
        {
            rc = 0;
            goto pmend;
        }
    }
    else if( msg->efiles->size )
    {
        if( !smtp_write( mail->smtp, sstr( related ) )
                || !write_boundary( mail->smtp, r_boundary )
                || !smtp_write( mail->smtp, sstr( out ) )
                || !mail_EmbedFiles( mail, msg, r_boundary )
                || !write_boundary_end( mail->smtp, r_boundary ) )
        {
            rc = 0;
            goto pmend;
        }
    }
    else
    {
        if( slen( out ) && !smtp_write( mail->smtp, sstr( out ) ) )
        {
            rc = 0;
            goto pmend;
        }
    }

    pmend: sdel( out );
    sdel( related );
    sdel( multipart );
    smtp_END_DATA( mail->smtp );
    return rc;

    /*
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
     if( !related || !xscatc( related, r_boundary, "\"\r\n\r\n", NULL ) )
     {
     rc = 0;
     goto pmend;
     }
     }

     if( smtp->afiles->size )
     {
     mimeMakeBoundary( mp_boundary );
     multipart = sfromchar( "Content-Type: multipart/mixed; boundary=\"" );
     if( !multipart
     || !xscatc( multipart, mp_boundary, "\"\r\n\r\n", NULL ) )
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
     */
}

