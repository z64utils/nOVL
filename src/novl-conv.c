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
#include "novl-mips.h"

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
uint32_t           ovl_starts[OVL_S_COUNT];
uint32_t           sizes[OVL_S_COUNT];
static int         elf_fd;
static Elf       * elf;
static GElf_Ehdr   elf_head;

const char * section_names[OVL_S_COUNT] = {
    ".text", ".data", ".rodata", ".bss"
};

#define NOVL_MAX_RELOCS 100000
static novl_reloc_entry relocs[NOVL_MAX_RELOCS];
static int nrelocs;

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

/* Generate a Nintendo relocation */
uint32_t
novl_reloc_mk ( int sec, int offset, int type )
{
    uint32_t w;
    
    w = sec << 30;
    w |= offset & 0x00FFFFFF;
    w |= (type & 0x3F) << 24;
    
    return w;
}

void novl_parse_hilo16 ( int startrelocnum );

void
elf_to_novl_relocs ( )
{
    /* Converts relocs from ELF format to nOVL format, and extracts the
    tgt_addr for each reloc from the instructions (or data). */
    size_t sh_strcount;
    Elf_Scn * section;
    int i;
    
    nrelocs = 0;
    
    /* Step through section list */
    elf_getshdrstrndx( elf, &sh_strcount );
    section = NULL;
    
    while( (section = elf_nextscn(elf, section)) )
    {
        int id, n;
        GElf_Shdr sh_header;
        char * name;
        Elf_Data * data;
        
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
        DEBUG( "Processing relocation entries from '%s' @%08X", name);
        
        data = NULL;
        
        /* Get section data */
        for( n = 0; n < sh_header.sh_size && (data = elf_getdata(section, data)); n += data->d_size )
        {
            GElf_Rel *rel_list;
            GElf_Rel rel;
            int nr;
            int startrelocnum = nrelocs;
            
            /* Relocs may not be in order! Need to count them, then sort them. */
            for( nr = 0; gelf_getrel(data, nr, &rel); nr++ );
            
            DEBUG( "%d relocs", nr);
            
            rel_list = malloc(nr * sizeof(GElf_Rel));
            
            for( nr = 0; gelf_getrel(data, nr, &rel); nr++ )
            {
                for( i = nr - 1; i >= 0; --i )
                {
                    if( rel.r_offset > rel_list[i].r_offset ) break;
                    
                    rel_list[i+1] = rel_list[i];
                }
                ++i;
                rel_list[i] = rel;
                
            }
            
            for( i = 0; i < nr; ++i, ++nrelocs)
            {
                uint32_t elf_addr = (uint32_t)rel_list[i].r_offset;
                uint32_t contents = g_ntohl(*(uint32_t*)&memory[elf_addr - elf_ep]);
                
                /* Check reloc type */
                uint8_t type = (int)rel_list[i].r_info;
                if( id == 0 && 
                    !(type == R_MIPS_HI16 || type == R_MIPS_LO16 || type == R_MIPS_26)
                    || id != 0 && type != R_MIPS_32 )
                {
                    ERROR( "Unsupported relocation in ELF file of type %s in section %s",
                        novl_str_reloc(type), section_names[id] );
                    exit(EXIT_FAILURE);
                }
                
                /* Write to list */
                relocs[nrelocs].elf_addr = elf_addr;
                relocs[nrelocs].type = type;
                relocs[nrelocs].elf_sec = id;
                
                /* If this is an easy type, just read the address */
                if(type == R_MIPS_26)
                {
                    relocs[nrelocs].tgt_addr = ((contents & 0x03FFFFFF) << 2) | 0x80000000;
                }
                else if(type == R_MIPS_32)
                {
                    relocs[nrelocs].tgt_addr = contents;
                }
                else
                {
                    relocs[nrelocs].tgt_addr = 0xFFFFFFFF;
                }
            }
            
            free(rel_list);
            
            if(id == 0)
            {
                novl_parse_hilo16(startrelocnum);
            }
        }
    }        
}

void
novl_parse_hilo16 ( int startrelocnum )
{
    /*
    For .text section, iterate through all instructions in order. We need to
    actually follow the code execution because branches can carry over which
    register contains what value. For example:
    lui v0, %hi(A)      ; v0 set to A
    beq t1, zero, lbl1
    nop
    lw  t2, %lo(A)(v0)  ; v0 used (A)
    sw  t3, 0x0000(t2)
    lui v0, %hi(B)      ; v0 set to B
    sw  zero, %lo(B)(v0) ; v0 used (B)
    beq zero, zero, lbl2
    nop
    lbl1:
    lw  t2, %lo(A)(v0)  ; v0 used (A), but "last" set to B!
    If A is relocatable (in the overlay) but B is not or vice versa, this will
    corrupt one of the addresses!
    */
    
    int r;
    uint32_t a;
    int branchdelay_mode = 0;
    int is_branch_likely = 0;
    uint32_t branch_offset;
    
    DEBUG("Parsing code for HI16/LO16 addresses");
    novl_mips_clearall();
    
    for( r = startrelocnum, a = elf_starts[0]; 
        a < elf_starts[0] + sizes[0] && r < nrelocs;
        a += 4)
    {
        novl_mips_checkmerge(a);
        
        uint32_t instr = g_ntohl(*(uint32_t*)&memory[a - elf_ep]);
        if(relocs[r].elf_addr == a)
        {
            if(relocs[r].type == R_MIPS_HI16)
            {
                if(is_branch_likely)
                {
                    ERROR("@%08X instr %08X has HI16 reloc in branch likely delay slot, not yet supported!",
                        a, instr);
                    exit(EXIT_FAILURE);
                }
                
                novl_mips_got_hi16(a, instr, &relocs[r]);
            }
            else if(relocs[r].type == R_MIPS_LO16)
            {
                novl_mips_got_lo16(a, instr, &relocs[r]);
            }
            ++r;
        }
        
        if(branchdelay_mode == 1)
        {
            /* Forward branch */
            novl_mips_futurestate(a + 4 + branch_offset);
        }
        else if(branchdelay_mode == 2)
        {
            /* Other jump: backward branch, j, jr $ra only, jal, jalr */
            novl_mips_clearstate();
        }
        else if(branchdelay_mode == 3)
        {
            /* jr (not jr $ra) */
            novl_mips_jr();
        }
        branchdelay_mode = 0;
        
        if( novl_mips_is_forward_branch(instr) )
        {
            branchdelay_mode = 1;
            branch_offset = (instr & 0xFFFF) << 2;
        }
        else if( novl_mips_is_jump(instr) )
        {
            branchdelay_mode = 2;
        }
        else if( novl_mips_is_jr(instr) )
        {
            branchdelay_mode = 3;
        }
        is_branch_likely = novl_mips_is_branch_likely(instr);
    }
    
    if(r < nrelocs)
    {
        ERROR("Internal error, ran out of .text before finishing all hi/lo relocs!");
        exit(EXIT_FAILURE);
    }
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
    char * name;
    Elf_Data * data;
    
    struct ovl_header_t new_head;
    int ninty_count;
    int i, n, r, v;
    uint32_t tmp;
    uint32_t greatest;
    uint32_t backwards;
    uint32_t ovl_end_addr, ovl_file_header_addr;
    
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
    
    /* Set entry point */
    elf_ep = elf_head.e_entry;
    
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
    greatest = 0;
    
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
    
    /* Load relocs into memory, and extract target addresses from code/data */
    elf_to_novl_relocs();
    
    /* Sort relocs into sections */
    DEBUG("Sorting relocs into sections");
    ninty_count = 0;
    
    for(r = 0; r < nrelocs; ++r)
    {
        novl_reloc_entry * rel = &relocs[r];
        for(i = 0; i < OVL_S_COUNT; ++i)
        {
            if(rel->tgt_addr >= elf_starts[i] && rel->tgt_addr < elf_starts[i] + sizes[i])
            {
                DEBUG("Reloc at %08X to %08X type %s is to section %s",
                    rel->elf_addr, rel->tgt_addr, novl_str_reloc(rel->type), section_names[i]);
                ++ninty_count;
                break;
            }
        }
        if(i == OVL_S_COUNT)
        {
            DEBUG("Reloc at %08X to %08X type %s is to outside the overlay, discarding",
                rel->elf_addr, rel->tgt_addr, novl_str_reloc(rel->type));
            rel->elf_sec = -1;
        }
    }
    DEBUG("ninty_count = %d", ninty_count);
    
    /* Compute overlay (output) addresses */
    
    ovl_end_addr = settings.base_addr; /* new entry point */
    ovl_starts[OVL_S_TEXT] = ovl_end_addr;
    ovl_end_addr += sizes[OVL_S_TEXT]; /* section sizes */
    ovl_starts[OVL_S_DATA] = ovl_end_addr;
    ovl_end_addr += sizes[OVL_S_DATA];
    ovl_starts[OVL_S_RODATA] = ovl_end_addr;
    ovl_end_addr += sizes[OVL_S_RODATA];
    ovl_file_header_addr = ovl_end_addr - settings.base_addr;
    ovl_end_addr += 5 * sizeof(uint32_t); /* header size */
    ovl_end_addr += ninty_count * sizeof(uint32_t); /* relocation block size */
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
    
    /* Copy sizes */
    memcpy( new_head.sizes, sizes, sizeof(new_head.sizes) );
    new_head.relocation_count = ninty_count;
    
    /* Swap... */
    for( i = 0; i < sizeof(new_head)/4; i++ )
        ((uint32_t*)&new_head)[i] = g_htonl(((uint32_t*)&new_head)[i]);
    
    /* Write it */
    DEBUG( "Header offset: 0x%X", ovl_file_header_addr );
    fseek( ovl_out, ovl_file_header_addr, SEEK_SET );
    v = fwrite( &new_head, 1, sizeof(new_head), ovl_out );
    MESG( "Wrote section descriptions (%ib).", v );
    
    /* Write the relocation entries */
    i = 0;
    for( r = 0; r < nrelocs; ++r )
    {
        int8_t id = relocs[r].elf_sec;
        if(id < 0) continue;
        
        uint32_t w = novl_reloc_mk(id + 1, relocs[r].elf_addr - elf_starts[id], relocs[r].type);
        w = g_htonl( w );
        v += fwrite( &w, 1, sizeof(w), ovl_out );
        
        ++i;
    }
    if(i != ninty_count)
    {
        ERROR("Inconsistent number of valid relocations! %d vs %d", i, ninty_count);
        exit(EXIT_FAILURE);
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
    
    /* Done with elf file */
    elf_end( elf );
    close( elf_fd );
}
