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

Pair createAddr( const char * src )
{
    char * scopy;
    Pair addr = pair_Create( NULL, NULL);
    if( !addr ) return NULL;
    scopy = Strdup( src );
    if( !scopy )
    {
        Free( addr );
        return NULL;
    }

    if( strchr( scopy, '<' ) && *scopy != '<' )
    {
        char *tok = strtok( scopy, "<" );
        A_NAME(addr) = Strdup( tok );
        if( !A_NAME(addr) )
        {
            Free( addr );
            Free( scopy );
            return NULL;
        }
        tok = strtok( NULL, "<" );
        tok = strtok( tok, ">" );
        if( tok == NULL )
        {
            pair_Delete( addr );
            Free( scopy );
            return NULL;
        }
        else
        {
            A_EMAIL(addr) = Strdup( tok );
            if( !A_EMAIL(addr) )
            {
                pair_Delete( addr );
                Free( scopy );
                return NULL;
            }
        }
    }
    else
    {
        A_EMAIL(addr) = Strdup( scopy );
        if( !A_EMAIL(addr) )
        {
            pair_Delete( addr );
            Free( scopy );
            return NULL;
        }
    }

    if( A_NAME(addr) ) _stripName( A_NAME(addr) );
    if( A_EMAIL(addr) ) _stripEmail( A_EMAIL(addr) );
    Free( scopy );
    return addr;
}
