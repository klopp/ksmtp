/*
 * addr.c, part of "ksmtp" project.
 *
 *  Created on: 20.05.2015, 01:14
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "addr.h"

/*
 * TODO best filtration?
 */
static char * _stripName( char *str )
{
    char *ptr;
    size_t size = chomp( str );

    if( (ptr = strrchr( str, '"' )) )
    {
        *ptr = '\0';
    }
    ptr = str;
    if( *ptr == '"' )
    {
        ptr++; size--;
    }
    if( ptr != str ) memmove( str, ptr, size );
    return str;
}

static char * _stripEmail( char *str )
{
    char *ptr;
    size_t size = chomp( str );

    if( (ptr = strrchr( str, '"' )) )
    {
        *ptr = '\0';
    }
    if( (ptr = strrchr( str, '>' )) )
    {
        *ptr = '\0';
    }

    ptr = str;
    if( (*ptr == '"') || (*ptr == '<') )
    {
        ptr++;
        size--;
    }
    if( (*ptr == '<') || (*ptr == '"') )
    {
        ptr++;
        size--;
    }

    if( ptr != str ) memmove( str, ptr, size );
    return str;
}

Addr createAddr( const char * src )
{
    char * scopy;
    Addr addr = calloc( sizeof(struct _Addr), 1 );
    if( !addr ) return NULL;
    scopy = strdup( src );
    if( !scopy )
    {
        free( addr );
        return NULL;
    }

    if( strchr( scopy, '<' ) && *scopy != '<' )
    {
        char *tok = strtok( scopy, "<" );
        addr->name = strdup( tok );
        if( !addr->name )
        {
            free( addr );
            free( scopy );
            return NULL;
        }
        tok = strtok( NULL, "<" );
        tok = strtok( tok, ">" );
        if( tok == NULL )
        {
            free( addr->name );
            free( addr );
            free( scopy );
            return NULL;
        }
        else
        {
            addr->email = strdup( tok );
            if( !addr->email )
            {
                free( addr->name );
                free( addr );
                free( scopy );
                return NULL;
            }
        }
    }
    else
    {
        addr->email = strdup( scopy );
        if( !addr->email )
        {
            free( addr );
            free( scopy );
            return NULL;
        }
    }

    if( addr->name ) _stripName( addr->name );
    if( addr->email ) _stripEmail( addr->email );
    free( scopy );
    return addr;
}
