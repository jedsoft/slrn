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

/* Keep this separate from autoconf's settings for now */
#define MY_VERSION "0.9.8.0"
#define SLRN_VERSION_NUMBER 908000
#undef PATCH_LEVEL
#define SLRN_RELEASE_DATE "2003-08-25"

#ifdef PATCH_LEVEL
# define SLRN_VERSION (MY_VERSION "pl" PATCH_LEVEL)
#else  
# define SLRN_VERSION (MY_VERSION)
#endif

#ifndef SLRNPULL_CODE
extern char *Slrn_Version;
extern int Slrn_Version_Number;
extern char *Slrn_Date;
extern char *slrn_get_os_name (void);
extern void slrn_show_version (void);
#endif
