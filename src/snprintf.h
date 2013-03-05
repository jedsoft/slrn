/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.

 Copyright (c) 2001-2006 Thomas Schultz <tststs@gmx.de>

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
#ifndef _SLRN_SNPRINTF_H
#define _SLRN_SNPRINTF_H
#include <stdarg.h>

#include "jdmacros.h"

extern char *slrn_strdup_strcat (const char*, ...);
extern char *slrn_strdup_printf (const char*, ...) ATTRIBUTE_PRINTF(1,2);
extern char *slrn_strdup_vprintf (const char*, va_list);

extern int slrn_snprintf (char *, size_t, const char *, ...) ATTRIBUTE_PRINTF(3,4);
extern int slrn_vsnprintf (char *, size_t, const char *, va_list);

#ifndef HAVE_VSNPRINTF
/* extern int snprintf (char *, size_t, const char *, ...); */
/* extern int vsnprintf (char *, size_t, const char *, va_list); */
#endif /* !HAVE_VSNPRINTF */
#endif /* _SLRN_SNPRINTF_H */
