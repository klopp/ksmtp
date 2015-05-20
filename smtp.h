/*
 * smtp.h, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 03:39
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#ifndef SMTP_H_
#define SMTP_H_

#include "ksmtp.h"

int smtp_mail_from( Smtp smtp, const char * email );
int smtp_rcpt_to( Smtp smtp, const char * email );
int smtp_data( Smtp smtp );
int smtp_end_data( Smtp smtp );


/*
211 System status, or system help reply
         214 Help message
            [Information on how to use the receiver or the meaning of a
            particular non-standard command; this reply is useful only
            to the human user]
         220 <domain> Service ready
         221 <domain> Service closing transmission channel
         250 Requested mail action okay, completed
         251 User not local; will forward to <forward-path>

         354 Start mail input; end with <CRLF>.<CRLF>

         421 <domain> Service not available,
             closing transmission channel
            [This may be a reply to any command if the service knows it
            must shut down]
         450 Requested mail action not taken: mailbox unavailable
            [E.g., mailbox busy]
         451 Requested action aborted: local error in processing
         452 Requested action not taken: insufficient system storage

         500 Syntax error, command unrecognized
            [This may include errors such as command line too long]
         501 Syntax error in parameters or arguments
         502 Command not implemented
         503 Bad sequence of commands
         504 Command parameter not implemented
         550 Requested action not taken: mailbox unavailable
            [E.g., mailbox not found, no access]
         551 User not local; please try <forward-path>
         552 Requested mail action aborted: exceeded storage allocation
         553 Requested action not taken: mailbox name not allowed
            [E.g., mailbox syntax incorrect]
         554 Transaction failed
*/

#endif /* SMTP_H_ */
