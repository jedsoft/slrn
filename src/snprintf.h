/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.

 Copyright (c) 2001 Thomas Schultz <tststs@gmx.de>

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
#ifndef _SLRN_SNPRINTF_H
#define _SLRN_SNPRINTF_H
#include <stdarg.h>

extern char *slrn_strdup_strcat (const char*, ...);
extern char *slrn_strdup_printf (const char*, ...);
extern char *slrn_strdup_vprintf (const char*, va_list);

extern int slrn_snprintf (char *, size_t, const char *, ...);
extern int slrn_vsnprintf (char *, size_t, const char *, va_list);

extern char *slrn_strncpy (char *, const char*, size_t);

#ifndef HAVE_VSNPRINTF
extern int snprintf (char *, size_t, const char *, ...);
extern int vsnprintf (char *, size_t, const char *, va_list);
#endif /* !HAVE_VSNPRINTF */
#endif /* _SLRN_SNPRINTF_H */
