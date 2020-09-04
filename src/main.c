/*
*   nOVL -- main.c
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
#include <getopt.h>
#include "novl.h"
#include "overlay.h"
#include "decoder.h"
#include "mesg.h"
#include "ansi.h"


/* ----------------------------------------------
   Constants
   ---------------------------------------------- */

enum nOVLopmode
{
    OP_NONE,
    OP_LIST_SECTIONS,
    OP_LIST_RELOCATIONS,
    OP_CONVERT,
    OP_DISASSEMBLE,
    OP_DISASSEMBLE_ALL
};


/* ----------------------------------------------
   Local variables
   ---------------------------------------------- */

/* Getopt format string */
static char * flags_fmt = "clrDdVgvsho:A:?";

/* Command line arguments - values */
static uint32_t Aval;
static char * oval;

/* Filenames */
static GList * filenames;


/* ----------------------------------------------
   Local functions
   ---------------------------------------------- */

/* Show usage */
static void
show_usage ( int argc, char ** argv )
{
    char * s = "[" ANSI_SET_FG_YELLOW "?" ANSI_RESET_DEFAULT "]";
    
    mk_mesg( NOVL_VERBOSE_NOTICE, s, "Application prototype:" );
    mk_mesg( NOVL_VERBOSE_NOTICE, s, "%s [-lrcDd] [-ghVvs] [-A address] [-o filename]", argv[0] );
    
    exit( EXIT_FAILURE );
}

/* Disassemble an actor file */
static void
disassemble ( char * filename, int all )
{
    int n, i, cnt;
    uint32_t w, addr;
    char buff[512];
    OVL * ovl;
    
    /* Load it */
    if( !(ovl = ovl_load(filename)) )
    {
        ERROR( "Couldn't load overlay file \"%s\": %s", filename, ovl_errmesg );
        exit( EXIT_FAILURE );
    }
    
    /* Disassemble everything? */
    if( all == 1 )
        cnt = 3;
    else
        cnt = 1;
    
    /* Set base address */
    addr = Aval;
    
    /* Start disassembly on a section */
    for( i = 0; i < cnt; i++ )
    {
        /* Header */
        printf( 
            "Disassembly of \"%s\": section %s\n",
            filename,
            ovl_section_names[i]
        );
        
        /* Begin reading & printing */
        for( n = 0; n < ovl->sections[i]->size; n += 4 )
        {
            /* Read word */
            w = *(uint32_t*)(ovl->sections[i]->data + n);
            w = g_ntohl( w ); /* swap */
            
            /* Disassemble it */
            mr4kd_disassemble( w, addr, buff );
            
            /* Print it */
            if( GET_FLAG('V') )
                printf( "%08X: (%08X) %s\n", addr, w, buff );
            else
                printf( "%08X: %s\n", addr, buff );
            
            /* Update address */
            addr += 4;
        }
    }
    
    /* Finished */
    ovl_free( ovl );
}


/* List the relocations inside an overlay */
static void
list_relocations ( char * filename )
{
    int i;
    OVL * ovl;
    
    /* Load it */
    if( !(ovl = ovl_load(filename)) )
    {
        ERROR( "Couldn't load overlay file \"%s\": %s", filename, ovl_errmesg );
        exit( EXIT_FAILURE );
    }
    
    /* Header */
    printf( "Dump of relocations from \"%s\":\n", filename );
    printf( "ADDRESS     SECTION   REL TYPE        VALUE\n" );
    
    /* Display */
    for( i = 0; i < ovl->relocation_count; i++ )
    {
        int a;
        
        a = OVL_RELOC_GET_ABS_ADDR(ovl, ovl->relocations[i]);
        
        printf( 
            "0x%08X  %-8s  %-14s  0x%08X\n", 
            a + Aval,
            ovl_section_names[OVL_RELOC_GET_SEC(ovl->relocations[i])],
            novl_str_reloc_types[OVL_RELOC_GET_TYPE(ovl->relocations[i])],
            g_ntohl( *(uint32_t*)(ovl->data + a) )
        );
    }
    
    /* Done with overlay file */
    ovl_free( ovl );
    
    /* Spacing */
    printf( "\n\n" );
}

/* List the sections of an overlay */
static void
list_sections ( char * filename )
{
    int i;
    OVL * ovl;
    char size[64];
    
    /* Load it */
    if( !(ovl = ovl_load(filename)) )
    {
        ERROR( "Couldn't load overlay file \"%s\": %s", filename, ovl_errmesg );
        exit( EXIT_FAILURE );
    }
    
    /* Header */
    printf( "Dump of sections from \"%s\":\n", filename );
    printf( "NAME       ADDRESS        SIZE\n" );
    
    /* Print them */
    for( i = 0; ovl->sections[i]; i++ )
    {
        if( GET_FLAG('h') )
            format_value( size, ovl->sections[i]->size );
        else
            snprintf( size, sizeof(size), "%i", ovl->sections[i]->size );
        
        printf( 
            "%-10s 0x%08X     %s\n", 
            ovl_section_names[i], 
            Aval + ovl->sections[i]->addr,
            size
        );
    }
    
    /* Done with overlay file */
    ovl_free( ovl );
    
    /* Spacing */
    printf( "\n\n" );
}

/* Convert an elf to an overlay */
static void
convert_elf ( char * filename )
{
    /* We need the -o flag */
    if( !GET_FLAG('o') )
    {
        ERROR( "Please specify an output file with -o." );
        exit( EXIT_FAILURE );
    }
    
    /* We need the -A flag */
    if( !GET_FLAG('A') )
    {
        ERROR( "Please specify a target address with -A." );
        exit( EXIT_FAILURE );
    }
    
    novl_conv( Aval, filename, oval );
    
    NOTICE( "Finished conversion." );
}


/* Check if an opmode has already been set... */
static void
check_opmode ( void )
{
    if( settings.opmode )
    {
        ERROR( "You can only specify one of the -clr options!" );
        exit( EXIT_FAILURE );
    }
}

/* Process command line arguments */
static void
args_process ( int argc, char ** argv )
{
    int c, i;
    
    /* Loop through arguments */
    while( (c = getopt(argc, argv, flags_fmt)) != -1 )
    {
        /* Note flag */
        SET_FLAG(c);
        
        /* Handle... */
        switch( c )
        {
            /* List sections */
            case 'l':
            {
              check_opmode();
              settings.opmode = OP_LIST_SECTIONS;
            }
            break;
            
            /* List relocations */
            case 'r':
            {
              check_opmode();
              settings.opmode = OP_LIST_RELOCATIONS;
            }
            break;
            
            /* Convert mode */
            case 'c':
            {
              check_opmode();
              settings.opmode = OP_CONVERT;
            }
            break;
            
            /* Disassemble .text */
            case 'd':
            {
              settings.opmode = OP_DISASSEMBLE;
            }
            break;
            
            /* Disassemble all */
            case 'D':
            {
              settings.opmode = OP_DISASSEMBLE_ALL;
            }
            break;
            
            
            /* Actor base address */
            case 'A':
            {
              sscanf( optarg, "%X", &Aval );
              settings.ovl_base = Aval;
            }
            break;
            
            /* Output file (for converter) */
            case 'o':
            {
              oval = optarg;
            }
            break;
            
            /* Make int values human readable */
            case 'h':
            {
              settings.human_readable = 1;
            }
            break;
            
            /* Disable colors in output */
            case 'g':
            {
              settings.no_colors = 1;
            }
            break;
            
            
            /* Alter verbosity */
            case 'v':
            {
              settings.verbosity++;
            }
            break;
            
            /* Silent mode -- no output */
            case 's':
            {
              settings.verbosity = -1;
            }
            break;
            
            
            case '?': default:
             show_usage( argc, argv );
             exit( EXIT_FAILURE );
        }
    }
    
    /* The rest of the arguments are filenames */
    for( i = optind; i < argc; i++ )
    {
        /* Append it to our list */
        filenames = g_list_append( filenames, argv[i] );
    }
}




int
main ( int argc, char ** argv )
{
    GList * s;
    
    /* Process the arguments */
    args_process( argc, argv );
    
    /* Header */
    NOTICE( 
        ANSI_SET_FG_WHITE_BOLD 
        NOVL_NAME 
        ANSI_RESET_DEFAULT
        " "
        NOVL_VERSION_STR
        " by "
        NOVL_AUTHOR
    );
    NOTICE(
        "Built: "
        __DATE__ " " __TIME__
    );
    
    /* Doop doop. Analyze flags */
    if( GET_FLAG('A') )
        DEBUG( "Base address set to 0x%08X.", settings.ovl_base );
    if( GET_FLAG('o') )
        DEBUG( "Output file set to \"%s\".", oval );
    if( GET_FLAG('h') )
        DEBUG( "Printing human-readable addresses." );
    if( GET_FLAG('V') )
        DEBUG( "Printing raw instructions next to disassembly." );
    if( GET_FLAG('g') )
        DEBUG( "Colors disabled." );
    
    /* Filename count... */
    if( g_list_length(filenames) < 1 )
    {
        ERROR( "Please provide at least one filename as an argument." );
        show_usage( argc, argv );
        return EXIT_FAILURE;
    }
    
    /* Take action! */
    switch( settings.opmode )
    {
        case OP_LIST_RELOCATIONS:
        {
          MESG( "Listing overlay relocations." );
          
          for( s = filenames; s; s = s->next )
              list_relocations( s->data );
        }
        break;
        
        case OP_LIST_SECTIONS:
        {
          MESG( "Listing overlay section details." );
          
          for( s = filenames; s; s = s->next )
              list_sections( s->data );
        }
        break;
        
        case OP_CONVERT:
        {
          MESG( "Converting ELF file to overlay." );
           
          /* Too many filenames? */
          if( g_list_length(filenames) > 1 )
          {
              ERROR( "ELF -> OVL conversion only takes one file argument." );
              return EXIT_FAILURE;
          }
           
          /* First filename only! */
          convert_elf( filenames->data );
        }
        break;
        
        case OP_DISASSEMBLE:
        {
          MESG( "Disassembling the .text section in overlay file(s)." );
          
          for( s = filenames; s; s = s->next )
              disassemble( s->data, 0 );
        }
        break;
        
        case OP_DISASSEMBLE_ALL:
        {
          MESG( "Disassembling every section in overlay file(s)." );
          
          for( s = filenames; s; s = s->next )
              disassemble( s->data, 1 );
        }
        break;
        
        
        default:
         show_usage( argc, argv );
         return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}

