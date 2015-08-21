/*
 * kmsg.h, part of "kmail" project.
 *
 *  Created on: 20.05.2015, 00:42
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#ifndef KMSG_H_
#define KMSG_H_

#include "../klib/plist.h"
#include "../stringlib/stringlib.h"

#define KMSG_DEFAULT_CHARSET    "UTF-8"
#define KFILE_CONTENT_ID        "file@"

/*
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
*/

#define F_NAME( pair )  (pair)->first
#define F_CTYPE( pair ) (pair)->second

#define H_NAME( pair )  (pair)->first
#define H_VALUE( pair ) (pair)->second

typedef struct _EFile
{
    char * name;
    char * ctype;
    char cid[sizeof(KFILE_CONTENT_ID) * 2];
}*EFile;

typedef struct _MFile
{
    string headers;
    string body;
}*MFile;

typedef struct _TextPart
{
    char * body;
    char * ctype;
    char charset[32];
    char cprefix[40];
}*TextPart;

typedef struct _KMsg
{
    char charset[32];
    char cprefix[40];
    char *subject;
    char *xmailer;

    size_t lastid;
    PList afiles;
    List efiles;
    List parts;
    PList headers;
    PList to;
    PList cc;
    PList bcc;
    Pair from;
    Pair replyto;

}*KMsg;

KMsg msg_Create( void );
void msg_Destroy( KMsg msg );

int msg_SetFrom( KMsg msg, const char * from );
int msg_SetReplyTo( KMsg msg, const char * rto );

int msg_AddTo( KMsg msg, const char * to );
int msg_AddCc( KMsg msg, const char * cc );
int msg_AddBcc( KMsg msg, const char * bcc );
int msg_AttachFile( KMsg msg, const char * file, const char * ctype );
const char * msg_EmbedFile( KMsg msg, const char * file, const char * ctype );

void msg_ClearTo( KMsg msg );
void msg_ClearCc( KMsg msg );
void msg_ClearBcc( KMsg msg );
void msg_ClearAFiles( KMsg msg );
void msg_ClearEFiles( KMsg msg );

void msg_SetCharset( KMsg msg, const char * charset );

int msg_SetXmailer( KMsg msg, const char * xmailer );
int msg_AddHeader( KMsg msg, const char * key, const char * val );
void msg_ClearHeaders( KMsg msg );

int msg_SetSubject( KMsg msg, const char * subj );
int msg_AddUtfTextPart( KMsg msg, const char * body, const char * ctype );
int msg_AddDefTextPart( KMsg msg, const char * body, const char *ctype );
int msg_AddTextPart( KMsg msg, const char * body, const char * ctype,
        const char * charset );

string msg_CreateHeaders( KMsg msg );
string msg_CreateBody( KMsg msg );
int msg_CreateFile( KMsg msg, MFile file, string error, const char * boundary,
        const char * name, const char * ctype, const char * disposition,
        const char * cid );

#endif /* KMSG_H_ */
