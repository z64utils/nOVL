/*
*   nOVL -- novl.h
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
#ifndef __NOVL_H__
#define __NOVL_H__

#include <stdio.h>
#include <stdint.h>
#include <limits.h>
#ifdef _WIN32 /* windows */
#include <libelf.h>
#else /* linux */
#include <elf.h>
#endif
#include <libelf.h>
#include <gelf.h>
#include <glib.h>
#include "mips-reloc.h"


/* ----------------------------------------------
   Constants
   ---------------------------------------------- */

#define NOVL_VERSION_MAJOR  1
#define NOVL_VERSION_MINOR  0
#define NOVL_VERSION_MICRO  5
#define NOVL_VERSION_STR    "v1.0.5"
#define NOVL_NAME           "nOVL"
#define NOVL_FULL_NAME      "Nintendo Overlay Tool"
#define NOVL_AUTHOR         "Marshall R. <mbr@64.vg> fixes by AriaHiroshi64, Sauraen, z64me"

/* Relocation return values */
#define NOVL_RELOC_FAIL     2
#define NOVL_RELOC_SUCCESS  3
#define NOVL_RELOC_FAIL_DONTDELETEHI 4


#include "mesg.h"
#include "func.h"


/* ----------------------------------------------
   Macros
   ---------------------------------------------- */

/* Record a (CLI) flag */
#define SET_FLAG(f)        (settings.flags[(f) & 0xFF] = 1)

/* Get a (CLI) flag */
#define GET_FLAG(f)        (settings.flags[(f) & 0xFF])


/* ----------------------------------------------
   Data types
   ---------------------------------------------- */

typedef int (*novlDoReloc)
(
    uint32_t *, /* Pointer to instruction in memory */
    int         /* bool for whether to do a dry run */
);

/* Settings structure */
struct novl_settings_t
{
    int opmode;
    int verbosity;
    int no_colors;
    int human_readable;
    uint32_t base_addr;
    uint8_t flags[UCHAR_MAX + 1];
};

typedef struct
{
    uint32_t elf_addr; /* Address of the instruction etc. to be patched */
    uint32_t tgt_addr; /* Address encoded in the instruction etc. */
    uint8_t type; /* R_MIPS_32, R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16 */
    int8_t elf_sec; /* Section (0-3) elf_addr is within; set to -1 if tgt_addr 
        is outside overlay */
} novl_reloc_entry;

/* ----------------------------------------------
   Variable declarations
   ---------------------------------------------- */

extern char * novl_str_reloc_types[];
extern novlDoReloc novl_reloc_jt[];
extern struct novl_settings_t settings;


/* ----------------------------------------------
   Function declarations
   ---------------------------------------------- */

extern char * novl_str_reloc ( int );
extern void novl_conv ( char *, char * );
extern uint32_t novl_reloc_mk ( int, int, int );

/* Relocation functions */
extern int novl_reloc_do ( uint32_t *, int, int );

extern void novl_reloc_init ();
   

#endif /* __NOVL_H__ */
