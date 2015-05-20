/*
 * message.h, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 15:10
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#ifndef MESSAGE_H_
#define MESSAGE_H_

#include "ksmtp.h"

string createMessage( Smtp smtp );
int processMessage( Smtp smtp, string msg );

#endif /* MESSAGE_H_ */
