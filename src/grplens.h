/*
 This file is part of SLRN.

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
#ifndef _SLRN_GRPLENS_H
#define _SLRN_GRPLENS_H
extern int Slrn_Use_Group_Lens;
extern int slrn_init_grouplens (void);
extern void slrn_close_grouplens (void);
extern int slrn_put_grouplens_scores (void);
extern int slrn_get_grouplens_scores (void);
extern void slrn_group_lens_rate_article (Slrn_Header_Type *, int, int);

extern char *Slrn_GroupLens_Pseudoname;
extern char *Slrn_GroupLens_Host;
extern int slrn_grouplens_add_group (char *);

extern int Slrn_GroupLens_Port;

#endif


