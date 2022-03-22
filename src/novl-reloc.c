/*
*   nOVL -- novl-reloc.c
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
#include "novl.h"
#include "overlay.h"

extern uint32_t elf_starts[OVL_S_COUNT];
extern uint32_t ovl_starts[OVL_S_COUNT];
extern uint32_t sizes[OVL_S_COUNT];
extern const char * section_names[OVL_S_COUNT];

/* ----------------------------------------------
   Macros/shorthand
   ---------------------------------------------- */

#define STRTYPE(t)      novl_str_reloc_types[t]
   

/* ----------------------------------------------
   Local variables
   ---------------------------------------------- */

/* Temporary storage for building HI16/LO16 instructions */
static uint32_t * hilopair_ptrs[32];
static uint32_t hilopair_regs[32];
static uint8_t hilopair_status[32];

#define HILO_STATUS_INVALID 0
#define HILO_STATUS_VALID 1
#define HILO_STATUS_USED 2

/* ----------------------------------------------
   Local functions
   ---------------------------------------------- */

static int
get_target_section(uint32_t address)
{
    for(int i=0; i<OVL_S_COUNT; ++i)
    {
        if(address < elf_starts[i] || address >= elf_starts[i] + sizes[i]) continue;
        return i;
    }
    return -1;
}

static int
adjust_address(uint32_t *addr, int type, int dryrun)
{
    uint32_t oldvalue = *addr;
    int s = get_target_section(oldvalue);
    if(s < 0)
    {
        DEBUG_R( "%s: skipping 0x%08X - out of bounds", STRTYPE(type), oldvalue );
        return NOVL_RELOC_FAIL;
    }
    if(type == 4 && s != 0)
    {
        ERROR("26-bit jump instruction pointing to the %s section (not .text) (target 0x%08X)!",
            section_names[s], oldvalue);
        return FALSE;
    }
    if(!dryrun)
    {
        *addr += ovl_starts[s] - elf_starts[s];
        DEBUG_R( "%s: 0x%08X -> 0x%08X", STRTYPE(type), oldvalue, *addr);
    }
    return NOVL_RELOC_SUCCESS;
}

/* Relocate a 32-bit pointer */
static int
novl_reloc_mips_32 ( uint32_t * i, int dryrun )
{
    const int type = 2;
    uint32_t w;
    int ret;
    
    /* Read word (in our endian) */
    w = g_ntohl( *i );
    
    /* Apply offset */
    if((ret = adjust_address(&w, type, dryrun)) != NOVL_RELOC_SUCCESS) return ret;
    
    if(!dryrun)
    {
        /* Write it back */
        *i = g_htonl( w );
    }
    
    return NOVL_RELOC_SUCCESS;
}

/* Relocate a 26-bit (jump target) pointer */
static int
novl_reloc_mips_26 ( uint32_t * i, int dryrun )
{
    const int type = 4;
    uint32_t w, addr;
    int ret;
    
    /* Read word (in our endian) */
    w = g_ntohl( *i );
    
    /* Convert to address */
    addr = ((w & 0x03FFFFFF) << 2) | 0x80000000;
    
    /* Apply offset */
    if((ret = adjust_address(&addr, type, dryrun)) != NOVL_RELOC_SUCCESS) return ret;
    
    if(!dryrun)
    {
        /* Convert back to jump instruction */
        w = (w & ~0x03FFFFFF) | ((addr >> 2) & 0x03FFFFFF);
        
        /* Write it back */
        *i = g_htonl( w );
    }
    
    return NOVL_RELOC_SUCCESS;
}

/* Relocate the high part of an immediate value */
static int
novl_reloc_mips_hi16 ( uint32_t * i, int dryrun )
{
    uint32_t w;
    int reg;
    
    /* Read word (in our endian) */
    w = g_ntohl( *i );
    
    /* Get affected register */
    reg = (w >> 16) & 0x1F;
    
    /* Begin building value */
    hilopair_regs[reg] = (w & 0xFFFF) << 16;
    
    /* Store pointer to this current instruction */
    hilopair_ptrs[reg] = i;
    
    /* Mark as in use */
    hilopair_status[reg] = HILO_STATUS_VALID;
    
    //DEBUG_R( "R_MIPS_HI16: reg %d %08X", reg, hilopair_regs[reg]);
    
    return NOVL_RELOC_SUCCESS;
}

/* Relocate the low part of an immediate value */
static int
novl_reloc_mips_lo16 ( uint32_t * i, int dryrun )
{
    const int type = 6;
    int16_t val;
    uint32_t w, old_w, addr, hi, lo;
    int reg;
    int ret;
    
    /* Read word (in our endian) */
    w = g_ntohl( *i );
    
    /* Get affected register */
    reg = (w >> 21) & 0x1F;
    
    /* Get the immediate value */
    val = (int16_t)(w & 0xFFFF);
    
    /* Check status */
    if(hilopair_status[reg] == HILO_STATUS_INVALID)
    {
        ERROR("Instruction %08X uses low immediate value relative to reg %d, but not previously set!",
            w, reg);
        return FALSE;
    }
    
    /* Finish building the value */
    addr = hilopair_regs[reg] + val;
    
    if(hilopair_status[reg] == HILO_STATUS_USED)
    {
        DEBUG_R("    Hi reg %d previously used", reg);
    }
    
    /* Apply offset */
    if((ret = adjust_address(&addr, type, dryrun)) != NOVL_RELOC_SUCCESS)
    {
        if(ret == NOVL_RELOC_FAIL && hilopair_status[reg] == HILO_STATUS_USED)
        {
            ret = NOVL_RELOC_FAIL_DONTDELETEHI;
        }
        
    }
    else if(!dryrun)
    {
        /* Cut it up */
        lo = addr & 0xFFFF;
        hi = (addr >> 16) + ((lo & 0x8000) ? 1 : 0);
        
        /* Get old word (part 1) */
        old_w = g_ntohl( *hilopair_ptrs[reg] );
        
        /* Update the instructions */
        *i = g_htonl( (w & 0xFFFF0000) | lo );
        *hilopair_ptrs[reg] = g_htonl( (old_w & 0xFFFF0000) | hi );
    }
    
    hilopair_status[reg] = HILO_STATUS_USED;
    return ret;
}


/* ----------------------------------------------
   Global variables 
   ---------------------------------------------- */

/* A jump table performing relocations */
novlDoReloc novl_reloc_jt[R_MIPS_NUM] = 
{
    NULL,
    NULL,
    novl_reloc_mips_32,
    NULL,
    novl_reloc_mips_26,
    novl_reloc_mips_hi16,
    novl_reloc_mips_lo16
    
    /* rest are NULL! */
};


/* ----------------------------------------------
   Global functions
   ---------------------------------------------- */
   
/* Do a relocation */
int
novl_reloc_do ( uint32_t * i, int type, int dryrun )
{
    novlDoReloc handler;
    
    /* Grab handler */
    handler = novl_reloc_jt[type & 0x7F];
    
    /* Don't have one? */
    if( !handler )
    {
        /* Yep -- unhandled */
        return FALSE;
    }
    
    /* Call it */
    return handler( i, dryrun );
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

void
novl_reloc_init ()
{
    for(int i=0; i<32; ++i){
        hilopair_ptrs[i] = NULL;
        hilopair_regs[i] = 0;
        hilopair_status[i] = HILO_STATUS_INVALID;
    }
}
