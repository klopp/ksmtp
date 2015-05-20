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

#endif /* SMTP_H_ */
