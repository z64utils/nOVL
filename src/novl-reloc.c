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


/* This is an ugly hack! */
extern uint32_t bin_min;
extern uint32_t bin_max;


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


/* ----------------------------------------------
   Local functions
   ---------------------------------------------- */

/* Relocate a 32-bit pointer */
static int
novl_reloc_mips_32 ( uint32_t * i, uint32_t address, int type, int offset )
{
    uint32_t w;
    
    /* Read word (in our endian) */
    w = g_ntohl( *i );
    
    /* Apply offset */
    w += offset;
    
    /* Write it back */
    *i = g_htonl( w );
    
    /**/ DEBUG_R( "%s: 0x%08X -> 0x%08X", STRTYPE(type), w - offset, w );
    
    return NOVL_RELOC_SUCCESS;
}

/* Relocate a 26-bit (jump target) pointer */
static int
novl_reloc_mips_26 ( uint32_t * i, uint32_t address, int type, int offset )
{
    #define MKR(x)  (((x)<<2)|0x80000000)
    uint32_t w, old_tgt, new_tgt;
    
    /* Read word (in our endian) */
    w = g_ntohl( *i );
    
    /* Get target */
    old_tgt = w & 0x03FFFFFF;
    
    /* In range? */
    if( bin_min && bin_max )
    if( !(MKR(old_tgt) >= bin_min && MKR(old_tgt) < bin_max) )
    {
        DEBUG_R( "%s: skipping 0x%08X - out of bounds", STRTYPE(type), MKR(old_tgt) );
        
        return NOVL_RELOC_FAIL;
    }
    
    /* Make new target */
    new_tgt = old_tgt + (offset / 4);
    
    /* Apply */
    w &= ~0x03FFFFFF;
    w |= new_tgt & 0x03FFFFFF;
    
    /* Write it back */
    *i = g_htonl( w );
    
    /**/ DEBUG_R( "%s: 0x%08X -> 0x%08X", STRTYPE(type), MKR(old_tgt), MKR(new_tgt) );
    
    return NOVL_RELOC_SUCCESS;
}

/* Relocate the high part of an immediate value */
static int
novl_reloc_mips_hi16 ( uint32_t * i, uint32_t address, int type, int offset )
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
    
    return NOVL_RELOC_SUCCESS;
}

/* Relocate the low part of an immediate value */
static int
novl_reloc_mips_lo16 ( uint32_t * i, uint32_t address, int type, int offset )
{
    int16_t val;
    uint32_t w, old_w, new_addr, hi, lo;
    int reg;
    
    /* Read word (in our endian) */
    w = g_ntohl( *i );
    
    /* Get affected register */
    reg = (w >> 21) & 0x1F;
    
    /* Get the immediate value */
    val = (int16_t)(w & 0xFFFF);
    
    /* Finish building the value */
    hilopair_regs[reg] += val;
    
    /* Skip this? */
    if( bin_min && bin_max )
    if( !(hilopair_regs[reg] >= bin_min  && hilopair_regs[reg] < bin_max) )
    {
        DEBUG_R( "HI16/LO16: Skipping 0x%08X - out of bounds", hilopair_regs[reg] );
        
        return NOVL_RELOC_FAIL;
    }
    
    /* Relocate it */
    new_addr = hilopair_regs[reg] + offset;
    
    /* Cut it up */
    lo = new_addr & 0xFFFF;
    hi = (new_addr >> 16) + ((lo & 0x8000) ? 1 : 0);
    
    /* Get old word (part 1) */
    old_w = g_ntohl( *hilopair_ptrs[reg] );
    
    /* Update the instructions */
    *i = g_htonl( (w & 0xFFFF0000) | lo );
    *hilopair_ptrs[reg] = g_htonl( (old_w & 0xFFFF0000) | hi );
    
    /**/ DEBUG_R( "HI16/LO16: 0x%08X -> 0x%08X", hilopair_regs[reg], new_addr );
    
    return NOVL_RELOC_SUCCESS;
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
novl_reloc_do ( uint32_t * i, uint32_t address, int type, int offset )
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
    return handler( i, address, type, offset );
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

