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
#ifndef _SLRN_PRINT_H_
#define _SLRN_PRINT_H_
typedef struct _Slrn_Print_Type Slrn_Print_Type;

extern Slrn_Print_Type *slrn_open_printer (void);
extern int slrn_close_printer (Slrn_Print_Type *);
extern int slrn_write_to_printer (Slrn_Print_Type *, char *, unsigned int);

extern int slrn_print_file (char *);

extern char *Slrn_Printer_Name;

#endif

