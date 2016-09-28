/* variables common to slrnpull and slrn */
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
#ifndef _SLRN_COMMON_H_
#define _SLRN_COMMON_H_
extern int Slrn_UTF8_Mode;
extern void slrn_error (char *, ...) ATTRIBUTE_PRINTF(1,2);  /* in misc.c */
extern void slrn_exit_error (char *, ...) ATTRIBUTE_PRINTF(1,2);

#endif
