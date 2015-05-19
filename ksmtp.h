/*
 * ksmtp.h, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 00:42
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#ifndef KSMTP_H_
#define KSMTP_H_

#include "../klib/config.h"
#include "../lists/list.h"
#include "../stringlib/stringlib.h"
#include <limits.h>

typedef enum _AuthType
{
    AUTH_LOGIN = 1, AUTH_PLAIN = 2
} AuthType;

typedef struct _Addr
{
    char * name;
    char * email;
}*Addr;

typedef struct _File
{
    char * name;
    char * ctype;
}*File;

typedef struct _TextPart
{
    char * body;
    char * ctype;
//    CharSetType cs;
}*TextPart;

typedef struct _Smtp
{
    int tls;
    int debug;

    int port;
    int timeout;

//    dsocket *sd;
    string error;
    char boundary[32];
#ifndef __WINDOWS__
    char nodename[ HOST_NAME_MAX + 1 ];
#else
    char nodename[ PATH_MAX + 1 ];
#endif
    char *subject;
    char *xmailer;
    char *smtp_user;
    AuthType smtp_auth;
    char *smtp_password;
    char *host;

    List files;
    List parts;
    List headers;
    List to;
    List cc;
    List bcc;
    Addr from;
    Addr replyto;
}*Smtp;

Smtp smtpCreate( void );
int smtpDestroy( Smtp smtp, int retcode );
const char * smtpGetError( Smtp smtp );
//Smtp smtpSetError( Smtp smtp, const char * error );
Smtp smtpFormatError( Smtp smtp, const char *fmt, ... );

int smtpSetFrom( Smtp smtp, const char * from );
int smtpSetReplyTo( Smtp smtp, const char * rto );

int smtpAddTo( Smtp smtp, const char * to );
int smtpAddCc( Smtp smtp, const char * cc );
int smtpAddBcc( Smtp smtp, const char * bcc );
Smtp smtpAddFile( Smtp smtp, const char * file, const char * ctype );

Smtp smtpClearTo( Smtp smtp );
Smtp smtpClearCc( Smtp smtp );
Smtp smtpClearBcc( Smtp smtp );
Smtp smtpClearFiles( Smtp smtp );

Smtp smtpSetNodename( Smtp smtp, const char * node );
int smtpSetTimeout( Smtp smtp, int timeout );
int smtpSetAuth( Smtp smtp, AuthType auth );

int smtpSetSMTP( Smtp smtp, const char * host, int port );
Smtp smtpSetHost( Smtp smtp, const char * host );
int smtpSetPort( Smtp smtp, int port );
Smtp smtpSetLogin( Smtp smtp, const char * login );
Smtp smtpSetPassword( Smtp smtp, const char * password );

Smtp smtpSetXmailer( Smtp smtp, const char * xmailer );
Smtp smtpAddHeader( Smtp smtp, const char * hdr );
//Smtp smtpAddHeaderPair( Smtp smtp, const char * key, const char * val );
Smtp smtpClearHeaders( Smtp smtp );

Smtp smtpSetSubject( Smtp smtp, const char * subj );
int smtpAddTextPart( Smtp smtp, const char * body, const char * ctype );

int smtpOpenSession( Smtp smtp );
void smtpCloseSession( Smtp smtp );
int smtpSendMail( Smtp smtp );
int smtpSendOneMail( Smtp smtp );

#endif /* KSMTP_H_ */
