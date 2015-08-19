# Usage

```C
knet_init();

KMail mail = mail_Create( 0, NULL, 0 );
mail_SetSMTP( mail, HOST, PORT );
mail_SetLogin( mail, USER );
mail_SetPassword( mail, PASSWORD );
mail_SetLogin( mail, USER );

KMsg msg = msg_Create();
msg_AddTo( msg, TO );
msg_SetFrom( msg, USER );
msg_SetSubject( msg, "Тема" );
msg_AddDefTextPart( msg, "Текст", "plain" );
msg_AttachFile( msg, "/tmp/5.png", NULL );

if( !mail_OpenSession( mail, 1, AUTH_LOGIN ) )
{
    printf( "[%s]\n", mail_GetError( mail ) );
}
else
{
    if( !mail_SendMessage( mail, msg ) )
    {
        printf( "<%s>\n", mail_GetError( mail ) );
    }
    mail_CloseSession( mail );
}
msg_Destroy( msg );
mail_Destroy( mail );
knet_down();
```

