/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999 John E. Davis <davis@space.mit.edu>
 Copyright (c) 2001, 2002 Thomas Schultz <tststs@gmx.de>

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
#ifndef _SLRN_CHMAP_H
#define _SLRN_CHMAP_H
extern int slrn_set_charset (char *);
extern int slrn_chmap_fix_file (char *, int);
extern void slrn_chmap_fix_body (Slrn_Article_Type *, int);
extern void slrn_chmap_fix_header (Slrn_Header_Type *);

#if SLRN_HAS_CHARACTER_MAP
extern char *Slrn_Charset;
extern void slrn_chmap_show_supported (void);
#endif

#endif
