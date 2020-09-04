/*
*   nOVL -- mesg.c
*   Copyright (c) 2009  Marshall B. Rogers [mbr@64.vg]
*
*   This program is free software; you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation; either version 2 of the License, or
*   any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public
*   Licence along with this program; if not, write to the Free
*   Software Foundation, Inc., 51 Franklin Street, Fifth Floor, 
*   Boston, MA  02110-1301, USA
*/
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <glib.h>
#include "novl.h"
#include "mesg.h"


/*
    MESSAGE VERBOSITY LEVELS:
     - notice: always shown
     - message: shown with -v
     - debug: shown with -vv
     - debug2: shown with -vvv
     
    Verbosity of -1 indicates silent mode.
*/



char *
mesg_strip_control_characters ( char * str )
{
    char * src, * tgt, * new;
    int c;
    
    src = str;
    tgt = (new = malloc( strlen(str) ));
    
    while( (c = *src) )
    {
        /* ANSI escape sequence? */
        if( c == ANSI_START[0] )
        {
            /* Yes, skip all characters in sequence */
            while( (*(src++) != 'm') );
            continue;
        }
        
        /* Regular characters */
        *(tgt++) = c;
        
        src++;
    }
    
    *tgt = '\0';
    
    return new;
}


int
mk_mesg_f ( int v, char * symbol, char * fmt, va_list ap )
{
    char * final, * new_format;
    int length;
    char * stripped;
    
    /* Verbosity within range? */
    if( v > settings.verbosity )
    {
        /* Nope */
        return 0;
    }
    
    new_format = g_strdup_printf( "%s %s\n", symbol, fmt );
    final = g_strdup_vprintf( new_format, ap );
    length = strlen( final );
    
    if( settings.no_colors )
    {
        stripped = mesg_strip_control_characters( final );
        fprintf( stderr, "%s", stripped );
        free( stripped );
    }
    else
    {
        fprintf( stderr, "%s", final );
    }
    
    g_free( new_format );
    g_free( final );
    
    return length;
}


int
mk_mesg ( int v, char * symbol, char * fmt, ... )
{
    va_list ap;
    int len;
    
    /* Verbosity within range? */
    if( v > settings.verbosity )
    {
        /* Nope */
        return 0;
    }
    
    va_start( ap, fmt );
    len = mk_mesg_f( v, symbol, fmt, ap );
    va_end( ap );
    
    return len;
}


int
NOTICE ( char * fmt, ... )
{
    va_list ap;
    int len;
    
    va_start( ap, fmt );
    len = mk_mesg_f( NOVL_VERBOSE_NOTICE, "[" ANSI_SET_FG_BLUE "-" ANSI_RESET_DEFAULT "]", fmt, ap );
    va_end( ap );
    
    return len;
}


int
MESG ( char * fmt, ... )
{
    va_list ap;
    int len;
    
    va_start( ap, fmt );
    len = mk_mesg_f( NOVL_VERBOSE_MESSAGE, "[" ANSI_SET_FG_BLUE "-" ANSI_RESET_DEFAULT "]", fmt, ap );
    va_end( ap );
    
    return len;
}


int
ERROR ( char * fmt, ... )
{
    va_list ap;
    int len;
    
    va_start( ap, fmt );
    len = mk_mesg_f( NOVL_VERBOSE_NOTICE, "[" ANSI_SET_FG_RED "!" ANSI_RESET_DEFAULT "]", fmt, ap );
    va_end( ap );
    
    return len;
}


int
DEBUG ( char * fmt, ... )
{
    va_list ap;
    int len;
    
    va_start( ap, fmt );
    len = mk_mesg_f( NOVL_VERBOSE_DEBUG, "[" ANSI_SET_FG_GREEN "@" ANSI_RESET_DEFAULT "]", fmt, ap );
    va_end( ap );
    
    return len;
}


int
DEBUG_R ( char * fmt, ... )
{
    va_list ap;
    int len;
    
    va_start( ap, fmt );
    len = mk_mesg_f( NOVL_VERBOSE_DEBUG2, "[" ANSI_SET_FG_CYAN "*" ANSI_RESET_DEFAULT "]", fmt, ap );
    va_end( ap );
    
    return len;
}

