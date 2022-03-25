/*
*   nOVL -- novl-mips.c
*   Copyright (c) 2022 Sauraen <sauraen@gmail.com>
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
#include "novl-mips.h"
#include <stdio.h>
#include "novl.h"
#include "mips-reloc.h"

#define REG_UNSET 0
#define REG_SET 1
#define REG_USED 2
#define REG_CONFLICT 3

typedef struct {
    uint8_t status;
    uint16_t hival;
    novl_reloc_entry * hireloc;
} reg_state;

typedef struct exec_state_s {
    struct exec_state_s * next;
    uint32_t a;
    reg_state r[32];
} exec_state;

static int cur_is_clean;
static exec_state cur_state;
static exec_state * future_states;

void
novl_mips_clearall ( )
{
    novl_mips_clearstate ();
    future_states = NULL;
}

void
novl_mips_mergestate ( exec_state * dest, exec_state * src, int dest_is_clean )
{
    int i;
    for(i=0; i<32; ++i)
    {
        if(dest_is_clean)
        {
            dest->r[i] = src->r[i];
        }
        else if(dest->r[i].status == REG_UNSET && src->r[i].status == REG_UNSET)
        {
            (void)0; /* Leave unset */
        }
        else if((dest->r[i].status == REG_SET || dest->r[i].status == REG_USED)
            && (src->r[i].status == REG_SET || src->r[i].status == REG_USED)
            && dest->r[i].hival == src->r[i].hival
            && dest->r[i].hireloc == src->r[i].hireloc)
        {
            if(dest->r[i].status == REG_USED || src->r[i].status == REG_USED)
            {
                dest->r[i].status = REG_USED;
            }
            else
            {
                dest->r[i].status == REG_SET;
            }
        }
        else
        {
            dest->r[i].status = REG_CONFLICT;
        }
    }
}

void
novl_mips_checkmerge ( uint32_t a )
{
    /* We have now gotten to address a; check if there is a future state for
    this address */
    exec_state * last = NULL;
    exec_state * s = future_states;
    while( 1 )
    {
        if( s == NULL ) return;
        
        if( s->a == a ) break;
        
        last = s;
        s = s->next;
    }
    
    /* Merge into current state */
    DEBUG("Merging state @%08X", a);
    novl_mips_mergestate( &cur_state, s, cur_is_clean );
    cur_is_clean = 0;
    
    /* Delete s */
    if(last == NULL)
    {
        future_states = s->next;
    }
    else
    {
        last->next = s->next;
    }
    free( s );
}

void
novl_mips_got_hi16 ( uint32_t a, uint32_t instr, novl_reloc_entry * reloc )
{
    int r = (instr >> 16) & 0x1F;
    cur_state.r[r].status = REG_SET;
    cur_state.r[r].hival = instr & 0xFFFF;
    cur_state.r[r].hireloc = reloc;
    DEBUG("@%08X instr %08X hi %04X", a, instr, cur_state.r[r].hival);
}

void
novl_mips_got_lo16 ( uint32_t a, uint32_t instr, novl_reloc_entry * reloc )
{
    int r = (instr >> 16) & 0x1F;
    int16_t imm = instr & 0xFFFF;
    uint8_t status = cur_state.r[r].status;
    if(status == REG_UNSET)
    {
        ERROR("@%08X instr %08X lo reloc uses hi from reg %d, but not set!",
            a, instr, r);
        exit(EXIT_FAILURE);
    }
    else if(status == REG_CONFLICT)
    {
        ERROR("@%08X instr %08X lo reloc uses hi from reg %d, but the hi value is different on different codepaths!",
            a, instr, r);
        exit(EXIT_FAILURE);
    }
    
    uint32_t tgt_addr = ((int32_t)cur_state.r[r].hival << 16) + (int32_t)imm;
    
    if(status == REG_USED)
    {
        if(cur_state.r[r].hireloc->tgt_addr != tgt_addr)
        {
            ERROR("@%08X instr %08X lo reloc forms addr %08X, but hi already used to form addr %08X!",
                a, instr, tgt_addr, cur_state.r[r].hireloc->tgt_addr);
            exit(EXIT_FAILURE);
        }
        
        DEBUG("@%08X instr %08X reusing hi value %04X from reg %d --> addr %08X",
            a, instr, cur_state.r[r].hival, r, tgt_addr);
    }
    else
    {
        cur_state.r[r].hireloc->tgt_addr = tgt_addr;
        
        DEBUG("@%08X instr %08X --> addr %08X",
            a, instr, tgt_addr);
    }
    cur_state.r[r].status = REG_USED;
    reloc->tgt_addr = tgt_addr;
}

void
novl_mips_futurestate ( uint32_t a )
{
    /* Check if there is already a future state for this address */
    int i;
    exec_state * last = NULL;
    exec_state * s = future_states;
    while( 1 )
    {
        if( s == NULL ) break;
        
        if( s->a == a )
        {
            /* Merge into future state */
            DEBUG("Future state @%08X already exists, merging", a);
            
            novl_mips_mergestate( s, &cur_state, 0 );
            return;
        }
        
        last = s;
        s = s->next;
    }
    
    /* Create another */
    DEBUG("Creating new future state @%08X", a);
    
    last = malloc(sizeof(exec_state));
    memcpy(last, &cur_state, sizeof(exec_state));
    last->next = NULL;
    last->a = a;
    s->next = last;
}

void
novl_mips_clearstate ( )
{
    int i;
    DEBUG("Clearing state");
    
    for(i=0; i<32; ++i)
    {
        cur_state.r[i].status = REG_UNSET;
    }
    cur_is_clean = 1;
}

void
novl_mips_jr ( )
{
    ERROR("jr handling not implemented yet!");
    exit(EXIT_FAILURE);
}

int
novl_mips_is_branch ( uint32_t instr )
{
    uint8_t opbyte = (instr >> 24) & 0xFC;
    uint8_t regimm_mask = (instr >> 16) & 0x0C;
    return (opbyte >= 0x10 && opbyte <= 0x1C) 
        || (opbyte >= 0x50 && opbyte <= 0x5C)
        || (opbyte == 4 && regimm_mask == 0x00);
}

int
novl_mips_is_branch_likely ( uint32_t instr )
{
    uint8_t opbyte = (instr >> 24) & 0xFC;
    uint8_t regimm_mask = (instr >> 16) & 0x0E;
    return (opbyte >= 0x50 && opbyte <= 0x5C)
        || (opbyte == 4 && regimm_mask == 0x02);
}

int
novl_mips_is_forward_branch ( uint32_t instr )
{
    return novl_mips_is_branch( instr ) && (instr & 0xFFFF) <= 0x7FFF;
}

int
novl_mips_is_jump ( uint32_t instr )
{
    uint8_t opbyte = (instr >> 24) & 0xFC;
    uint8_t arith = instr & 0x3F;
    uint8_t regb = (instr >> 21) & 0x1F;
    return novl_mips_is_branch( instr ) && (instr & 0xFFFF) >= 0x8000
        || opbyte == 0x08 || opbyte == 0x0C /* j, jal */
        || (opbyte == 0x00 && (arith == 0x09 || /* jalr */
            (arith == 0x08 && regb == 0x1F))); /* jr $ra */
}

int
novl_mips_is_jr ( uint32_t instr )
{
    uint8_t opbyte = (instr >> 24) & 0xFC;
    uint8_t arith = instr & 0x3F;
    uint8_t regb = (instr >> 21) & 0x1F;
    return opbyte == 0x00 && arith == 0x08 && regb != 0x1F;
}
