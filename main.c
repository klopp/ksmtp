/*
 * main.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 01:42
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "ksmtp.h"

int main( void )
{
    Smtp smtp = smtpCreate();

/*
#define USER        "vsevolod.lutovinov@ibic.se"
#define PASSWORD    "0UnrsZvNYGby"
#define HOST        "mail.ibic.se"

#define TO          "klopp@yandex.ru"

    smtpSetFrom( smtp, "Бумбастик <"USER">" );

    smtpSetAuth( smtp, AUTH_LOGIN );
    smtpSetSMTP( smtp, HOST, 2525 );
    smtpSetLogin( smtp, USER );
    smtpSetPassword( smtp, PASSWORD );
    smtpSetLogin( smtp, USER );

    smtpAddTo( smtp, TO );
    smtpSetSubject( smtp, "Проба" );
    smtpAddTextPart( smtp, html, "html" );

    smtpAddHeaderPair( smtp, "X-Custom-One", "One" );
    smtpAddHeader( smtp, "X-Custom-Two: Two" );

    smtpAddFile( smtp, "/home/klopp/tmp/проба.png", NULL );
*/

    return smtpDestroy( smtp, 0 );
}

