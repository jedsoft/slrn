/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999 John E. Davis <davis@space.mit.edu>
 Copyright (c) 2001-2003 Thomas Schultz <tststs@gmx.de>

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
 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifndef _SLRN_XOVER_H
#define _SLRN_XOVER_H

/* In this structure, only subject_malloced will be malloced.  All other 
 * pointers point to a location in that space.  It is done this way because
 * art.c uses this convention and the pointer can just be passed to it.
 */

typedef struct
{
   int id;
   char *subject_malloced; /* keep these separate from the rest */
   char *from;
   char *date_malloced;
   char *message_id;
   char *references;
   char *xref;
   int bytes;
   int lines;
   Slrn_Header_Line_Type *add_hdrs;
}
Slrn_XOver_Type;

extern void slrn_map_xover_to_header (Slrn_XOver_Type *, Slrn_Header_Type *);
extern void slrn_free_additional_headers (Slrn_Header_Line_Type *);
extern void slrn_clear_requested_headers (void);
extern void slrn_request_additional_header (char *, int);
extern char *slrn_extract_add_header (Slrn_Header_Type *, char *);

extern int slrn_read_overview_fmt (void);

#ifndef SLRNPULL_CODE
extern int slrn_xover_for_msgid (char *, Slrn_XOver_Type *);
extern int slrn_open_xover (int, int);
extern int slrn_read_xover (Slrn_XOver_Type *);
extern void slrn_close_xover (void);

extern void slrn_open_all_add_xover (void);
extern int slrn_open_add_xover (int, int);
extern int slrn_read_add_xover (Slrn_Header_Line_Type **);
extern void slrn_close_add_xover (int);
extern int slrn_add_xover_missing (void);
extern void slrn_append_add_xover_to_header (Slrn_Header_Type *,
					     Slrn_Header_Line_Type *);

extern void slrn_open_suspend_xover (void);
extern void slrn_close_suspend_xover (void);
#endif

#endif				       /* _SLRN_XOVER_H */
