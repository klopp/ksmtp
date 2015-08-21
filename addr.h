/*
 * addr.h, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 01:15
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#ifndef ADDR_H_
#define ADDR_H_

#include "kmsg.h"
#include "../klib/plist.h"

#define A_EMAIL( pair ) (pair)->second
#define A_NAME( pair )  (pair)->first

Pair createAddr( const char * src );

#endif /* ADDR_H_ */
