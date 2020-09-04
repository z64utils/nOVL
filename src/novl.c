/*
*   nOVL -- novl.c
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
#include "novl.h"
#include "overlay.h"



/* ----------------------------------------------
   Global variables
   ---------------------------------------------- */

/* Literal versions of relocation types */
char * novl_str_reloc_types[] = 
{
    "R_MIPS_NONE",            "R_MIPS_16",
    "R_MIPS_32",              "R_MIPS_REL32",
    "R_MIPS_26",              "R_MIPS_HI16",
    "R_MIPS_LO16",            "R_MIPS_GPREL16",
    "R_MIPS_LITERAL",         "R_MIPS_GOT16",
    "R_MIPS_PC16",            "R_MIPS_CALL16",
    "R_MIPS_GPREL32",         "R_MIPS_SHIFT5",
    "R_MIPS_SHIFT6",          "R_MIPS_64",
    "R_MIPS_GOT_DISP",        "R_MIPS_GOT_PAGE",
    "R_MIPS_GOT_OFST",        "R_MIPS_GOT_HI16",
    "R_MIPS_GOT_LO16",        "R_MIPS_SUB",
    "R_MIPS_INSERT_A",        "R_MIPS_INSERT_B",
    "R_MIPS_DELETE",          "R_MIPS_HIGHER",
    "R_MIPS_HIGHEST",         "R_MIPS_CALL_HI16",
    "R_MIPS_CALL_LO16",       "R_MIPS_SCN_DISP",
    "R_MIPS_REL16",           "R_MIPS_ADD_IMMEDIATE",
    "R_MIPS_PJUMP",           "R_MIPS_RELGOT",
    "R_MIPS_JALR",            "R_MIPS_TLS_DTPMOD32",
    "R_MIPS_TLS_DTPREL32",    "R_MIPS_TLS_DTPMOD64",
    "R_MIPS_TLS_DTPREL64",    "R_MIPS_TLS_GD",
    "R_MIPS_TLS_LDM",         "R_MIPS_TLS_DTPREL_HI16",
    "R_MIPS_TLS_DTPREL_LO16", "R_MIPS_TLS_GOTTPREL",
    "R_MIPS_TLS_TPREL32",     "R_MIPS_TLS_TPREL64",
    "R_MIPS_TLS_TPREL_HI16",  "R_MIPS_TLS_TPREL_LO16",
    "R_MIPS_GLOB_DAT",        "R_MIPS_COPY",
    "R_MIPS_JUMP_SLOT",       NULL
};

/* The settings structure */
struct novl_settings_t settings;


/* ----------------------------------------------
   Global functions
   ---------------------------------------------- */

/* Get a string from a MIPS relocation type */
char *
novl_str_reloc ( int reloc )
{
    return novl_str_reloc_types[reloc];
}


