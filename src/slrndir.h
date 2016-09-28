/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999, 2007-2016 John E. Davis <jed@jedsoft.org>

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
typedef struct
{
   char name [SLRN_MAX_PATH_LEN + 1];
   unsigned int name_len;
   unsigned int flags;
}
Slrn_Dirent_Type;

typedef struct _Slrn_Dir_Type Slrn_Dir_Type;

extern Slrn_Dir_Type *slrn_open_dir (char *);
extern void slrn_close_dir (Slrn_Dir_Type *);
extern Slrn_Dirent_Type *slrn_read_dir (Slrn_Dir_Type *);

extern char *slrn_getcwd (char *, unsigned int);
extern int slrn_chdir (char *);

