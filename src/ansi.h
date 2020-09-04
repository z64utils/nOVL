/* $Id: ansi-color.h,v 1.2 2004/10/01 19:05:23 ekalin Exp $ */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#ifndef __ANSI_H__
#define __ANSI_H__


/*************************
 * ANSI Escape Sequences *
 *************************/

/* Basic building blocks for colors */
#define ANSI_START 		 "\033["
#define ANSI_COLOR_END   "m"
#define ANSI_SEP         ";"

#define ANSI_ATTRIBS_OFF "0"
#define ANSI_BOLD_ON     "1"

#define ANSI_FG_BLACK    "30"
#define ANSI_FG_RED      "31"
#define ANSI_FG_GREEN    "32"
#define ANSI_FG_YELLOW   "33"
#define ANSI_FG_BLUE     "34"
#define ANSI_FG_MAGENTA  "35"
#define ANSI_FG_CYAN     "36"
#define ANSI_FG_WHITE    "37"

#define ANSI_BG_BLACK    "40"
#define ANSI_BG_RED      "41"
#define ANSI_BG_GREEN    "42"
#define ANSI_BG_YELLOW   "43"
#define ANSI_BG_BLUE     "44"
#define ANSI_BG_MAGENTA  "45"
#define ANSI_BG_CYAN     "46"
#define ANSI_BG_WHITE    "47"

/* Codes to set colors */
#define ANSI_RESET_DEFAULT  ANSI_START ANSI_ATTRIBS_OFF ANSI_COLOR_END

#define ANSI_SET_FG_BLACK   ANSI_START ANSI_ATTRIBS_OFF ANSI_SEP \
                            ANSI_FG_BLACK   ANSI_COLOR_END
#define ANSI_SET_FG_RED     ANSI_START ANSI_ATTRIBS_OFF ANSI_SEP \
                            ANSI_FG_RED     ANSI_COLOR_END
#define ANSI_SET_FG_GREEN   ANSI_START ANSI_ATTRIBS_OFF ANSI_SEP \
                            ANSI_FG_GREEN   ANSI_COLOR_END
#define ANSI_SET_FG_BLUE    ANSI_START ANSI_ATTRIBS_OFF ANSI_SEP \
                            ANSI_FG_BLUE    ANSI_COLOR_END
#define ANSI_SET_FG_YELLOW  ANSI_START ANSI_ATTRIBS_OFF ANSI_SEP \
                            ANSI_FG_YELLOW  ANSI_COLOR_END
#define ANSI_SET_FG_MAGENTA ANSI_START ANSI_ATTRIBS_OFF ANSI_SEP \
                            ANSI_FG_MAGENTA ANSI_COLOR_END
#define ANSI_SET_FG_CYAN    ANSI_START ANSI_ATTRIBS_OFF ANSI_SEP \
                            ANSI_FG_CYAN    ANSI_COLOR_END
#define ANSI_SET_FG_WHITE   ANSI_START ANSI_ATTRIBS_OFF ANSI_SEP \
                            ANSI_FG_WHITE   ANSI_COLOR_END

#define ANSI_SET_BG_BLACK   ANSI_START ANSI_BG_BLACK   ANSI_COLOR_END
#define ANSI_SET_BG_RED     ANSI_START ANSI_BG_RED     ANSI_COLOR_END
#define ANSI_SET_BG_GREEN   ANSI_START ANSI_BG_GREEN   ANSI_COLOR_END
#define ANSI_SET_BG_BLUE    ANSI_START ANSI_BG_BLUE    ANSI_COLOR_END
#define ANSI_SET_BG_YELLOW  ANSI_START ANSI_BG_YELLOW  ANSI_COLOR_END
#define ANSI_SET_BG_MAGENTA ANSI_START ANSI_BG_MAGENTA ANSI_COLOR_END
#define ANSI_SET_BG_CYAN    ANSI_START ANSI_BG_CYAN    ANSI_COLOR_END
#define ANSI_SET_BG_WHITE   ANSI_START ANSI_BG_WHITE   ANSI_COLOR_END

#define ANSI_SET_FG_BLACK_BOLD   ANSI_START ANSI_BOLD_ON ANSI_SEP \
                                 ANSI_FG_BLACK   ANSI_COLOR_END
#define ANSI_SET_FG_RED_BOLD     ANSI_START ANSI_BOLD_ON ANSI_SEP \
                                 ANSI_FG_RED     ANSI_COLOR_END
#define ANSI_SET_FG_GREEN_BOLD   ANSI_START ANSI_BOLD_ON ANSI_SEP \
                                 ANSI_FG_GREEN   ANSI_COLOR_END
#define ANSI_SET_FG_BLUE_BOLD    ANSI_START ANSI_BOLD_ON ANSI_SEP \
                                 ANSI_FG_BLUE    ANSI_COLOR_END
#define ANSI_SET_FG_YELLOW_BOLD  ANSI_START ANSI_BOLD_ON ANSI_SEP \
                                 ANSI_FG_YELLOW  ANSI_COLOR_END
#define ANSI_SET_FG_MAGENTA_BOLD ANSI_START ANSI_BOLD_ON ANSI_SEP \
                                 ANSI_FG_MAGENTA ANSI_COLOR_END
#define ANSI_SET_FG_CYAN_BOLD    ANSI_START ANSI_BOLD_ON ANSI_SEP \
                                 ANSI_FG_CYAN    ANSI_COLOR_END
#define ANSI_SET_FG_WHITE_BOLD   ANSI_START ANSI_BOLD_ON ANSI_SEP \
                                 ANSI_FG_WHITE    ANSI_COLOR_END

#endif /* __ANSI_H__ */

