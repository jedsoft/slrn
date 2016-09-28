/*
 This file is part of SLRN.

 Copyright (c) 2007-2016 John E. Davis <jed@jedsoft.org>

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

#ifndef SLRN_HDRUTILS_H
#define SLRN_HDRUTILS_H
extern Slrn_Article_Line_Type *slrn_find_header_line (Slrn_Article_Type *a, char *header);
extern Slrn_Article_Line_Type *slrn_append_to_header (Slrn_Article_Type *a, char *buf, int free_on_error);
extern Slrn_Article_Line_Type *slrn_append_header_keyval (Slrn_Article_Type *a, char *key, char *value);
#endif
