/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.

 Copyright (c) 2003-2006 Thomas Schultz <tststs@gmx.de>

 partly based on code by John E. Davis:
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

/*{{{ include files */
#include "config.h"
#include "slrnfeat.h"

#include <string.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <slang.h>
#include "ranges.h"
#include "util.h"
#include "strutil.h"
/*}}}*/

/* line is expected to contain newsrc-style integer ranges. They are put
 * into a range list which is returned.
 */
Slrn_Range_Type *slrn_ranges_from_newsrc_line (char *line) /*{{{*/
{
   Slrn_Range_Type *retval=NULL;

   while (1)
     {
	int min, max;
	char ch;
	/* skip white space and delimiters */
	while (((ch = *line) != 0) && ((ch <= ' ') || (ch == ','))) line++;
	if ((ch < '0') || (ch > '9')) break;
	min = atoi (line++);
	while (((ch = *line) != 0) && (ch >= '0') && (ch <= '9')) line++;
	if (ch == '-')
	  {
	     line++;
	     max = atoi (line);
	     while (((ch = *line) != 0) && (ch >= '0') && (ch <= '9')) line++;
	  }
	else max = min;

	retval = slrn_ranges_add (retval, min, max);
     }

   return retval;
}
/*}}}*/

/* Writes the range list r to the file fp in newsrc format.
 * If max is non-zero, no numbers larger than max are written.
 * Returns 0 on success, -1 otherwise.
 */
int slrn_ranges_to_newsrc_file (Slrn_Range_Type *r, NNTP_Artnum_Type max, FILE* fp) /*{{{*/
{
   while ((r != NULL) && ((max<=0) || (r->min <= max)))
     {
	NNTP_Artnum_Type minmax = r->max;
	if ((max>0) && (minmax > max))
	  minmax = max;

	if (r->min != minmax)
	  {
	     if (fprintf (fp, NNTP_FMT_ARTRANGE, r->min, minmax) < 0)
	       return -1;
	  }
	else if (fprintf (fp, NNTP_FMT_ARTNUM, r->min) < 0)
	  return -1;

	r = r->next;
	if ((r != NULL) && (EOF == putc (',', fp)))
	  return -1;
     }
   return 0;
}
/*}}}*/

/* Adds the range [min, max] to the given ranges structure r
 * Note: does not allocate a new list, but changes r; the function still
 * returns a pointer in case the first element of the list changes.
 * (r==NULL) is allowed; in this case, a new list is created
 */
Slrn_Range_Type *slrn_ranges_add (Slrn_Range_Type *r, NNTP_Artnum_Type min, NNTP_Artnum_Type max) /*{{{*/
{
   Slrn_Range_Type *head = r;

   if (min>max) return head;

   /* Do we need to insert at the beginning of the list? */
   if ((r==NULL) || (max+1 < r->min))
     {
	head = (Slrn_Range_Type *) slrn_safe_malloc (sizeof(Slrn_Range_Type));
	head->min = min;
	head->max = max;
	head->next = r;
	head->prev = NULL;
	if (r!=NULL)
	  r->prev = head;

	return head;
     }

   /* Skip ranges below min */
   while ((r->next!=NULL) && (r->max+1 < min))
     r = r->next;

   /* Do we need to append a new range? */
   if (min > r->max+1)
     {
	Slrn_Range_Type *n;
	n = (Slrn_Range_Type *) slrn_safe_malloc (sizeof(Slrn_Range_Type));
	n->min = min;
	n->max = max;
	n->next = NULL;
	n->prev = r;
	r->next = n;

	return head;
     }

   /* Do we need to insert a new range? */
   if (max+1 < r->min)
     {
        Slrn_Range_Type *n;
        n = (Slrn_Range_Type *) slrn_safe_malloc (sizeof(Slrn_Range_Type));
        n->min = min;
        n->max = max;
        n->next = r;
        n->prev = r->prev;
        n->prev->next = n;
        r->prev = n;

        return head;
     }

   /* Update min / max values */
   if (min < r->min)
     r->min = min;
   if (max > r->max)
     r->max = max;

   /* Clean up successive ranges */
   while ((r->next != NULL) &&
	  (r->next->min <= r->max+1))
     {
	Slrn_Range_Type *next = r->next;
	if (next->max > r->max)
	  r->max = next->max;

	r->next = next->next;
	if (r->next != NULL)
	  r->next->prev = r;
	SLFREE (next);
     }

   return head;
}
/*}}}*/

/* Removes the range [min, max] from the given ranges structure r
 * Note: does not allocate a new list, but changes r; the function still
 * returns a pointer in case the first element of the list gets deleted.
 */
Slrn_Range_Type *slrn_ranges_remove (Slrn_Range_Type *r, NNTP_Artnum_Type min,  NNTP_Artnum_Type max) /*{{{*/
{
   Slrn_Range_Type *head = r;

   if ((min>max) || (r==NULL))
     return head;

   /* Skip ranges below min */
   while ((r->next != NULL) && (r->max < min))
     r = r->next;

   /* Do we need to split the current range? */
   if ((r->min < min) && (r->max > max))
     {
	Slrn_Range_Type *n;
	n = (Slrn_Range_Type *) slrn_safe_malloc (sizeof(Slrn_Range_Type));
	n->min = max+1;
	n->max = r->max;
	r->max = min-1;
	n->next = r->next;
	if (n->next != NULL)
	  n->next->prev = n;
	n->prev = r;
	r->next = n;

	return head;
     }

   /* Change or delete successive nodes */
   while (r->next != NULL)
     {
	if (r->next->max <= max)
	  {
	     Slrn_Range_Type *next = r->next;
	     if (next->next != NULL)
	       next->next->prev = r;
	     r->next = next->next;

	     SLFREE (next);
	  }
	else
	  {
	     if (r->next->min <= max)
	       r->next->min = max+1;
	     break;
	  }
     }

   if ((min <= r->max) && (max >= r->min))
     {
	if (max < r->max) /* Change this node */
	  r->min = max+1;
	else if (min > r->min)
	  r->max = min-1;
	else /* Delete this node */
	  {
	     Slrn_Range_Type *next = r->next;
	     if (next != NULL)
	       next->prev = r->prev;
	     if (r->prev != NULL)
	       r->prev->next = next;
	     else
	       head = next;

	     SLFREE (r);
	  }
     }

   return head;
}
/*}}}*/

/* Merges two range lists a and b and returns the result
 * Note: does not allocate a new list, but changes a
 */
Slrn_Range_Type *slrn_ranges_merge (Slrn_Range_Type *a, Slrn_Range_Type *b) /*{{{*/
{
   Slrn_Range_Type *retval = a;

   while (b != NULL)
     {
	retval = slrn_ranges_add (retval, b->min, b->max);
	b = b->next;
     }

   return retval;
}
/*}}}*/

/* Creates a new range list that contains only the numbers that are
 * both in a and b and returns it; a and b remain untouched.
 */
Slrn_Range_Type *slrn_ranges_intersect (Slrn_Range_Type *a, Slrn_Range_Type *b) /*{{{*/
{
   Slrn_Range_Type *retval=NULL, *r=NULL, *n;
   do
     {
	/* skip ranges that don't intersect at all */
	do
	  {
	     if (b != NULL)
	       while ((a != NULL) && (a->max < b->min))
		 a = a->next;

	     if (a != NULL)
	       while ((b != NULL) && (b->max < a->min))
		 b = b->next;
	  }
	while ((a!=NULL) && (b!=NULL) && (a->max < b->min));

	/* append a range containing the next intersection */
	if ((a!=NULL) && (b!=NULL))
	  {
	     n = (Slrn_Range_Type *) slrn_safe_malloc (sizeof(Slrn_Range_Type));
	     n->next = NULL;
	     if (retval==NULL)
	       {
		  n->prev = NULL;
		  r = retval = n;
	       }
	     else
	       {
		  n->prev = r;
		  r->next = n;
		  r = n;
	       }
	     n->min = (a->min < b->min) ? b->min : a->min;
	     n->max = (a->max > b->max) ? b->max : a->max;

	     if (n->max == a->max)
	       a = a->next;
	     else
	       b = b->next;
	  }
     }
   while ((a!=NULL) && (b!=NULL));
   return retval;
}
/*}}}*/

/* Checks if n is in r; returns 1 if true, 0 if false. */
int slrn_ranges_is_member (Slrn_Range_Type *r, NNTP_Artnum_Type n) /*{{{*/
{
   while (r != NULL)
     {
	if ((r->min <= n) && (r->max >= n))
	  return 1;
	if (n <= r->max)
	  return 0;
	r = r->next;
     }
   return 0;
}
/*}}}*/

/* Makes a copy of the range list r and returns it (exits if out of memory!) */
Slrn_Range_Type *slrn_ranges_clone (Slrn_Range_Type *r) /*{{{*/
{
   Slrn_Range_Type *retval=NULL, *c, *n;

   if (r==NULL) return NULL;

   retval = (Slrn_Range_Type *) slrn_safe_malloc (sizeof(Slrn_Range_Type));
   retval->prev = NULL;
   c = retval;

   while (r != NULL)
     {
	c->min = r->min;
	c->max = r->max;
	r = r->next;
	if (r != NULL)
	  {
	     n = (Slrn_Range_Type *) slrn_safe_malloc (sizeof(Slrn_Range_Type));
	     c->next = n;
	     n->prev = c;
	     c = n;
	  }
     }
   c->next = NULL;

   return retval;
}
/*}}}*/

/* Compares two range lists and returns 0 if they are equal */
int slrn_ranges_compare (Slrn_Range_Type *a, Slrn_Range_Type *b) /*{{{*/
{
   while ((a != NULL) && (b != NULL)
	  && (a->min == b->min)
	  && (a->max == b->max))
     {
	a = a->next;
	b = b->next;
     }

   return ((a == NULL) && (b == NULL)) ? 0 : 1;
}
/*}}}*/

/* Frees the memory used by the range list r */
void slrn_ranges_free (Slrn_Range_Type *r) /*{{{*/
{
   Slrn_Range_Type *next;

   while (r != NULL)
     {
	next = r->next;
	SLFREE (r);
	r = next;
     }
}
/*}}}*/
