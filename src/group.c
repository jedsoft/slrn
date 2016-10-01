/* -*- mode: C; mode: fold -*- */
/*
 This file is part of SLRN.

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

/*{{{ Include files */

#include <stdio.h>
#include <string.h>
#include <time.h>
#ifndef VMS
# include <sys/types.h>
# include <sys/stat.h>
#else
# include "vms.h"
#endif

#include <signal.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include "slrn.h"
#include "group.h"
#include "art.h"
#include "misc.h"
#include "post.h"
#include "util.h"
#include "server.h"
#include "hash.h"
#include "score.h"
#include "menu.h"
#include "startup.h"
#include "slrndir.h"
#include "vfile.h"
#include "snprintf.h"
#include "hooks.h"
#include "common.h"
#include "strutil.h"

/*}}}*/

/*{{{ Global Variables */

int Slrn_Query_Group_Cutoff = 1000;
int Slrn_Groups_Dirty;	       /* 1 == need to write newsrc */
int Slrn_List_Active_File = 0;
int Slrn_Write_Newsrc_Flags = 0;       /* if 1, do not save unsubscribed
					* if 2, do not save new unsubscribed.
					*/

int Slrn_Display_Cursor_Bar;
char *Slrn_Group_Help_Line;
char *Slrn_Group_Status_Line;

Slrn_Group_Type *Slrn_Group_Current_Group;
static SLscroll_Window_Type Group_Window;
static Slrn_Group_Type *Groups;

int Slrn_No_Backups = 0;
int Slrn_No_Autosave = 0;

int Slrn_Unsubscribe_New_Groups = 0;
int Slrn_Check_New_Groups = 1;
int Slrn_Drop_Bogus_Groups = 1;
int Slrn_Max_Queued_Groups = 20;

SLKeyMap_List_Type *Slrn_Group_Keymap;
int *Slrn_Prefix_Arg_Ptr;

/*}}}*/
/*{{{ Static Variables */

#define GROUP_HASH_TABLE_SIZE 1250
static Slrn_Group_Type *Group_Hash_Table [GROUP_HASH_TABLE_SIZE];

static unsigned int Last_Cursor_Row;
static int Groups_Hidden;	/* if 1, hide groups with no articles;
				 * if 2, groups are hidden by user request */
static int Kill_After_Max = 1;

typedef struct Unsubscribed_Slrn_Group_Type
{
   char *group_name;
   struct Unsubscribed_Slrn_Group_Type *next;
}
Unsubscribed_Slrn_Group_Type;

static Unsubscribed_Slrn_Group_Type *Unsubscribed_Groups;

/*}}}*/

/*{{{ Forward Function Declarations */

static void group_update_screen (void);
static void group_quick_help (void);
static void save_newsrc_cmd (void);
static void read_and_parse_active (int);
static int  parse_active_line (unsigned char *, unsigned int *, int *, int *);
static void remove_group_entry (Slrn_Group_Type *);

/*}}}*/

/*{{{ Functions that deal with Group Range */

static NNTP_Artnum_Type count_unread (Slrn_Range_Type *r)
{
   NNTP_Artnum_Type nread = 0;
   NNTP_Artnum_Type rmax = r->max;

   while (r->next != NULL)
     {
	r = r->next;
	nread += r->max - r->min + 1;
     }
   if (nread > rmax)
     return 0;

   return rmax - nread;
}

void slrn_group_recount_unread (Slrn_Group_Type *g)
{
   /* Make sure old (unavailable) messages are marked read */
   if (g->range.min>1)
     g->range.next = slrn_ranges_add (g->range.next, 1, g->range.min-1);
   g->unread = count_unread (&g->range);
}

static int is_article_requested (Slrn_Group_Type *g, NNTP_Artnum_Type num) /*{{{*/
{
#if SLRN_HAS_SPOOL_SUPPORT
   if (Slrn_Server_Id != SLRN_SERVER_ID_SPOOL) return 0;

   if (g->requests_loaded == 0)
     {
	g->requests = slrn_spool_get_requested_ranges (g->group_name);
	g->requests_loaded = 1;
     }

   return slrn_ranges_is_member (g->requests, num);
#else
   (void) g; (void) num;
   return 0;
#endif
}
/*}}}*/

static void group_mark_article_as_read (Slrn_Group_Type *g, NNTP_Artnum_Type num) /*{{{*/
{
   Slrn_Range_Type *r;

   /* Never mark articles as read if their body has been requested. */
   if (is_article_requested (g, num)) return;

   r = &g->range;
   if (r->max < num)  /* not at server yet so update our data */
     {
	r->max = num;
	g->unread += 1;
     }

   r = r->next;

   while (r != NULL)
     {
	/* Already read */
	if ((num <= r->max) && (num >= r->min)) return;
	if (num < r->min) break;
	r = r->next;
     }

   if (g->unread != 0) g->unread -= 1;
   Slrn_Groups_Dirty = 1;
   g->range.next = slrn_ranges_add (g->range.next, num, num);
}

/*}}}*/

void slrn_mark_articles_as_read (char *group,
				 NNTP_Artnum_Type rmin, NNTP_Artnum_Type rmax) /*{{{*/
{
   Slrn_Group_Type *g;

   if (group == NULL)
     g = Slrn_Group_Current_Group;
   else
     {
	unsigned long hash = slrn_compute_hash ((unsigned char *) group,
						(unsigned char *) group + strlen (group));

	g = Group_Hash_Table[hash % GROUP_HASH_TABLE_SIZE];

	while (g != NULL)
	  {
	     if ((g->hash == hash) && !strcmp (group, g->group_name))
	       {
		  /* If it looks like we have read this group, mark it read. */
		  if ((g->flags & GROUP_UNSUBSCRIBED)
		      && (g->range.next == NULL))
		    return;

		  break;
	       }
	     g = g->hash_next;
	  }
     }

   if (g == NULL)
     return;

   while (rmin <= rmax)
     {
	group_mark_article_as_read (g, rmin);
	rmin++;
     }
}

/*}}}*/

static int group_update_range (Slrn_Group_Type *g, NNTP_Artnum_Type min, NNTP_Artnum_Type max) /*{{{*/
{
   NNTP_Artnum_Type n, max_available;
   Slrn_Range_Type *r;

   if (max == 0)
     {
	NNTP_Artnum_Type nmax, nmin;

	nmax = g->range.max;
	nmin = g->range.min;

	/* Server database inconsistent. */
	if (nmax > 0)
	  for (n = nmin; n <= nmax; n++)
	    group_mark_article_as_read (g, n);

	/* g->unread = 0; */
	Slrn_Full_Screen_Update = 1;
	Slrn_Groups_Dirty = 1;
	return -1;
     }

   g->range.min = min;

   /* This might be due to a cancel -- or to a rescan delay in inn after we
    * read the active file; if Kill_After_Max is 0, play it safe and do not
    * mark any articles as read */
   if (Kill_After_Max && (max < g->range.max))
     {
	NNTP_Artnum_Type nmax = g->range.max;
	for (n = max + 1; n <= nmax; n++)
	  group_mark_article_as_read (g, n);
     }
   else g->range.max = max;

   /* In case more articles arrived at the server between the time that
    * slrn was first started and when the server was just queried, update
    * the ranges of read/unread articles.
    */

   max_available = g->range.max - g->range.min + 1;

   r = &g->range;
   if (r->next != NULL)
     {
	n = r->max;
	while (r->next != NULL)
	  {
	     r = r->next;
	     n -= r->max - r->min + 1;
	  }
	if (n < 0) n = 0;
	if (n > max_available) n = max_available;
	g->unread = n;

	r = &g->range;
	if (r->next->min <= r->min)
	  r->next->min = 1;
     }
   else
     g->unread = max_available;

   if ((g->flags & GROUP_UNSUBSCRIBED) == 0)
     {
	if (g->unread > 0)
	  g->flags &= ~GROUP_HIDDEN;
	else if ((Groups_Hidden & 1) && (g != Slrn_Group_Current_Group))
	  g->flags |= GROUP_HIDDEN;
     }

   return 0;
}

/*}}}*/

static int group_sync_group_with_server (Slrn_Group_Type *g, NNTP_Artnum_Type *minp, NNTP_Artnum_Type *maxp) /*{{{*/
{
   char *group;
   int status, reselect;

   if (g == NULL) return -1;

   group = g->group_name;

   slrn_message_now (_("Selecting %s ..."), group);

   if (((reselect = Kill_After_Max) == 0) &&
       (strcmp (group, Slrn_Server_Obj->sv_current_group ())))
     reselect = 1;

   status = Slrn_Server_Obj->sv_select_group (group, minp, maxp);
   if (status == -1)
     return -1;

   if (status == ERR_NOGROUP)
     {
	slrn_message_now (_("Group %s is bogus%s."), group,
			  Slrn_Drop_Bogus_Groups ? _(" - dropping it") : "");
	Slrn_Saw_Warning = 1;
	if (Slrn_Drop_Bogus_Groups)
	  remove_group_entry (g);
	return -1;
     }
   else if (status != OK_GROUP)
     {
	slrn_error (_("Could not enter group %s."), group);
	return -1;
     }

   Kill_After_Max = reselect;

   return group_update_range (g, *minp, *maxp);
}

/*}}}*/

static void free_newsgroup_type (Slrn_Group_Type *g)
{
   if (g == NULL)
     return;
   slrn_free (g->descript);
   slrn_ranges_free (g->range.next);
   slrn_ranges_free (g->requests);
   slrn_free (g->group_name);
   slrn_free ((char *) g);
}

static void free_unsubscribed_group_type (Unsubscribed_Slrn_Group_Type *ug)
{
   if (ug == NULL)
     return;
   slrn_free (ug->group_name);
   slrn_free ((char *) ug);
}

void slrn_catchup_group (void) /*{{{*/
{
   if ((Slrn_Group_Current_Group == NULL)||
       (Slrn_Group_Current_Group->unread==0))
     return;

   Slrn_Group_Current_Group->range.next =
     slrn_ranges_add(Slrn_Group_Current_Group->range.next, 1,
		     Slrn_Group_Current_Group->range.max);
   Slrn_Group_Current_Group->unread = 0;
   Slrn_Groups_Dirty = 1;
   Slrn_Group_Current_Group->flags |= GROUP_TOUCHED;
}

/*}}}*/

void slrn_uncatchup_group (void) /*{{{*/
{
   if (Slrn_Group_Current_Group == NULL)
     return;
   slrn_ranges_free (Slrn_Group_Current_Group->range.next);
   Slrn_Group_Current_Group->range.next = NULL;
   slrn_group_recount_unread (Slrn_Group_Current_Group);
   Slrn_Groups_Dirty = 1;
   Slrn_Group_Current_Group->flags |= GROUP_TOUCHED;
}

/*}}}*/

/*}}}*/

/*{{{ Misc Utility Functions */
static void find_line_num (void) /*{{{*/
{
   Group_Window.lines = (SLscroll_Type *) Groups;
   Group_Window.current_line = (SLscroll_Type *) Slrn_Group_Current_Group;
   (void) SLscroll_find_line_num (&Group_Window);
}

/*}}}*/

static Slrn_Group_Type *find_group_entry (char *name, unsigned int len) /*{{{*/
{
   int hash_index;
   unsigned long hash;
   Slrn_Group_Type *g;

   hash = slrn_compute_hash ((unsigned char *) name,
			     (unsigned char *) name + len);

   hash_index = hash % GROUP_HASH_TABLE_SIZE;
   g = Group_Hash_Table[hash_index];

   while (g != NULL)
     {
	if ((g->hash == hash) && !strncmp (name, g->group_name, len))
	  {
	     if (len == strlen (g->group_name)) break;
	  }
	g = g->hash_next;
     }
   return g;
}

/*}}}*/
static Slrn_Group_Type *create_group_entry (char *name, unsigned int len, /*{{{*/
					    NNTP_Artnum_Type min, NNTP_Artnum_Type max, int query_server,
					    int skip_find)
{
   int hash_index;
   unsigned long hash;
   Slrn_Group_Type *g;

   if (skip_find == 0)
     {
	g = find_group_entry (name, len);
	if (g != NULL) return g;
     }

   g = (Slrn_Group_Type *) slrn_safe_malloc (sizeof (Slrn_Group_Type));
   g->requests = NULL;
   g->requests_loaded = 0;

   g->group_name = slrn_safe_malloc (len + 1);
   strncpy (g->group_name, name, len);
   g->group_name [len] = 0;

   if (query_server)
     {
	int status;

	status = Slrn_Server_Obj->sv_select_group (g->group_name, &min, &max);
	if (status == ERR_NOGROUP)
	  {
	     slrn_message_now (_("Group %s is bogus%s."), g->group_name,
			 Slrn_Drop_Bogus_Groups ? _(" - ignoring it") : "");
	     if (Slrn_Drop_Bogus_Groups)
	       {
		  Slrn_Groups_Dirty = 1;
		  free_newsgroup_type (g);
		  return NULL;
	       }
	  }
     }
   g->range.min = min;
   g->range.max = max;
   if (max > 0)
     g->unread = max - min + 1;
   else g->unread = 0;

   g->flags = (GROUP_UNSUBSCRIBED | GROUP_HIDDEN);

   hash = slrn_compute_hash ((unsigned char *) name,
			     (unsigned char *) name + len);
   hash_index = hash % GROUP_HASH_TABLE_SIZE;

   g->hash = hash;
   g->hash_next = Group_Hash_Table[hash_index];
   Group_Hash_Table[hash_index] = g;

   if (Groups == NULL)
     {
	Slrn_Group_Current_Group = Groups = g;
     }
   else
     {
	if (Slrn_Group_Current_Group == NULL) /* insert on top */
	  {
	     g->prev = NULL;
	     g->next = Groups;
	     Groups->prev = g;
	     Groups = g;
	  }
	else /* insert after current group */
	  {
	     g->next = Slrn_Group_Current_Group->next;
	     if (g->next != NULL) g->next->prev = g;
	     Slrn_Group_Current_Group->next = g;
	     g->prev = Slrn_Group_Current_Group;
	  }
     }
   Slrn_Group_Current_Group = g;
   return g;
}

/*}}}*/
static void remove_group_entry (Slrn_Group_Type *g) /*{{{*/
{
   Slrn_Group_Type *tmp;
   if (g == Groups)
     Groups = g->next;
   if (g == Slrn_Group_Current_Group)
     Slrn_Group_Current_Group = g->next != NULL ? g->next : g->prev;
   if (g->prev != NULL)
     g->prev->next = g->next;
   if (g->next != NULL)
     g->next->prev = g->prev;
   if (g == (tmp = Group_Hash_Table[g->hash % GROUP_HASH_TABLE_SIZE]))
     Group_Hash_Table[g->hash % GROUP_HASH_TABLE_SIZE] = g->hash_next;
   else while (tmp != NULL)
     {
	if (g == tmp->hash_next)
	  {
	     tmp->hash_next = g->hash_next;
	     break;
	  }
	tmp = tmp->hash_next;
     }
   free_newsgroup_type (g);
   find_line_num ();
   Slrn_Groups_Dirty = 1;
}
/*}}}*/
static int add_group (char *name, unsigned int len, /*{{{*/
		      unsigned int subscribe_flag, int query_server,
		      int create_flag)
{
   Slrn_Group_Type *g;

   g = find_group_entry (name, len);
   if (g == NULL)
     {
	if (Slrn_List_Active_File && Slrn_Drop_Bogus_Groups)
	  {
	     char *tmp_name;

	     if (NULL != (tmp_name = slrn_strnmalloc (name, len, 1)))
	       {
		  slrn_message_now (_("Group %s is bogus - ignoring it."), tmp_name);
		  Slrn_Saw_Warning = 1;
		  slrn_free (tmp_name);
	       }
	     return -1;
	  }
	else g = create_group_entry (name, len, -1, -1, query_server &&
				     !(subscribe_flag & GROUP_UNSUBSCRIBED),
				     0);
	if (g == NULL) return -1;
     }
   Slrn_Groups_Dirty = 1;

   /* If we have already processed this, then the group is duplicated in
    * the newsrc file.  Throw it out now.
    */
   if (g->flags & GROUP_PROCESSED) return -1;

   Slrn_Group_Current_Group = g;
   g->flags = subscribe_flag;
   g->flags |= GROUP_PROCESSED;

   if (subscribe_flag & GROUP_UNSUBSCRIBED)
     {
	g->flags |= GROUP_HIDDEN;

	if (create_flag) return 0;
	g->unread = 0;
	/* if (Slrn_List_Active_File == 0) return 0; */
     }

   if (create_flag) return 0;

   /* find ranges for this */
   name += len;			       /* skip past name */
   if (*name) name++;			       /* skip colon */
   g->range.next = slrn_ranges_from_newsrc_line (name);
   slrn_group_recount_unread (g);

   if ((g->range.next != NULL)
       && (g->range.next->min < g->unread)
       && (g->range.next->min > 1))
     g->unread -= g->range.next->min - 1;

   return 0;
}

/*}}}*/

/* Rearrange Slrn_Group_Current_Group such that it follows last_group and
 * return Slrn_Group_Current_Group.  If last_group is NULL, Slrn_Group_Current_Group
 * should be put at top of list.
 */
static Slrn_Group_Type *place_group_in_newsrc_order (Slrn_Group_Type *last_group) /*{{{*/
{
   Slrn_Group_Type *next_group, *prev_group;

   next_group = Slrn_Group_Current_Group->next;
   prev_group = Slrn_Group_Current_Group->prev;
   if (next_group != NULL) next_group->prev = prev_group;
   if (prev_group != NULL) prev_group->next = next_group;

   Slrn_Group_Current_Group->prev = last_group;
   if (last_group != NULL)
     {
	Slrn_Group_Current_Group->next = last_group->next;

	if (Slrn_Group_Current_Group->next != NULL)
	  Slrn_Group_Current_Group->next->prev = Slrn_Group_Current_Group;

	last_group->next = Slrn_Group_Current_Group;
     }
   else if (Slrn_Group_Current_Group != Groups)
     {
	Slrn_Group_Current_Group->next = Groups;
	if (Groups != NULL) Groups->prev = Slrn_Group_Current_Group;
	Groups = Slrn_Group_Current_Group;
     }
   else
     {
	/* correct next_group->prev since it was not set correctly above */
	if (next_group != NULL)
	  next_group->prev = Slrn_Group_Current_Group;
     }

   return Slrn_Group_Current_Group;
}

/*}}}*/
static void insert_new_groups (void) /*{{{*/
{
   Slrn_Group_Type *last_group = Groups;

   while (last_group != NULL)
     {
	/* unmark new groups from previous run */
	last_group->flags &= ~GROUP_NEW_GROUP_FLAG;
	last_group = last_group->next;
     }

   if (Unsubscribed_Groups != NULL)
     {
	unsigned int subscribe_flag;
	Unsubscribed_Slrn_Group_Type *ug = Unsubscribed_Groups, *ugnext;

	if (Slrn_Unsubscribe_New_Groups)
	  subscribe_flag = GROUP_UNSUBSCRIBED | GROUP_NEW_GROUP_FLAG;
	else subscribe_flag = GROUP_NEW_GROUP_FLAG;

	while (ug != NULL)
	  {
	     ugnext = ug->next;

	     if (-1 != add_group (ug->group_name, strlen (ug->group_name), subscribe_flag, 0, 0))
	       last_group = place_group_in_newsrc_order (last_group);

	     free_unsubscribed_group_type (ug);
	     ug = ugnext;
	  }
	Unsubscribed_Groups = NULL;
     }
}

/*}}}*/

static void init_group_win_struct (void) /*{{{*/
{
   Group_Window.nrows = SLtt_Screen_Rows - 3;
   Group_Window.hidden_mask = GROUP_HIDDEN;
   Group_Window.current_line = (SLscroll_Type *) Slrn_Group_Current_Group;
   Group_Window.cannot_scroll = SLtt_Term_Cannot_Scroll;
   Group_Window.lines = (SLscroll_Type *) Groups;
   Group_Window.border = 1;
   if (Slrn_Scroll_By_Page)
     {
	/* Slrn_Group_Window.border = 0; */
	Group_Window.cannot_scroll = 2;
     }
   find_line_num ();
}

/*}}}*/

static int find_group (char *name) /*{{{*/
{
   Slrn_Group_Type *g = find_group_entry (name, strlen (name));
   if (g == NULL) return 0;

   g->flags &= ~GROUP_HIDDEN;
   Slrn_Group_Current_Group = g;
   find_line_num ();
   return 1;
}

/*}}}*/

/* origpat needs enough space for SLRL_DISPLAY_BUFFER_SIZE chars */
static SLRegexp_Type *read_group_regexp (char *prompt, char *origpat) /*{{{*/
{
   static char pattern[SLRL_DISPLAY_BUFFER_SIZE];

   if (slrn_read_input (prompt, NULL, pattern, 1, 0) <= 0) return NULL;

   if (origpat != NULL)
     strcpy (origpat, pattern); /* safe */

   return slrn_compile_regexp_pattern (slrn_fix_regexp (pattern));
}

/*}}}*/

static void add_unsubscribed_group (unsigned char *name) /*{{{*/
{
   Unsubscribed_Slrn_Group_Type *g;
   unsigned char *p;

   g = (Unsubscribed_Slrn_Group_Type *) slrn_safe_malloc (sizeof (Unsubscribed_Slrn_Group_Type));

   g->next = Unsubscribed_Groups;
   Unsubscribed_Groups = g;

   p = name;
   while (*p > ' ') p++;
   *p = 0;

   g->group_name = slrn_safe_strmalloc ((char*)name);
}

/*}}}*/

/*}}}*/

static char *Group_Display_Formats [SLRN_MAX_DISPLAY_FORMATS];
static unsigned int Group_Format_Number;

int slrn_set_group_format (unsigned int num, char *fmt)
{
   return slrn_set_display_format (Group_Display_Formats, num, fmt);
}

static void toggle_group_formats (void)
{
   Group_Format_Number = slrn_toggle_format (Group_Display_Formats,
					     Group_Format_Number);
}

int slrn_group_search (char *str, int dir) /*{{{*/
{
#if SLANG_VERSION < 20000
   SLsearch_Type st;
#else
   SLsearch_Type *st = NULL;
   unsigned int flags;
#endif
   Slrn_Group_Type *g;
   int found = 0;

   g = Slrn_Group_Current_Group;
   if (g == NULL) return 0;

#if SLANG_VERSION < 20000
   SLsearch_init (str, 1, 0, &st);
#else
   flags = SLSEARCH_CASELESS;
   if (Slrn_UTF8_Mode)
     flags |= SLSEARCH_UTF8;

   st = SLsearch_new ((SLuchar_Type *) str, flags);
   if (st == NULL)
     return 0;
#endif

   do
     {
	if (dir > 0)
	  g = g->next;
	else
	  g = g->prev;
	if (g == NULL)
	  {
	     g = Groups;
	     if (dir < 0)
	       while (g->next != NULL)
		 g = g->next;
	  }

	if ((g->flags & GROUP_HIDDEN) == 0)
	  {
#if SLANG_VERSION < 20000
	     if ((NULL != SLsearch ((unsigned char *) g->group_name,
				    (unsigned char *) g->group_name + strlen (g->group_name),
				   &st))
		 || ((NULL != g->descript)
		     && (NULL != SLsearch ((unsigned char *) g->descript,
					   (unsigned char *) g->descript + strlen (g->descript),
					   &st))))
	       {
		  found = 1;
		  break;
	       }
#else
	     if ((NULL != SLsearch_forward (st, (unsigned char *) g->group_name,
					   (unsigned char *) g->group_name + strlen (g->group_name)))
		 || ((NULL != g->descript)
		     && (NULL != SLsearch_forward (st, (unsigned char *) g->descript,
						  (unsigned char *) g->descript + strlen (g->descript)))))
	       {
		  found = 1;
		  break;
	       }
#endif
	  }
     }
   while (g != Slrn_Group_Current_Group);

#if SLANG_VERSION >= 20000
   SLsearch_delete (st);
#endif

   Slrn_Group_Current_Group = g;
   find_line_num ();
   return found;
}

/*}}}*/
unsigned int slrn_group_up_n (unsigned int n) /*{{{*/
{
   n = SLscroll_prev_n (&Group_Window, n);
   Slrn_Group_Current_Group = (Slrn_Group_Type *) Group_Window.current_line;
   return n;
}

/*}}}*/
unsigned int slrn_group_down_n (unsigned int n) /*{{{*/
{
   n = SLscroll_next_n (&Group_Window, n);
   Slrn_Group_Current_Group = (Slrn_Group_Type *) Group_Window.current_line;
   return n;
}

/*}}}*/

int slrn_group_select_group (void) /*{{{*/
{
   NNTP_Artnum_Type min, max, n, max_available, last_n;
   int ret;
   Slrn_Range_Type *r;
   int prefix;

   if (Slrn_Prefix_Arg_Ptr != NULL)
     {
	prefix = *Slrn_Prefix_Arg_Ptr;
	Slrn_Prefix_Arg_Ptr = NULL;
     }
   else prefix = 0;

   if (Slrn_Group_Current_Group == NULL) return -1;

   last_n = Slrn_Group_Current_Group->unread;

   if (-1 == group_sync_group_with_server (Slrn_Group_Current_Group, &min, &max))
     {
	slrn_message (_("No articles to read."));
	return -1;
     }

   n = Slrn_Group_Current_Group->unread;

#if 1
   if ((prefix == 0) && (n == 0) && (n != last_n))
     return -1;
#endif

   max_available = Slrn_Group_Current_Group->range.max - Slrn_Group_Current_Group->range.min + 1;

   if ((prefix != 0) || (n == 0))
     n = max_available;

   if ((prefix & 1)
       || ((Slrn_Query_Group_Cutoff > 0)
	   && (n > (NNTP_Artnum_Type)Slrn_Query_Group_Cutoff))
       || ((Slrn_Query_Group_Cutoff < 0)
	   && (n > (NNTP_Artnum_Type)(-Slrn_Query_Group_Cutoff))))
     {
	char int_prompt_buf[512];
	if ((prefix & 1) || (Slrn_Query_Group_Cutoff > 0))
	  {
	     slrn_snprintf (int_prompt_buf, sizeof (int_prompt_buf),
			    _("%s: Read how many? "),
			    Slrn_Group_Current_Group->group_name);
	     if ((-1 == slrn_read_artnum_int (int_prompt_buf, &n, &n))
		 || (n <= 0))
	       {
		  slrn_clear_message ();
		  Slrn_Full_Screen_Update = 1;
		  return 0;
	       }
	  }
	else
	  {
	     slrn_message_now (_("Only downloading %d of " NNTP_FMT_ARTNUM " articles."),
			       -Slrn_Query_Group_Cutoff, n);
	     n = -Slrn_Query_Group_Cutoff;
	  }

	if ((0 == prefix)
	    && (Slrn_Group_Current_Group->unread != 0))
	  {
	     r = Slrn_Group_Current_Group->range.next;
	     if (r != NULL)
	       {
		  while (r->next != NULL) r = r->next;
		  if (r->max + n > max)
		    n = -n;	       /* special treatment in article mode
					* because we will need to query the
					* server about articles in a group
					* that we have already read.
					*/
	       }
	  }
     }
   else if ((0 == prefix) && (Slrn_Group_Current_Group->unread != 0))
     n = 0;

   ret = slrn_select_article_mode (Slrn_Group_Current_Group, n,
				   ((prefix & 2) == 0));

   if (ret == -2)
     slrn_catchup_group ();

   return ret;
}

/*}}}*/
void slrn_select_next_group (void) /*{{{*/
{
   if (Slrn_Group_Current_Group == NULL)
     return;

   while ((SLang_get_error () == 0) && (1 == slrn_group_down_n (1)))
     {
	if (Slrn_Group_Current_Group->unread == 0)
	  continue;

	if (0 == slrn_group_select_group ())
	  break;
	else if (SLang_get_error () == INTRINSIC_ERROR)
	  /* all articles killed by scorefile, so proceed */
	  {
	     SLang_set_error (0);
	     slrn_clear_message ();
	  }
     }
}

/*}}}*/
void slrn_select_prev_group (void) /*{{{*/
{
   if (Slrn_Group_Current_Group == NULL)
     return;

   while ((SLang_get_error () == 0) && (1 == slrn_group_up_n (1)))
     {
       if (Slrn_Group_Current_Group->unread == 0)
         continue;

       if (0 == slrn_group_select_group ())
         break;
     }
}

/*}}}*/

/*{{{ Interactive commands */

void slrn_group_quit (void) /*{{{*/
{
   if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_QUIT)
       && (Slrn_Batch == 0)
       && (slrn_get_yesno (1, _("Do you really want to quit")) <= 0)) return;

   if ((Slrn_Groups_Dirty) && (-1 == slrn_write_newsrc (0)))
     {
	if (Slrn_Batch)
	  slrn_quit (1);

	slrn_smg_refresh ();
	if (Slrn_Batch == 0) slrn_sleep (2);
	if (slrn_get_yesno (0, _("Write to newsrc file failed.  Quit anyway")) <= 0)
	  return;
     }
   slrn_quit (0);
}

/*}}}*/

static void group_pagedown (void) /*{{{*/
{
   Slrn_Full_Screen_Update = 1;

   if (-1 == SLscroll_pagedown (&Group_Window))
     slrn_error (_("End of Buffer."));
   Slrn_Group_Current_Group = (Slrn_Group_Type *) Group_Window.current_line;
}

/*}}}*/

static void group_pageup (void) /*{{{*/
{
   Slrn_Full_Screen_Update = 1;

   if (-1 == SLscroll_pageup (&Group_Window))
     slrn_error (_("Top of Buffer."));

   Slrn_Group_Current_Group = (Slrn_Group_Type *) Group_Window.current_line;
}

/*}}}*/

static void group_up (void) /*{{{*/
{
   if (0 == slrn_group_up_n (1))
     {
	slrn_error (_("Top of buffer."));
     }
}

/*}}}*/

static void set_current_group (void) /*{{{*/
{
   Slrn_Group_Type *g;

   g = Slrn_Group_Current_Group;
   if (g == NULL) g = Groups;

   while ((g != NULL) && (g->flags & GROUP_HIDDEN)) g = g->next;
   if ((g == NULL) && (Slrn_Group_Current_Group != NULL))
     {
	g = Slrn_Group_Current_Group -> prev;
	while ((g != NULL) && (g->flags & GROUP_HIDDEN)) g = g->prev;
     }
   Slrn_Group_Current_Group = g;

   /* When there are less than SCREEN_HEIGHT-2 groups, they should all get
    * displayed; scroll to the top of the buffer to ensure this. */
   while (SLscroll_prev_n (&Group_Window, 1000));
   (void) SLscroll_find_top (&Group_Window);
   find_line_num ();

   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

static void refresh_groups (Slrn_Group_Type **c) /*{{{*/
{
   Slrn_Group_Type *g = Groups;
   Slrn_Group_Range_Type *ranges;

   if (Slrn_Max_Queued_Groups <= 0)
     Slrn_Max_Queued_Groups = 1;
   ranges = (Slrn_Group_Range_Type *) slrn_safe_malloc
     (Slrn_Max_Queued_Groups * sizeof(Slrn_Group_Range_Type));

   while (g != NULL)
     {
	Slrn_Group_Type *start = g;
	int i = 0, j = 0;

	while ((g != NULL) && (i < Slrn_Max_Queued_Groups))
	  {
	     if (!(g->flags & GROUP_UNSUBSCRIBED))
	       {
		  ranges[i].name = g->group_name;
		  i++;
	       }
	     g = g->next;
	  }
	if (Slrn_Server_Obj->sv_refresh_groups (ranges, i))
	  {
	     slrn_error (_("Server connection dropped."));
	     goto free_and_return;
	  }
	g = start;
	while (j < i)
	  {
	     if (g->flags & GROUP_UNSUBSCRIBED)
	       {
		  g = g->next;
		  continue;
	       }
	     if (ranges[j].min == -1)
	       {
		  slrn_message_now (_("Group %s is bogus%s."), g->group_name,
			      Slrn_Drop_Bogus_Groups ? _(" - dropping it") : "");
		  Slrn_Saw_Warning = 1;
		  if (Slrn_Drop_Bogus_Groups)
		    {
		       Slrn_Group_Type *tmp = g->next;
		       remove_group_entry (g);
		       if (g == *c) *c = Groups;
		       g = tmp;
		       j++;
		       continue;
		    }
	       }
	     else
	       group_update_range (g, ranges[j].min, ranges[j].max);
	     g = g->next;
	     j++;
	  }
	while ((g != NULL) && (g->flags & GROUP_UNSUBSCRIBED))
	  g = g->next;
     }
   free_and_return:
   SLfree ((char*) ranges);
}
/*}}}*/

static void refresh_groups_cmd (void) /*{{{*/
{
   Slrn_Group_Type *c = Slrn_Group_Current_Group;
   Slrn_Group_Current_Group = NULL;

   slrn_message_now (_("Checking news%s ..."),
		     Slrn_List_Active_File ? _(" via active file") : "");

   if (Slrn_List_Active_File)
     {
	read_and_parse_active (0);
	if ((Slrn_Server_Obj->sv_id == SERVER_ID_INN) && (c != NULL))
	  /* hack: avoid a problem with inn not updating the high water mark */
	  {
	     Slrn_Group_Type *a = c, *b;
	     NNTP_Artnum_Type min, max;

	     if (((NULL != (b = c->next)) || (NULL != (b = c->prev))) &&
		 (0 == strcmp (c->group_name,
			       Slrn_Server_Obj->sv_current_group ())))
	       {
		  a = b;
	       }

	     (void) group_sync_group_with_server (a, &min, &max);
	  }
     }

   if (Slrn_Check_New_Groups)
     {
	slrn_get_new_groups (0);
	insert_new_groups ();
     }

   if (!Slrn_List_Active_File)
     {
	refresh_groups (&c);
     }

   slrn_read_group_descriptions ();

   Slrn_Group_Current_Group = c;
   set_current_group ();
   group_quick_help ();
}

/*}}}*/

static void generic_group_search (int dir) /*{{{*/
{
   static char search_str[SLRL_DISPLAY_BUFFER_SIZE];
   char* prompt;
   Slrn_Group_Type *g;
   unsigned int n;
   int ret;

   g = Slrn_Group_Current_Group;
   if (g == NULL) return;

   prompt = slrn_strdup_strcat ((dir > 0 ? _("Forward") : _("Backward")),
				_(" Search: "), NULL);
   ret = slrn_read_input (prompt, search_str, NULL, 1, 0);
   slrn_free (prompt);
   if (ret <= 0) return;

   n = Group_Window.line_num;
   if (0 == slrn_group_search (search_str, dir))
     {
        slrn_error (_("Not found."));
	return;
     }

   if (((dir > 0) && (n > Group_Window.line_num)) ||
       ((dir < 0) && (n < Group_Window.line_num)))
     slrn_message (_("Search wrapped."));
}

/*}}}*/

static void group_search_forward (void) /*{{{*/
{
   generic_group_search (1);
}
/*}}}*/

static void group_search_backward (void) /*{{{*/
{
   generic_group_search (-1);
}
/*}}}*/

int slrn_add_group (char *group) /*{{{*/
{
   int retval = 0;
   if (!find_group (group))
     {
	if (Slrn_List_Active_File == 0)
	  {
	     retval = add_group (group, strlen (group), 0, 1, 0);
	     slrn_read_group_descriptions ();
	  }
	else
	  {
	     slrn_error (_("Group %s does not exist."), group);
	     retval = -1;
	  }
     }
   Slrn_Groups_Dirty = 1;
   Slrn_Full_Screen_Update = 1;
   find_line_num ();
   return retval;
}
/*}}}*/

static void add_group_cmd (void) /*{{{*/
{
   char group[SLRL_DISPLAY_BUFFER_SIZE];

   *group = 0;
   if (slrn_read_input (_("Add group: "), NULL, group, 1, 0) > 0)
     (void) slrn_add_group (group);
}

/*}}}*/

static void group_down (void) /*{{{*/
{
   if (1 != slrn_group_down_n (1))
     {
	slrn_error (_("End of Buffer."));
     }
}

/*}}}*/

static void transpose_groups (void) /*{{{*/
{
   Slrn_Group_Type *g, *g1, *tmp;

   if (NULL == (g = Slrn_Group_Current_Group))
     return;

   if (1 != slrn_group_up_n (1))
     return;

   g1 = Slrn_Group_Current_Group;
   tmp = g1->next;

   g1->next = g->next;
   if (g1->next != NULL) g1->next->prev = g1;
   g->next = tmp;
   tmp->prev = g;		       /* tmp cannot be NULL but it can be
					* equal to g.  This link is corrected
					* below
					*/

   tmp = g1->prev;
   g1->prev = g->prev;
   g1->prev->next = g1;		       /* g1->prev cannot be NULL */
   g->prev = tmp;
   if (tmp != NULL) tmp->next = g;

   if (g1 == Groups) Groups = g;

   find_line_num ();

   (void) slrn_group_down_n (1);

   Slrn_Full_Screen_Update = 1;
   Slrn_Groups_Dirty = 1;
}

/*}}}*/

static void move_group_cmd (void) /*{{{*/
{
   SLang_Key_Type *key;
   void (*f)(void);
   Slrn_Group_Type *from, *to;

   if (Slrn_Batch) return;
   if (Slrn_Group_Current_Group == NULL) return;
   from = Slrn_Group_Current_Group;

   /* Already centering the window here should avoid confusing the user */
   Group_Window.top_window_line = NULL;
   slrn_update_screen ();

   while (1)
     {
	slrn_message_now (_("Moving %s. Press RETURN when finished."), Slrn_Group_Current_Group->group_name);

	/* key = SLang_do_key (Slrn_Group_Keymap, (int (*)(void)) SLang_getkey); */
	key = SLang_do_key (Slrn_Group_Keymap, slrn_getkey);

	if ((key == NULL)
	    || (key->type == SLKEY_F_INTERPRET))
	  f = NULL;
	else f = (void (*)(void)) key->f.f;

	if ((f == group_up) || (f == group_down))
	  {
	     if (f == group_down)
	       (void) slrn_group_down_n (1);
	     else
	       (void) slrn_group_up_n (1);

	     to = Slrn_Group_Current_Group;
	     if (from == to) break;

	     Slrn_Full_Screen_Update = 1;
	     Slrn_Groups_Dirty = 1;

	     if (NULL != from->next)
	       from->next->prev = from->prev;
	     if (NULL != from->prev)
	       from->prev->next = from->next;

	     if (f == group_down)
	       {
		  if (NULL != to->next)
		    to->next->prev = from;
		  from->next = to->next;
		  from->prev = to;
		  to->next = from;
		  if (from == Groups) Groups = to;
	       }
	     else
	       {
		  if (NULL != to->prev)
		    to->prev->next = from;
		  from->prev = to->prev;
		  from->next = to;
		  to->prev = from;
		  if (to == Groups) Groups = from;
	       }
	  }
	else break;

	if (from != Slrn_Group_Current_Group)
	  {
	     Slrn_Group_Current_Group = from;
	     find_line_num ();
	  }

	/* For a recenter if possible. */
	/* if (Group_Window.top_window_line == Group_Window.current_line) */
	Group_Window.top_window_line = NULL;

	slrn_update_screen ();
     }
}

/*}}}*/

static void subscribe (void) /*{{{*/
{
   SLRegexp_Type *re;
   Slrn_Group_Type *g;

   if (Slrn_Prefix_Arg_Ptr == NULL)
     {
	if (Slrn_Group_Current_Group != NULL)
	  {
	     Slrn_Group_Current_Group->flags &= ~GROUP_UNSUBSCRIBED;
	     Slrn_Group_Current_Group->flags |= GROUP_TOUCHED;
	     slrn_group_down_n (1);
	     Slrn_Groups_Dirty = 1;
	  }
	return;
     }

   Slrn_Prefix_Arg_Ptr = NULL;

   if (NULL == (re = read_group_regexp (_("Subscribe pattern: "), NULL)))
     return;

   g = Groups;
   while (g != NULL)
     {
	if (g->flags & GROUP_UNSUBSCRIBED)
	  {
	     if (NULL != slrn_regexp_match (re, g->group_name))
	       {
		  g->flags &= ~GROUP_HIDDEN;
		  g->flags &= ~GROUP_UNSUBSCRIBED;
		  g->flags |= GROUP_TOUCHED;
		  Slrn_Groups_Dirty = 1;
	       }
	  }
	g = g->next;
     }
   find_line_num ();
   Slrn_Full_Screen_Update = 1;
#if SLANG_VERSION >= 20000
   SLregexp_free (re);
#endif
}

/*}}}*/

static void catch_up (void) /*{{{*/
{
   if ((Slrn_Group_Current_Group == NULL)
       || ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_CATCHUP)
	   && (Slrn_Batch == 0)
	   && slrn_get_yesno(1, _("Mark %s as read"), Slrn_Group_Current_Group->group_name) <= 0))
     return;

   slrn_catchup_group ();
   slrn_message (_("Group marked as read."));
   (void) slrn_group_down_n (1);
}

/*}}}*/

static void uncatch_up (void) /*{{{*/
{
   if ((Slrn_Group_Current_Group == NULL)
       || ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_CATCHUP)
	   && (Slrn_Batch == 0)
	   && slrn_get_yesno(1, _("Mark %s as un-read"), Slrn_Group_Current_Group->group_name) <= 0))
     return;

   slrn_uncatchup_group ();
   slrn_message (_("Group marked as un-read."));
   (void) slrn_group_down_n (1);
}

/*}}}*/

static void unsubscribe (void) /*{{{*/
{
   SLRegexp_Type *re;
   Slrn_Group_Type *g;

   if (Slrn_Group_Current_Group == NULL) return;

   if (Slrn_Prefix_Arg_Ptr == NULL)
     {
	Slrn_Group_Current_Group->flags |= GROUP_UNSUBSCRIBED | GROUP_TOUCHED;
	slrn_group_down_n (1);
	Slrn_Groups_Dirty = 1;
	return;
     }

   Slrn_Prefix_Arg_Ptr = NULL;

   if (NULL == (re = read_group_regexp (_("Un-Subscribe pattern: "), NULL)))
     return;

   g = Groups;
   while (g != NULL)
     {
	if ((g->flags & GROUP_UNSUBSCRIBED) == 0)
	  {
	     if (NULL != (slrn_regexp_match (re, g->group_name)))
	       {
		  g->flags &= ~GROUP_HIDDEN;
		  g->flags |= (GROUP_TOUCHED | GROUP_UNSUBSCRIBED);
	       }
	  }
	g = g->next;
     }
   find_line_num ();
   Slrn_Full_Screen_Update = 1;
#if SLANG_VERSION >= 20000
   SLregexp_free (re);
#endif
}

/*}}}*/

static void group_bob (void)
{
   while (slrn_group_up_n (1000));
}

static void group_eob (void)
{
   while (slrn_group_down_n (1000));
}

static void toggle_list_all_groups1 (int hide_flag) /*{{{*/
{
   Slrn_Group_Type *g, *first_found = NULL;
   static int all_hidden = 1;

   g = Groups;

   if (hide_flag != -1)
     {
	all_hidden = hide_flag;
     }
   else all_hidden = !all_hidden;

   if (all_hidden)
     {
	while (g != NULL)
	  {
	     if (g->flags & GROUP_UNSUBSCRIBED) g->flags |= GROUP_HIDDEN;
	     g = g->next;
	  }
     }
   else if (hide_flag != -1)
     {
	while (g != NULL)
	  {
	     if (g->flags & GROUP_UNSUBSCRIBED) g->flags &= ~GROUP_HIDDEN;
	     g = g->next;
	  }
     }
   else
     {
	SLRegexp_Type *re;
	char origpat[SLRL_DISPLAY_BUFFER_SIZE];

	if (NULL == (re = read_group_regexp (_("List Groups (e.g., comp*unix*): "),
					     origpat)))
	  {
	     all_hidden = 1;
	     return;
	  }

	if ((Slrn_List_Active_File == 0)
	    && (OK_GROUPS == Slrn_Server_Obj->sv_list_active (origpat)))
	  {
	     char buf [NNTP_BUFFER_SIZE];
	     Slrn_Group_Type *save = Slrn_Group_Current_Group;
	     Slrn_Group_Current_Group = NULL;

	     while (Slrn_Server_Obj->sv_read_line (buf, sizeof (buf)) > 0)
	       {
		  unsigned int len;
		  int min, max;

		  parse_active_line ((unsigned char *)buf, &len, &min, &max);
		  g = create_group_entry (buf, len, min, max, 0, 0);

		  if (g != NULL)
		    {
		       g->flags &= ~GROUP_HIDDEN;
		       if ((first_found == NULL) && (g->flags & GROUP_UNSUBSCRIBED))
			 first_found = g;
		    }
	       }

	     Slrn_Group_Current_Group = save;
	  }
	else while (g != NULL)
	  {
	     if (NULL != slrn_regexp_match (re, g->group_name))
	       {
		  if ((first_found == NULL) && (g->flags & GROUP_UNSUBSCRIBED))
		    first_found = g;
		  g->flags &= ~GROUP_HIDDEN;
	       }
	     g = g->next;
	  }
#if SLANG_VERSION >= 20000
	SLregexp_free (re);
#endif
     }

   g = Slrn_Group_Current_Group;
   if (first_found != NULL)
     g = first_found;
   else
     {
	while ((g != NULL) && (g->flags & GROUP_HIDDEN)) g = g->next;
	if ((g == NULL) && (Slrn_Group_Current_Group != NULL))
	  {
	     g = Slrn_Group_Current_Group -> prev;
	     while ((g != NULL) && (g->flags & GROUP_HIDDEN)) g = g->prev;
	  }
     }
   Slrn_Group_Current_Group = g;

   Slrn_Full_Screen_Update = 1;

   if ((all_hidden == 0) && (Slrn_Group_Current_Group == NULL))
     {
	Slrn_Group_Current_Group = Groups;
	if ((Slrn_Group_Current_Group != NULL)
	    && (Slrn_Group_Current_Group->flags & GROUP_HIDDEN))
	  {
	     Slrn_Group_Current_Group = NULL;
	  }
     }

   find_line_num ();
}

/*}}}*/

static void toggle_list_all_groups (void) /*{{{*/
{
   int mode = -1;

   if (Slrn_Prefix_Arg_Ptr != NULL)
     {
	mode = *Slrn_Prefix_Arg_Ptr;
	Slrn_Prefix_Arg_Ptr = NULL;
	if (mode == 2) mode = 0;
     }

   toggle_list_all_groups1 (mode);
}

/*}}}*/

void slrn_list_all_groups (int mode)
{
   toggle_list_all_groups1 (!mode);
}

void slrn_hide_current_group (void) /*{{{*/
{
   if (Slrn_Group_Current_Group == NULL) return;
   Groups_Hidden |= 2;
   Slrn_Group_Current_Group->flags |= GROUP_HIDDEN;
   set_current_group ();
}
/*}}}*/

static void toggle_hide_groups (void) /*{{{*/
{
   Slrn_Group_Type *g;

   Groups_Hidden = !Groups_Hidden;

   g = Groups;

   if (Groups_Hidden)
     {
	while (g != NULL)
	  {
	     if ((g->unread == 0)
		 && ((g->flags & GROUP_UNSUBSCRIBED) == 0))
	       g->flags |= GROUP_HIDDEN;

	     g = g->next;
	  }
     }
   else
     {
	while (g != NULL)
	  {
	     if ((g->flags & GROUP_UNSUBSCRIBED) == 0)
	       g->flags &= ~GROUP_HIDDEN;

	     g = g->next;
	  }
     }

   set_current_group ();
}

/*}}}*/

void slrn_hide_groups (int mode)
{
   Groups_Hidden = !mode;
   toggle_hide_groups ();
}

static void select_group_cmd (void)
{
   if (-1 == slrn_group_select_group ())
     slrn_error (_("No unread articles."));
}

void slrn_post_cmd (void) /*{{{*/
{
   char *name;
   char group[SLRL_DISPLAY_BUFFER_SIZE];
   char followupto[SLRL_DISPLAY_BUFFER_SIZE];
   char subj[SLRL_DISPLAY_BUFFER_SIZE];

   if (Slrn_Post_Obj->po_can_post == 0)
     {
	slrn_error (_("Posting not allowed."));
	return;
     }

   if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_POST) && (Slrn_Batch == 0) &&
       (slrn_get_yesno (1, _("Are you sure that you want to post")) <= 0))
     return;

   slrn_run_hooks (HOOK_POST, 0);
   if (SLang_get_error ())
     return;

   if (Slrn_Group_Current_Group == NULL)
     name = "";
   else name = Slrn_Group_Current_Group->group_name;

   if (strlen (name) >= sizeof (group))
     name = "";
   slrn_strncpy (group, name, sizeof (group));
   if (slrn_read_input (_("Newsgroup: "), NULL, group, 1, -1) <= 0) return;
   if (slrn_strbyte (group, ',') != NULL)
     {
	slrn_strncpy (followupto, name, sizeof (followupto));
	(void) slrn_read_input (_("Followup-To: "), NULL, followupto, 1, -1);
     }
   else
     *followupto = '\0';

   *subj = 0; if (slrn_read_input (_("Subject: "), NULL, subj, 1, 0) <= 0) return;

   (void) slrn_post (group, followupto, subj);
}

/*}}}*/

static void toggle_scoring (void) /*{{{*/
{
   /* Note to translators: Here, "fF" means "full", "sS" is "simple",
    * "nN" means "none" and "cC" is "cancel".
    * As always, don't change the length of the string; you cannot use
    * the default characters for different fields.
    */
   char rsp, *responses = _("fFsSnNcC");

   if (-1 == slrn_check_batch ())
     return;
   if (strlen (responses) != 8)
     responses = "";
   rsp = slrn_get_response ("fFsSnNcC\007", responses,
    _("Select scoring mode: \001Full, \001Simple, \001None, \001Cancel"));

   if (rsp != 7)
     rsp = slrn_map_translated_char ("fFsSnNcC", responses, rsp) | 0x20;
   switch (rsp)
     {
      case 'f':
	Slrn_Perform_Scoring = SLRN_XOVER_SCORING | SLRN_EXPENSIVE_SCORING;
	slrn_message (_("Full Header Scoring enabled."));
	break;

      case 's':
	Slrn_Perform_Scoring = SLRN_XOVER_SCORING;
	slrn_message (_("Expensive Scoring disabled."));
	break;

      case 'n':
	Slrn_Perform_Scoring = 0;
	slrn_message (_("Scoring disabled."));
	break;

      default:
	slrn_clear_message ();
	break;
     }
}

/*}}}*/
static void save_newsrc_cmd (void) /*{{{*/
{
   if (Slrn_Groups_Dirty)
     {
	slrn_write_newsrc (0);
     }
   else
     {
	slrn_message (_("No changes need to be saved."));
     }
   slrn_smg_refresh ();
}

/*}}}*/

/*}}}*/

/*{{{ Group Mode Initialization/Keybindings */

static void slrn_group_hup (int sig)
{
   slrn_write_newsrc (0);
   slrn_quit (sig);
}

static void enter_group_mode_hook (void)
{
   if (Slrn_Scroll_By_Page)
     Group_Window.cannot_scroll = 2;
   else
     Group_Window.cannot_scroll = SLtt_Term_Cannot_Scroll;
   slrn_run_hooks (HOOK_GROUP_MODE, 0);
}

static void group_winch_sig (int old_r, int old_c)
{
   (void) old_r; (void) old_c;

   if (SLtt_Screen_Rows > 3)
     Group_Window.nrows = SLtt_Screen_Rows - 3;
   else
     Group_Window.nrows = 1;
}

static Slrn_Mode_Type Group_Mode_Cap =
{
   NULL,
   group_update_screen,		       /* redraw */
   group_winch_sig,		       /* sig winch hook */
   slrn_group_hup,		       /* hangup hook */
   enter_group_mode_hook,	       /* enter_mode_hook */
   SLRN_GROUP_MODE
};

/*{{{ Group Mode Keybindings */

#define A_KEY(s, f)  {s, (int (*)(void)) f}
static SLKeymap_Function_Type Group_Functions [] = /*{{{*/
{
   A_KEY("add_group", add_group_cmd),
   A_KEY("bob", group_bob),
   A_KEY("catchup", catch_up),
   A_KEY("digit_arg", slrn_digit_arg),
   A_KEY("eob", group_eob),
   A_KEY("evaluate_cmd", slrn_evaluate_cmd),
   A_KEY("group_search", group_search_forward),
   A_KEY("group_search_backward", group_search_backward),
   A_KEY("group_search_forward", group_search_forward),
   A_KEY("help", slrn_group_help),
   A_KEY("line_down", group_down),
   A_KEY("line_up", group_up),
   A_KEY("move_group", move_group_cmd),
   A_KEY("page_down", group_pagedown),
   A_KEY("page_up", group_pageup),
   A_KEY("post", slrn_post_cmd),
   A_KEY("post_postponed", slrn_post_postponed),
   A_KEY("quit", slrn_group_quit),
   A_KEY("redraw", slrn_redraw),
   A_KEY("refresh_groups", refresh_groups_cmd),
   A_KEY("repeat_last_key", slrn_repeat_last_key),
   A_KEY("save_newsrc", save_newsrc_cmd),
   A_KEY("select_group", select_group_cmd),
   A_KEY("subscribe", subscribe),
   A_KEY("suspend", slrn_suspend_cmd),
   A_KEY("toggle_group_formats", toggle_group_formats),
   A_KEY("toggle_hidden", toggle_hide_groups),
   A_KEY("toggle_list_all", toggle_list_all_groups),
   A_KEY("toggle_scoring", toggle_scoring),
   A_KEY("transpose_groups", transpose_groups),
   A_KEY("uncatchup", uncatch_up),
   A_KEY("unsubscribe", unsubscribe),
#if 1 /* FIXME: These ones are going to be deleted before 1.0 */
   A_KEY("down", group_down),
   A_KEY("group_bob", group_bob),
   A_KEY("group_eob", group_eob),
   A_KEY("pagedown", group_pagedown),
   A_KEY("pageup", group_pageup),
   A_KEY("toggle_group_display", toggle_group_formats),
   A_KEY("uncatch_up", uncatch_up),
   A_KEY("up", group_up),
#endif
   A_KEY(NULL, NULL)
};

/*}}}*/

/*{{{ Mouse Functions*/

/* actions for different regions:
 *	- top status line (help)
 *	- normal region
 *	- bottom status line
 */
static void group_mouse (void (*top_status)(void),
			 void (*bot_status)(void),
			 void (*normal_region)(void)
			 )
{
   int r,c;

   slrn_get_mouse_rc (&r, &c);

   /* take top status line into account */
   if (r == 1)
     {
	if (Slrn_Use_Mouse)
	  slrn_execute_menu (c);
	else
	  if (NULL != top_status) (*top_status) ();
 	return;
     }

   if (r >= SLtt_Screen_Rows)
     return;

   /* bottom status line */
   if (r == SLtt_Screen_Rows - 1)
     {
	if (NULL != bot_status) (*bot_status) ();
	return;
     }

   r -= (1 + Last_Cursor_Row);
   if (r < 0)
     {
	r = -r;
	if (r != (int) slrn_group_up_n (r)) return;
     }
   else if (r != (int) slrn_group_down_n (r)) return;

   if (NULL != normal_region) (*normal_region) ();
}

static void group_mouse_left (void)
{
   group_mouse (slrn_group_help, group_pagedown, select_group_cmd);
}

static void group_mouse_middle (void)
{
   group_mouse (toggle_group_formats, toggle_hide_groups, select_group_cmd);
#if 1
   /* Make up for buggy rxvt which have problems with the middle key. */
   if (NULL != getenv ("COLORTERM"))
     {
	if (SLang_input_pending (7))
	  {
	     while (SLang_input_pending (0))
	       (void) SLang_getkey ();
	  }
     }
#endif
}

static void group_mouse_right (void)
{
   group_mouse (slrn_group_help, group_pageup, select_group_cmd);
}

/*}}}*/

/*}}}*/

#define USE_TEST_FUNCTION 0
#if USE_TEST_FUNCTION
static void test_function (void)
{
   char *file;
   file = slrn_browse_dir (".");
   if (file != NULL)
     {
	slrn_message (file);
	slrn_free (file);
     }
}
#endif

void slrn_init_group_mode (void) /*{{{*/
{
   char  *err = _("Unable to create group keymap!");

   if (NULL == (Slrn_Group_Keymap = SLang_create_keymap ("group", NULL)))
     slrn_exit_error ("%s", err);

   Group_Mode_Cap.keymap = Slrn_Group_Keymap;

   Slrn_Group_Keymap->functions = Group_Functions;

   SLkm_define_key ("\0331", (FVOID_STAR) slrn_digit_arg, Slrn_Group_Keymap);
   SLkm_define_key ("\0332", (FVOID_STAR) slrn_digit_arg, Slrn_Group_Keymap);
   SLkm_define_key ("\0333", (FVOID_STAR) slrn_digit_arg, Slrn_Group_Keymap);
   SLkm_define_key ("\0334", (FVOID_STAR) slrn_digit_arg, Slrn_Group_Keymap);
   SLkm_define_key ("\0335", (FVOID_STAR) slrn_digit_arg, Slrn_Group_Keymap);
   SLkm_define_key ("\0336", (FVOID_STAR) slrn_digit_arg, Slrn_Group_Keymap);
   SLkm_define_key ("\0337", (FVOID_STAR) slrn_digit_arg, Slrn_Group_Keymap);
   SLkm_define_key ("\0338", (FVOID_STAR) slrn_digit_arg, Slrn_Group_Keymap);
   SLkm_define_key ("\0339", (FVOID_STAR) slrn_digit_arg, Slrn_Group_Keymap);
   SLkm_define_key ("\0330", (FVOID_STAR) slrn_digit_arg, Slrn_Group_Keymap);
   SLkm_define_key  ("^K\033[A", (FVOID_STAR) group_bob, Slrn_Group_Keymap);
   SLkm_define_key  ("^K\033OA", (FVOID_STAR) group_bob, Slrn_Group_Keymap);
   SLkm_define_key  ("^K\033[B", (FVOID_STAR) group_eob, Slrn_Group_Keymap);
   SLkm_define_key  ("^K\033OB", (FVOID_STAR) group_eob, Slrn_Group_Keymap);
   SLkm_define_key  ("\033a", (FVOID_STAR) toggle_group_formats, Slrn_Group_Keymap);
   SLkm_define_key  ("\033>", (FVOID_STAR) group_eob, Slrn_Group_Keymap);
   SLkm_define_key  ("\033<", (FVOID_STAR) group_bob, Slrn_Group_Keymap);
   SLkm_define_key  ("^D", (FVOID_STAR) group_pagedown, Slrn_Group_Keymap);
   SLkm_define_key  ("^V", (FVOID_STAR) group_pagedown, Slrn_Group_Keymap);
#if defined(IBMPC_SYSTEM)
   SLkm_define_key  ("^@Q", (FVOID_STAR) group_pagedown, Slrn_Group_Keymap);
   SLkm_define_key  ("\xE0Q", (FVOID_STAR) group_pagedown, Slrn_Group_Keymap);
   SLkm_define_key  ("^@I", (FVOID_STAR) group_pageup, Slrn_Group_Keymap);
   SLkm_define_key  ("\xE0I", (FVOID_STAR) group_pageup, Slrn_Group_Keymap);
#else
   SLkm_define_key  ("\033[6~", (FVOID_STAR) group_pagedown, Slrn_Group_Keymap);
   SLkm_define_key  ("\033[G", (FVOID_STAR) group_pagedown, Slrn_Group_Keymap);
   SLkm_define_key  ("\033[5~", (FVOID_STAR) group_pageup, Slrn_Group_Keymap);
   SLkm_define_key  ("\033[I", (FVOID_STAR) group_pageup, Slrn_Group_Keymap);
#endif
   SLkm_define_key  ("m", (FVOID_STAR) move_group_cmd, Slrn_Group_Keymap);
   SLkm_define_key  ("^U", (FVOID_STAR) group_pageup, Slrn_Group_Keymap);
   SLkm_define_key  ("\033V", (FVOID_STAR) group_pageup, Slrn_Group_Keymap);
   SLkm_define_key  ("a", (FVOID_STAR) add_group_cmd, Slrn_Group_Keymap);
   SLkm_define_key  ("u", (FVOID_STAR) unsubscribe, Slrn_Group_Keymap);
   SLkm_define_key  ("s", (FVOID_STAR) subscribe, Slrn_Group_Keymap);
   SLkm_define_key  ("\033u", (FVOID_STAR) uncatch_up, Slrn_Group_Keymap);
   SLkm_define_key  ("c", (FVOID_STAR) catch_up, Slrn_Group_Keymap);
   SLkm_define_key  ("K", (FVOID_STAR) toggle_scoring, Slrn_Group_Keymap);
   SLkm_define_key  ("L", (FVOID_STAR) toggle_list_all_groups, Slrn_Group_Keymap);
   SLkm_define_key  ("l", (FVOID_STAR) toggle_hide_groups, Slrn_Group_Keymap);
   SLkm_define_key  ("^Z", (FVOID_STAR) slrn_suspend_cmd, Slrn_Group_Keymap);
   SLkm_define_key  (" ", (FVOID_STAR) select_group_cmd, Slrn_Group_Keymap);
   SLkm_define_key  (".", (FVOID_STAR) slrn_repeat_last_key, Slrn_Group_Keymap);
   SLkm_define_key  ("P", (FVOID_STAR) slrn_post_cmd, Slrn_Group_Keymap);
   SLkm_define_key  ("\033P", (FVOID_STAR) slrn_post_postponed, Slrn_Group_Keymap);
   SLkm_define_key  ("?", (FVOID_STAR) slrn_group_help, Slrn_Group_Keymap);
   SLkm_define_key  ("\r", (FVOID_STAR) select_group_cmd, Slrn_Group_Keymap);
   SLkm_define_key  ("q", (FVOID_STAR) slrn_group_quit, Slrn_Group_Keymap);
   SLkm_define_key  ("^X^C", (FVOID_STAR) slrn_group_quit, Slrn_Group_Keymap);
   SLkm_define_key  ("^X^T", (FVOID_STAR) transpose_groups, Slrn_Group_Keymap);
   SLkm_define_key  ("^X^[", (FVOID_STAR) slrn_evaluate_cmd, Slrn_Group_Keymap);
   SLkm_define_key  ("^R", (FVOID_STAR) slrn_redraw, Slrn_Group_Keymap);
   SLkm_define_key  ("^L", (FVOID_STAR) slrn_redraw, Slrn_Group_Keymap);
   SLkm_define_key  ("^P", (FVOID_STAR) group_up, Slrn_Group_Keymap);
#if defined(IBMPC_SYSTEM)
   SLkm_define_key  ("^@H", (FVOID_STAR) group_up, Slrn_Group_Keymap);
   SLkm_define_key  ("\xE0H", (FVOID_STAR) group_up, Slrn_Group_Keymap);
   SLkm_define_key  ("^@P", (FVOID_STAR) group_down, Slrn_Group_Keymap);
   SLkm_define_key  ("\xE0P", (FVOID_STAR) group_down, Slrn_Group_Keymap);
#else
   SLkm_define_key  ("\033[A", (FVOID_STAR) group_up, Slrn_Group_Keymap);
   SLkm_define_key  ("\033OA", (FVOID_STAR) group_up, Slrn_Group_Keymap);
   SLkm_define_key  ("\033[B", (FVOID_STAR) group_down, Slrn_Group_Keymap);
   SLkm_define_key  ("\033OB", (FVOID_STAR) group_down, Slrn_Group_Keymap);
#endif
   SLkm_define_key  ("N", (FVOID_STAR) group_down, Slrn_Group_Keymap);
   SLkm_define_key  ("^N", (FVOID_STAR) group_down, Slrn_Group_Keymap);
   SLkm_define_key  ("/", (FVOID_STAR) group_search_forward, Slrn_Group_Keymap);
   SLkm_define_key  ("\\", (FVOID_STAR) group_search_backward, Slrn_Group_Keymap);
   SLkm_define_key  ("G", (FVOID_STAR) refresh_groups_cmd, Slrn_Group_Keymap);
   SLkm_define_key  ("X", (FVOID_STAR) save_newsrc_cmd, Slrn_Group_Keymap);

   /* mouse (left/right/middle) */
   SLkm_define_key  ("\033[M\040", (FVOID_STAR) group_mouse_left, Slrn_Group_Keymap);
   SLkm_define_key  ("\033[M\041", (FVOID_STAR) group_mouse_middle, Slrn_Group_Keymap);
   SLkm_define_key  ("\033[M\042", (FVOID_STAR) group_mouse_right, Slrn_Group_Keymap);
   SLkm_define_key  ("\033[M\043", (FVOID_STAR) group_mouse_left, Slrn_Group_Keymap);
   SLkm_define_key  ("\033[M\044", (FVOID_STAR) group_mouse_left, Slrn_Group_Keymap);
#if USE_TEST_FUNCTION
   SLkm_define_key  ("y", (FVOID_STAR) test_function, Slrn_Group_Keymap);
#endif
   if (SLang_get_error ()) slrn_exit_error ("%s", err);
}

/*}}}*/

/*}}}*/

int slrn_select_group_mode (void) /*{{{*/
{
   init_group_win_struct ();

   Last_Cursor_Row = 0;
   group_quick_help ();
   Slrn_Full_Screen_Update = 1;

   slrn_push_mode (&Group_Mode_Cap);
   return 0;
}

/*}}}*/

/*{{{ Read/Write Newsrc, Group Descriptions */

static void add_group_description (unsigned char *s, unsigned char *smax, /*{{{*/
				   unsigned char *dsc)
{
   Slrn_Group_Type *g;
   unsigned long hash;

   hash = slrn_compute_hash (s, smax);
   g = Group_Hash_Table[hash % GROUP_HASH_TABLE_SIZE];
   while (g != NULL)
     {
	if ((g->hash == hash) && (!strncmp (g->group_name, (char *) s, (unsigned int) (smax - s))))
	  {
	     /* Sometimes these get repeated --- not by slrn but on the server! */
	     slrn_free (g->descript);

	     g->descript = slrn_strmalloc ((char *) dsc, 0);
	     /* Ok to fail. */
	     return;
	  }
	g = g->hash_next;
     }
}

/*}}}*/
void slrn_get_group_descriptions (void) /*{{{*/
{
   FILE *fp;
   char line[2 * SLRN_MAX_PATH_LEN];
   char file[SLRN_MAX_PATH_LEN];
   int num;

#ifdef VMS
   slrn_snprintf (file, sizeof (file), "%s-dsc", Slrn_Newsrc_File);
#else
# ifdef SLRN_USE_OS2_FAT
   slrn_os2_make_fat (file, sizeof (file), Slrn_Newsrc_File, ".dsc");
# else
   slrn_snprintf (file, sizeof (file), "%s.dsc", Slrn_Newsrc_File);
# endif
#endif

   if (NULL == (fp = fopen (file, "w")))
     {
	slrn_exit_error (_("\
Unable to create newsgroup description file:\n%s\n"), file);
     }

   fprintf (stdout, _("\nCreating description file %s.\n"), file);
   fprintf (stdout, _("Getting newsgroup descriptions from server.\n\
Note: This step may take some time if you have a slow connection!!!\n"));

   fflush (stdout);

   if (OK_GROUPS != Slrn_Server_Obj->sv_list_newsgroups ())
     {
	slrn_error (_("Server failed on list newsgroups command."));
	slrn_fclose (fp);
	return;
     }

   num = 0;

   while (1)
     {
	unsigned char *b, *bmax, *dsc, ch;
	int status;

	status = Slrn_Server_Obj->sv_read_line (line, sizeof (line));
	if (status == -1)
	  {
	     slrn_error (_("Error reading line from server"));
	     slrn_fclose (fp);
	     return;
	  }
	if (status == 0)
	  break;

	num = num % 25;
	if (num == 0)
	  {
	     putc ('.', stdout);
	     fflush (stdout);
	  }

	/* Check the syntax on this line. They are often corrupt */
	b = (unsigned char *) slrn_skip_whitespace (line);

	bmax = b;
	while ((ch = *bmax) > ' ') bmax++;
	if ((ch == 0) || (ch == '\n')) continue;
	*bmax = 0;

	/* News group marked off, now get the description. */
	dsc = bmax + 1;
	while (((ch = *dsc) <= ' ') && (ch != 0)) dsc++;
	if ((ch == 0) || (ch == '?') || (ch == '\n')) continue;

	/* add_group_description (b, bmax, dsc); */

	fputs ((char *) b, fp);
	putc(':', fp);
	fputs ((char *) dsc, fp);
	putc('\n', fp);

	num++;
     }
   slrn_fclose (fp);
   putc ('\n', stdout);
}

/*}}}*/
int slrn_read_group_descriptions (void) /*{{{*/
{
   FILE *fp;
   char line[2 * SLRN_MAX_PATH_LEN];
   char file[SLRN_MAX_PATH_LEN];

#ifdef VMS
   slrn_snprintf (file, sizeof (file), "%s-dsc", Slrn_Newsrc_File);
#else
# ifdef SLRN_USE_OS2_FAT
   slrn_os2_make_fat (file, sizeof (file), Slrn_Newsrc_File, ".dsc");
# else
   slrn_snprintf (file, sizeof (file), "%s.dsc", Slrn_Newsrc_File);
# endif
#endif

   if (NULL == (fp = fopen (file, "r")))
     {
#ifdef VMS
	slrn_snprintf (file, sizeof (file), "%snewsgroups.dsc",
		       SLRN_LIB_DIR);
	if (NULL == (fp = fopen (file, "r")))
	  {
	     slrn_snprintf (file, sizeof (file), "%snewsgroups-dsc",
			    SLRN_LIB_DIR);
	     fp = fopen (file, "r");
	  }
#else
	slrn_snprintf (file, sizeof (file), "%s/newsgroups.dsc",
		       SLRN_LIB_DIR);
	fp = fopen (file, "r");
#endif
	if (fp == NULL) return -1;
     }

   while (NULL != fgets (line, sizeof (line) - 1, fp))
     {
	unsigned char *bmax, *dsc, ch;

	bmax = (unsigned char *) line;
	while (((ch = *bmax) != ':') && ch) bmax++;
	if (ch <= ' ') continue;
	*bmax = 0;

	dsc = bmax + 1;
	add_group_description ((unsigned char *) line, bmax, dsc);
     }
   slrn_fclose (fp);
   return 0;
}

/*}}}*/

/* Map 1998 --> 98, 2003 --> 3, etc.... */
static int rfc977_patchup_year (int year)
{
   return year - 100 * (year / 100);
}

int slrn_get_new_groups (int create_flag) /*{{{*/
{
   FILE *fp;
   time_t tloc;
   struct tm *tm_struct;
   char line[NNTP_BUFFER_SIZE];
   char file[SLRN_MAX_PATH_LEN];
   int num;
   char *p;
   int parse_error = 0;

#ifdef VMS
   slrn_snprintf (file, sizeof (file), "%s-time", Slrn_Newsrc_File);
#else
# ifdef SLRN_USE_OS2_FAT
   slrn_os2_make_fat (file, sizeof (file), Slrn_Newsrc_File, ".tim");
# else
   slrn_snprintf (file, sizeof (file), "%s.time", Slrn_Newsrc_File);
# endif
#endif

   if ((create_flag == 0)
       && (NULL != (fp = fopen (file, "r"))))
     {
	char ch;
	int i;
	*line = 0;
	fgets (line, sizeof (line), fp);
	slrn_fclose (fp);

	slrn_message_now (_("Checking for new groups ..."));

	time (&tloc);
	parse_error = 1;

	/* parse this line to make sure it is ok.  If it is bad, issue a warning
	 * and go on.
	 */
	if (strncmp ("NEWGROUPS ", line, 10)) goto parse_error_label;
	p = line + 10;

	p = slrn_skip_whitespace (p);

	/* parse yymmdd */
	for (i = 0; i < 6; i++)
	  {
	     ch = p[i];
	     if ((ch < '0') || (ch > '9')) goto parse_error_label;
	  }
	if (p[6] != ' ') goto parse_error_label;

	ch = p[2];
	if (ch > '1') goto parse_error_label;
	if ((ch == '1') && (p[3] > '2')) goto parse_error_label;
	ch = p[4];
	if (ch > '3') goto parse_error_label;
	if ((ch == '3') && (p[5] > '1')) goto parse_error_label;

	/* Now the hour: hhmmss */
	p = slrn_skip_whitespace (p + 6);

	for (i = 0; i < 6; i++)
	  {
	     ch = p[i];
	     if ((ch < '0') || (ch > '9')) goto parse_error_label;
	  }
	ch = p[0];
	if (ch > '2') goto parse_error_label;
	if ((ch == '2') && (p[1] > '3')) goto parse_error_label;
	if ((p[2] > '5') || (p[4] > '5')) goto parse_error_label;

	p = slrn_skip_whitespace (p + 6);

	if ((p[0] == 'G') && (p[1] == 'M') && (p[2] == 'T'))
	  p += 3;
	*p = 0;

	parse_error = 0;

	switch (Slrn_Server_Obj->sv_put_server_cmd (line, line, sizeof (line)))
	  {
	   case OK_NEWGROUPS:
	     break;

	   case ERR_FAULT:
	     return -1;

	   case ERR_COMMAND:
	     slrn_message (_("Server does not implement NEWGROUPS command."));
	     return 0;

	   default:
	     slrn_message (_("Server failed to return proper response to NEWGROUPS:\n%s\n"),
			   line);
	     goto parse_error_label;
	  }

	num = 0;

	while (1)
	  {
	     int status = Slrn_Server_Obj->sv_read_line (line, sizeof (line));
	     if (status == 0)
	       break;
	     if (status == -1)
	       {
		  slrn_error (_("Read from server failed"));
		  return -1;
	       }

	     /* line contains new newsgroup name */
	     add_unsubscribed_group ((unsigned char*)line);
	     num++;
	  }

	if (num)
	  {
	     slrn_message (_("%d new newsgroup(s) found."), num);
	  }
     }
   else time (&tloc);

   parse_error_label:
   if (parse_error)
     {
	slrn_message (_("\
%s appears corrupt.\n\
I expected to see: NEWGROUPS yymmdd hhmmss GMT\n\
I will patch the file up for you.\n"), file);
     }

#ifdef VMS
   if (NULL == (fp = fopen (file, "w", "fop=cif")))
#else
   if (NULL == (fp = fopen (file, "w")))
#endif
     {
	slrn_exit_error (_("Unable to open %s to record date."), file);
     }

   /* According to rfc977, the year must be in the form YY with the closest
    * century specifying the rest of the year, i.e., 99 is 1999, 30 is 2030,
    * etc.
    */
#if defined(VMS) || defined(__BEOS__)
   /* gmtime is broken on BEOS */
   tm_struct = localtime (&tloc);
   tm_struct->tm_year = rfc977_patchup_year (tm_struct->tm_year + 1900);
   fprintf (fp, "NEWGROUPS %02d%02d%02d %02d%02d%02d",
            tm_struct->tm_year, 1 + tm_struct->tm_mon,
            tm_struct->tm_mday, tm_struct->tm_hour,
            tm_struct->tm_min, tm_struct->tm_sec);
#else
   tm_struct = gmtime (&tloc);
   tm_struct->tm_year = rfc977_patchup_year (tm_struct->tm_year + 1900);
   fprintf (fp, "NEWGROUPS %02d%02d%02d %02d%02d%02d GMT",
	    tm_struct->tm_year, 1 + tm_struct->tm_mon,
	    tm_struct->tm_mday, tm_struct->tm_hour,
	    tm_struct->tm_min, tm_struct->tm_sec);
#endif
   slrn_fclose (fp);
   return 0;
}

/*}}}*/

static int parse_active_line (unsigned char *name, unsigned int *lenp, /*{{{*/
			      int *minp, int *maxp)
{
   unsigned char *p;

   p = name;
   while (*p > ' ') p++;
   *lenp = (unsigned int) (p - name);

   while (*p == ' ') p++;
   *maxp = atoi ((char*)p);
   while (*p > ' ') p++;
   while (*p == ' ') p++;
   *minp = atoi((char*)p);
   if (*maxp < *minp) *minp = *maxp + 1;
   return 0;
}

/*}}}*/

static char *Subscriptions [] =
{
   "news.answers",
     "news.announce.newusers",
     "news.newusers.questions",
     "news.groups.questions",
     "news.software.readers",
     "alt.test",
     NULL
};

static void read_and_parse_active (int create_flag) /*{{{*/
{
   char line[NNTP_BUFFER_SIZE];
   int count = 0, ret;
   int initial_run = (Groups == NULL);

   if (OK_GROUPS != (ret = Slrn_Server_Obj->sv_list_active (NULL)))
     {
	if (ret == ERR_NOAUTH)
	  slrn_exit_error (_("Server failed LIST ACTIVE - authorization missing."));
	slrn_exit_error (_("Server failed LIST ACTIVE."));
     }

   while (1)
     {
	unsigned int len;
	int min, max;
	int status;

	status = Slrn_Server_Obj->sv_read_line (line, sizeof(line));
	if (status == -1)
	  {
	     /*	     Slrn_Groups_Dirty = 0;*/ /* Huh? */
	     slrn_exit_error (_("Read from server failed"));
	  }
	if (status == 0)
	  break;

	parse_active_line ((unsigned char *)line, &len, &min, &max);

	if (!initial_run)
	  {
	     Slrn_Group_Type *g = find_group_entry (line, len);
	     if (g != NULL)
	       {
		  group_update_range (g, min, max);
		  continue;
	       }
	  }

	if (NULL == create_group_entry (line, len, min, max, 0, 0))
	  continue;

	if (create_flag)
	  {
	     count++;
	     count = count % 50;
	     if (count == 0)
	       {
		  putc ('.', stdout);
		  fflush (stdout);
	       }
	     add_group (line, len, GROUP_UNSUBSCRIBED, 0, 1);
	  }
     }

   if (create_flag)
     {
	char *name;
	unsigned int i = 0;
	Slrn_Group_Type *save = Slrn_Group_Current_Group, *last = NULL;

	if (OK_GROUPS == Slrn_Server_Obj->sv_list ("SUBSCRIPTIONS"))
	  {
	     name = line;
	     if (Slrn_Server_Obj->sv_read_line (line, sizeof (line)) <= 0)
	       name = NULL;
	  }
	else
	  name = *Subscriptions;

	while (name != NULL)
	  {
	     if (NULL != (Slrn_Group_Current_Group = find_group_entry (name, strlen (name))))
	       {
		  Slrn_Group_Current_Group->flags &= ~GROUP_HIDDEN;
		  Slrn_Group_Current_Group->flags &= ~GROUP_UNSUBSCRIBED;
		  last = place_group_in_newsrc_order (last);
	       }

	     if (name != line)
	       name = Subscriptions[++i];
	     else if (Slrn_Server_Obj->sv_read_line (line, sizeof (line)) <= 0)
	       name = NULL;
	  }

	Slrn_Group_Current_Group = save;
	Slrn_Groups_Dirty = 1;
	Slrn_Write_Newsrc_Flags = 0;
     }

   Kill_After_Max = 0;
}

/*}}}*/

static int read_and_parse_newsrc_file (void)
{
   VFILE *vp;
   char *vline;
   unsigned int vlen;
   char file[SLRN_MAX_PATH_LEN];
   char *newsrc_filename = Slrn_Newsrc_File;
   Slrn_Group_Type *last_group = NULL;
   int ret_stat_o, ret_stat_as;
   struct stat st_o, st_as;

#ifdef VMS
   slrn_snprintf (file, sizeof (file), "%s-as", Slrn_Newsrc_File);
#else
# ifdef SLRN_USE_OS2_FAT
   slrn_os2_make_fat (file, sizeof (file), Slrn_Newsrc_File, ".as");
# else
   slrn_snprintf (file, sizeof (file), "%s.as", Slrn_Newsrc_File);
# endif
#endif

   ret_stat_o = stat (newsrc_filename, &st_o);
   ret_stat_as = stat (file, &st_as);

   if ((ret_stat_as != -1) &&
       (((ret_stat_o == -1) || (st_as.st_mtime > st_o.st_mtime))))
     {
	slrn_message (_("\n* The autosave file of %s is newer than the file "
		      "itself.\n"), newsrc_filename);
	if (slrn_get_yesno (1, _("Do you want to restore your newsrc from "
			    "the autosave version")))
	  {
	     newsrc_filename = file;
	  }
     }

   if (NULL == (vp = vopen (newsrc_filename, 4096, 0)))
#if 0
       && (NULL == (vp = slrn_open_home_vfile (".newsrc", file,
					       sizeof (file))))
#endif
       slrn_exit_error (_("Unable to open %s."), newsrc_filename);

   while (NULL != (vline = vgets (vp, &vlen)))
     {
	char *p = vline;
	char *pmax = p + vlen;
	char ch = 0;

	while ((p < pmax)
	       && ((ch = *p) != '!') && (ch != ':'))
	  p++;

	if ((p == pmax) || (p == vline))
	  continue;

	if (vline[vlen-1] == '\n')
	  vline[vlen-1] = 0;
	else
	  vline[vlen] = 0;

	if (-1 == add_group (vline, (unsigned int) (p - vline),
			     ((ch == '!') ? GROUP_UNSUBSCRIBED : 0), 0, 0))
	  continue;

	/* perform a re-arrangement to match arrangement in the
	 * newsrc file
	 */
	/*if (Slrn_List_Active_File && (ch !=  '!'))*/
	  last_group = place_group_in_newsrc_order (last_group);
     }
   vclose (vp);
   if (!Slrn_List_Active_File)
     refresh_groups (&Slrn_Group_Current_Group);
   return 0;
}

int slrn_read_newsrc (int create_flag) /*{{{*/
{
   slrn_message_now (_("Checking news%s ..."),
		     Slrn_List_Active_File ? _(" via active file") : "");

   if (create_flag)
     {
	FILE *fp;

	/* See if we can open the file. */
#ifdef VMS
	if (NULL == (fp = fopen (Slrn_Newsrc_File, "w", "fop=cif")))
#else
	if (NULL == (fp = fopen (Slrn_Newsrc_File, "w")))
#endif
	  {
	     slrn_exit_error (_("Unable to create %s."), Slrn_Newsrc_File);
	  }
	fclose (fp);
	fputs (_("\n--The next step may take a while if the NNTP connection is slow.--\n\n"), stdout);
	fprintf (stdout, _("Creating %s."), Slrn_Newsrc_File);
	fflush (stdout);
     }

   if (create_flag || Slrn_List_Active_File)
     {
	read_and_parse_active (create_flag);
     }

   if ((create_flag == 0)
       && (-1 == read_and_parse_newsrc_file ()))
     slrn_exit_error (_("Unable to read newsrc file"));

   insert_new_groups ();

   Slrn_Group_Current_Group = Groups;

   init_group_win_struct ();

   toggle_hide_groups ();

   /* Unhide the new groups.  Do it here so that if there are no unread
    * articles, it will be visible but also enables user to toggle them
    * so that they will become invisble again.
    */
   Slrn_Group_Current_Group = Groups;
   while ((Slrn_Group_Current_Group != NULL) &&
	  (create_flag || (Slrn_Group_Current_Group->flags & GROUP_NEW_GROUP_FLAG)))
     {
	Slrn_Group_Current_Group->flags &= ~GROUP_HIDDEN;
	Slrn_Group_Current_Group = Slrn_Group_Current_Group->next;
     }

   Slrn_Group_Current_Group = Groups;
   while ((Slrn_Group_Current_Group != NULL)
	  && (Slrn_Group_Current_Group->flags & GROUP_HIDDEN))
     Slrn_Group_Current_Group = Slrn_Group_Current_Group->next;

   find_line_num ();

   group_bob ();
   return 0;
}

/*}}}*/

int slrn_write_newsrc (int auto_save) /*{{{*/
/* auto_save == 0: save to newsrc, == 1: save to autosave file. */
{
   Slrn_Group_Type *g;
   Slrn_Range_Type *r;
   char autosave_file[SLRN_MAX_PATH_LEN];
   char *newsrc_filename;
   static FILE *fp;
   int max;
   struct stat filestat;
   int stat_worked = 0;
   int have_backup = 0;

   slrn_init_hangup_signals (0);

   if (Slrn_Groups_Dirty == 0)
     {
	slrn_init_hangup_signals (1);
	return 0;
     }

   /* In case of hangup and we were writing the file, make sure it is closed.
    * This will not hurt since we are going to do it again anyway.
    */
   if (fp != NULL)
     {
	slrn_fclose (fp);
	fp = NULL;
     }

#ifdef VMS
   slrn_snprintf (autosave_file, sizeof (autosave_file), "%s-as",
		  Slrn_Newsrc_File);
#else
# ifdef SLRN_USE_OS2_FAT
   slrn_os2_make_fat (autosave_file, sizeof (autosave_file), Slrn_Newsrc_File,
		      ".as");
# else
   slrn_snprintf (autosave_file, sizeof (autosave_file), "%s.as",
		  Slrn_Newsrc_File);
# endif
#endif

   if ((auto_save == 1) && (Slrn_No_Autosave != 2))
     {
	if (Slrn_No_Autosave)
	  {
	     slrn_init_hangup_signals (1);
	     return 0;
	  }

	newsrc_filename = autosave_file;
	stat_worked = (-1 != stat (newsrc_filename, &filestat));
     }
   else
     newsrc_filename = Slrn_Newsrc_File;

   slrn_message_now (_("Writing %s ..."), newsrc_filename);
   /* Try to preserve .newsrc permissions and owner/group.  This also
    * confirms existence of file.  If an autosave file does not (yet)
    * exist, use permissions of the .newsrc.
    */
   if (stat_worked == 0)
     stat_worked = (-1 != stat (Slrn_Newsrc_File, &filestat));

   if (newsrc_filename == Slrn_Newsrc_File)
     {
	/* Create a temp backup file.  Delete it later if user
	 * does not want backups.
	 */
	have_backup = (0 == slrn_create_backup (newsrc_filename));
     }

   if (NULL == (fp = fopen (newsrc_filename, "w")))
     {
	slrn_error (_("Unable to open file %s for writing."), newsrc_filename);
	if (have_backup) slrn_restore_backup (newsrc_filename);
	slrn_init_hangup_signals (1);
	return -1;
     }

#ifdef __unix__
# if !defined(IBMPC_SYSTEM)
   /* Try to preserve .newsrc permissions and owner/group */
#  ifndef S_IRUSR
#   define S_IRUSR 0400
#   define S_IWUSR 0200
#   define S_IXUSR 0100
#  endif
   if (stat_worked)
     {
	if (-1 == chmod (newsrc_filename, filestat.st_mode & (S_IRUSR | S_IWUSR | S_IXUSR)))
	  (void) chmod (newsrc_filename, S_IWUSR | S_IRUSR);

	(void) chown (newsrc_filename, filestat.st_uid, filestat.st_gid);
     }
# endif
#endif

   g = Groups;
   while (g != NULL)
     {
	if ((g->flags & GROUP_UNSUBSCRIBED) &&
	    ((Slrn_Write_Newsrc_Flags == 1) ||
	    ((Slrn_Write_Newsrc_Flags == 2) && (g->range.next == NULL))))
	  {
	     g = g->next;
	     continue;
	  }
	if ((EOF == fputs (g->group_name, fp)) ||
	    (EOF == putc (((g->flags & GROUP_UNSUBSCRIBED) ? '!' : ':'), fp)))
	  goto write_error;

	r = g->range.next;
	max = g->range.max;
	if (r != NULL)
	  {
	     NNTP_Artnum_Type max_newsrc_number=0;
	     /* Make this check because the unsubscribed group
	      * range may not have been initialized from the server.
	      */
	     if ((max != -1) && (g->range.min != -1)
		 && ((g->flags & GROUP_UNSUBSCRIBED) == 0))
	       max_newsrc_number = max;

	     if (EOF == putc (' ', fp))
	       goto write_error;

	     if (-1 == slrn_ranges_to_newsrc_file (r, max_newsrc_number, fp))
	       goto write_error;
	  }
	else if (g->range.min == 2)
	  {
	     if (EOF == fputs (" 1", fp))
	       goto write_error;
	  }
	else if (g->range.min > 2)
	  {
	     if (fprintf (fp, (" 1-" NNTP_FMT_ARTNUM), g->range.min - 1) < 0)
	       goto write_error;
	  }

	if (EOF == putc ('\n', fp))
	  goto write_error;
	g = g->next;
     }

   if (-1 == slrn_fclose (fp))
     goto write_error;
   fp = NULL;

   if (Slrn_No_Backups)
     {
	if (have_backup) slrn_delete_backup (newsrc_filename);
     }

   if (newsrc_filename == Slrn_Newsrc_File)
     {
	Slrn_Groups_Dirty = 0;
	if (Slrn_No_Autosave == 0)
	  slrn_delete_file (autosave_file);
     }
   if (Slrn_TT_Initialized & SLRN_TTY_INIT)
     slrn_message (_("Writing %s ... done."), newsrc_filename);

   slrn_init_hangup_signals (1);
   return 0;

   write_error:

   slrn_fclose (fp); fp = NULL;
   slrn_error (_("Write to %s failed! Disk Full?"), newsrc_filename);

   /* Put back orginal file */
   if (have_backup) slrn_restore_backup (newsrc_filename);

   slrn_init_hangup_signals (1);
   return -1;
}

/*}}}*/

/*}}}*/

static void group_quick_help (void) /*{{{*/
{
   char *hlp = _("SPC:Select  p:Post  c:CatchUp  l:List  q:Quit  ^R:Redraw  (u)s:(Un)Subscribe");

   if (Slrn_Batch)
     return;

   if (Slrn_Group_Help_Line != NULL)
     hlp = Slrn_Group_Help_Line;

   if (0 == slrn_message ("%s", hlp))
     Slrn_Message_Present = 0;
}

/*}}}*/

static char *group_display_format_cb (char ch, void *data, int *len, int *color) /*{{{*/
{
   Slrn_Group_Type *g = (Slrn_Group_Type*) data;
   static char buf[512];
   char *retval = buf;

   *retval = 0;
   if (g == NULL) return retval;

   switch (ch)
     {
      case 'F':
	if (g->flags & GROUP_UNSUBSCRIBED) retval = "U";
	else if (g->flags & GROUP_NEW_GROUP_FLAG) retval = "N";
	else retval = " ";
	*len = 1;
	break;
      case 'd':
	if (NULL == (retval = g->descript))
	  retval = " ";
	if (color != NULL) *color = GROUP_DESCR_COLOR;
	break;
      case 'h':
	if (g->range.max == -1)
	  {
	     retval = "?";
	     *len = 1;
	  }
	else
#ifdef HAVE_ANSI_SPRINTF
	  *len =
#endif
	  sprintf (buf, NNTP_FMT_ARTNUM, g->range.max); /* safe */
	break;
      case 'l':
	if (g->range.min == -1)
	  {
	     retval = "?";
	     *len = 1;
	  }
	else
#ifdef HAVE_ANSI_SPRINTF
	  *len =
#endif
	  sprintf (buf, NNTP_FMT_ARTNUM, g->range.min); /* safe */
	break;
      case 'n':
	retval = g->group_name;
	if (color != NULL) *color = GROUP_COLOR;
	break;
      case 't':
	if (g->range.max == -1)
	  {
	     retval = "?";
	     *len = 1;
	  }
	else
#ifdef HAVE_ANSI_SPRINTF
	  *len =
#endif
	  sprintf (buf, NNTP_FMT_ARTNUM, g->range.max - g->range.min + 1); /* safe */
	break;
      case 'u':
#ifdef HAVE_ANSI_SPRINTF
	*len =
#endif
	sprintf (buf, NNTP_FMT_ARTNUM, g->unread); /* safe */
	break;
     }

   return retval;
}
/*}}}*/

static char *group_status_line_cb (char ch, void *data, int *len, int *color) /*{{{*/
{
   static char buf[66];
   char *retval = NULL;

   (void) data; (void) color; /* we currently don't use these */

   switch (ch)
     {
      case 'L':
	retval = slrn_print_percent (buf, &Group_Window, 1);
	break;
      case 'P':
	retval = slrn_print_percent (buf, &Group_Window, 0);
	break;
      case 'D':
	retval = Slrn_Groups_Dirty ? "*" : "-";
	*len = 1;
	break;
      case 's':
	retval = Slrn_Server_Obj->sv_name;
	break;
      default:
	if (Slrn_Group_Current_Group != NULL)
	  retval = group_display_format_cb (ch,
					    (void *) Slrn_Group_Current_Group,
					    len, NULL);
	break;
     }

   return retval;
}
/*}}}*/

static void group_update_screen (void) /*{{{*/
{
   Slrn_Group_Type *g;
   char *fmt = Group_Display_Formats[Group_Format_Number];
   int height = (int) Group_Window.nrows;
   int row;

   /* erase last cursor */
   if (Last_Cursor_Row && !Slrn_Full_Screen_Update)
     {
	SLsmg_gotorc (Last_Cursor_Row, 0);
	SLsmg_write_string ("  ");
     }

   g = (Slrn_Group_Type *) Group_Window.top_window_line;
   (void) SLscroll_find_top (&Group_Window);

   if (g != (Slrn_Group_Type *) Group_Window.top_window_line)
     {
	Slrn_Full_Screen_Update = 1;
	g = (Slrn_Group_Type *) Group_Window.top_window_line;
     }

   if ((fmt == NULL) || (*fmt == 0))
     fmt = "  %F%-5u  %n%45g%d";

   for (row = 0; row < height; row++)
     {
	while ((g != NULL) && (g->flags & GROUP_HIDDEN))
	  g = g->next;

	if (g != NULL)
	  {
	     if (Slrn_Full_Screen_Update || (g->flags & GROUP_TOUCHED))
	       {
		  slrn_custom_printf (fmt, group_display_format_cb,
				      (void *) g, row + 1, 0);
		  g->flags &= ~GROUP_TOUCHED;
	       }
	     g = g->next;
	  }
	else if (Slrn_Full_Screen_Update)
	  {
	     SLsmg_gotorc (row + 1, 0);
	     SLsmg_erase_eol ();
	  }
     }

   fmt = Slrn_Group_Status_Line;
   if ((fmt == NULL) || (*fmt == 0))
     fmt = _("-%D-News Groups: %s %-20g -- %L (%P)");
   slrn_custom_printf (fmt, group_status_line_cb, NULL, SLtt_Screen_Rows - 2,
		       STATUS_COLOR);

   if (Slrn_Use_Mouse) slrn_update_group_menu ();
   else slrn_update_top_status_line ();

   if (Slrn_Message_Present == 0) group_quick_help ();

   Last_Cursor_Row = 1 + Group_Window.window_row;
   SLsmg_gotorc (Last_Cursor_Row, 0);

   slrn_set_color (CURSOR_COLOR);

#if SLANG_VERSION > 10003
   if (Slrn_Display_Cursor_Bar)
     SLsmg_set_color_in_region (CURSOR_COLOR, Last_Cursor_Row, 0, 1, SLtt_Screen_Cols);
   else
#endif
     SLsmg_write_string ("->");

   slrn_set_color (0);
   Slrn_Full_Screen_Update = 0;
}

/*}}}*/

/* intrinsic functions */
void slrn_intr_get_group_order (void) /*{{{*/
{
   Slrn_Group_Type *g;
   SLang_Array_Type *retval;
   int n = 0;

   g = Groups;
   while (g != NULL)
     {
	n++;
	g = g->next;
     }

   retval = SLang_create_array (SLANG_STRING_TYPE, 0, NULL, &n, 1);
   if (retval == NULL)
     return;

   n = 0;
   g = Groups;
   while (g != NULL)
     {
	if (-1 == SLang_set_array_element (retval, &n, &g->group_name))
	  {
	     SLang_free_array (retval);
	     return;
	  }
	n++;
	g = g->next;
     }

   (void) SLang_push_array (retval, 1);
}
/*}}}*/

void slrn_intr_set_group_order (void) /*{{{*/
{
   SLang_Array_Type *at;
   Slrn_Group_Type *last, *rest, *g;
   int i, rows;

   if (-1 == SLang_pop_array_of_type (&at, SLANG_STRING_TYPE))
     {
	slrn_error (_("Array of string expected."));
	return;
     }

   if (at->num_dims != 1)
     {
	slrn_error (_("One-dimensional array expected."));
	SLang_free_array (at);
	return;
     }

   if ((Groups == NULL) || (Groups->next == NULL))
     {
	SLang_free_array (at);
	return;
     }

   rows = at->dims[0];
   last = NULL;
   rest = Groups;

   for (i = 0; i < rows; i++)
     {
	char *name;

	if ((-1 == SLang_get_array_element (at, &i, &name))
	    || (name == NULL))
	  continue;

	if (NULL == (g = find_group_entry (name, strlen (name))))
	  continue;

	/* It is possible for name to occur multiple times in the array.
	 * So we have to be careful.  There are two lists here: the part
	 * that runs from Groups to last, and the rest. The first group
	 * is the already sorted part, and the second is unsorted.
	 * Ordinarily g will come from the second (rest), but if name appears
	 * more than once in the array, it will come from the first part when
	 * seen the second time.
	 */
	if (g == last)		       /* at tail of first part */
	  continue;

	if (g == Groups)	       /* at head of first part */
	  Groups = g->next;

	if (g == rest)		       /* at head of second part */
	  rest = rest->next;

	/* Remove it from its current location */
	if (g->next != NULL)
	  g->next->prev = g->prev;
	if (g->prev != NULL)
	  g->prev->next = g->next;

	g->prev = last;		       /* append it to first part */

	if (last == NULL)	       /* the head of the first part */
	  Groups = g;
	else
	  last->next = g;

	g->next = rest;		       /* connect up the rest */
	if (rest != NULL)
	  rest->prev = g;

	last = g;
     }

   SLang_free_array (at);

   find_line_num ();
   Slrn_Full_Screen_Update = 1;
   Slrn_Groups_Dirty = 1;
}
/*}}}*/
