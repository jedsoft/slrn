/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.
 It contains the sorting routines for article mode.

 Copyright (c) 1994, 1999, 2007-2016 John E. Davis <jed@jedsoft.org>
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
#include "config.h"
#include "slrnfeat.h"

/*{{{ system include files */
#include <stdio.h>
#include <string.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <slang.h>
/*}}}*/
/*{{{ slrn include files */
#include "jdmacros.h"
#include "slrn.h"
#include "group.h"
#include "art.h"
#include "art_sort.h"
#include "misc.h"
#include "menu.h"
#include "hash.h"
#include "util.h"
#include "strutil.h"
#include "snprintf.h"
#include "hooks.h"
#include "common.h"

/*}}}*/

/*{{{ extern global variables  */
int _art_Headers_Threaded = 0;
int Slrn_New_Subject_Breaks_Threads = 0;
int Slrn_Uncollapse_Threads = 0;

/* Sorting mode:
 *   0 No sorting
 *   1 sort by threads
 *   2 sort by subject
 *   3 sort by subject and threads
 *   4 sort by score
 *   5 sort by score and threads
 *   6 sort by score then by subject
 *   7 thread, then sort by score then subject
 *   8 sort by date
 */
#define SORT_BY_THREADS  1
#define SORT_BY_SUBJECT  2
#define SORT_BY_SCORE   4
#define SORT_BY_DATE	8
#define SORT_CUSTOM     12

int Slrn_Sorting_Mode = (SORT_BY_THREADS|SORT_BY_SUBJECT);

/* Custom sorting: */
int Slrn_Sort_By_Threads = 0;
char *Slrn_Sort_Order=NULL;

/*}}}*/
/*{{{ static global variables */
typedef int (*Header_Cmp_Func_Type)(Slrn_Header_Type *, Slrn_Header_Type *);

typedef struct sort_function_type
{
   Header_Cmp_Func_Type fun;
   int inverse;
   struct sort_function_type *next;
}
sort_function_type;

static sort_function_type *Sort_Functions=NULL, *Sort_Thread_Functions=NULL;

#define ALL_THREAD_FLAGS (FAKE_PARENT | FAKE_CHILDREN | FAKE_HEADER_HIGH_SCORE)
/*}}}*/

/*{{{ static function declarations */
static char *get_current_sort_order (int*);
static void recompile_sortorder (char*);
static void sort_by_threads (void);
static int header_cmp (sort_function_type *sort_function, Slrn_Header_Type **unsorted, Slrn_Header_Type **sorted);
static int header_initial_cmp(Slrn_Header_Type **unsorted, Slrn_Header_Type **sorted);
static int header_thread_cmp(Slrn_Header_Type **unsorted, Slrn_Header_Type **sorted);

/*}}}*/

void _art_toggle_sort (void) /*{{{*/
{
   int rsp;

   rsp = slrn_sbox_sorting_method ();
   if (rsp != -1)
     {
	Slrn_Sorting_Mode = rsp;
	slrn_sort_headers ();
     }
}
/*}}}*/

/*{{{ Functions that compare headers */

/* Helper function to compare two subjects; will be used for sorting and
 * when linking lost relatives later on */
int _art_subject_cmp (char *sa, char *sb) /*{{{*/
{
   char *end_a = sa + strlen (sa);
   char *end_b = sb + strlen (sb);
   char *was;

   /* find "(was: ...)" and set end_[ab] */
   if (NULL != (was = strstr (sa, "(was:"))
       && (was != sa) && (*(end_a - 1) == ')'))
     end_a = was;

   if (NULL != (was = strstr (sb, "(was:"))
       && (was != sb) && (*(end_b - 1) == ')'))
     end_b = was;

   /* skip past re: */
   while (*sa == ' ') sa++;
   while (*sb == ' ') sb++;

   if (((*sa | 0x20) == 'r') && ((*(sa + 1) | 0x20) == 'e')
       && (*(sa + 2) == ':'))
     {
	sa += 3;
     }

   if (((*sb | 0x20) == 'r') && ((*(sb + 1) | 0x20) == 'e')
       && (*(sb + 2) == ':'))
     {
	sb += 3;
     }

   while (1)
     {
	char cha, chb;

	while ((cha = *sa) == ' ') sa++;
	while ((chb = *sb) == ' ') sb++;

	if ((sa == end_a) && (sb == end_b))
	  return 0;

	/* This hack sorts "(3/31)" before "(25/31)" */
	if (isdigit (cha) && isdigit (chb))
	  {
	     int a = atoi (sa), b = atoi (sb);
	     if (a != b)
	       return a - b;
	  }

	cha = UPPER_CASE(cha);
	chb = UPPER_CASE(chb);

	if (cha != chb)
	  return (int) cha - (int) chb;

	sa++;
	sb++;
     }
}
/*}}}*/

/* Now the functions for the various sorting criteria.
 * Default sorting order:
 * header_subject_cmp	=> Subjects alphabetically a-z, case insensitive
 * header_date_cmp	=> Oldest articles first
 * header_has_body_cmp  => Articles without body first
 * header_highscore_cmp	=> Articles without high scores first
 * header_score_cmp	=> Lowest scores first
 * header_author_cmp	=> Realnames alphabetically A-z
 * header_num_cmp	=> Lowest server numbers first
 * header_lines_cmp	=> Lowest number of lines first
 * header_msgid_cmp	=> Message-Ids alphabetically A-z
 */

static int header_subject_cmp (Slrn_Header_Type *a, Slrn_Header_Type *b) /*{{{*/
{
   return _art_subject_cmp (a->subject, b->subject);
}
/*}}}*/

static int header_date_cmp (Slrn_Header_Type *a, Slrn_Header_Type *b) /*{{{*/
{
   long asec, bsec;

   asec = slrn_date_to_order_parm (a->date);
   bsec = slrn_date_to_order_parm (b->date);

   return (int) (asec - bsec);
}
/*}}}*/

static int header_has_body_cmp (Slrn_Header_Type *a, Slrn_Header_Type *b) /*{{{*/
{
   int abody, bbody;

   abody = (0 != (a->flags & HEADER_WITHOUT_BODY));
   bbody = (0 != (b->flags & HEADER_WITHOUT_BODY));

   return bbody - abody;
}
/*}}}*/

static int header_highscore_cmp (Slrn_Header_Type *a, Slrn_Header_Type *b) /*{{{*/
{
   int ahigh, bhigh;

   ahigh = (0 != (a->flags & (HEADER_HIGH_SCORE | FAKE_HEADER_HIGH_SCORE)));
   bhigh = (0 != (b->flags & (HEADER_HIGH_SCORE | FAKE_HEADER_HIGH_SCORE)));

   return ahigh - bhigh;
}
/*}}}*/

static int header_score_cmp (Slrn_Header_Type *a, Slrn_Header_Type *b) /*{{{*/
{
   return a->thread_score - b->thread_score;
}
/*}}}*/

static int header_author_cmp (Slrn_Header_Type *a, Slrn_Header_Type *b) /*{{{*/
{
   return strcmp(a->realname, b->realname);
}
/*}}}*/

static int header_num_cmp (Slrn_Header_Type *a, Slrn_Header_Type *b) /*{{{*/
{
   return a->number - b->number;
}
/*}}}*/

static int header_lines_cmp (Slrn_Header_Type *a, Slrn_Header_Type *b) /*{{{*/
{
   return a->lines - b->lines;
}
/*}}}*/

static int header_msgid_cmp (Slrn_Header_Type *a, Slrn_Header_Type *b) /*{{{*/
{
   return strcmp(a->msgid, b->msgid);
}
/*}}}*/

/* This functions looks at the user-defined Sort_Functions and calls the
 * appropriate functions above to compare the headers.
 */
static int header_cmp (sort_function_type *sort_function, Slrn_Header_Type **unsorted, Slrn_Header_Type **sorted) /*{{{*/
{
   int result=0;

   while (sort_function != NULL)
     {
        result = (sort_function->fun)(*unsorted, *sorted);
        if (result) break;
        sort_function = sort_function->next;
     }

   if (sort_function != NULL && sort_function->inverse) result *= -1;

   return result;
}
/*}}}*/

static int header_initial_cmp (Slrn_Header_Type **unsorted, Slrn_Header_Type **sorted) /*{{{*/
{
    return header_cmp(Sort_Functions, unsorted, sorted);
}
/*}}}*/

static int header_thread_cmp (Slrn_Header_Type **unsorted, Slrn_Header_Type **sorted) /*{{{*/
{
    return header_cmp(Sort_Thread_Functions, unsorted, sorted);
}
/*}}}*/

/*{{{ Functions that do the actual sorting */

/* This function is called to sort the headers in article mode. */
void slrn_sort_headers (void) /*{{{*/
{
   Slrn_Header_Type **header_list, *h;
   unsigned int i, nheaders;
   static char *prev_sort_order = NULL;
   char *sort_order;
   int do_threading;
   void (*qsort_fun) (char *, unsigned int,
		      unsigned int, int (*)(Slrn_Header_Type **, Slrn_Header_Type **));

   /* This is a silly hack to make up for braindead compilers and the lack of
    * uniformity in prototypes for qsort.
    */
   qsort_fun = (void (*)(char *, unsigned int,
			 unsigned int, int (*)(Slrn_Header_Type **, Slrn_Header_Type **)))
     qsort;

   /* Maybe we must (re-)compile Sort_Functions first */
   sort_order = get_current_sort_order (&do_threading);
   if ((sort_order != NULL) &&
       ((prev_sort_order == NULL) ||
	(strcmp (prev_sort_order, sort_order))))
     {
	slrn_free (prev_sort_order);
	prev_sort_order = slrn_safe_strmalloc (sort_order);
	recompile_sortorder (sort_order);
     }

   /* Pre-sort by thread / server number */
   if (do_threading)
     sort_by_threads ();
   else
     _art_sort_by_server_number();

   /* sort_by_threads already did the sorting inside of the threads; what's
    * left to us is the sorting of headers that don't have parents.
    * First, count their number and get memory for an array of them. */
   nheaders = 0;
   h = Slrn_First_Header;
   while (h != NULL)
     {
	if (h->parent == NULL) nheaders++;
	h = h->real_next;
     }
   if (nheaders < 2)
     goto cleanup_screen_and_return;

   if (NULL == (header_list = (Slrn_Header_Type **) SLCALLOC (sizeof (Slrn_Header_Type *), nheaders + 1)))
     {
	slrn_error (_("slrn_sort_headers(): memory allocation failure."));
	goto cleanup_screen_and_return;
     }

   /* Now, fill the array and call qsort on it; use our header_initial_cmp function
    * to do the comparison. */
   h = Slrn_First_Header;
   nheaders = 0;
   while (h != NULL)
     {
	if (h->parent == NULL)
	  header_list[nheaders++] = h;
	h = h->real_next;
     }
   header_list[nheaders] = NULL;

   (*qsort_fun) ((char *) header_list, nheaders, sizeof (Slrn_Header_Type *), header_initial_cmp);

   /* What we do now depends on the threading state. If the headers are
    * unthreaded, simply link them in the order returned by qsort. */
   if (!do_threading)
     {
	header_list[0]->next = header_list[1];
	header_list[0]->prev = NULL;

	for (i = 1; i < nheaders; i++)
	  {
	     h = header_list[i];
	     h->next = header_list[i + 1];
	     h->prev = header_list[i - 1];
	  }
     }
   else
     {
	/* If the headers are threaded, we have sorted parents. Always link
	 * them with the last child of the previous thread. */
	h = NULL;
	for (i = 0; i <= nheaders; i++)
	  {
	     Slrn_Header_Type *h1 = header_list[i];
	     if (h != NULL)
	       {
		  h->sister = h1;
		  while (h->child != NULL)
		    {
		       h = h->child;
		       while (h->sister != NULL) h = h->sister;
		    }
		  h->next = h1;
	       }
	     if (h1 != NULL) h1->prev = h;
	     h = h1;
	  }
     }

   /* Finally, free the array and clean up the screen. */
   _art_Headers = header_list[0];
   SLFREE (header_list);

   cleanup_screen_and_return:
   _art_find_header_line_num ();
   Slrn_Full_Screen_Update = 1;

   if (!Slrn_Uncollapse_Threads)
     slrn_collapse_threads (1);
}
/*}}}*/

/* Sort headers by server number, un-threading them if necessary.
 * Note: This function does *not* sync the screen. */
void _art_sort_by_server_number (void) /*{{{*/
{
   Slrn_Header_Type *h;

   /* This is easy since the real_next, prev are already ordered. */
   h = Slrn_First_Header;
   while (h != NULL)
     {
	h->next = h->real_next;
	h->prev = h->real_prev;
	h->num_children = 0;
	h->flags &= ~(HEADER_HIDDEN | ALL_THREAD_FLAGS);
	h->sister = h->parent = h->child = NULL;
	h->thread_score = h->score;
	h = h->real_next;
     }
   _art_Headers_Threaded = 0;

   /* Now find out where to put the Slrn_Headers pointer */
   while (_art_Headers->prev != NULL) _art_Headers = _art_Headers->prev;
}
/*}}}*/

/*{{{ Sorting by thread */

/* Insertion of children found while linking lost relatives / same subjects */
static void insert_fake_child (Slrn_Header_Type *parent, Slrn_Header_Type *new_child) /*{{{*/
{
   Slrn_Header_Type *child = parent->child, *last_child = NULL;

   /* Order: "normal" children first, so skip them; also skip all faked
    * children that sort higher than new_child */
   while ((child != NULL) &&
	  (((child->flags & FAKE_PARENT) == 0) ||
	   (header_thread_cmp (&new_child, &child) >= 0)))
     {
	last_child = child;
	child = child->sister;
     }

   /* Now, insert new_child */
   if (last_child == NULL)
     parent->child = new_child;
   else
     last_child->sister = new_child;
   new_child->sister = child;
   new_child->parent = parent;
   parent->flags |= FAKE_CHILDREN;
   new_child->flags |= FAKE_PARENT;
}
/*}}}*/

typedef struct /*{{{*/
{
   unsigned long ref_hash;
   Slrn_Header_Type *h;
}
/*}}}*/
Relative_Type;

/* Find articles whose References header starts with the same Message-Id
 * and put them into the same thread. */
static void link_lost_relatives (void) /*{{{*/
{
   unsigned int n, i, j;
   Slrn_Header_Type *h;
   Relative_Type *relatives;

   /* Count the number of possible lost relatives */
   n = 0;
   h = Slrn_First_Header;
   while (h != NULL)
     {
	if ((h->parent == NULL)
	    && (h->refs != NULL)
	    && (*h->refs != 0)) n++;

	h = h->real_next;
     }
   if (n < 2) return;

   /* Allocate an array for them and fill it, remembering a hash value of the
    * first Message-ID in the References header line. */
   relatives = (Relative_Type *) slrn_malloc (sizeof (Relative_Type) * n, 0, 0);
   if (relatives == NULL)
     return;

   n = 0;
   h = Slrn_First_Header;
   while (h != NULL)
     {
	if ((h->parent == NULL)
	    && (h->refs != NULL)
	    && (*h->refs != 0))
	  {
	     unsigned char *r, *ref_begin;

	     r = (unsigned char *) h->refs;
	     while (*r && (*r != '<')) r++;
	     if (*r)
	       {
		  ref_begin = r;
		  while (*r && (*r != '>')) r++;
		  if (*r) r++;
		  relatives[n].ref_hash = slrn_compute_hash (ref_begin, r);
		  relatives[n].h = h;
		  n++;
	       }
	  }
	h = h->real_next;
     }

   /* Walk through the array and mark all headers with the same hash value
    * as relatives. */
   for (i = 0; i < n; i++)
     {
	unsigned long ref_hash;
	Relative_Type *ri = relatives + i;
	Slrn_Header_Type *rih;

	ref_hash = ri->ref_hash;
	rih = ri->h;

	for (j = i + 1; j < n; j++)
	  {
	     if (relatives[j].ref_hash == ref_hash)
	       {
		  Slrn_Header_Type *rjh = relatives[j].h;

		  if ((Slrn_New_Subject_Breaks_Threads & 1)
		      && (rih->subject != NULL)
		      && (rjh->subject != NULL)
		      && (0 != _art_subject_cmp (rih->subject, rjh->subject)))
		    continue;

		  if (rih->parent != NULL)
		    {
		       /* rih has a parent, so make rjh also a faked child of rih->parent */
		       insert_fake_child (rih->parent, rjh);
		    }
		  else /* make rjh a faked child of rih */
		    {
		       insert_fake_child (rih, rjh);
		    }
		  break;
	       } /* if (found same hash value) */
	  }
     }
   SLFREE (relatives);
}
/*}}}*/

static int qsort_subject_cmp (Slrn_Header_Type **a, Slrn_Header_Type **b) /*{{{*/
{
   return _art_subject_cmp ((*a)->subject, (*b)->subject);
}
/*}}}*/

/* Find articles with the same subject and put them into the same thread. */
static void link_same_subjects (void) /*{{{*/
{
   Slrn_Header_Type **header_list, *h;
   unsigned int i, nparents;
   int use_hook = 0;
   void (*qsort_fun) (char *, unsigned int, unsigned int, int (*)(Slrn_Header_Type **, Slrn_Header_Type **));

   /* This is a silly hack to make up for braindead compilers and the lack of
    * uniformity in prototypes for qsort.
    */
   qsort_fun = (void (*)(char *,
			 unsigned int, unsigned int,
			 int (*)(Slrn_Header_Type **, Slrn_Header_Type **)))
     qsort;

   /* Count number of threads we might want to link. */
   h = Slrn_First_Header;
   nparents = 0;
   while (h != NULL)
     {
	if (h->parent == NULL)
	  nparents++;
	h = h->real_next;
     }
   if (nparents < 2) return;

   /* Allocate an array for them, fill and qsort() it. */
   if (NULL == (header_list = (Slrn_Header_Type **) SLCALLOC (sizeof (Slrn_Header_Type *), nparents)))
     {
	slrn_error (_("link_same_subjects: memory allocation failure."));
	return;
     }

   h = Slrn_First_Header;
   i = 0;
   while (i < nparents)
     {
	if (h->parent == NULL) header_list[i++] = h;
	h = h->real_next;
     }

   (*qsort_fun) ((char *) header_list,
		 nparents, sizeof (Slrn_Header_Type *), qsort_subject_cmp);

   if (0 != slrn_is_hook_defined(HOOK_SUBJECT_COMPARE))
     use_hook = 1;

   h = header_list[0];
   for (i = 1; i < nparents; i++)
     {
	Slrn_Header_Type *h1 = header_list[i];
	int differ;

	differ = _art_subject_cmp (h->subject, h1->subject);

	if (differ && use_hook)
	  {
	     int rslt;

	     if ((1 == slrn_run_hooks (HOOK_SUBJECT_COMPARE, 2, h->subject, h1->subject))
		 && (-1 != SLang_pop_integer (&rslt)))
	       differ = rslt;
	  }

	if (differ == 0)
	  {
	     /* h and h1 have the same subject. Now make h1 a (faked) child of h. */
	     insert_fake_child (h, h1);

	     if (h1->flags & FAKE_CHILDREN)
	       {
		  /* h1 has fake children, we have to link them up to the new
		   * parent.  That is, h1 will become their sister.  So,
		   * extract the adopted children of h1 and make them the sister,
		   */
		  Slrn_Header_Type *child = h1->child, *last_child;
		  last_child = child;

		  /* child CANNOT be NULL here!! (the parent claims to have
		   *				  children) */
		  child = child->sister;
		  while ((child != NULL) && ((child->flags & FAKE_PARENT) == 0))
		    {
		       last_child = child;
		       child = child->sister;
		    }

		  if (last_child->flags & FAKE_PARENT) /* h1 has only fake children */
		    {
		       child = last_child;
		       h1->child = NULL;
		    }
		  else
		    last_child->sister = NULL;

		  last_child = child;
		  while (last_child != NULL)
		    {
		       child = last_child->sister;
		       insert_fake_child (h, last_child);
		       last_child = child;
		    }
		  h1->flags &= ~FAKE_CHILDREN;
	       } /* if (h1 had faked children) */
	  } /* if (found same subject) */
	else h = h1;
     } /* traversing the array */
   SLFREE (header_list);
}
/*}}}*/

/* Returns the number of children for h and sets the field num_children for
 * all of them. */
static unsigned int compute_num_children (Slrn_Header_Type *h) /*{{{*/
{
   unsigned int n = 0, dn;

   h = h->child;
   while (h != NULL)
     {
	n++;
	if (h->child == NULL) dn = 0;
	else
	  {
	     dn = compute_num_children (h);
	     n += dn;
	  }
	h->num_children = dn;
	h = h->sister;
     }
   return n;
}
/*}}}*/

/* Sets the next/prev pointers and draws the tree. */
static Slrn_Header_Type *fixup_thread_node (Slrn_Header_Type *h, char *tree) /*{{{*/
{
   Slrn_Header_Type *last = NULL;
   static unsigned int level;
   unsigned char vline_char;

   if (h == NULL) return NULL;

   vline_char = Graphic_VLine_Char;

   while (1)
     {
	last = h;

	if (h->child != NULL)
	  {
	     Slrn_Header_Type *child = h->child;
	     unsigned int tree_level;
	     unsigned int save_level = level;

	     h->next = child;
	     child->prev = h;

	     if (level == 0)
	       {
		  if ((h->flags & FAKE_CHILDREN) &&
		      ((child->flags & FAKE_PARENT) == 0))
		    level = 1;
	       }
	     else if (h->flags & FAKE_PARENT)
	       {
		  if (h->sister != NULL) tree[0] = vline_char;
		  else tree[0] = ' ';
		  tree[1] = ' ';
		  level = 1;
	       }

	     tree_level = 2 * level - 2;

	     if (level && (tree_level < MAX_TREE_SIZE - 2))
	       {
		  if (h->sister != NULL)
		    {
		       if (((h->sister->flags & FAKE_PARENT) == 0)
			   || (h->flags & FAKE_PARENT))
			 {
			    tree[tree_level] = vline_char;
			 }
		       else tree[tree_level] = ' ';
		    }
		  else
		    {
		       if ((h->parent == NULL) && (h->flags & FAKE_CHILDREN))
			 {
			    tree[tree_level] = vline_char;
			 }
		       else tree[tree_level] = ' ';
		    }
		  tree[tree_level + 1] = ' ';
		  tree[tree_level + 2] = 0;
	       }

	     level++;
	     last = fixup_thread_node (h->child, tree);
	     level--;

	     if (level && ((tree_level < MAX_TREE_SIZE - 2)))
	       tree[tree_level] = 0;

	     level = save_level;
	  }

	if (h->flags & FAKE_PARENT) *tree = 0;

	slrn_free (h->tree_ptr);
	h->tree_ptr = NULL;

	if (*tree)
	  h->tree_ptr = slrn_strmalloc (tree, 0);   /* NULL ok here */

	h = h->sister;
	last->next = h;
	if (h == NULL) break;
	h->prev = last;
     }
   return last;
}
/*}}}*/

/* This function fixes the attributes within all threads (number of children,
 * thread scores) and calls the routine to set next/prev pointers. */
static void fixup_threads (void) /*{{{*/
{
   Slrn_Header_Type *h;
   char tree[MAX_TREE_SIZE];

   /* Set the top of the header window. */
   h = Slrn_First_Header;
   _art_Headers = NULL;

   if (h == NULL) return;
   while ((h != NULL) && (h->parent != NULL))
     h = h->real_next;
   if (h == NULL)
     slrn_exit_error (_("Internal Error in fixup_threads()."));
   else
     _art_Headers = h;

   /* Do the next/prev linking. */
   *tree = 0;
   fixup_thread_node (_art_Headers, tree);
   while (_art_Headers->prev != NULL) _art_Headers = _art_Headers->prev;

   /* Set number of children / thread scores.
    * Thread scores are only calculated for top level parents. */
   h = _art_Headers;
   while (h != NULL)
     {
	if (h->child == NULL) h->num_children = 0;
	else
	  {
	     Slrn_Header_Type *next;
	     h->num_children = compute_num_children (h);
	     next = h->next;
	     while ((next != NULL) && (next->parent != NULL))
	       {
		  if (next->flags & HEADER_HIGH_SCORE)
		    h->flags |= FAKE_HEADER_HIGH_SCORE;
		  if (next->score > h->thread_score)
		    h->thread_score = next->score;
		  next = next->next;
	       }
	  }
	h = h->sister;
     }
}
/*}}}*/

/* This function first does the threading by references, then calls the
 * functions to link lost relatives / same subjects and to fixup the
 * attributes within the threads. */
static void sort_by_threads (void) /*{{{*/
{
   Slrn_Header_Type *h, *ref;
   char *r0, *r1, *rmin;

   /* First, resolve existing threads. */
   h = Slrn_First_Header;
   while (h != NULL)
     {
	h->next = h->prev = h->child = h->parent = h->sister = NULL;
	h->flags &= ~(HEADER_HIDDEN | ALL_THREAD_FLAGS);
	slrn_free (h->tree_ptr);
	h->tree_ptr = NULL;
	h->thread_score = h->score;
	h = h->real_next;
     }

   slrn_message_now (_("Threading by references ..."));
   h = Slrn_First_Header;
   while (h != NULL)
     {
	if (*h->refs == 0)
	  {
	     h = h->real_next;
	     continue;
	  }

	rmin = h->refs;
	r1 = rmin + strlen (rmin);

	/* Try to find an article from the References header */
	while (1)
	  {
	     while ((r1 > rmin) && (*r1 != '>')) r1--;
	     r0 = r1 - 1;
	     while ((r0 >= rmin) && (*r0 != '<')) r0--;
	     if ((r0 < rmin) || (r1 == rmin)) break;

	     ref = _art_find_header_from_msgid (r0, r1 + 1);

	     if (ref != NULL)
	       {
		  Slrn_Header_Type *child, *last_child, *rparent;

		  if ((Slrn_New_Subject_Breaks_Threads & 1)
		      && (h->subject != NULL)
		      && (ref->subject != NULL)
		      && (0 != _art_subject_cmp (h->subject, ref->subject)))
		    break;

		  rparent = ref;
		  while (rparent->parent != NULL) rparent = rparent->parent;
		  if (rparent == h) /* self referencing!!! */
		    {
		       int err = SLang_get_error ();
		       slrn_error (_("Article " NNTP_FMT_ARTNUM " is part of reference loop!"), h->number);
		       if (err == 0)
			 {
			    SLang_set_error (0);
			 }
		    }
		  else
		    {
		       h->parent = ref;
		       child = ref->child;
		       last_child = NULL;
		       /* skip all children that sort higher than this one */
		       while ((child != NULL) && (header_thread_cmp (&h, &child) >= 0))
			 {
			    last_child = child;
			    child = child->sister;
			 }
		       /* insert new child */
		       if (last_child == NULL)
			 ref->child = h;
		       else
			 last_child->sister = h;
		       h->sister = child;
		       break;
		    }
	       }
	     r1 = r0;
	  }
	h = h->real_next;
     }

   /* Now perform a re-arrangement such that those without parents but that
    * share the same reference are placed side-by-side as sisters. */
   slrn_message_now (_("Linking \"lost relatives\" ..."));
   link_lost_relatives ();

   /* Now perform sort on subject to catch those that have fallen through the
    * cracks, i.e., no references */
   if (!(Slrn_New_Subject_Breaks_Threads & 2))
     {
	slrn_message_now (_("Linking articles with identical subjects ..."));
	link_same_subjects ();
     }

   /* Now link up others as sisters */
   h = Slrn_First_Header;
   while ((h != NULL) && (h->parent != NULL))
     {
	h = h->real_next;
     }

   while (h != NULL)
     {
	Slrn_Header_Type *next;
	next = h->real_next;
	while ((next != NULL) && (next->parent != NULL))
	  next = next->real_next;
	h->sister = next;
	h = next;
     }

   _art_Headers_Threaded = 1;
   _art_Threads_Collapsed = 0;
   fixup_threads ();
}
/*}}}*/

/*}}}*/

/*}}}*/

/*{{{ Functions for Sort_Functions */

/* Allocate memory for a new comparing function and append it. */
static void add_sort_function(sort_function_type **Functions, Header_Cmp_Func_Type fun, int inverse) /*{{{*/
{
   sort_function_type *ptr, *newfnc;

   newfnc = (sort_function_type *) slrn_safe_malloc (sizeof (sort_function_type));
   newfnc->fun = fun;
   newfnc->inverse = inverse;
   newfnc->next = NULL;

   if (*Functions == NULL)
     *Functions = newfnc;
   else
     {
	ptr = *Functions;
	while (ptr->next != NULL) ptr = ptr->next;
	ptr->next = newfnc;
     }
}
/*}}}*/

/* Returns the current sort order according to Slrn_Sorting_Mode. */
static char *get_current_sort_order (int *do_threading) /*{{{*/
{
   if (Slrn_Sorting_Mode == SORT_CUSTOM)
     {
	*do_threading = Slrn_Sort_By_Threads;
	return (Slrn_Sort_Order == NULL ? "" : Slrn_Sort_Order);
     }

   *do_threading = (Slrn_Sorting_Mode & SORT_BY_THREADS);
   if (Slrn_Sorting_Mode & SORT_BY_DATE)
     return (Slrn_Sorting_Mode & 0x2 ? "Highscore,date" : "Highscore,Date");
   if (Slrn_Sorting_Mode & SORT_BY_SCORE)
     return (Slrn_Sorting_Mode & SORT_BY_SUBJECT ? "Score,subject" : "Score");
   if (Slrn_Sorting_Mode & SORT_BY_SUBJECT)
     return "Highscore,subject";
   return "number";
}
/*}}}*/

static void compile_function_list(char *order, sort_function_type **Functions) /*{{{*/
{
   char buf[256];
   unsigned int nth=0;

   while (*Functions != NULL)
     {
	sort_function_type *next = (*Functions)->next;
	SLFREE (*Functions);
	*Functions = next;
     }

   if ((order == NULL) || !(*order)) return;

   while (-1 != SLextract_list_element (order, nth, ',', buf, sizeof(buf)))
     {
	if (! slrn_case_strcmp(buf, "Subject"))
	  add_sort_function(Functions, header_subject_cmp, isupper(buf[0]));
	else if (! slrn_case_strcmp(buf, "Score"))
	  add_sort_function(Functions, header_score_cmp, isupper(buf[0]));
	else if (! slrn_case_strcmp(buf, "Highscore"))
	  add_sort_function(Functions, header_highscore_cmp, isupper(buf[0]));
	else if (! slrn_case_strcmp(buf, "Date"))
	  add_sort_function(Functions, header_date_cmp, isupper(buf[0]));
	else if (! slrn_case_strcmp(buf, "Author"))
	  add_sort_function(Functions, header_author_cmp, isupper(buf[0]));
	else if (! slrn_case_strcmp(buf, "Lines"))
	  add_sort_function(Functions, header_lines_cmp, isupper(buf[0]));
	else if (! slrn_case_strcmp(buf, "Number"))
	  add_sort_function(Functions, header_num_cmp, isupper(buf[0]));
	else if (! slrn_case_strcmp(buf, "Id"))
	  add_sort_function(Functions, header_msgid_cmp, isupper(buf[0]));
	else if (! slrn_case_strcmp(buf, "Body"))
	  add_sort_function(Functions, header_has_body_cmp, isupper(buf[0]));
	else /* Nonexistant sorting method */
	  {
	     slrn_error(_("Can't sort according to `%s'"), buf);
	  }

	nth++;
     } /* while (...) */
}
/*}}}*/

/* Use the given sort_order to generate a Sort_Functions list.
 * If sort_order contains a '|' char, it needs to be writeable */
static void recompile_sortorder(char *sort_order) /*{{{*/
{
    char *separator;

    separator = slrn_strbyte(sort_order, '|');
    if (separator) {
        *separator='\0';
        compile_function_list(sort_order, &Sort_Functions);
        *separator='|';
        compile_function_list(separator+1, &Sort_Thread_Functions);
    }
    else {
        compile_function_list(sort_order, &Sort_Functions);
        compile_function_list(sort_order, &Sort_Thread_Functions);
    }
}
/*}}}*/

/*}}}*/
