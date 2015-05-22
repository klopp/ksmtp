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

#include "knet.h"

#include <limits.h>

typedef enum _AuthType
{
    AUTH_LOGIN = 1, AUTH_PLAIN = 2
} AuthType;

#define KSMTP_DEFAULT_CHARSET   "UTF-8"
#define KFILE_CONTENT_ID        "file@"

typedef struct _Addr
{
    char * name;
    char * email;
}*Addr;

typedef struct _Header
{
    char * title;
    char * value;
}*Header;

typedef struct _AFile
{
    char * name;
    char * ctype;
}*AFile;

typedef struct _EFile
{
    char * name;
    char * ctype;
    char cid[sizeof(KFILE_CONTENT_ID) + 8 - (sizeof(KFILE_CONTENT_ID) % 8)];
}*EFile;

typedef struct _TextPart
{
    char * body;
    char * ctype;
    char charset[32];
    char cprefix[32];
}*TextPart;

typedef enum _KsmtpFlags
{
    KSMTP_DEBUG = 0x01, KSMTP_USE_TLS = 0x02, KSMTP_DEFAULT = 0x00
} KsmtpFlags;

typedef struct _Smtp
{
    KsmtpFlags flags;

    int port;
    int timeout;

    struct _ksocket sd;
    string error;
    string current;
    char *boundary;
#ifndef __WINDOWS__
    char nodename[HOST_NAME_MAX + 1];
#else
    char nodename[ PATH_MAX + 1 ];
#endif
    char charset[32];
    char cprefix[32];
    char *subject;
    char *xmailer;
    char *smtp_user;
    AuthType smtp_auth;
    char *smtp_password;
    char *host;

    size_t lastid;
    List afiles;
    List efiles;
    List parts;
    List headers;
    List to;
    List cc;
    List bcc;
    Addr from;
    Addr replyto;

}*Smtp;

Smtp smtpCreate( KsmtpFlags flags );
int smtpDestroy( Smtp smtp, int retcode );

#define smtpGetError( smtp ) sstr((smtp)->error)
#define smtpSetError( smtp, err ) scpyc( (smtp)->error, (err) )
#define smtpFormatError( smtp, fmt, ... ) sprint( (smtp)->error, (fmt), __VA_ARGS__ )

/*
 * smtpSetFrom( smtp, "doe@test.com" )
 * smtpAddTo( smtp, "John Doe <john@test.com>" )
 */
int smtpSetFrom( Smtp smtp, const char * from );
int smtpSetReplyTo( Smtp smtp, const char * rto );

int smtpAddTo( Smtp smtp, const char * to );
int smtpAddCc( Smtp smtp, const char * cc );
int smtpAddBcc( Smtp smtp, const char * bcc );
int smtpAttachFile( Smtp smtp, const char * file, const char * ctype );
const char * smtpEmbedFile( Smtp smtp, const char * file, const char * ctype );

void smtpClearTo( Smtp smtp );
void smtpClearCc( Smtp smtp );
void smtpClearBcc( Smtp smtp );
void smtpClearAFiles( Smtp smtp );
void smtpClearEFiles( Smtp smtp );

void smtpSetCharset( Smtp smtp, const char * charset );
void smtpSetNodename( Smtp smtp, const char * node );
int smtpSetTimeout( Smtp smtp, int timeout );
int smtpSetAuth( Smtp smtp, AuthType auth );

int smtpSetSMTP( Smtp smtp, const char * host, int port );
int smtpSetHost( Smtp smtp, const char * host );
int smtpSetPort( Smtp smtp, int port );
int smtpSetLogin( Smtp smtp, const char * login );
int smtpSetPassword( Smtp smtp, const char * password );

int smtpSetXmailer( Smtp smtp, const char * xmailer );
int smtpAddHeader( Smtp smtp, const char * key, const char * val );
void smtpClearHeaders( Smtp smtp );

int smtpSetSubject( Smtp smtp, const char * subj );
int smtpAddTextPart( Smtp smtp, const char * body, const char * ctype,
        const char * charset );
int smtpAddUtfTextPart( Smtp smtp, const char * body, const char * ctype );
int smtpAddDefTextPart( Smtp smtp, const char * body, const char *ctype );

int smtpOpenSession( Smtp smtp );
int smtpSendMail( Smtp smtp );
void smtpCloseSession( Smtp smtp );

int smtpSendOneMail( Smtp smtp );

#endif /* KSMTP_H_ */
