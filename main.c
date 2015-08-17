/*
 * main.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 01:42
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "ksmtp.h"
#include <signal.h>

int main( void )
{
    char html[] = "<html>"
            "<head><title>title</title></head>\n"
            "<body><h1>h1</h1>\n"
            "<h2>h2</h2>\n"
            //"<img src=\"http://ato.su/resizer/i/t/d/1/aafde8d5.jpg\" />\n"
            "<h3>h3</h3>\n"
            "<img src=\"cid:file@1\" />\n"
            "<h4>h4</h4>\n"
            //"<img src=\"test.png\" />\n"
            "</body></html>";

    Smtp smtp = smtpCreate(
            0 | KSMTP_USE_TLS/*| KSMTP_VERBOSE_MSG | KSMTP_VERBOSE_SMTP */);

/*
#define USER        "vsevolod.lutovinov@ibic.se"
#define PASSWORD    "0UnrsZvNYGby"
#define HOST        "mail.ibic.se"
#define TO          "Zazaza <klopp@yandex.ru>"
#define PORT        2525
*/

     #define USER        "klopp@yandex.ru"
     #define PASSWORD    "easypass123"
     #define HOST        "smtp.yandex.com"
     #define TO          "vsevolod.lutovinov@ibic.se" // "Zazaza <klopp.spb@gmail.com>"
     #define PORT        25

    /*
     #define USER        "kloppsob@bk.ru"
     #define PASSWORD    "vla212850a"
     #define HOST        "smtp.mail.ru"
     #define TO          "vsevolod.lutovinov@ibic.se" // "Zazaza <klopp.spb@gmail.com>"
     #define PORT        25
     */

    smtpSetFrom( smtp, "Бумбастик <"USER">" );

    smtpSetAuth( smtp, AUTH_LOGIN );
    smtpSetSMTP( smtp, HOST, PORT );
    smtpSetLogin( smtp, USER );
    smtpSetPassword( smtp, PASSWORD );
    smtpSetLogin( smtp, USER );

    smtpAddTo( smtp, TO );
    smtpAddTo( smtp, "v@ato.su" );
    smtpSetSubject( smtp,
            "А вот как насчёт такого очень-очень-очень офигенно длинного поля сабжект?" );
    //smtpSetSubject( smtp, "А вот как насчёт?" );

    smtpAddUtfTextPart( smtp, "ляляля", "plain" );
//    smtpAddUtfTextPart( smtp, "кукукук", "html" );
    smtpAddTextPart( smtp, html, "html", "us-ascii" );

    smtpAddHeader( smtp, "X-Custom-One", "One" );
    smtpAddHeader( smtp, "X-Custom-Two", "Two" );

#ifndef __WINDOWS__
//    smtpEmbedFile( smtp, "/home/klopp/tmp/1.png", NULL );
//    smtpAttachFile( smtp, "/home/klopp/tmp/test.png", NULL );
#else
    smtpEmbedFile( smtp, "/tmp/0.png", NULL );
    smtpAttachFile( smtp, "/tmp/0.png", NULL );
#endif

    signal( SIGTERM, SIG_IGN );
    signal( SIGINT, SIG_IGN );
    signal( SIGHUP, SIG_IGN );
    signal( SIGQUIT, SIG_IGN );
#ifndef __WINDOWS__
    signal( SIGPIPE, SIG_IGN );
#endif
    if( !smtpOpenSession( smtp ) )
    {
        printf( "[%s]\n", smtpGetError( smtp ) );
    }
    else
    {
        if( !smtpSendMail( smtp ) )
        {
            printf( "(%s)\n", smtpGetError( smtp ) );
        }
        else
        {
            printf( "OK\n" );
        }
//    smtpSetNodename( smtp, "klopp-local" );
//   smtpSendMail( smtp );
//    smtp->flags &= ~KSMTP_USE_TLS;
//    smtpSetPort( smtp, 26 );
//    smtpSendMail( smtp );
        smtpCloseSession( smtp );
    }

    //mp_dump( NULL, stdout, 70 );

    return smtpDestroy( smtp, 0 );
    /*
     if( !smtpSendOneMail( smtp ) )
     {
     printf( "ERROR %s\n", smtpGetError( smtp ) );
     return smtpDestroy( smtp, 1 );
     }
     printf( "Sent OK\n" );
     return smtpDestroy( smtp, 0 );
     */
}

