/*
*   nOVL -- mesg.h
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
#ifndef __NOVL_MESG_H__
#define __NOVL_MESG_H__

#include <stdarg.h>
#include "novl.h"
#include "ansi.h"

/* Verbosity values */
#define NOVL_VERBOSE_NONE      -1
#define NOVL_VERBOSE_NOTICE     0
#define NOVL_VERBOSE_MESSAGE    1
#define NOVL_VERBOSE_DEBUG      2
#define NOVL_VERBOSE_DEBUG2     3

extern int mk_mesg_f ( int, char *, char *, va_list );
extern int mk_mesg ( int, char *, char *, ... );
extern int NOTICE ( char *, ... );
extern int MESG ( char *, ... );
extern int ERROR ( char *, ... );
extern int DEBUG ( char *, ... );
extern int DEBUG_R ( char *, ... );

#endif /* __NOVL_MESG_H__ */

