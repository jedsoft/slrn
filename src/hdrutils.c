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

#include "group.h"
#include "art.h"
#include "strutil.h"
#include "hdrutils.h"

Slrn_Article_Line_Type *slrn_find_header_line (Slrn_Article_Type *a, char *header)
{
   Slrn_Article_Line_Type *line;
   unsigned char ch = (unsigned char) UPPER_CASE(*header);
   unsigned int len = strlen (header);

   if (a == NULL)
     return NULL;
   line = a->lines;

   while ((line != NULL) && (line->flags & HEADER_LINE))
     {
	unsigned char ch1 = (unsigned char) *line->buf;
	if ((ch == UPPER_CASE(ch1))
	    && (0 == slrn_case_strncmp (header, line->buf, len)))
	  return line;
	line = line->next;
     }
   return NULL;
}

/* Here it is assumed that buf is already allocated.  This routine steals
 * it, unless an error occurs, in which case it will return NULL.
 * If buf is NULL, then the header separator line will be added if it is not
 * already present.
 */

Slrn_Article_Line_Type *slrn_append_to_header (Slrn_Article_Type *a, char *buf, int free_on_error)
{
   Slrn_Article_Line_Type *hline, *bline;
   Slrn_Article_Line_Type *line;
   unsigned int flags = HEADER_LINE;

   hline = a->lines;
   bline = NULL;
   if (hline != NULL)
     {
	while ((hline != NULL)
	       && (hline->flags & HEADER_LINE)
	       && (NULL != (bline = hline->next))
	       && (bline->flags & HEADER_LINE))
	  hline = bline;
     }

   if (buf == NULL)
     {
	/* header separator */
	if ((bline != NULL) && (bline->buf[0] == 0))
	  return a->cline = bline;

	if (NULL == (buf = slrn_strmalloc ("", 1)))
	  return NULL;

	flags = 0;
     }

   line = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
   if (line == NULL)
     {
	if (free_on_error)
	  slrn_free (buf);
	return NULL;
     }

   line->flags = flags;
   line->buf = buf;
   if (hline == NULL)
     a->lines = line;
   else
     hline->next = line;

   line->prev = hline;
   line->next = bline;

   if (bline != NULL)
     bline->prev = line;

   return a->cline = line;
}

Slrn_Article_Line_Type *slrn_append_header_keyval (Slrn_Article_Type *a, char *key, char *value)
{
   unsigned int buflen;
   char *buf;

   buflen = strlen (key) + strlen(value) + 3;
   if (NULL == (buf = slrn_malloc (buflen, 0, 1)))
     return NULL;

   (void) SLsnprintf (buf, buflen, "%s: %s", key, value);

   return slrn_append_to_header (a, buf, 1);
}

