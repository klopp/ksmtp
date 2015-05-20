/*
 * mime.h, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 14:15
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#ifndef MIME_H_
#define MIME_H_

#include "../klib/config.h"
#include "../stringlib/stringlib.h"
#include "../stringlib/b64.h"

int isUsAscii( const char * s );
int isUsAsciiCs( const char * charset );
string mimeFileName( const char * name, const char * charset );
const char * getMimeType( const char * filename, const char * ctype );
char * mimeMakeBoundary( void );

#endif /* MIME_H_ */
