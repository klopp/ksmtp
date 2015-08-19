/*
 * kmail.h, part of "ksmtp" project.
 *
 *  Created on: 18.08.2015, 23:38
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#ifndef KMAIL_H_
#define KMAIL_H_

#include "../knet/ksmtp.h"
#include "kmsg.h"

typedef enum _AuthType
{
    AUTH_LOGIN = 1, AUTH_PLAIN = 2
} AuthType;

typedef enum _KmailFlags
{
    KMAIL_VERBOSE_MSG = 0x01,
    KMAIL_VERBOSE_SMTP = 0x02,
    KMAIL_DEFAULT = 0x00
} KmailFlags;

typedef struct _KMail
{
    KSmtp smtp;
    KmailFlags flags;
    string error;
    string login;
    string password;
    string host;
    int port;

}*KMail;

KMail mail_Create( KmailFlags flags, const char * node, int timeout );
void mail_Destroy( KMail mail );

#define mail_GetError( mail ) sstr((mail)->error)
#define mail_SetError( mail, err ) scpyc( (mail)->error, (err) )
#define mail_FormatError( mail, fmt, ... ) sprint( (mail)->error, (fmt), __VA_ARGS__ )

int mail_SetSMTP( KMail mail, const char * host, int port );
int mail_SetHost( KMail mail, const char * host );
int mail_SetPort( KMail mail, int port );
int mail_SetLogin( KMail mail, const char * login );
int mail_SetPassword( KMail mail, const char * password );

int mail_OpenSession( KMail mail, int tls, AuthType auth );
int mail_SendMessage( KMail mail, KMsg msg );
void mail_CloseSession( KMail mail );

#endif /* KMAIL_H_ */
