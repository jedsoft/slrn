/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.

 Copyright (c) 2003 Thomas Schultz <tststs@gmx.de>

 partly based on code by John E. Davis:
 Copyright (c) 1994, 1999 John E. Davis <davis@space.mit.edu>

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
#ifndef _SLRN_RANGES_H
#define _SLRN_RANGES_H

#include <stdio.h>

typedef struct Slrn_Range_Type 
{
   struct Slrn_Range_Type *next;
   struct Slrn_Range_Type *prev;
   int min, max;
} Slrn_Range_Type;

extern Slrn_Range_Type *slrn_ranges_from_newsrc_line (char *);
extern int slrn_ranges_to_newsrc_file (Slrn_Range_Type *, int, FILE*);

extern Slrn_Range_Type *slrn_ranges_add (Slrn_Range_Type *, int, int);
extern Slrn_Range_Type *slrn_ranges_remove (Slrn_Range_Type *, int, int);

extern Slrn_Range_Type *slrn_ranges_intersect (Slrn_Range_Type *, Slrn_Range_Type *);
extern Slrn_Range_Type *slrn_ranges_merge (Slrn_Range_Type *, Slrn_Range_Type *);

extern int slrn_ranges_is_member (Slrn_Range_Type *, int);

extern Slrn_Range_Type *slrn_ranges_clone (Slrn_Range_Type *);
extern int slrn_ranges_compare (Slrn_Range_Type *, Slrn_Range_Type *);
extern void slrn_ranges_free (Slrn_Range_Type *);
#endif /* _SLRN_RANGES_H */
