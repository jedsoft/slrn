/*
 This file is part of SLRN.
 It describes the interface between art.c and art_sort.c.
 Variables and functions meant for global use are in art.h!

 Copyright (c) 1994, 1999, 2007-2016 John E. Davis <jed@jedsoft.org>
 Copyright (c) 2001-2006 Thomas Schultz <tststs@gmx.de>

 This program is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 of the License, or (at your option)
 any later version.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
*/
#ifndef _SLRN_ART_SORT_H
#define _SLRN_ART_SORT_H

/* Defined in art_sort.c and also used by art.c : */
extern int _art_Headers_Threaded;

extern void _art_toggle_sort (void);
extern void _art_sort_by_server_number (void);
extern int _art_subject_cmp (char *sa, char *sb);

/* Defined in art.c and also used by art_sort.c : */
extern Slrn_Header_Type *_art_Headers;
extern int _art_Threads_Collapsed;

extern void _art_find_header_line_num (void);
extern Slrn_Header_Type *_art_find_header_from_msgid (char *r0, char *r1);

#endif				       /* _SLRN_ART_H */
