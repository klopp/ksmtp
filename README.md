# Usage

```C
Smtp smtp = smtpCreate( KSMTP_USE_TLS );
string html;

smtpSetNodename( smtp, "my-local-host" );

smtpSetAuth( smtp, AUTH_LOGIN );
smtpSetSMTP( smtp, "smtp.yandex.com", 25 );
smtpSetLogin( smtp, "user@yandex.ru" );
smtpSetPassword( smtp, "password" );

smtpAddTo(smtp, "Alice Cooper <alice@test.com>");
smtpAddTo(smtp, "Ryan Roxie <ryan@test.com>");
smtpAddCc(smtp, "Mike Jagger <mike@test.com>");
smtpAddBcc(smtp, "Gene Simmons <gene@test.com>");

smtpAddHeader( smtp, "X-Custom-One", "One" );
smtpAddHeader( smtp, "X-Custom-Two", "Two" );

smtpSetSubject( smtp, "Hi, All!" );

smtpAddUtfTextPart( smtp, "Пинфлой фарева!", "plain" );

sprint
( 
    html, 
    "<html>"
    "<head><title>Hi, ALL!</title></head><body>"
    "<img src=\"cid:%s\" />\n"
    "<img src=\"cid:%s\" />\n"
    "</body></html>",
    smtpEmbedFile( smtp, "/tmp/1.png", NULL ),
    smtpEmbedFile( smtp, "/tmp/2.gif", "image/gif" )
);

smtpAddTextPart( smtp, sstr( html ), "html", "us-ascii" );

smtpAttachFile( smtp, "/tmp/1.yz", "application/x-yz" );

if( !smtpSendMail( smtp ) )
{
    printf( "(%s)\n", smtpGetError( smtp ) );
}

smtpCloseSession( smtp );
return smtpDestroy( smtp, 22 ); // return 22
```

