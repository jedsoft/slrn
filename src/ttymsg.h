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
#ifndef _SLRN_TTYMSG_H
#define _SLRN_TTYMSG_H
#include <stdio.h>
#include <stdarg.h>

extern void slrn_tty_vmessage (FILE *, char *, va_list);
extern void slrn_tty_error (char *, ...);
extern void slrn_tty_message (char *, ...);
extern int slrn_message (char *, ...);
extern int slrn_message_now (char *, ...);
extern void slrn_error (char *, ...);
extern void slrn_error_now (unsigned int, char *, ...);

#endif				       /* _SLRN_TTYMSG_H */
