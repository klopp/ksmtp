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
    char html[] =
            "<html>"
                    "<head><title>title</title></head>\n"
                    "<body><h1>h1</h1>\n"
                    "<h2>h2</h2>\n"
                    //"<img src=\"http://ato.su/resizer/i/t/d/1/aafde8d5.jpg\" />\n"
                    "<h3>h3</h3>\n"
                    //"<img src=\"cid:test.png\" />\n"
                    "<h4>h4</h4>\n"
                    //"<img src=\"test.png\" />\n"
                    "</body></html>";

    Smtp smtp = smtpCreate();

     #define USER        "vsevolod.lutovinov@ibic.se"
     #define PASSWORD    "0UnrsZvNYGby"
     #define HOST        "mail.ibic.se"
     #define TO          "Zazaza <klopp@yandex.ru>"

/*
#define USER        "klopp@yandex.ru"
#define PASSWORD    "easypass123"
#define HOST        "smtp.yandex.com"
#define TO          "Zazaza <kloppspb@bk.ru>"
*/

    smtp->tls = 1;
    smtpSetFrom( smtp, "Бумбастик <"USER">" );

    smtpSetAuth( smtp, AUTH_LOGIN );
    smtpSetSMTP( smtp, HOST, 25 );
    smtpSetLogin( smtp, USER );
    smtpSetPassword( smtp, PASSWORD );
    smtpSetLogin( smtp, USER );

    smtpAddTo( smtp, TO );
    smtpSetSubject( smtp,
            "А вот как насчёт такого очень-очень-очень офигенно длинного поля сабжект?" );
    //smtpSetSubject( smtp, "А вот как насчёт?" );

//    smtpAddUtfTextPart( smtp, "ляляля", "plain" );
    //    smtpAddUtfTextPart( smtp, "кукукук", "html" );
    smtpAddTextPart( smtp, html, "html", "us-ascii" );

    smtpAddHeader( smtp, "X-Custom-One", "One" );
//    smtpAddHeader( smtp, "X-Custom-Two", "Two" );

//    smtpAddFile( smtp, "/home/klopp/tmp/test.png", NULL );
    smtpAddFile( smtp, "/home/klopp/tmp/проба.png", NULL );

    if( !knet_init( smtp->tls ) )
    {
        printf( "Can not init socket library!\n" );
        return smtpDestroy( smtp, 1 );
    }

    /*
     signal(SIGTERM, properExit);
     signal(SIGINT, properExit);
     signal(SIGPIPE, properExit);
     signal(SIGHUP, properExit);
     signal(SIGQUIT, properExit);
     */
#ifndef __WINDOWS__
    signal( SIGPIPE, SIG_IGN );
#endif
    if( !smtpSendOneMail( smtp ) )
    {
        knet_down();
        printf( "ERROR %s\n", smtpGetError( smtp ) );
        return smtpDestroy( smtp, 1 );
    }
    printf( "Sent OK\n" );
    knet_down();
    return smtpDestroy( smtp, 0 );
}

