/*
*   nOVL -- novl-conv.c
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
#include <errno.h>
#include <stdlib.h>
#ifdef _WIN32 /* windows */
#include <libelf.h>
#else /* linux */
#include <elf.h>
#endif
#include <libelf.h>
#include <gelf.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "novl.h"
#include "overlay.h"
#include "mesg.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

/* ----------------------------------------------
   Local variables 
   ---------------------------------------------- */

static FILE      * ovl_out;
static uint32_t    elf_ep;
static uint8_t     memory[4 * 1024 * 1024];
uint32_t           elf_starts[OVL_S_COUNT];
uint32_t           sizes[OVL_S_COUNT];
static uint32_t    ovl_starts[OVL_S_COUNT];
static int         elf_fd;
static Elf       * elf;
static GElf_Ehdr   elf_head;
static int         change_entry_point_offset;

static const char * section_names[OVL_S_COUNT] = {
    ".text", ".data", ".rodata", ".bss"
};


/* ----------------------------------------------
   Local functions 
   ---------------------------------------------- */

static int
valid_sec ( char * s )
{
    int i;
    
    for( i = 0; i < OVL_S_COUNT; i++ )
    {
        if( !strncmp(s, section_names[i], strlen(section_names[i])) )
            return i;
    }
    
    return -1;
}


/* ----------------------------------------------
   Global functions 
   ---------------------------------------------- */

/* Convert an ELF file to a Nintendo overlay */
void
novl_conv ( char * in, char * out )
{
    Elf_Kind kind;
    Elf_Scn * section;
    GElf_Shdr sh_header;
    size_t sh_strcount;
    GElf_Rel rel;
    GList * seek;
    uint32_t header_offset;
    struct ovl_header_t new_head;
    GList * ninty_relocs, * last;
    int ninty_count_pred, ninty_count_real;
    char * name;
    Elf_Data * data;
    int i, n, v;
    uint32_t tmp;
    uint32_t greatest;
    uint32_t backwards;
    uint32_t ovl_end_addr, ovl_file_header_addr;
    
    ninty_relocs = NULL;
    ninty_count_pred = 0;
    v = 0;
    greatest = 0;
    
    /* Open the ELF file for reading */
    if( (elf_fd = open(in, O_RDONLY | O_BINARY, 0)) < 0 )
    {
        ERROR( "open() failed: %s", strerror(errno) );
        exit( EXIT_FAILURE );
    }
    
    /* Set ELF version */
    elf_version( EV_CURRENT );
    
    /* Create libelf context */
    if( !(elf = elf_begin(elf_fd, ELF_C_READ, NULL)) )
    {
        ERROR( "elf_begin() failed: %s", elf_errmsg(-1) );
        exit( EXIT_FAILURE );
    }
    
    /* Get kind of file */
    switch( (kind = elf_kind(elf)) )
    {
        case ELF_K_AR:
         ERROR( "ar(1) archive provided -- elf files only, please!" );
         exit( EXIT_FAILURE );
        break;
        
        case ELF_K_ELF: break;
        
        default:
         ERROR( "Unsupported input format." );
         exit( EXIT_FAILURE );
    }
    
    /* Read header */
    if( !gelf_getehdr(elf, &elf_head) )
    {
        ERROR( "gelf_getehdr() failed: %s", elf_errmsg(-1) );
        exit( EXIT_FAILURE );
    }
    
    /* Check machine arch */
    if( elf_head.e_machine != EM_MIPS )
    {
        ERROR(
            "This program is for MIPS binaries only, silly!"
        );
        exit( EXIT_FAILURE );
    }
    
    /* Set entry point and how much we're moving the entry point (address of start of code) */
    elf_ep = elf_head.e_entry;
    change_entry_point_offset = settings.base_addr - elf_ep;
    
    /* Notice */
    MESG( "\"%s\": valid MIPS ELF file.", in );
    MESG( "Entry point: 0x%08X. Translating to 0x%08X.", elf_ep, settings.base_addr );
    
    /* Output file */
    if( !(ovl_out = fopen(out, "wb")) )
    {
        ERROR( 
            "Can't open \"%s\" for writing: %s",
            out, strerror(errno)
        );
        exit( EXIT_FAILURE );
    }
  
    /*
    **
    ** Start loading the file into memory
    **
    */
    
    for(i=0; i<OVL_S_COUNT; ++i){
        elf_starts[i] = 0;
        sizes[i] = 0;
    }
    
    /* Set up */
    elf_getshdrstrndx( elf, &sh_strcount );
    section = NULL;
    
    /* Loop through sections; find loadable ones */
    while( (section = elf_nextscn(elf, section)) )
    {
        int id;
        
        /* Get the header */
        gelf_getshdr( section, &sh_header );
        
        /* Get name */
        name = elf_strptr( elf, sh_strcount, sh_header.sh_name );
        
        /* Is this one we want? */
        if( (id = valid_sec(name)) == -1 )
        {
            /* No */
            continue;
        }
        
        /* Set the start addr & size */
        if(elf_starts[id] != 0){
            ERROR("Multiple %s sections defined", name);
            exit( EXIT_FAILURE );
        }
        elf_starts[id] = sh_header.sh_addr;
        sizes[id] = (sh_header.sh_size + 7) & ~7;
        
        /* Can we load this into memory? */
        if( sh_header.sh_type == SHT_PROGBITS )
        {
            
            /* Keep tabs on last boundry */
            if( sh_header.sh_size + sh_header.sh_addr > greatest )
                greatest = sh_header.sh_size + sh_header.sh_addr;
            
            data = NULL;
            
            /* Sanity check... */
            if( elf_ep > sh_header.sh_addr || (sh_header.sh_addr + sh_header.sh_size) - elf_ep > sizeof(memory) )
            {
                ERROR( 
                  "Error - memory bounds reached. Entry point must be before, but not too far before, "
                  "all valid sections (.text, .data, .rodata, .bss), usually at the start of .text.\n"
                  "EP: 0x%08X  %s: 0x%08X size 0x%08X", elf_ep, name, sh_header.sh_addr, sh_header.sh_size
                );
                exit( EXIT_FAILURE );
            }
            
            /* Load it */
            for( n = 0; n < sh_header.sh_size && (data = elf_getdata(section, data)); n += data->d_size )
            {
                memcpy( memory + (sh_header.sh_addr - elf_ep), data->d_buf, data->d_size );
            }
            
            DEBUG( "Copied '%s' (0x%X bytes) to 0x%08X.", name, (int)sh_header.sh_size, (int)sh_header.sh_addr );
        }
    }
    
    #ifdef NOVL_DEBUG
    DEBUG("ELF input addresses:");
    for(i=0; i<OVL_S_COUNT; ++i)
    {
        DEBUG("%s: 0x%08X size 0x%X", section_names[i], elf_starts[i], sizes[i]);
    }
    #endif
    
    /* predict size of resulting overlay (.bss starts after overlay in vram) */
    elf_getshdrstrndx( elf, &sh_strcount );
    section = NULL;
    while( (section = elf_nextscn(elf, section)) )
    {
        int id;
        
        /* Get the header */
        gelf_getshdr( section, &sh_header );
        
        /* Get name */
        name = elf_strptr( elf, sh_strcount, sh_header.sh_name );
        
        /* Relocation header? */
        if( strncmp(".rel.", name, 4) )
            continue;
        
        /* Is this one we want? */
        if( (id = valid_sec(name+4)) == -1 )
        {
            /* No */
            continue;
        }
        
        /* We want this */
        DEBUG( "Counting relocation entries from '%s'.", name ) ;
        
        data = NULL;
        
        /* Get section data */
        for( n = 0; n < sh_header.sh_size && (data = elf_getdata(section, data)); n += data->d_size )
        {
            /* Get relocations */
            for( i = 0; gelf_getrel(data, i, &rel); i++ )
            {
                uint32_t off;
                int v;
                
                /* Calculate offset */
                off = (uint32_t)rel.r_offset - elf_ep;
                
                /* Dry run of relocating */
                v = novl_reloc_do( (uint32_t*)(&memory[off]), (int)rel.r_info, 0, 1 );
                
                /* Check result */
                if( !v )
                {
                    /* Whaat? */
                    ERROR( "Relocation type %s has no registered handler. Abort.", novl_str_reloc((int)rel.r_info) );
                    DEBUG( "%i,%i", (int)rel.r_offset, (int)rel.r_info );
                    exit( EXIT_FAILURE );
                }
                
                /* Skip relocation generation? */
                if( v == NOVL_RELOC_FAIL )
                {
                    /* We may also have to remove the first half of a HI16/LO16
                       reloc */
                    if( (int)rel.r_info == R_MIPS_LO16 )
                    {
                        ninty_count_pred--;
                    }
                    continue;
                }
                
                ninty_count_pred++;
            }
        }
    }
    
    /* Compute overlay (output) addresses */
    
    ovl_end_addr = settings.base_addr; /* elf_ep + change_entry_point_offset */
    ovl_starts[OVL_S_TEXT] = ovl_end_addr;
    ovl_end_addr += sizes[OVL_S_TEXT]; /* section sizes */
    ovl_starts[OVL_S_DATA] = ovl_end_addr;
    ovl_end_addr += sizes[OVL_S_DATA];
    ovl_starts[OVL_S_RODATA] = ovl_end_addr;
    ovl_end_addr += sizes[OVL_S_RODATA];
    ovl_file_header_addr = ovl_end_addr - settings.base_addr;
    ovl_end_addr += 5 * sizeof(uint32_t); /* header size */
    ovl_end_addr += ninty_count_pred * sizeof(uint32_t); /* relocation block size */
    ovl_end_addr += 4; /* space for footer */
    ovl_end_addr = (ovl_end_addr + 15) & ~15; /* 16 byte alignment */
    ovl_starts[OVL_S_BSS] = ovl_end_addr;
    ovl_end_addr += sizes[OVL_S_BSS]; /* ovl_end_addr = end of overlay in vram */
    
    #ifdef NOVL_DEBUG
    DEBUG("Section reloc info:");
    DEBUG(" section   ELF addr   OVL addr   size");
    for( i = 0; i < OVL_S_COUNT; i++ )
    {
        DEBUG(" %-8s 0x%08X 0x%08X 0x%4X", section_names[i], elf_starts[i], ovl_starts[i], sizes[i]);
    }
    DEBUG("Overlay end addr: %08X", ovl_end_addr);
    #endif
    
    /*
    **
    ** Relocate the file
    **
    */
    
    /* Step through section list once more */
    elf_getshdrstrndx( elf, &sh_strcount );
    section = NULL;
    ninty_count_real = 0;
    while( (section = elf_nextscn(elf, section)) )
    {
        int id;
        
        /* Get the header */
        gelf_getshdr( section, &sh_header );
        
        /* Get name */
        name = elf_strptr( elf, sh_strcount, sh_header.sh_name );
        
        /* Relocation header? */
        if( strncmp(".rel.", name, 4) )
            continue;
        
        /* Is this one we want? */
        if( (id = valid_sec(name+4)) == -1 )
        {
            /* No */
            continue;
        }
        
        /* We want this */
        DEBUG( "Processing relocation entries from '%s'.", name ) ;
        
        data = NULL;
        last = NULL;
        
        /* Get section data */
        for( n = 0; n < sh_header.sh_size && (data = elf_getdata(section, data)); n += data->d_size )
        {
            /* Get relocations */
            for( i = 0; gelf_getrel(data, i, &rel); i++ )
            {
                uint32_t off, nr;
                int v;
                
                /* Calculate offset */
                off = (uint32_t)rel.r_offset - elf_ep;
                
                /* Relocate! */
                DEBUG("OVL %08X ELF %08X", ovl_starts[id], elf_starts[id]);
                v = novl_reloc_do( (uint32_t*)(&memory[off]), (int)rel.r_info, 
                    (int)ovl_starts[id] - (int)elf_starts[id], 0 );
                
                /* Check result */
                if( !v )
                {
                    /* Whaat? */
                    ERROR( "Relocation type %s has no registered handler. Abort.", novl_str_reloc((int)rel.r_info) );
                    DEBUG( "%i,%i", (int)rel.r_offset, (int)rel.r_info );
                    exit( EXIT_FAILURE );
                }
                
                /* Skip relocation generation? */
                if( v == NOVL_RELOC_FAIL )
                {
                    /* We may also have to remove the first half of a HI16/LO16
                       reloc */
                    if( (int)rel.r_info == R_MIPS_LO16 )
                    {
                        GList * n;
                        
                        /* Yep */
                        n = last;
                        
                        /* Seek last back one */
                        last = last->prev;
                        
                        /* Remove node */
                        ninty_relocs = g_list_delete_link( ninty_relocs, n );
                        ninty_count_real--;
                    }
                    continue;
                }
                
                /* Generate a nintendo relocation */
                nr = novl_reloc_mk(id + 1, (int)rel.r_offset - elf_starts[id], (int)rel.r_info);
                ninty_relocs = g_list_append( ninty_relocs, GUINT_TO_POINTER(nr) );
                ninty_count_real++;
                
                /* Set last link node */
                if( !last )
                {
                    last = g_list_last( ninty_relocs );
                }
                else
                {
                    last = last->next;
                }
            }
        }
    }
    
    if(ninty_count_pred != ninty_count_real)
    {
        ERROR("Relocation prediction failed! ninty_count_pred = %d, ninty_count_real = %d"
            , ninty_count_pred, ninty_count_real);
        fclose(ovl_out);
        remove(out);
        exit( EXIT_FAILURE );
    }
    
    /*
     Okay! So at this point, the binary has been relocated, and the Nintendo-
     friendly relocation entries generated.
     Now all we have to do is generate the header and write the data to disk
    */
    
    /* Copy sizes */
    memcpy( new_head.sizes, sizes, sizeof(new_head.sizes) );
    new_head.relocation_count = ninty_count_real;
    
    /* Swap... */
    for( i = 0; i < sizeof(new_head)/4; i++ )
        ((uint32_t*)&new_head)[i] = g_htonl(((uint32_t*)&new_head)[i]);
    
    /* Write it */
    DEBUG( "Header offset: 0x%X", ovl_file_header_addr );
    fseek( ovl_out, ovl_file_header_addr, SEEK_SET );
    v = fwrite( &new_head, 1, sizeof(new_head), ovl_out );
    MESG( "Wrote section descriptions (%ib).", v );
    
    /* Write the relocation entries */
    for( seek = ninty_relocs, v = 0; seek && seek->data; seek = seek->next )
    {
        uint32_t w;
            
        w = GPOINTER_TO_UINT(seek->data);
        w = g_htonl( w );
        
        v += fwrite( &w, 1, sizeof(w), ovl_out );
    }
    MESG( "Wrote relocation entries (%ib).", v );
    
    /* Write offset from here to header */
    backwards = ftell(ovl_out) - ovl_file_header_addr + 4;
    
    /* Check alignment */
    if( ((i = ftell(ovl_out)) + 4) % 16 )
    {
        int new;
        
        /* Where do we seek to now? */
        new = i-((i+4)%16)+16;
        
        /* Update */
        backwards += new - i;
        
        fseek( ovl_out, new, SEEK_SET );
    }
        
    tmp = g_htonl( backwards );
    v = fwrite( &tmp, 1, sizeof(tmp), ovl_out );
    MESG( "Finalized footer (%ib).", v );
    
    /* the size of the resulting overlay should be what
     * we predicted to be the start of the .bss section
     */
    if (ovl_starts[OVL_S_BSS] != ftell(ovl_out) + settings.base_addr)
    {
        ERROR(
            "Relocation prediction failed! .bss address predicted as %08X, is actually %08X"
            , ovl_starts[OVL_S_BSS]
            , ftell(ovl_out) + settings.base_addr
        );
        fclose(ovl_out);
        remove(out);
        exit( EXIT_FAILURE );
    }
    
    /* Write the data */
    fseek( ovl_out, 0, SEEK_SET );
    for( i = 0; i < OVL_S_BSS; i++ )
    {
        if(ovl_starts[i] != ftell(ovl_out) + settings.base_addr)
        {
            ERROR("Internal error with arranging sections! %s should be at 0x%08X but is actually 0x%08X"
                , section_names[i], ovl_starts[i], ftell(ovl_out) + settings.base_addr);
            fclose(ovl_out);
            remove(out);
            exit( EXIT_FAILURE );
        }
        v = fwrite( memory + (elf_starts[i] - elf_ep), 1, sizes[i], ovl_out );
        MESG( "Wrote section %s (%ib).", section_names[i], v );
    }
    if(ovl_file_header_addr != ftell(ovl_out))
    {
        ERROR("Internal error with arranging sections! header should be at 0x%08X but is actually 0x%08X"
            , section_names[i], ovl_file_header_addr, ftell(ovl_out));
        fclose(ovl_out);
        remove(out);
        exit( EXIT_FAILURE );
    }
    fclose( ovl_out );
    
    /* Free up some memory */
    g_list_free( ninty_relocs );
    
    /* Done with elf file */
    elf_end( elf );
    close( elf_fd );
}
