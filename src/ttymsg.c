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
#include "config.h"
#include "slrnfeat.h"

#include <stdio.h>
#include <string.h>

#include "ttymsg.h"

void slrn_tty_vmessage (FILE *fp, char *fmt, va_list ap)
{
   static FILE *last_fp;
   
   if ((fp == stdout) || (last_fp == stdout)) fputc ('\n', fp);
   (void) vfprintf(fp, fmt, ap);
   if (fp == stderr) fputc ('\n', fp);
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
