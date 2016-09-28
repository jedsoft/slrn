/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999, 2007-2016 John E. Davis <jed@jedsoft.org>
 Copyright (c) 2002-2006 Thomas Schultz <tststs@gmx.de>
 Copyright (c) 2007 John E. Davis <jed@jedsoft.org>

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
#include <errno.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#include <slang.h>
#include "jdmacros.h"
#include "strutil.h"
#include "common.h"

/* This function allows NULL as a parameter. This fact _is_ exploited */
char *slrn_skip_whitespace (char *b) /*{{{*/
{
   if (b == NULL) return NULL;

   while (isspace (*b))
     b++;

   return b;
}

/*}}}*/

char *slrn_bskip_whitespace (char *smin)
{
   char *s;

   if (smin == NULL) return NULL;
   s = smin + strlen (smin);

   while (s > smin)
     {
	s--;
	if (0 == isspace(*s))
	  return s + 1;
     }
   return s;
}

/* returns a pointer to the end of the string */
char *slrn_trim_string (char *s) /*{{{*/
{
   s = slrn_bskip_whitespace (s);
   if (s != NULL)
     *s = 0;

   return s;
}
/*}}}*/

char *slrn_strbyte (char *s, char ch) /*{{{*/
{
   register char ch1;

   while (((ch1 = *s) != 0) && (ch != ch1)) s++;
   if (ch1 == 0) return NULL;
   return s;
}

/*}}}*/

/* Search for characters from list in string str.  If found, return a pointer
 * to the first occurrence.  If not found, return NULL. */
char *slrn_strbrk (char *str, char *list) /*{{{*/
{
   char ch, ch1, *p;

   while ((ch = *str) != 0)
     {
	p = list;
	while ((ch1 = *p) != 0)
	  {
	     if (ch == ch1) return str;
	     p++;
	  }
	str++;
     }
   return NULL;
}

/*}}}*/

char *slrn_simple_strtok (char *s, char *chp) /*{{{*/
{
   static char *s1;
   char ch = *chp;

   if (s == NULL)
     {
	if (s1 == NULL) return NULL;
	s = s1;
     }
   else s1 = s;

   while (*s1 && (*s1 != ch)) s1++;

   if (*s1 == 0)
     {
	s1 = NULL;
     }
   else *s1++ = 0;
   return s;
}

/*}}}*/

int slrn_case_strncmp (char *a, char *b, unsigned int n) /*{{{*/
{
   char *bmax;

   if (a == NULL)
     {
	if (b == NULL)
	  return 0;
	else
	  return -1;
     }
   if (b == NULL)
     return 1;

   if (Slrn_UTF8_Mode)
     return SLutf8_compare((SLuchar_Type *)a, (SLuchar_Type *)a+strlen(a),
			   (SLuchar_Type *)b, (SLuchar_Type *)b+strlen(b),
			   n, 0);
   bmax = b + n;
   while (b < bmax)
     {
	unsigned char cha = UPPER_CASE(*a);
	unsigned char chb = UPPER_CASE(*b);
	if (cha != chb)
	  {
	     return (int) cha - (int) chb;
	  }
	else if (chb == 0) return 0;
	b++;
	a++;
     }
   return 0;
}

/*}}}*/

int slrn_case_strcmp (char *a, char *b) /*{{{*/
{
   register unsigned char cha, chb;
   int len_a,len_b;

   if (a == NULL)
     {
	if (b == NULL)
	  return 0;
	else
	  return -1;
     }

   if (b == NULL)
     return 1;

   if (Slrn_UTF8_Mode)
     {
	len_a=strlen(a);
	len_b=strlen(b);

	return SLutf8_compare((SLuchar_Type *)a, (SLuchar_Type *)a+len_a,
			      (SLuchar_Type *)b, (SLuchar_Type *)b+len_b,
			      ((len_a > len_b) ? len_a : len_b), 0);
     }

   while (1)
     {
	cha = UPPER_CASE(*a);
	chb = UPPER_CASE(*b);
	if (cha != chb)
	  {
	     return (int) cha - (int) chb;
	  }
	else if (chb == 0) break;
	b++;
	a++;
     }
   return 0;
}

/*}}}*/

char *slrn_strncpy (char *dest, const char *src, size_t n) /*{{{*/
{
   strncpy (dest, src, n);
   dest[n-1] = '\0';
   return dest;
}
/*}}}*/

/*{{{ Memory Allocation Routines */

static char *do_malloc_error (int do_error)
{
   if (do_error) slrn_error (_("Memory allocation failure."));
   return NULL;
}

char *slrn_safe_strmalloc (char *s) /*{{{*/
{
   s = SLmake_string (s);
   if (s == NULL) slrn_exit_error (_("Out of memory."));
   return s;
}

/*}}}*/

char *slrn_safe_strnmalloc (char *s, unsigned int len)
{
   s = SLmake_nstring (s, len);
   if (s == NULL) slrn_exit_error (_("Out of memory."));
   return s;
}

char *slrn_strnmalloc (char *s, unsigned int len, int do_error)
{
   s = SLmake_nstring (s, len);

   if (s == NULL)
     return do_malloc_error (do_error);

   return s;
}

char *slrn_strmalloc (char *s, int do_error)
{
   if (s == NULL) return NULL;
   return slrn_strnmalloc (s, strlen (s), do_error);
}

char *slrn_malloc (unsigned int len, int do_memset, int do_error)
{
   char *s;

   s = (char *) SLmalloc (len);
   if (s == NULL)
     return do_malloc_error (do_error);

   if (do_memset)
     memset (s, 0, len);

   return s;
}

char *slrn_realloc (char *s, unsigned int len, int do_error)
{
   if (s == NULL)
     return slrn_malloc (len, 0, do_error);

   s = SLrealloc (s, len);
   if (s == NULL)
     return do_malloc_error (do_error);

   return s;
}

char *slrn_safe_malloc (unsigned int len)
{
   char *s;

   s = slrn_malloc (len, 1, 0);

   if (s == NULL)
     slrn_exit_error (_("Out of memory"));

   return s;
}

void slrn_free (char *s)
{
   if (s != NULL) SLfree (s);
}

/* return a:amax + s + b:bmax */
char *slrn_substrjoin (char *a, char *amax, char *b, char *bmax, char *s)
{
   unsigned int len_a, len_b, len_s, len;
   char *c;

   if (a == NULL) a = amax = "";
   if (b == NULL) b = bmax = "";
   if (s == NULL) s = "";

   len_a = (amax == NULL) ? strlen (a) : (unsigned int)(amax - a);
   len_b = (bmax == NULL) ? strlen (b) : (unsigned int)(bmax - b);

   if ((len_a == 0) || (len_b == 0))
     len_s = 0;
   else
     len_s = strlen (s);

   len = len_a + len_s + len_b;
   c = slrn_malloc (len+1, 0, 1);
   if (c == NULL)
     return NULL;

   strncpy (c, a, len_a);
   if (len_s != 0)
     strcpy (c+len_a, s);
   strncpy (c+len_a+len_s, b, len_b);

   c[len] = 0;
   return c;
}

/* return a + s + b */
char *slrn_strjoin (char *a, char *b, char *s)
{
   return slrn_substrjoin (a, NULL, b, NULL, s);
}

