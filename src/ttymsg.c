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
#include "config.h"
#include "slrnfeat.h"

#include <stdio.h>
#include <string.h>
#include <slang.h>

#include "jdmacros.h"

#include "ttymsg.h"
#include "common.h"

void slrn_tty_vmessage (FILE *fp, char *fmt, va_list ap)
{
   static FILE *last_fp;
   char *b, buf[1024];

   if ((fp == stdout) || (last_fp == stdout)) fputc ('\n', fp);

   /* Use SLvsnprintf to write to a buffer so that any \001 highlight
    * indicators can be stripped.
    */
   /* (void) vfprintf(fp, fmt, ap); */
   (void) SLvsnprintf (buf, sizeof(buf), fmt, ap);
   b = buf;
   while (*b != 0)
     {
	if (*b != '\001')
	  (void) fputc (*b, fp);
	b++;
     }

   if (fp == stderr) (void) fputc ('\n', fp);
   fflush (fp);

   last_fp = fp;
}

void slrn_tty_message (char *fmt, ...)
{
   va_list ap;

   va_start (ap, fmt);
   slrn_tty_vmessage (stdout, fmt, ap);
   va_end (ap);
}

void slrn_tty_error (char *fmt, ...)
{
   va_list ap;

   va_start (ap, fmt);
   slrn_tty_vmessage (stderr, fmt, ap);
   va_end (ap);
}

