/*
*   nOVL -- func.c
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
#include <stdlib.h>
#include <glib.h>


/* Specification for formatting... */
static struct FSpec
{
    int maxval;
    int divby;
    int decplaces;
    char * suffix;
}
spec[] = 
{
    { 1024,         1.0,           0, "b" },
    { 1024 * 1024,  1024.0,        2, "k" },
    { -1,           1024 * 1024.0, 2, "m" },
    { 0,                                  }
};
    

/* Format an integer value into something human-readable */
int
format_value ( char * tgt, unsigned val )
{
    double nval;
	struct FSpec * f;
    int v;
    
    f = &spec[0];
    
    while( f->suffix )
    {
        /* Is this the last one? */
        if( !(f+1)->suffix )
        {
            /* yeah, we have to use it */
            break;
        }
        
        /* Do we fit into this category? */
        if( f->maxval > val )
        {
            /* Yep, use it */
            break;
        }
        
        /* Go to next */
        f++;
    }
    
    /* Calculate the value to print */
    nval = (double)val / f->divby;
    
    /* Format */
    v = sprintf( tgt, "%.*f%s", f->decplaces, nval, f->suffix );
    
    return v;
}

