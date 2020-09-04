/*
*   nOVL -- overlay.c
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
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <elf.h>
#include <glib.h>
#include "overlay.h"


/* ----------------------------------------------
   Macros
   ---------------------------------------------- */

#define U32(x)      \
(                   \
    (x)[0] << 24 |  \
    (x)[1] << 16 |  \
    (x)[2] << 8  |  \
    (x)[3]          \
)


/* ----------------------------------------------
   Local variables
   ---------------------------------------------- */

/* Buffer for error message */
static char errbuff[512];


/* ----------------------------------------------
   Global variables
   ---------------------------------------------- */

/* Global error message pointer in case of error */
char * ovl_errmesg;

/* Section names */
char * ovl_section_names[] = 
{
    ".text",
    ".data",
    ".rodata",
    ".bss",
    NULL
};


/* ----------------------------------------------
   Global functions
   ---------------------------------------------- */

/* Set an error */
int
ovl_set_error ( char * f, ... )
{
    int len;
	va_list ap;
    
    /* Process string */
    va_start( ap, f );
    len = vsnprintf( errbuff, sizeof(errbuff), f, ap );
    va_end( ap );
    
    /* Set the pointer */
    ovl_errmesg = errbuff;
    
    return len;
}

/* Read & swap values appropriately for ovl header */
uint8_t *
ovl_get_header ( struct ovl_t * ovl, struct ovl_header_t * dest )
{
    int offset, i;
    uint32_t * ptr;
    
    /* Get offset to header */
    offset = g_ntohl( *(uint32_t*)(ovl->data + ovl->filesize - 4) );
    
    /* Invalid offset? */
    if( offset > ovl->filesize || (offset % 4) )
    {
        /* Yes... */
        ovl_set_error( "not an overlay file" );
        return NULL;
    }
    
    /* Set the word pointer */
    ptr = (uint32_t*)(ovl->data + ovl->filesize - offset);
    
    /* Read it in... */
    for( i = 0; i < sizeof(struct ovl_header_t) / 4; i++ )
    {
        uint32_t w;
        
        /* Get next word & swap */
        w = g_ntohl( ptr[i] );
        
        /* Copy it over */
        ((uint32_t*)dest)[i] = w;
    }
    
    return (uint8_t*)ptr;
}

/* Create an OVL object from filename */
struct ovl_t *
ovl_load ( char * filename )
{
    struct ovl_t * ovl;
    struct ovl_header_t head;
    uint8_t * raw;
    FILE * h;
    int i, total;
    
    /* Open file for reading */
    if( !(h = fopen(filename, "rb")) )
    {
        ovl_set_error( "fopen(): %s", strerror(errno) );
        return NULL;
    }
    
    /* Create new ovl object */
    if( !(ovl = calloc( sizeof(struct ovl_t), 1 )) )
    {
        fclose( h );
        ovl_set_error( "calloc(): %s", strerror(errno) );
        return NULL;
    }
    
    /* Read file */
    fseek( h, 0, SEEK_END );
    if( !(ovl->data = malloc(ovl->filesize = ftell(h))) )
    {
        fclose( h );
        free( ovl );
        ovl_set_error( "malloc(): %s", strerror(errno) );
        return NULL;
    }
    fseek( h, 0, SEEK_SET );
    fread( ovl->data, 1, ovl->filesize, h );
    fclose( h );
    
    /* Read in the header */
    if( !(raw = ovl_get_header( ovl, &head )) )
    {
        free( ovl );
        free( ovl->data );
        return NULL;
    }
    raw += sizeof(head);
    
    /* Apply values */
    ovl->relocation_count = head.relocation_count;
    
    /* Allocate memory */
    ovl->sections = calloc( sizeof(struct ovl_section_t*), OVL_S_COUNT + 1 );
    ovl->relocations = calloc( sizeof(uint32_t), ovl->relocation_count );
    
    /* Copy over relocation words */
    for( i = 0; i < ovl->relocation_count; i++ )
        ovl->relocations[i] = g_ntohl( *(uint32_t*)&raw[i*4] );
    
    /* Process section information */
    for( i = 0, total = 0; i < OVL_S_COUNT; i++ )
    {
        /* Allocate memory */
        ovl->sections[i] = calloc( sizeof(struct ovl_section_t), 1 );
        
        /* Set attributes */
        ovl->sections[i]->addr = total;
        ovl->sections[i]->size = head.sizes[i];
        
        /* Not BSS? */
        if( i != OVL_S_BSS )
        {
            /* Make pointer to data */
            ovl->sections[i]->data = ovl->data + ovl->sections[i]->addr;
        }
        
        /* Update total */
        total += head.sizes[i];
    }
    
    /* Return final product */
    return ovl;
}

/* Free an overlay file */
void
ovl_free ( struct ovl_t * ovl )
{
    int i;
    
    for( i = 0; ovl->sections[i]; i++ )
    {
        free( ovl->sections[i] );
    }
    
    free( ovl->relocations );
    free( ovl->data );
    free( ovl );
}

