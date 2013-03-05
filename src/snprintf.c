/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.

 Copyright (c) 2001-2006 Thomas Schultz <tststs@gmx.de>

 Based on code from glib 1.2.8; original copyright notice:
 Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald
 Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 file for a list of people on the GLib Team.  See the ChangeLog
 files for a list of changes.  These files are distributed with
 GLib at ftp://ftp.gtk.org/pub/gtk/. 

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

/*{{{ include files */
#include "config.h"
#include "slrnfeat.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include "jdmacros.h"
#include "snprintf.h"
#include "util.h"
#include "strutil.h"

/*}}}*/

/*{{{ static function declarations and defines */
static unsigned int printf_string_upper_bound (const char*, va_list);

/* Define VA_COPY() to do the right thing for copying va_list variables.
 * config.h may have already defined VA_COPY as va_copy or __va_copy.
 */
#ifndef VA_COPY
# if (defined (__GNUC__) && defined (__PPC__) && (defined (_CALL_SYSV) || defined (__WIN32__))) || defined (__WATCOMC__)
#  define VA_COPY(ap1, ap2)	  (*(ap1) = *(ap2))
# elif defined (VA_COPY_AS_ARRAY)
#  define VA_COPY(ap1, ap2)	  memmove ((ap1), (ap2), sizeof (va_list))
# else /* va_list is a pointer */
#  define VA_COPY(ap1, ap2)	  ((ap1) = (ap2))
# endif /* va_list is a pointer */
#endif /* !VA_COPY */

/*}}}*/

/* Remember that you explicitly need to pass NULL as the final argument! */
char *slrn_strdup_strcat (const char *str, ...) /*{{{*/
{
   char *buffer, *cur;
   const char *p;
   unsigned int len = 0;
   va_list args;
   
   if ((p = str) == NULL) return NULL;
   
   va_start (args, str);
   while (p != NULL)
     {
	len += strlen (p);
	p = va_arg (args, const char *);
     }
   va_end (args);
   
   cur = buffer = slrn_safe_malloc (len + 1);
   
   va_start (args, str);
   p = str;
   while (p != NULL)
     {
	strcpy (cur, p); /* safe */
	p = va_arg (args, const char *);
	cur += strlen (cur);
     }
   va_end (args);
   
   return buffer;
}
/*}}}*/

char *slrn_strdup_vprintf (const char *format, va_list args1) /*{{{*/
{
   char *buffer;
   va_list args2;
   
   if (format == NULL) return NULL;
   
   VA_COPY (args2, args1);
   
   buffer = slrn_safe_malloc (printf_string_upper_bound (format, args1));
   
   vsprintf (buffer, format, args2); /* safe */
   va_end (args2);
   
   return buffer;
}

/*}}}*/

char *slrn_strdup_printf (const char *format, ... ) /*{{{*/
{
   va_list args;
   char *retval;
   
   va_start (args, format);
   retval = slrn_strdup_vprintf (format, args);
   va_end (args);
   
   return retval;
}

/*}}}*/

int slrn_vsnprintf (char *str, size_t n, const char *format, /*{{{*/
		    va_list ap) 
{
   int retval;
   
   retval = vsnprintf (str, n, format, ap);
   
   if ((retval == -1) || (retval > (int) n))
     {
	str[n-1] = '\0';
	retval = (int) n;
     }
   
   return retval;
}

/*}}}*/

int slrn_snprintf (char *str, size_t n, const char *format, ... ) /*{{{*/
{
   va_list args;
   int retval;
   
   va_start (args, format);
   retval = vsnprintf (str, n, format, args);
   va_end (args);
   
   if ((retval == -1) || (retval > (int) n))
     {
	str[n-1] = '\0';
	retval = (int) n;
     }
   
   return retval;
}

/*}}}*/

#ifndef HAVE_VSNPRINTF
int snprintf (char *str, size_t n, const char *format, ... ) /*{{{*/
{
   va_list args;
   int retval;
   char *printed;
   
   if (str == NULL) return 0;
   if (n <= 0) return 0;
   if (format == NULL) return 0;

   va_start (args, format);
   printed = slrn_strdup_vprintf (format, args);
   va_end (args);
   
   strncpy (str, printed, n);
   retval = strlen (printed); /* behave like glibc 2.1 */
   
   slrn_free (printed);
   
   return retval;
}

/*}}}*/

int vsnprintf (char *str, size_t n, const char *format, va_list ap) /*{{{*/
{
   char *printed;
   int retval;
   
   if (str == NULL) return 0;
   if (n <= 0) return 0;
   if (format == NULL) return 0;
   
   printed = slrn_strdup_vprintf (format, ap);
   strncpy (str, printed, n);
   retval = strlen (printed); /* behave like glibc 2.1 */
   
   slrn_free (printed);
   
   return retval;
}

/*}}}*/
#endif /* !HAVE_VSNPRINTF */

/* Note: This function is completely rewritten in glib 1.2.9 and later.
 * In the long run, I should use their new code; currently, I believe slrn
 * uses no format strings that could cause problems with this version. */

static unsigned int printf_string_upper_bound (const char* format, /*{{{*/
					       va_list args)
{
   unsigned int len = 1;
   
   while (*format)
     {
	int long_int = 0;
	int extra_long = 0;
	char c;
	
	c = *format++;
	
	if (c == '%')
	  {
	     int done = 0;
	     
	     while (*format && !done)
	       {
		  switch (*format++)
		    {
		       char *string_arg;
		       
		     case '*':
		       len += va_arg (args, int);
		       break;
		     case '1':
		     case '2':
		     case '3':
		     case '4':
		     case '5':
		     case '6':
		     case '7':
		     case '8':
		     case '9':
		       /* add specified format length, since it might exceed
			* the size we assume it to have. */
		       format -= 1;
		       len += strtol (format, (char **)&format, 10);
		       break;
		     case 'h':
		       /* ignore short int flag, since all args have at least
			* the same size as an int */
		       break;
		     case 'l':
		       if (long_int)
			 extra_long = 1; /* linux specific */
		       else
			 long_int = 1;
		       break;
		     case 'q':
		     case 'L':
		       long_int = 1;
		       extra_long = 1;
		       break;
		     case 's':
		       string_arg = va_arg (args, char *);
		       if (NULL != string_arg)
			 len += strlen (string_arg);
		       else
			 {
			    /* add enough padding to hold "(null)" identifier */
			    len += 16;
			 }
		       done = 1;
		       break;
		     case 'd':
		     case 'i':
		     case 'o':
		     case 'u':
		     case 'x':
		     case 'X':
			 {
			    if (long_int)
			      (void) va_arg (args, long);
			    else
			      (void) va_arg (args, int);
			 }
		       len += extra_long ? 64 : 32;
		       done = 1;
		       break;
		     case 'D':
		     case 'O':
		     case 'U':
		       (void) va_arg (args, long);
		       len += 32;
		       done = 1;
		       break;
		     case 'e':
		     case 'E':
		     case 'f':
		     case 'g':
#ifdef HAVE_LONG_DOUBLE
 /* Warning: There is currently no test in ./configure to enable this */
		       if (extra_long)
			 (void) va_arg (args, long double);
		       else
#endif	/* HAVE_LONG_DOUBLE */
			 (void) va_arg (args, double);
		       len += extra_long ? 128 : 64;
		       done = 1;
		       break;
		     case 'c':
		       (void) va_arg (args, int);
		       len += 1;
		       done = 1;
		       break;
		     case 'p':
		     case 'n':
		       (void) va_arg (args, void*);
		       len += 32;
		       done = 1;
		       break;
		     case '%':
		       len += 1;
		       done = 1;
		       break;
		     default:
		       /* ignore unknow/invalid flags */
		       break;
		    }
	       }
	  }
	else
	  len += 1;
     }
   
   return len;
}

/*}}}*/
