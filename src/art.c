/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999, 2007-2012 John E. Davis <jed@jedsoft.org>
 Copyright (c) 2001-2006  Thomas Schultz <tststs@gmx.de>

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

/*{{{ system include files */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <slang.h>
#include "jdmacros.h"

/*}}}*/
/*{{{ slrn include files */
#include "slrn.h"
#include "group.h"
#include "art.h"
#include "art_sort.h"
#include "misc.h"
#include "post.h"
/* #include "clientlib.h" */
#include "startup.h"
#include "hash.h"
#include "score.h"
#include "menu.h"
#include "util.h"
#include "server.h"
#include "xover.h"
#include "charset.h"
#include "print.h"
#include "snprintf.h"
#include "mime.h"
#include "hooks.h"
#include "help.h"
#include "strutil.h"
#include "common.h"

#if SLRN_HAS_UUDEVIEW
# include <uudeview.h>
#endif
#include "decode.h"

#if SLRN_HAS_CANLOCK
# include <canlock.h>
#endif

#if SLRN_HAS_GROUPLENS
# include "grplens.h"
#endif

/*}}}*/

/*{{{ extern Global variables  */

SLKeyMap_List_Type *Slrn_Article_Keymap;
char *Slrn_X_Browser;
char *Slrn_NonX_Browser;
char *Slrn_Quote_String;
char *Slrn_Save_Directory;
char *Slrn_Header_Help_Line;
char *Slrn_Header_Status_Line;
char *Slrn_Art_Help_Line;
char *Slrn_Art_Status_Line;
char *Slrn_Followup_Custom_Headers;
char *Slrn_Reply_Custom_Headers;
char *Slrn_Supersedes_Custom_Headers;
char *Slrn_Overview_Date_Format;
char *Slrn_Followup_Date_Format;
int Slrn_Use_Localtime = 1;

int Slrn_Emphasized_Text_Mode = 3;
#define EMPHASIZE_ARTICLE       1
#define EMPHASIZE_QUOTES	2
#define EMPHASIZE_SIGNATURE     4
#define EMPHASIZE_HEADER	8
int Slrn_Emphasized_Text_Mask = EMPHASIZE_ARTICLE;
int Slrn_Highlight_Urls = 1;

int Slrn_Process_Verbatim_Marks = 1;

Slrn_Article_Type *Slrn_Current_Article;

int Slrn_Use_Tildes = 1;

#if defined(IBMPC_SYSTEM)
int Slrn_Generate_Email_From = 1;
#else
int Slrn_Generate_Email_From = 0;
#endif

int Slrn_Startup_With_Article = 0;
int Slrn_Followup_Strip_Sig = 1;
int Slrn_Smart_Quote = 1;
#define PIPE_RAW		0
#define PIPE_CONVERTED		1
#define PIPE_DECODED		2
int Slrn_Pipe_Type = 0;

int Slrn_Query_Next_Article = 1;
int Slrn_Query_Next_Group = 1;
int Slrn_Auto_CC_To_Poster = 1;
int Slrn_Use_Tmpdir = 0;
int Slrn_Use_Header_Numbers = 1;
int Slrn_Warn_Followup_To = 1;
int Slrn_Color_By_Score = 3;
int Slrn_Highlight_Unread = 1;
int Slrn_High_Score_Min = 1;
int Slrn_Low_Score_Max = 0;
int Slrn_Kill_Score_Max = -9999;
FILE *Slrn_Kill_Log_FP = NULL;
int Slrn_Article_Window_Border = 0;
int Slrn_Reads_Per_Update = 50;
int Slrn_Sig_Is_End_Of_Article = 0;
#if SLRN_HAS_SPOILERS
int Slrn_Spoiler_Char = 42;
int Slrn_Spoiler_Display_Mode = 1;
#endif

#if SLRN_HAS_UUDEVIEW
int Slrn_Use_Uudeview = 1;
#endif

int Slrn_Del_Article_Upon_Read = 1;

char *Slrn_Current_Group_Name;
Slrn_Header_Type *Slrn_First_Header;
Slrn_Header_Type *Slrn_Current_Header;

Slrn_Header_Type *_art_Headers;

/* range of articles on server for current group */
NNTP_Artnum_Type Slrn_Server_Min, Slrn_Server_Max;

/* If +1, threads are all collapsed.  If zero, none are.  If -1, some may
 * be and some may not.  In other words, if -1, this variable should not
 * be trusted.
 */
int _art_Threads_Collapsed = 0;

/*}}}*/
/*{{{ static global variables */
static SLscroll_Window_Type Slrn_Article_Window;
static SLscroll_Window_Type Slrn_Header_Window;

static int Header_Window_Nrows;
static unsigned int Number_Killed;
static unsigned int Number_Score_Killed;
static unsigned int Number_High_Scored;
static unsigned int Number_Low_Scored;
static unsigned int Number_Read;
static unsigned int Number_Total;
static int User_Aborted_Group_Read;
#if SLRN_HAS_SPOILERS
static Slrn_Header_Type *Spoilers_Visible;
static char Num_Spoilers_Visible = 1; /* show text up to Nth spoiler char */
#endif

#if SLRN_HAS_GROUPLENS
static int Num_GroupLens_Rated = -1;
#endif
static Slrn_Header_Type *Mark_Header;  /* header with mark set */

static Slrn_Group_Type *Current_Group; /* group being processed */

static int Total_Num_Headers;	       /* headers retrieved from server.  This
					* number is used only by update meters */
static int Last_Cursor_Row;	       /* row where --> cursor last was */
static Slrn_Header_Type *Header_Showing;    /* header whose article is selected */
static Slrn_Header_Type *Last_Read_Header;
static int Article_Visible;	       /* non-zero if article window is visible */
static char Output_Filename[SLRN_MAX_PATH_LEN];

#define HEADER_TABLE_SIZE 1250
static Slrn_Header_Type *Header_Table[HEADER_TABLE_SIZE];
static int Do_Rot13;
static int Perform_Scoring;
static int Largest_Header_Number;
static int Article_Window_Nrows;
static int Article_Window_HScroll;
static int Header_Window_HScroll;

static Slrn_Header_Type *At_End_Of_Article;
/* If this variable is NULL, then we are not at the end of an article.  If it
 * points at the current article, the we are at the end of that article.
 * If it points anywhere else, ignore it.
 */

static int Headers_Hidden_Mode = 1;
int Slrn_Quotes_Hidden_Mode = 0;
int Slrn_Signature_Hidden = 0;
int Slrn_Pgp_Signature_Hidden = 0;
int Slrn_Verbatim_Marks_Hidden = 0;
int Slrn_Verbatim_Hidden = 0;

/*}}}*/
/*{{{ static function declarations */

static void toggle_header_formats (void);
static void slrn_art_hangup (int);
static void hide_or_unhide_quotes (void);
static void art_update_screen (void);
static void art_next_unread (void);
static void art_quit (void);
static int select_header (Slrn_Header_Type *, int);
static int select_article (int);
static void quick_help (void);
static void for_this_tree (Slrn_Header_Type *, void (*)(Slrn_Header_Type *));
static void find_non_hidden_header (void);
static void get_missing_headers (void);
static void decode_rot13 (unsigned char *);

static void skip_to_next_group (void);

#if SLRN_HAS_SPOILERS
static void show_spoilers (void);
#endif
/*}}}*/

/*{{{ portability functions */
#ifndef HAVE_GETTIMEOFDAY
# ifdef VMS
#  define HAVE_GETTIMEOFDAY
#  include <starlet.h>
struct timeval { long tv_sec; long tv_usec;};

static int gettimeofday (struct timeval* z, void* ignored)
{
   unsigned long tod[2];
   SYS$GETTIM(tod);

   (void) ignored;
   z->tv_sec=( (tod[0]/10000000) + ((tod[1]* 429 )&0x7fffffffl) );
   z->tv_usec=((tod[0]/10)%1000000);
   return 0;
}
# endif
# ifdef __WIN32__
#  define HAVE_GETTIMEOFDAY
#  ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
struct timeval { long tick_count; };

static int gettimeofday (struct timeval* timestruct , void* ignored)
{
   (void) ignored;
   timestruct->tick_count = GetTickCount();
   return 0;
}
# endif
#endif

#ifdef HAVE_GETTIMEOFDAY
#ifdef __WIN32__
/* Return time differences in microseconds */
static unsigned long time_diff(struct timeval t1, struct timeval t2)
{
   return (t1.tick_count - t2.tick_count)*1000;
}
#else /* Proper UNIX or VMS */
/* Return time differences in microseconds */
static unsigned long time_diff(struct timeval t1, struct timeval t2)
{
   return (t1.tv_sec - t2.tv_sec)*1000000 + (t1.tv_usec - t2.tv_usec);
}
#endif
#endif
/*}}}*/

/*{{{ SIGWINCH and window resizing functions */

/* This function syncs the current article */
static void art_winch (void) /*{{{*/
{
   static int rows = 0;

   /* Check to see if rows is still 0. If so, this is the first
    * call here and we should run resize_screen_hook to allow it
    * to change the initial Article_Window_Nrows.
    */
   if (rows == 0)
     {
	rows = SLtt_Screen_Rows;
	slrn_run_hooks (HOOK_RESIZE_SCREEN, 0);
     }

   if ((rows != SLtt_Screen_Rows)
       || (Article_Window_Nrows <= 0)
       || (Article_Window_Nrows >= SLtt_Screen_Rows - 3))
     {
	rows = SLtt_Screen_Rows;

	if (rows <= 28)
	  Article_Window_Nrows = rows - 8;
	else
	  Article_Window_Nrows = (3 * rows) / 4;
     }

   Header_Window_Nrows = rows - 3;
   if (Article_Visible)
     {
	Header_Window_Nrows -= Article_Window_Nrows;

	/* allow for art status line, unless we are zoomed. */
	if (Header_Window_Nrows != 0)
	  Header_Window_Nrows--;
     }

   if (Header_Window_Nrows < 0) Header_Window_Nrows = 1;
   if (Article_Window_Nrows < 0) Article_Window_Nrows = 1;

   Slrn_Article_Window.nrows = (unsigned int) Article_Window_Nrows;
   Slrn_Header_Window.nrows = (unsigned int) Header_Window_Nrows;

   if (Slrn_Current_Article != NULL)
     {
	if (Slrn_Current_Article->is_wrapped)
	  _slrn_art_wrap_article (Slrn_Current_Article);

	if (Slrn_Current_Article->needs_sync)
	  slrn_art_sync_article (Slrn_Current_Article);
     }
}

/*}}}*/

/* This function syncs the article */
static void set_article_visibility (int visible)
{
   if (visible == Article_Visible)
     return;
   Article_Visible = visible;
   art_winch ();
}

static void art_winch_sig (int old_r, int old_c) /*{{{*/
{
   (void) old_c;
   if (old_r != SLtt_Screen_Rows)
     Article_Window_Nrows = 0;

   art_winch ();
}

/*}}}*/

static void shrink_window (void) /*{{{*/
{
   if (Article_Visible == 0) return;
   Article_Window_Nrows++;
   art_winch ();
}

/*}}}*/

static void enlarge_window (void) /*{{{*/
{
   if (Article_Visible == 0) return;
   Article_Window_Nrows--;
   art_winch ();
}

/*}}}*/

void slrn_set_article_window_size (int nrows)
{
   Article_Window_Nrows = nrows;
   art_winch ();
}

int slrn_get_article_window_size (void)
{
   return Article_Window_Nrows;
}

unsigned int slrn_art_count_lines (void)
{
   return Slrn_Article_Window.num_lines;
}

unsigned int slrn_art_cline_num (void)
{
   return Slrn_Article_Window.line_num;
}

/*}}}*/
/*{{{ header hash functions */
static void delete_hash_table (void) /*{{{*/
{
   SLMEMSET ((char *) Header_Table, 0, sizeof (Header_Table));
}

/*}}}*/

static void make_hash_table (void) /*{{{*/
{
   Slrn_Header_Type *h;
   delete_hash_table ();
   h = Slrn_First_Header;
   while (h != NULL)
     {
	h->hash_next = Header_Table[h->hash % HEADER_TABLE_SIZE];
	Header_Table[h->hash % HEADER_TABLE_SIZE] = h;
	h = h->real_next;
     }
}

/*}}}*/

static void remove_from_hash_table (Slrn_Header_Type *h) /*{{{*/
{
   Slrn_Header_Type *tmp;
   if (h == (tmp = Header_Table[h->hash % HEADER_TABLE_SIZE]))
     Header_Table[h->hash % HEADER_TABLE_SIZE] = h->hash_next;
   else while (tmp != NULL)
     {
	if (h == tmp->hash_next)
	  {
	     tmp->hash_next = h->hash_next;
	     break;
	  }
	tmp = tmp->hash_next;
     }
}
/*}}}*/

static void free_this_header (Slrn_Header_Type *h)
{
   slrn_free (h->tree_ptr);
   slrn_free (h->subject);
   slrn_free (h->from);
   slrn_free (h->date);
   slrn_free (h->msgid);
   slrn_free (h->refs);
   slrn_free (h->xref);
   slrn_free (h->realname);
   slrn_free_additional_headers (h->add_hdrs);
   slrn_free ((char *) h);
}

static void free_killed_header (Slrn_Header_Type *h)
{
   Number_Total--;
   if (h->flags & HEADER_READ)
     Number_Read--;
   if (h->flags & HEADER_HIGH_SCORE)
     Number_High_Scored--;
   if (h->flags & HEADER_LOW_SCORE)
     Number_Low_Scored--;
   remove_from_hash_table (h);
   free_this_header (h);
}

static void free_all_headers (void)
{
   Slrn_Header_Type *h = _art_Headers;

   while (h != NULL)
     {
	Slrn_Header_Type *next = h->next;
	free_this_header (h);
	h = next;
     }

   _art_Headers = NULL;
}

/*}}}*/

/*{{{ article line specific functions */

void slrn_art_sync_article (Slrn_Article_Type *a)
{
   Slrn_Article_Line_Type *l;

   if (a == NULL)
     return;

   /* Make sure Article_Current_Line is not hidden */
   l = a->cline;
   while ((l != NULL) && (l->flags & HIDDEN_LINE))
     l = l->prev;
   if (l == NULL)
     l = a->cline;
   while ((l != NULL) && (l->flags & HIDDEN_LINE))
     l = l->next;

   a->cline = l;

   Slrn_Article_Window.current_line = (SLscroll_Type *) l;

   /* Force current line to be at top of window */
   Slrn_Article_Window.top_window_line = Slrn_Article_Window.current_line;

   SLscroll_find_line_num (&Slrn_Article_Window);
   a->needs_sync = 0;
}

static void find_article_line_num (void) /*{{{*/
{
   if (Slrn_Current_Article == NULL)
     return;

   slrn_art_sync_article (Slrn_Current_Article);
}

/*}}}*/

static void init_article_window_struct (void) /*{{{*/
{
   Slrn_Article_Type *a;

   if (Slrn_Current_Article == NULL)
     return;

   a = Slrn_Current_Article;

   Slrn_Article_Window.hidden_mask = HIDDEN_LINE;
   Slrn_Article_Window.current_line = (SLscroll_Type *) a->cline;
   Slrn_Article_Window.cannot_scroll = SLtt_Term_Cannot_Scroll;
   Slrn_Article_Window.lines = (SLscroll_Type *) a->lines;
   Slrn_Article_Window.border = (unsigned int) Slrn_Article_Window_Border;
   art_winch ();		       /* set nrows element */
   find_article_line_num ();
}

/*}}}*/

void slrn_art_free_line (Slrn_Article_Line_Type *l)
{
   slrn_free ((char *) l->buf);
   slrn_free ((char *) l);
}

void slrn_art_free_article_line_list (Slrn_Article_Line_Type *lines)
{
   while (lines != NULL)
     {
	Slrn_Article_Line_Type *next = lines->next;
	slrn_art_free_line (lines);
	lines = next;
     }
}

static Slrn_Article_Line_Type *copy_article_line (Slrn_Article_Line_Type *l)
{
   Slrn_Article_Line_Type *retval = NULL, *r, *last = NULL;

   while (l != NULL)
     {
	r = (Slrn_Article_Line_Type*) slrn_malloc (sizeof(Slrn_Article_Line_Type), 1, 1);
	if ((r == NULL) ||
	    (NULL == (r->buf = slrn_strmalloc (l->buf, 0))))
	  {
	     slrn_art_free_article_line_list (retval);
	     slrn_free ((char *)r);
	     return NULL;
	  }
	r->flags = l->flags;
	if (l->flags & QUOTE_LINE)
	  {
	     r->v.quote_level = l->v.quote_level;
	  }

	r->next = NULL;
	if (retval == NULL)
	  {
	     r->prev = NULL;
	     retval = r;
	  }
	else
	  {
	     r->prev = last;
	     last->next = r;
	  }
	last = r;
	l = l->next;
     }
   return retval;
}

static void free_article_lines (Slrn_Article_Type *a)
{
   if (a == NULL)
     return;

   slrn_art_free_article_line_list (a->lines);
   slrn_art_free_article_line_list (a->raw_lines);
   a->lines = NULL;
   a->raw_lines = NULL;
   a->cline=NULL;
}

void slrn_art_free_article (Slrn_Article_Type *a)
{
   if (a == NULL)
     return;

   if (a == Slrn_Current_Article)
     Slrn_Current_Article = NULL;

   slrn_mime_free(&a->mime);
   free_article_lines (a);
   slrn_free ((char *) a);
}

static void free_article (void) /*{{{*/
{
   slrn_art_free_article (Slrn_Current_Article);
   Slrn_Current_Article = NULL;

   memset ((char *) &Slrn_Article_Window, 0, sizeof(SLscroll_Window_Type));

   Header_Showing = NULL;
   set_article_visibility (0);
}

/*}}}*/

static void skip_quoted_text (void) /*{{{*/
{
   _slrn_art_skip_quoted_text (Slrn_Current_Article);
}

/*}}}*/

static void skip_digest_forward (void) /*{{{*/
{
   if (-1 == _slrn_art_skip_digest_forward (Slrn_Current_Article))
     slrn_error (_("No next digest."));
}

/*}}}*/

static char *extract_header (Slrn_Header_Type *h, char *hdr, unsigned int len) /*{{{*/
{
   char *retval;

   if (h == NULL)
     return NULL;

   if ((len > 2) && (hdr[len-1] == ' ') && (hdr[len-2] == ':'))
     len--;

   if (0 == slrn_case_strncmp ("From: ", hdr, len))
     return slrn_skip_whitespace (h->from);
   if (0 == slrn_case_strncmp ("Subject: ", hdr, len))
     return h->subject;
   if (0 == slrn_case_strncmp ("Message-Id: ", hdr, len))
     return slrn_skip_whitespace (h->msgid);
   if (0 == slrn_case_strncmp ("Date: ", hdr, len))
     return h->date;
   if (0 == slrn_case_strncmp ("References: ", hdr, len))
     return slrn_skip_whitespace (h->refs);
   if (0 == slrn_case_strncmp ("Xref: ", hdr, len))
     return h->xref;
   if (0 == slrn_case_strncmp ("Lines: ", hdr, len))
     {
	static char lines_buf[32];
	sprintf (lines_buf, "%d", h->lines); /* safe */
	return lines_buf;
     }

   retval = slrn_extract_add_header (h, hdr);
   if ((retval == NULL) && (h == Header_Showing))
     retval = slrn_art_extract_header (hdr, len);

   return retval;
}
/*}}}*/

char *slrn_extract_header (char *hdr, unsigned int len) /*{{{*/
{
   return extract_header (Header_Showing, hdr, len);
}
/*}}}*/

char *slrn_cur_extract_header (char *hdr, unsigned int len)
{
   return extract_header (Slrn_Current_Header, hdr, len);
}

/*{{{ wrap article functions  */

static void unwrap_article (void)
{
   if (Header_Showing == NULL) return;
   _slrn_art_unwrap_article (Slrn_Current_Article);
   hide_or_unhide_quotes ();
}

static void wrap_article (void) /*{{{*/
{
   if (Header_Showing == NULL) return;
   _slrn_art_wrap_article (Slrn_Current_Article);
   hide_or_unhide_quotes ();
}

/*}}}*/

static void toggle_wrap_article (void)
{
   Slrn_Article_Type *a = Slrn_Current_Article;

   if (a == NULL)
     return;

   if (a->is_wrapped)
     unwrap_article ();
   else
     {
	if (Slrn_Prefix_Arg_Ptr != NULL) Slrn_Wrap_Mode = 0x7F;
	Slrn_Prefix_Arg_Ptr = NULL;
	wrap_article ();
     }
}

/*}}}*/

/* selects the article that should be affected by interactive commands */
static int select_affected_article (void) /*{{{*/
{
   if ((Slrn_Current_Article == NULL) || !Article_Visible)
     {
	slrn_uncollapse_this_thread (Slrn_Current_Header, 1);
	if (select_header (Slrn_Current_Header, Slrn_Del_Article_Upon_Read) < 0)
	  return -1;
	else
	  return 0;
     }
   return 1;
}
/*}}}*/

Slrn_Article_Line_Type *slrn_search_article (char *string, /*{{{*/
					     char **ptrp,
					     int is_regexp,
					     int set_current_flag,
					     int dir)
  /* dir: 0 is backward, 1 is forward, 2 is "find first" */
{
#if SLANG_VERSION < 20000
   SLsearch_Type st;
#else
   SLsearch_Type *st = NULL;
#endif
   Slrn_Article_Line_Type *l;
   Slrn_Article_Type *a;
   char *ptr;
   int ret;
   SLRegexp_Type *re = NULL;

   if ((*string == 0) ||
       (-1 == (ret = select_affected_article ())))
     return NULL;

   if (is_regexp)
     {
	re = slrn_compile_regexp_pattern (string);
	if (re == NULL)
	  return NULL;
     }
#if SLANG_VERSION < 20000
   else SLsearch_init (string, 1, 0, &st);
#else
   else
     {
	int flags = SLSEARCH_CASELESS;
	if (Slrn_UTF8_Mode)
	  flags |= SLSEARCH_UTF8;

	st = SLsearch_new ((SLuchar_Type *) string, flags);
	if (st == NULL)
	  return NULL;
     }
#endif

   a = Slrn_Current_Article;
   if (dir == 2)
     l = a->lines;
   else
     {
	l = a->cline;
	if (ret == 1)
	  l = (dir ? l->next : l->prev);
     }

   while (l != NULL)
     {
	if ((l->flags & HIDDEN_LINE) == 0)
	  {
	     if (is_regexp)
	       ptr = (char *) slrn_regexp_match (re, l->buf);
#if SLANG_VERSION < 20000
	     else ptr = (char *) SLsearch ((unsigned char *) l->buf,
					   (unsigned char *) l->buf + strlen (l->buf),
					   &st);
#else
	     else ptr = (char *)SLsearch_forward (st, (SLuchar_Type *) l->buf, (SLuchar_Type *) l->buf + strlen (l->buf));
#endif

	     if (ptr != NULL)
	       {
		  if (ptrp != NULL) *ptrp = ptr;
		  if (set_current_flag)
		    {
		       set_article_visibility (1);
		       a->cline = l;
		       find_article_line_num ();
		    }
		  break;
	       }
	  }
	l = (dir ? l->next : l->prev);
     }

#if SLANG_VERSION >= 20000
   if (re != NULL) SLregexp_free (re);
   if (st != NULL) SLsearch_delete (st);
#endif
   return l;
}

/*}}}*/

static char *find_url (char *l_buf, unsigned int *p_len) /*{{{*/
{
   char *ptr, *tmp, ch;

   while (NULL != (ptr = slrn_strbyte (l_buf, ':')))
     {
	int is_news;
	tmp = ptr;

	while ((ptr > l_buf)
	       && isalpha((unsigned char)(*(ptr - 1))))
	  ptr--;

	/* all registered and reserved scheme names are >= 3 chars long */
	if ((ptr + 3 > tmp) ||
	    ((0 == (is_news = !strncmp (ptr, "news:",5))) &&
	     ((tmp[1] != '/') || (tmp[2] != '/'))))
	  {
	     l_buf = tmp + 1; /* skip : */
	     continue;
	  }

	tmp = ptr;

	if (is_news)
	  {
	     int saw_opening_bracket = 0;

	     ptr+=5;
	     if (*ptr == '<')
	       {
		  ptr++;
		  saw_opening_bracket = 1;
	       }
	     while ((ch = *ptr) && (NULL == slrn_strbyte (" \t\n<>", ch)))
	       ptr++;
	     if ((*ptr == '>') && saw_opening_bracket)
	       ptr++;
	  }
	else
	  {
	     while ((ch = *ptr) && (NULL == slrn_strbyte (" \t\n\"{}<>", ch)))
	       ptr++;
	  }

	/* at the end of the URL, these are probably punctuation */
	while ((tmp < ptr) && (NULL != slrn_strbyte (".,;:()", *(ptr - 1))))
	  ptr--;

	l_buf = ptr;

	if ((*p_len = (unsigned int) (ptr - tmp)) < 6)
	  continue;

	ptr -= 3;

	if ((ptr[0] == ':')
	    && (ptr[1] == '/')
	    && (ptr[2] == '/'))
	  continue;

	return tmp;
     }

   return NULL;
}

/*}}}*/

static int extract_urls (unsigned int *argc_ptr, char **argv, unsigned int max_argc,
			 unsigned int *start) /*{{{*/
{
   Slrn_Article_Line_Type *l;
   Slrn_Article_Type *a;
   unsigned int argc;
   int was_wrapped;

   if (NULL == (a = Slrn_Current_Article))
     return -1;

   if ((was_wrapped = (Slrn_Wrap_Method && a->is_wrapped)))
     _slrn_art_unwrap_article (a);

   l = a->lines;
   *start = 0;

   argc = 0;
   while ((l != NULL) && (argc < max_argc))
     {
	char *ptr;
	unsigned int len;

	if (l == a->cline)
	  *start = argc;

	ptr = l->buf;
	while (NULL != (ptr = find_url (ptr, &len)))
	  {
	     unsigned int iterator;

	     if (argc == max_argc)
	       break;

	     if (NULL == (argv[argc] = slrn_strnmalloc (ptr, len, 1)))
	       {
		  slrn_free_argc_argv_list (argc, argv);
		  return -1;
	       }
	     if (Do_Rot13)
	       decode_rot13 ((unsigned char *) argv[argc]);
	     ptr += len;

	     /* remove duplicates */
	     for (iterator = 0; iterator < argc; ++iterator)
	       {
		  if (!strcmp (argv[iterator], argv[argc]))
		    {
		       slrn_free(argv[argc]);
		       --argc;
		       break;
		    }
	       }
	     argc++;
	  }

	l = l->next;
     }

   if (was_wrapped)
     _slrn_art_wrap_article (a);

   *argc_ptr = argc;
   return 0;
}

/*}}}*/

static char *quote_url (char *url)
{
   char *p;
   char *new_url;
   unsigned int len;

   p = url;
   len = 0;
   while (*p != 0)
     {
	if (isalnum (*p))
	  len++;
	else
	  len += 3;
	p++;
     }

   new_url = SLmalloc (len + 1);
   if (new_url == NULL)
     return NULL;
   p = new_url;

   while (*url != 0)
     {
	switch (*url)
	  {
	   default:
	     *p++ = *url;
	     break;

	     /* RFC 2396 suggests that the following may be escaped without
	      * changing the semantics of the URL.
	      */
	   /* case '-': */
	   /* case '_': */
	   /* case '.': */
	   case '!':
	   case '~':
	   case '*':
	   case '\'':
	   case '(':
	   case ')':
	     sprintf (p, "%%%2X", (unsigned char) *url); /* safe */
	     p += 3;
	     break;
	  }
	url++;
     }

   *p = 0;
   return new_url;
}

static char *create_browser_command (char *cmd, char *url) /*{{{*/
{
   unsigned int len, urllen;
   char ch, *buf, *bp, *p = cmd;

   len = strlen (cmd) + 1;
   urllen = strlen (url);
   while (0 != (ch = *p++))
     {
	if (ch == '%')
	  {
	     ch = *p++;
	     if (ch == 's')
	       len += urllen - 1;
	     else if (ch != '%')
	       {
		  slrn_error (_("Invalid Browser definition."));
		  return NULL;
	       }
	  }
     }
   buf = bp = slrn_safe_malloc (len);
   p = cmd;
   while (0 != (ch = *p++))
     {
	if (ch == '%')
	  {
	     ch = *p++;
	     if (ch == 's')
	       {
		  strcpy (bp, url); /* safe */
		  bp += urllen;
	       }
	     else
	       *bp++ = '%';
	  }
	else
	  *bp++ = ch;
     }
   return buf;
}
/*}}}*/

/* If want_edit is 1, url can be changed by user. */
static void launch_url (char *url, int want_edit) /*{{{*/
{
   char *command, *browser, *has_percent;
   int reinit;

   if (want_edit
       && (slrn_read_input (_("Browse (^G aborts): "), NULL, url, 0, -1) <= 0))
     {
	slrn_error (_("Aborted."));
	return;
     }

   if (!strncmp (url, "news:", 5)) /* we can handle this ourself */
     {
	char bracket = '>';
	if (url[strlen(url)-1] == '>')
	  bracket = 0;
	url += 5;
	if (!strncmp (url, "//", 2))
	  url += 2; /* not RFC compliant, but accept it anyway */
	if (*url == '<')
	  url++; /* not RFC compliant either */
	if (NULL != slrn_strbyte (url, '@'))
	  {
	     char *msgid = slrn_strdup_printf ("<%s%c", url, bracket);
	     if (NULL == msgid)
	       return;
	     slrn_locate_header_by_msgid (msgid, 0, 1);
	     slrn_free (msgid);
	     return;
	  }
	if (!strcmp (url, "*")) /* special case */
	  {
	     if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_URL) &&
		 (slrn_get_yesno (1, _("Show all available groups")) == 0))
	       return;

	     art_quit ();
	     slrn_hide_groups (0);
	     slrn_list_all_groups (1);
	     return;
	  }
	/* else: it's a group name */
	if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_URL) &&
	    (slrn_get_yesno (1, _("Try switching to %s"), url) == 0))
	  return;

	art_quit ();
	if (0 == slrn_add_group (url))
	  if (-1 == slrn_group_select_group ())
	    slrn_error (_("Group contains no articles."));
	return;
     }

   reinit = 0;
   if ((NULL == getenv ("DISPLAY"))
       || (NULL == (browser = slrn_skip_whitespace (Slrn_X_Browser)))
       || (browser [0] == 0))
     {
	reinit = 1;
	browser = Slrn_NonX_Browser;
     }

   if (browser == NULL)
     {
	slrn_error (_("No Web Browser has been defined."));
	return;
     }

   if (reinit == 0)		       /* ==> X_Browser != NULL */
     {
	/* Non_X and X browsers may be same. */
	if ((Slrn_NonX_Browser != NULL)
	    && (0 == strcmp (Slrn_NonX_Browser, Slrn_X_Browser)))
	  reinit = 1;
     }

   /* Perform a simple-minded syntax check. */
   has_percent = slrn_strbyte (browser, '%');
   if (has_percent != NULL)
     {
	if ((has_percent[1] != 's')
	    || ((has_percent != browser) && (*(has_percent - 1) == '\\')))
	  has_percent = NULL;
     }

   if (NULL == (url = quote_url (url)))
     return;

   if (has_percent != NULL)
     command = create_browser_command (browser, url);
   else
     /* Is this quoting ok on VMS and OS/2?? */
     command = slrn_strdup_printf ("%s '%s'", browser, url);

   if (command != NULL)
     (void) slrn_posix_system (command, reinit);

   SLfree (command);
   SLfree (url);
}

static void browse_url (void) /*{{{*/
{
   char url[SLRL_DISPLAY_BUFFER_SIZE];
   int selected;
   unsigned int argc, start_argc;
#define MAX_URLS 1024
   char *argv[MAX_URLS];
   int want_edit;

   if (-1 == extract_urls (&argc, argv, MAX_URLS, &start_argc))
     return;

   if (0 == argc)
     {
	slrn_error (_("No URLs found."));
	return;
     }

   selected = 0;
   want_edit = 1;

   if (argc > 1)
     selected = slrn_select_list_mode ("URL", argc, argv, start_argc, 1, &want_edit);

   if (-1 == selected)
     {
	slrn_free_argc_argv_list (argc, argv);
	return;
     }

   strncpy (url, argv[selected], sizeof (url));
   url[sizeof(url) - 1] = 0;

   slrn_free_argc_argv_list (argc, argv);

   slrn_redraw ();
   launch_url (url, want_edit);
}

/*}}}*/

static void article_search (void) /*{{{*/
{
   static char search_str[SLRL_DISPLAY_BUFFER_SIZE];
   Slrn_Article_Line_Type *l;

   if (slrn_read_input (_("Search: "), search_str, NULL, 0, 0) <= 0) return;

   l = slrn_search_article (search_str, NULL, 0, 1, 1);

   if (l == NULL) slrn_error (_("Not found."));
}

/*}}}*/

/*}}}*/
/*{{{ current article movement functions */

unsigned int slrn_art_lineup_n (unsigned int n) /*{{{*/
{
   if (select_article (0) <= 0) return 0;

   n = SLscroll_prev_n (&Slrn_Article_Window, n);

   Slrn_Current_Article->cline = (Slrn_Article_Line_Type *) Slrn_Article_Window.current_line;

   /* Force current line to be at top of window */
   Slrn_Article_Window.top_window_line = Slrn_Article_Window.current_line;
   Slrn_Full_Screen_Update = 1;
   return n;
}

/*}}}*/

static void art_pageup (void) /*{{{*/
{
   /* Since we always require the current line to be at the top of the
    * window, SLscroll_pageup cannot be used.  Instead, do it this way:
    */
   (void) slrn_art_lineup_n (Slrn_Article_Window.nrows - 1);
}

/*}}}*/

int slrn_get_next_pagedn_action (void)
{
   Slrn_Header_Type *h;

   if (Slrn_Current_Header == NULL)
     return -1;

   if ((Article_Visible == 0)
       || (At_End_Of_Article != Slrn_Current_Header))
     return 0;

   h = Slrn_Current_Header->next;
   while (h != NULL)
     {
	if (0 == (h->flags & HEADER_READ))
	  return 1;
	h = h->next;
     }

   return 2;
}

static void art_pagedn (void) /*{{{*/
{
   char *msg = NULL;

   if (Slrn_Current_Header == NULL) return;

#if SLRN_HAS_SPOILERS
   if (Spoilers_Visible == Slrn_Current_Header)
     {
	show_spoilers ();
	return;
     }
#endif

   if ((Article_Visible == 0) || (At_End_Of_Article != Slrn_Current_Header))
     {
	int av = Article_Visible;

	At_End_Of_Article = NULL;

	if ((select_article (0) <= 0)
	    || (av == 0))
	  return;

	SLscroll_pagedown (&Slrn_Article_Window);
	Slrn_Current_Article->cline = (Slrn_Article_Line_Type *) Slrn_Article_Window.current_line;
	/* Force current line to be at top of window */
#if 1
	Slrn_Article_Window.top_window_line = Slrn_Article_Window.current_line;
#endif
	return;
     }

   At_End_Of_Article = NULL;

   if (Slrn_Batch) return;

   if (Slrn_Current_Header->next == NULL)
     {
	if (Slrn_Query_Next_Group)
	  msg = _("At end of article, press %s for next group.");
     }
   else if (Slrn_Query_Next_Article)
     msg = _("At end of article, press %s for next unread article.");

   if (msg != NULL)
     {
	char *keyseq = slrn_help_keyseq_from_function ("article_page_down", Slrn_Article_Keymap);
	if (keyseq != NULL)
	  {
	     SLang_Key_Type *key;
	     char *keystr = slrn_help_keyseq_to_string(keyseq+1,*keyseq-1);
	     if (keystr == NULL)
	       keystr = SLang_make_keystring((unsigned char*) keyseq);

	     slrn_message_now (msg, keystr);
	     key = SLang_do_key (Slrn_Article_Keymap, slrn_getkey);
	     if (key == NULL) return;
	     if ((key->type != SLKEY_F_INTRINSIC) ||
		 (key->f.f != (FVOID_STAR) art_pagedn))
	       {
		  SLang_ungetkey_string (key->str+1, (unsigned int)*(key->str)-1);
		  return;
	       }
	  }
	else /* article_page_down is unbound */
	  {
	     unsigned int ch;

	     slrn_message_now (msg, _("<Space>"));
#if 0
	     if (SLANG_GETKEY_ERROR == (ch = SLang_getkey ()))
	       slrn_exit_error ("SLang_getkey failed");
#else
	     ch = slrn_getkey ();
#endif
	     if (ch != ' ')
	       {
		  SLang_ungetkey ((unsigned char) ch);
		  return;
	       }
	  }
     }

   At_End_Of_Article = NULL;
   if (Slrn_Current_Header->next != NULL) art_next_unread ();
   else skip_to_next_group ();
}

/*}}}*/

static void art_lineup (void) /*{{{*/
{
   slrn_art_lineup_n (1);
}

/*}}}*/

static void art_bob (void) /*{{{*/
{
   while (0xFFFF == slrn_art_lineup_n (0xFFFF));
}

/*}}}*/

unsigned int slrn_art_linedn_n (unsigned int n) /*{{{*/
{
   unsigned int new_article = 0;
   switch (select_article (0))
     {
      case 0:
	new_article = 1;
	if (n-- > 1)
	  break; /* else fall through */
      case -1:
	return new_article;
     }

   n = SLscroll_next_n (&Slrn_Article_Window, n);
   Slrn_Current_Article->cline = (Slrn_Article_Line_Type *) Slrn_Article_Window.current_line;

   /* Force current line to be at top of window */
   Slrn_Article_Window.top_window_line = Slrn_Article_Window.current_line;
   Slrn_Full_Screen_Update = 1;
   return n + new_article;
}

/*}}}*/

static void art_linedn (void) /*{{{*/
{
   (void) slrn_art_linedn_n (1);
}

/*}}}*/

static void art_eob (void) /*{{{*/
{
   while (slrn_art_linedn_n (0xFFFF) > 0)
     ;
   (void) slrn_art_lineup_n (Slrn_Article_Window.nrows - 1);
}

/*}}}*/

/*}}}*/

/*{{{ Tag functions */

typedef struct /*{{{*/
{
   Slrn_Header_Type **headers;
   unsigned int max_len;
   unsigned int len;
}

/*}}}*/
Num_Tag_Type;

static Num_Tag_Type Num_Tag_List;

static void free_tag_list (void) /*{{{*/
{
   if (Num_Tag_List.headers != NULL)
     {
	SLFREE (Num_Tag_List.headers);
	Num_Tag_List.headers = NULL;
	Num_Tag_List.len = Num_Tag_List.max_len = 0;
     }
}

/*}}}*/

int slrn_goto_num_tagged_header (int *nump) /*{{{*/
{
   unsigned int num;

   num = (unsigned int) *nump;
   num--;

   if (num >= Num_Tag_List.len)
     return 0;

   if (Num_Tag_List.headers == NULL)
     return 0;

   if (-1 == slrn_goto_header (Num_Tag_List.headers[num], 0))
     return 0;

   Slrn_Full_Screen_Update = 1;
   return 1;
}

/*}}}*/

static void num_tag_header (void) /*{{{*/
{
   Slrn_Header_Type *h = Slrn_Current_Header;
   unsigned int len, tag_thread = 0;

   if ((h->child != NULL)
       && (h->child->flags & HEADER_HIDDEN))
     tag_thread = 1;

   if (Num_Tag_List.headers == NULL)
     {
	Slrn_Header_Type **headers;
	unsigned int max_len = 20;

	headers = (Slrn_Header_Type **) slrn_malloc (max_len * sizeof (Slrn_Header_Type),
						     0, 1);
	if (headers == NULL)
	  return;

	Num_Tag_List.max_len = max_len;
	Num_Tag_List.headers = headers;
	Num_Tag_List.len = 0;
     }

   do
     {
	if (Num_Tag_List.max_len == Num_Tag_List.len)
	  {
	     Slrn_Header_Type **headers = Num_Tag_List.headers;
	     unsigned int max_len = Num_Tag_List.max_len + 20;

	     headers = (Slrn_Header_Type **) slrn_realloc ((char *)headers,
							   max_len * sizeof (Slrn_Header_Type),
							   1);
	     if (headers == NULL)
	       return;

	     Num_Tag_List.max_len = max_len;
	     Num_Tag_List.headers = headers;
	  }

	Slrn_Full_Screen_Update = 1;
	if ((h->flags & HEADER_NTAGGED) == 0)
	  {
	     Num_Tag_List.headers[Num_Tag_List.len] = h;
	     Num_Tag_List.len += 1;
	     h->tag_number = Num_Tag_List.len;
	     h->flags |= HEADER_NTAGGED;
	     continue;
	  }

	/* It is already tagged.  Giving this header the last number lead to
	 * the slightly annoying fact that you had to hit "tag" twice to untag,
	 * unless you were on the last tag.  We now simply remove it and
	 * renumber the others that follow it.
	 */
	for (len = h->tag_number + 1; len <= Num_Tag_List.len; len++)
	  {
	     Slrn_Header_Type *tmp = Num_Tag_List.headers[len - 1];
	     Num_Tag_List.headers[len - 2] = tmp;
	     tmp->tag_number -= 1;
	  }
	Num_Tag_List.len--;
	h->tag_number = 0;
	h->flags &= ~HEADER_NTAGGED;
     }
   while ((tag_thread) && (NULL != (h = h->next)) && (h->parent != NULL));
   (void) slrn_header_down_n (1, 0);
}

/*}}}*/

static void num_untag_headers (void) /*{{{*/
{
   unsigned int len;
   for (len = 1; len <= Num_Tag_List.len; len++)
     {
	Slrn_Header_Type *h = Num_Tag_List.headers[len - 1];
	h->flags &= ~HEADER_NTAGGED;
	h->tag_number = 0;
     }
   Num_Tag_List.len = 0;
   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

static void toggle_one_header_tag (Slrn_Header_Type *h) /*{{{*/
{
   if (h == NULL) return;
   if (h->flags & HEADER_TAGGED)
     {
	h->flags &= ~HEADER_TAGGED;
     }
   else
     {
	h->flags |= HEADER_TAGGED;
	if (h->flags & HEADER_READ)
	  {
	     h->flags &= ~HEADER_READ;
	     Number_Read--;
	  }
     }
}

/*}}}*/

static void toggle_header_tag (void) /*{{{*/
{
   if (Slrn_Prefix_Arg_Ptr != NULL)
     {
	Slrn_Header_Type *h;

	Slrn_Prefix_Arg_Ptr = NULL;
	h = _art_Headers;
	while (h != NULL)
	  {
	     h->flags &= ~HEADER_TAGGED;
	     h = h->next;
	  }
	Slrn_Full_Screen_Update = 1;
	return;
     }

   if ((Slrn_Current_Header->parent != NULL)/* in middle of thread */
       || (Slrn_Current_Header->child == NULL)/* At top with no child */
       /* or at top with child showing */
       || (0 == (Slrn_Current_Header->child->flags & HEADER_HIDDEN)))
     {
	toggle_one_header_tag (Slrn_Current_Header);
     }
   else
     {
	for_this_tree (Slrn_Current_Header, toggle_one_header_tag);
     }
   (void) slrn_header_down_n (1, 0);
   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

int slrn_prev_tagged_header (void) /*{{{*/
{
   Slrn_Header_Type *h = Slrn_Current_Header;

   if (h == NULL) return 0;

   while (h->prev != NULL)
     {
	h = h->prev;
	if (h->flags & HEADER_TAGGED)
	  {
	     slrn_goto_header (h, 0);
	     return 1;
	  }
     }
   return 0;
}

/*}}}*/

int slrn_next_tagged_header (void) /*{{{*/
{
   Slrn_Header_Type *h = Slrn_Current_Header;

   if (h == NULL) return 0;

   while (h->next != NULL)
     {
	h = h->next;
	if (h->flags & HEADER_TAGGED)
	  {
	     slrn_goto_header (h, 0);
	     return 1;
	  }
     }
   return 0;
}

/*}}}*/

/*}}}*/
/*{{{ Header specific functions */

void _art_find_header_line_num (void) /*{{{*/
{
   Slrn_Full_Screen_Update = 1;
   find_non_hidden_header ();
   Slrn_Header_Window.lines = (SLscroll_Type *) _art_Headers;
   Slrn_Header_Window.current_line = (SLscroll_Type *) Slrn_Current_Header;
   SLscroll_find_line_num (&Slrn_Header_Window);
}

/*}}}*/

static void init_header_window_struct (void) /*{{{*/
{
   Slrn_Header_Window.nrows = 0;
   Slrn_Header_Window.hidden_mask = HEADER_HIDDEN;
   Slrn_Header_Window.current_line = (SLscroll_Type *) Slrn_Current_Header;

   Slrn_Header_Window.cannot_scroll = SLtt_Term_Cannot_Scroll;
   Slrn_Header_Window.border = 1;

   if (Slrn_Scroll_By_Page)
     {
	/* Slrn_Header_Window.border = 0; */
	Slrn_Header_Window.cannot_scroll = 2;
     }

   Slrn_Header_Window.lines = (SLscroll_Type *) _art_Headers;
   art_winch ();		       /* get row information correct */

   _art_find_header_line_num ();
}

/*}}}*/

static Slrn_Header_Type *find_header_from_serverid (NNTP_Artnum_Type id) /*{{{*/
{
   Slrn_Header_Type *h;

   h = Slrn_First_Header;
   while (h != NULL)
     {
	if (h->number > id) return NULL;
	if (h->number == id) break;
	h = h->real_next;
     }
   return h;
}

/*}}}*/

static void kill_cross_references (Slrn_Header_Type *h) /*{{{*/
{
   char *b;
   char *group, *g;
   NNTP_Artnum_Type num;

   if ((h->xref == NULL) || (*h->xref == 0))
     {
	if ((Header_Showing != h)
	    || (NULL == (b = slrn_art_extract_header ("Xref: ", 6))))
	  {
	     return;
	  }
     }
   else b = h->xref;

   /* The format appears to be:
    * Xref: machine group:num group:num...
    */

   /* skip machine name */
   while (*b > ' ') b++;

   while (*b != 0)
     {
	while (*b == ' ') b++;
	if (*b == 0) break;

	/* now we are looking at the groupname */
	g = b;
	while (*b && (*b != ':')) b++;
	if ((g == b) || (b[0] == 0) || (b[1] == 0)
	    || (NULL == (group = slrn_strnmalloc (g, (unsigned int)(b-g), 0))))
	  break;
	b++;			       /* skip ':' */
	num = NNTP_STR_TO_ARTNUM (b);
	while ((*b <= '9') && (*b >= '0')) b++;
	if ((num != h->number)
	    || strcmp (group, Slrn_Current_Group_Name))
	  slrn_mark_articles_as_read (group, num, num);
	SLfree (group);
     }
}

/*}}}*/

static void for_all_headers (void (*func)(Slrn_Header_Type *), int all) /*{{{*/
{
   Slrn_Header_Type *h, *end;

   Slrn_Full_Screen_Update = 1;

   if (func == NULL) return;

   if (all) end = NULL; else end = Slrn_Current_Header;

   h = _art_Headers;

   while (h != end)
     {
	(*func)(h);
	h = h->next;
     }
}

/*}}}*/

/* When using this, make sure *header points to a valid header. */
int slrn_goto_header (Slrn_Header_Type *header, int read_flag) /*{{{*/
{
   Slrn_Current_Header = header;
   if (header->flags & HEADER_HIDDEN)
     slrn_uncollapse_this_thread (header, 0);
   _art_find_header_line_num ();

   if (read_flag) select_article (1);
   return 0;
}

/*}}}*/

/*{{{ parse_from  */
static char *read_comment (char *start, char *dest, size_t max) /*{{{*/
{
   int depth = 1; /* RFC 2822 allows nesting */
   unsigned int len = 1;

   if ((*start != '(') || (*(++start) == ')'))
     depth = 0;
   while (*start && depth)
     {
	if (*start == '(') depth++;
	else if (*start == ')') depth--;
	else if ((*start == '\\') && (*(++start) == '\0'))
	  break;
	if (depth && (len < max))
	  {
	     *dest++ = *start; len++;
	  }
	start++;
     }
   if (len < max) *dest = '\0';
   if (*start == ')') start++;
   while ((*start == ' ') || (*start == '\t')) start++;

   return start;
}
/*}}}*/

/* Copy a quoted string from start to dest.
 * Assumptions: start is the beginning of a quoted string
 *              dest can hold at least max characters
 * Returns: Length of the quoted string; if it is > max, it has not been
 *          fully copied to dest
 */
static size_t read_quotedstring (char *start, char *dest, size_t max) /*{{{*/
{
   size_t len = 0;

   if (len < max)
      *dest++ = '"';
   len++;
   start++;

   while (*start && (*start != '"'))
     {
	unsigned int backslash = 0;
	if (*start == '\\')
	  {
	     backslash = 1;
	     if (*(++start) == '\0')
		break;
	  }
	if (len + backslash < max)
	  {
	     if (backslash) *dest++ = '\\';
	     *dest++ = *start;
	  }
	len += 1 + backslash;
	start++;
     }
   if (*start == '"')
     {
	if (len < max)
	   *dest++ = '"';
	len++;
	start++;
     }
   return len;
}
/*}}}*/

/* Copy a dot-atom from start to dest (which can hold at least max chars).
 * Returns: Length of the dot-atom; if it is > max, it has not been fully
 * copied to dest
 */
static size_t read_dotatom (char *start, char *dest, size_t max) /*{{{*/
{
   size_t len = 0;

   if (len < max)
      *dest++ = *start;
   len++;
   start++;

   while (1)
     {
	unsigned int dot = 0;
	if (*start == '.')
	  {
	     dot = 1;
	     start++;
	  }
	if ((*start == '\0') ||
	    (NULL != slrn_strbyte ("\t \"(),.:;<>@[\\]", *start)))
	   break;
	if (len + dot < max)
	  {
	     if (dot) *dest++ = '.';
	     *dest++ = *start;
	  }
	len += 1 + dot;
	start++;
     }
   return len;
}
/*}}}*/

/* Copy the localpart of an email address from start to dest.
 * The result will be NUL-terminated.
 * Returns a pointer to the char behind the localpart (or NULL, if nothing
 * is left behind the localpart).
 */
static char *read_localpart (char *start, char *dest, size_t max) /*{{{*/
{
   size_t len = 0; /* Can become larger than max! */

   if (max) *dest = '\0';

   if (*start == 0)
     return NULL;

   while ((*start == ' ') || (*start == '\t')) start++;
   while (*start == '(')
     {
	if (NULL == (start = read_comment (start, NULL, 0)))
	   return NULL;
     }

   if (*start == '"')
     {
	len = read_quotedstring (start, dest, max ? max-1 : 0);
     }
   else if (*start && (NULL == slrn_strbyte ("\t \"(),.:;<>@[\\]", *start)))
     {
	len = read_dotatom (start, dest, max ? max-1 : 0);
     }
   else
     {
	if (max) *dest = '\0';
	len = 1;
     }
   start += len;
   if (max)
     {
	if (len <= max-1)
	   dest[len] = '\0';
	else
	   dest[max-1] = '\0';
     }

   while ((*start == ' ') || (*start == '\t')) start++;
   while ((start != NULL) && (*start == '('))
     {
	start = read_comment (start, NULL, 0);
     }

   if ((start != NULL) && (*start == '\0'))
      return NULL;
   return start;
}
/*}}}*/

/* Copy the domain part of an email address from start to dest.  The result
 * will be NUL-terminated (which means that max is expected to be at least 1).
 * Returns a pointer to the char behind the domain (in start) or NULL if no
 * valid domain name is found or it did not fit.
 */
static char *read_domain (char *start, char *dest, size_t max) /*{{{*/
{
   size_t len = 0; /* Can become larger than max! */

   *dest = '\0';
   while ((*start == ' ') || (*start == '\t')) start++;
   while (*start == '(')
     {
	if (NULL == (start = read_comment (start, NULL, 0)))
	   return NULL;
     }

   if (*start == '[') /* read domain literal */
     {
	while ((*start == ' ') || (*start == '\t')) start++;
	while (*start && (*start != ']'))
	  {
	     size_t dotatom_len = read_dotatom (start, dest, max-len-1);
	     if (dotatom_len >= max-len)
	       {
		  dest[max-1] = '\0';
		  if (start[max-2] == '.')
		     dest[max-2] = '\0';
		  return NULL;
	       }
	     start += dotatom_len;
	     dest += dotatom_len;
	     len += dotatom_len;

	     while ((*start == ' ') || (*start == '\t'))
	       {
		  if (len >= max-1)
		    {
		       dest[max-1] = '\0';
		       return NULL;
		    }
		  *dest++ = *start++;
		  len++;
	       }
	  }
	if ((*start != ']') || (len >= max-1))
	  {
	     dest[max-1] = '\0';
	     return NULL;
	  }
	*dest++ = ']';
	len++;
	start++;
     }
   else if (*start && (NULL == slrn_strbyte ("\t \"(),.:;<>@\\]", *start)))
     {
	len = read_dotatom (start, dest, max);
	start += len;
	dest += len;
     }
   else
      return NULL;

   if (len>=max)
      return NULL;
   *dest = '\0';

   return start;
}
/*}}}*/

/* Parse an address line and try to copy the first email address to dest.
 * dest must be able to hold at least max characters
 * max must be at least 1, result is always NUL-terminated
 * Returns pointer to the first character behind the address in from or
 * NULL if no (valid) address could be found (or it did not fit). */
static char *parse_from (char *from, char *dest, size_t max) /*{{{*/
{
   unsigned int len;
   char *p;

   /* First, try to find an address in <>;
    * else, assume simple form (read from beginning) */

   if (from == NULL) return NULL;
   *dest = '\0';
   from = slrn_skip_whitespace (from);
   p = from;

   /* The allowed syntax before the address in <> is similar to
    * the one of the local part. */
   while ((p != NULL) && (*p != '<'))
     {
	p = read_localpart (p, NULL, 0);
     }

   if (p != NULL) from = p + 1;

   if (NULL == (p = read_localpart (from, dest, max)))
      return NULL;
   len = strlen (dest);
   if ((*p != '@') || (len >= max - 3) || (len == 0))
     return NULL;

   dest[len] = '@';
   return read_domain (p + 1, dest + len + 1, max - len - 1);
}

/*}}}*/

/*}}}*/

/*}}}*/

/*{{{ get_header_real_name */

static void unquote_string (char *str)
{
   int offset = 1;
   if (*str == '"')
     {
	if (str[1] == '\\')
	  offset = 2;
	while (str[offset] &&
	       ((str[offset]!='"') || str[offset+1]))
	  {
	     str[0] = str[offset];
	     str++;
	     if (str[offset] == '\\')
	       {
		  offset++;
	       }
	  }
	*str = '\0';
     }
}

static void get_header_real_name (Slrn_Header_Type *h) /*{{{*/
{
   char buf[128];
   char *from = h->from, *f, *p;
   /* First, try to find "display name <address>";
    * else, skip the address and look for a comment */

   f = from = slrn_skip_whitespace (from);
   p = buf;
   *buf = '\0';

   while ((f != NULL) && (*f != '<'))
     {
	f = read_localpart (f, p, (unsigned int) (sizeof(buf) - (p-buf)));
	unquote_string(p); /* the name may have been quoted */
	p += strlen (p);
	if ((f != NULL) && (p + 1 < buf + sizeof (buf)))
	  {
	     *p++ = ' '; *p = '\0';
	  }
     }

   if (f == NULL)
     {
	char dummy[256];

	*buf = '\0';
        /* This will give us a pointer behind the address */
	f = parse_from (from, dummy, sizeof(dummy));
	if (f != NULL)
	  {
	     while (*f && ((*f == ' ') || (*f == '\t'))) f++;
	     if (*f == '(')
	       read_comment (f, buf, sizeof (buf));
	  }
     }

   if (*buf == '\0')
     {
	slrn_strncpy (buf, from, sizeof (buf));
     }

   if (*buf != '\0')
     {
	f = buf + strlen (buf) - 1;
	while ((f != buf) && ((*f == ' ') || (*f == '\t')))
	  {
	     *f-- = '\0';
	  }
     }
   if (h->realname != NULL)
     slrn_free (h->realname);

   h->realname = slrn_strmalloc (buf, 0);
}

/*}}}*/

/*}}}*/

Slrn_Header_Type *_art_find_header_from_msgid (char *r0, char *r1) /*{{{*/
{
   unsigned long hash;
   Slrn_Header_Type *h;
   unsigned int len;
   len = (unsigned int) (r1 - r0);
   hash = slrn_compute_hash ((unsigned char *) r0, (unsigned char *) r1);

   h = Header_Table[hash % HEADER_TABLE_SIZE];
   while (h != NULL)
     {
	if (!slrn_case_strncmp ( h->msgid,
				 r0,
				len))
	  break;

	h = h->hash_next;
     }
   return h;
}

/*}}}*/

Slrn_Header_Type *slrn_find_header_with_msgid (char *msgid) /*{{{*/
{
   return _art_find_header_from_msgid (msgid, msgid + strlen (msgid));
}

/*}}}*/

static void goto_article (void) /*{{{*/
{
   Slrn_Header_Type *h;
   NNTP_Artnum_Type want_n;

   if (-1 == slrn_read_artnum_int (_("Goto article: "), NULL, &want_n))
     return;

   h = _art_Headers;
   while (h != NULL)
     {
	if (h->number == want_n)
	  {
	     Slrn_Current_Header = h;
	     if (h->flags & HEADER_HIDDEN) slrn_uncollapse_this_thread (h, 0);
	     _art_find_header_line_num ();
	     return;
	  }

	h = h->next;
     }

   slrn_error (_("Article not found."));
}

/*}}}*/

int slrn_is_article_visible (void)
{
   int mask = 0;

   if (Slrn_Current_Header != NULL)
     {
	if (Slrn_Current_Header == Header_Showing)
	  mask |= 2;
	if (Article_Visible)
	  mask |= 1;
     }
   return mask;
}

/*}}}*/

static int prepare_article (Slrn_Article_Type *a)
{
   if (a == NULL)
     return -1;

   slrn_art_mark_quotes (a);

#if SLRN_HAS_SPOILERS
   slrn_art_mark_spoilers (a);
   Num_Spoilers_Visible = 1;
#endif

   /* We should ignore signature delimiters in verbatim parts, so
    * order matters. */
   if (Slrn_Process_Verbatim_Marks)
     slrn_art_mark_verbatim (a);

   /* mark_signature unmarks lines in the signature which look like
    * quotes, so do it after mark_quotes, but before hide_or_unhide_quotes */
   slrn_art_mark_signature (a);
   slrn_art_mark_pgp_signature (a);

   /* The actual wrapping is done elsewhere. */
   if (Slrn_Wrap_Mode & 0x4)
     a->is_wrapped = 1;
   if (Headers_Hidden_Mode)
     _slrn_art_hide_headers (a);
   if (Slrn_Quotes_Hidden_Mode)
     _slrn_art_hide_quotes (a, 0);
   if (Slrn_Signature_Hidden)
     _slrn_art_hide_signature (a);
   if (Slrn_Pgp_Signature_Hidden)
     _slrn_art_hide_pgp_signature (a);
   if (Slrn_Verbatim_Hidden)
     _slrn_art_hide_verbatim (a);
   return 0;
}

/* Downloads an article and returns it; header lines are marked */
static Slrn_Article_Type *read_article (Slrn_Header_Type *h, int kill_refs) /*{{{*/
{
   Slrn_Article_Type *retval;
   Slrn_Article_Line_Type *cline, *l;
   int status, num_lines_update;
   unsigned int len, total_lines;
   char buf[NNTP_BUFFER_SIZE];
#ifdef HAVE_GETTIMEOFDAY
   double current_bps;
   struct timeval start_time, new_time;
   gettimeofday(&start_time, NULL);
#endif

   if (h->tag_number) slrn_message_now (_("#%2d/%-2d: Retrieving... %s"),
					h->tag_number, Num_Tag_List.len,
					h->subject);
   else slrn_message_now (_("[" NNTP_FMT_ARTNUM "] Reading..."), h->number);

   status = Slrn_Server_Obj->sv_select_article (h->number, h->msgid);
   if (status != OK_ARTICLE)
     {
	if (status == -1)
	  {
	     slrn_error ("%s", _("Server failed to return the article."));
	     return NULL;
	  }

	slrn_error (_("Article " NNTP_FMT_ARTNUM " is unavailable."), h->number);

	if (kill_refs && ((h->flags & HEADER_READ) == 0) &&
	    ((h->flags & HEADER_DONT_DELETE_MASK) == 0))
	  {
	     kill_cross_references (h);
	     h->flags |= HEADER_READ;
	     Number_Read++;
	  }
	return NULL;
     }

   if ((num_lines_update = Slrn_Reads_Per_Update) < 5)
     {
	if (h->lines < 200)
	  num_lines_update = 20;
	else
	  num_lines_update = 50;
     }

   retval = (Slrn_Article_Type*) slrn_malloc (sizeof(Slrn_Article_Type), 1, 1);
   if (retval == NULL) return NULL;

   cline = NULL;
   total_lines = 0;

   /* Reset byte counter */
   (void) Slrn_Server_Obj->sv_nntp_bytes(1);

   while (1)
     {
	status = Slrn_Server_Obj->sv_read_line(buf, sizeof(buf));
	if (status == 0)
	  break;

	if ((status == -1) || (SLang_get_error() == USER_BREAK))
	  {
	     if (Slrn_Server_Obj->sv_reset != NULL)
	       Slrn_Server_Obj->sv_reset ();
	     slrn_error ("%s", _("Article transfer aborted or connection lost."));
	     break;
	  }

	total_lines++;
	if ((1 == (total_lines % num_lines_update))
	    /* Just so the ratio does not confuse the reader because of the
	     * header lines... */
	    && (total_lines < (unsigned int) h->lines))
	  {
#ifdef HAVE_GETTIMEOFDAY
	     gettimeofday(&new_time, NULL);
	     current_bps = time_diff(new_time, start_time)/1000000.0;
	     if (current_bps > 0)
	       current_bps = (Slrn_Server_Obj->sv_nntp_bytes(0) / 1024.0)/current_bps;
#endif
	     if (h->tag_number)
#ifdef HAVE_GETTIMEOFDAY
	       slrn_message_now (_("#%2d/%-2d: Read %4d/%-4d lines (%s) at %.2fkB/sec"),
				 h->tag_number, Num_Tag_List.len, total_lines,
				 h->lines, h->subject, current_bps);
#else
	       slrn_message_now (_("#%2d/%-2d: Read %4d/%-4d lines (%s)"),
				 h->tag_number, Num_Tag_List.len,
				 total_lines, h->lines, h->subject);
#endif
	     else
#ifdef HAVE_GETTIMEOFDAY
	       slrn_message_now (_("[" NNTP_FMT_ARTNUM "] Read %d/%d lines so far at %.2fkB/sec"),
				 h->number, total_lines, h->lines, current_bps);
#else
	       slrn_message_now (_("[" NNTP_FMT_ARTNUM"] Read %d/%d lines so far"),
				 h->number, total_lines, h->lines);
#endif
	  }

	len = strlen (buf);

	l = (Slrn_Article_Line_Type *) slrn_malloc (sizeof (Slrn_Article_Line_Type),
						    1, 1);
	if ((l == NULL)
	    || (NULL == (l->buf = slrn_malloc (len + 1, 0, 1))))
	  {
	     slrn_free ((char *) l);
	     slrn_art_free_article (retval);
	     return NULL;
	  }

	/* Note: I no longer remove _^H combinations, as it corrupts yenc-
	 * encoded data and seems unnecessary in today's usenet.
	 *
	 * The processsing of the leading doubled dot took place here.
	 * However, that is now handled at the protocol layer.
	 */
#if 0
	if ((*buf == '.') && (*(buf + 1) == '.'))
	  strcpy (l->buf, buf + 1); /* safe */
	else
#endif
	  strcpy (l->buf, buf); /* safe */

	l->next = l->prev = NULL;
	l->flags = 0;

	if (retval->lines == NULL)
	  retval->lines = l;
	else
	  {
	     l->prev = cline;
	     cline->next = l;
	  }
	cline = l;
     }

   if (retval->lines == NULL)
     {
	slrn_error (_("Server sent empty article."));
	slrn_art_free_article (retval);
	return NULL;
     }

   retval->raw_lines = copy_article_line (retval->lines);
   if (retval->raw_lines == NULL)
     {
	slrn_art_free_article (retval);
	return NULL;
     }
   retval->cline = retval->lines;
   retval->needs_sync = 1;

   if (kill_refs && ((h->flags & HEADER_READ) == 0) &&
       ((h->flags & HEADER_DONT_DELETE_MASK) == 0))
     {
	kill_cross_references (h);
	h->flags |= HEADER_READ;
	Number_Read++;
     }

   slrn_mark_header_lines (retval);
   slrn_mime_init (&retval->mime);

   return retval;
}
/*}}}*/

/* On errors, free a and return -1 */
#if 0
static int art_undo_modifications (Slrn_Article_Type *a)
{
   unsigned int linenum = Slrn_Article_Window.line_num;
   if (a == NULL) return 0;

   a->is_modified = 0;
   a->is_wrapped = 0;
   slrn_mime_free (&a->mime);
   slrn_mime_init (&a->mime);

   slrn_art_free_article_line (a->lines);
   a->lines = NULL;
   if (NULL == (a->lines = copy_article_line (a->raw_lines)))
     {
	slrn_art_free_article (a);
	return -1;
     }

   a->cline = a->lines;
   slrn_mark_header_lines (a);
   if (-1 == _slrn_art_unfold_header_lines (a))
     {
	slrn_art_free_article (a);
	return -1;
     }
   prepare_article (a);
   init_article_window_struct();
   slrn_art_linedn_n (linenum-1); /* find initial line */

   return 0;
}
#endif

static int select_header (Slrn_Header_Type *h, int kill_refs) /*{{{*/
{
   Slrn_Article_Type *a;
   Slrn_Header_Type *last_header_showing;
   char *subj, *from;

   last_header_showing = Header_Showing;

   if ((Header_Showing == h)
       && (Slrn_Current_Article != NULL))
     { /* We already have the article in memory. */
	return 0;
     }

   free_article ();
   if (NULL == (a = read_article (h, kill_refs)))
     return -1;

   At_End_Of_Article = NULL;
   Do_Rot13 = 0;
   Article_Window_HScroll = 0;
   Slrn_Full_Screen_Update = 1;

   if (-1 == _slrn_art_unfold_header_lines (a))
     {
	slrn_art_free_article (a);
	return -1;
     }

   if ( -1 == slrn_mime_process_article (a))
     {
	slrn_art_free_article (a);
	slrn_error(_("MIME processing unsuccessful"));
	return -1;
     }

   /* Only now may the article be said to be the current article */
   Slrn_Current_Article = a;

   /* RFC 2980 says not to unfold headers when writing them to the overview.
    * Work around this by taking Subject and From out of the article. */
   subj = slrn_art_extract_header ("Subject: ", 9);
   from = slrn_art_extract_header ("From: ", 6);

   if ((NULL != subj) && (NULL != from) &&
       (strcmp (subj, h->subject) || strcmp (from, h->from)))
     {
	subj = slrn_strmalloc (subj, 1);
	if (subj == NULL)
	  return -1;
	slrn_free (h->subject);
	h->subject = subj;

	from = slrn_strmalloc (from, 1);
	if (from == NULL)
	  return -1;
	slrn_free (h->from);
	h->from = from;
	get_header_real_name (h);
     }

   if (last_header_showing != h)
     {
	Last_Read_Header = last_header_showing;
     }
   Header_Showing = h;

   if (h == Slrn_Current_Header)
     {
	(void) prepare_article (a);
	if (Last_Read_Header == NULL)
	  Last_Read_Header = h;
     }

   init_article_window_struct ();

   (void) slrn_run_hooks (HOOK_READ_ARTICLE, 0);

   /* slrn_set_suspension (0); */
   return 0;
}

/*}}}*/

int slrn_string_to_article (char *str, int handle_mime)
{
   char *estr;
   Slrn_Article_Line_Type *l, *cline = NULL;
   Slrn_Article_Type *a;

   if ((str == NULL) || (*str == 0))
     return -1;

   if (NULL == (a = Slrn_Current_Article))
     return -1;

   free_article_lines (a);
   a->is_modified = 1;
   a->needs_sync = 1;

   while (1)
     {
	unsigned int len;

	estr = slrn_strbyte (str, '\n');
	if (estr == NULL)
	  estr = str + strlen (str);

	len = (unsigned int) (estr - str);

	if ((0 == len) && (0 == *estr))
	  break;

	l = (Slrn_Article_Line_Type *) slrn_malloc (sizeof(Slrn_Article_Line_Type),
						    1, 1);
	if ((l == NULL)
	    || (NULL == (l->buf = slrn_malloc (len + 1, 0, 1))))
	  {
	     slrn_free ((char *) l);
	     free_article ();
	     return -1;
	  }
	strncpy (l->buf, str, len);
	l->buf[len] = 0;

	l->next = l->prev = NULL;
	l->flags = 0;

	if (a->lines == NULL)
	  a->lines = l;
	else
	  {
	     l->prev = cline;
	     cline->next = l;
	  }
	cline = l;

	str = estr;
	if (*str == 0)
	  break;
	else str++;		       /* skip \n */
     }

#if 0 /* does this make any sense? */
   Header_Showing = Slrn_Current_Header;
#endif
   if (NULL == (a->raw_lines = copy_article_line (a->lines)))
     {
	free_article ();
	return -1;
     }
   a->cline = a->lines;

   slrn_mark_header_lines (a);

   if (-1 == _slrn_art_unfold_header_lines (a))
     {
	free_article ();
	return -1;
     }

   if (handle_mime)
     {
	slrn_mime_free (&a->mime);
	slrn_mime_init (&a->mime);

	if (-1 == slrn_mime_process_article (a))
	  {
	     slrn_art_free_article (a);
	     slrn_error(_("MIME processing unsuccessful"));
	     return -1;
	  }
     }

   prepare_article (a);

   /* This must be called before any of the other functions are called because
    * they may depend upon the line number and window information.
    */
   init_article_window_struct ();
   set_article_visibility (1);
   return 0;
}

/*{{{ reply, reply_cmd, forward, followup */

static int insert_followup_format (char *f, FILE *fp) /*{{{*/
{
   char ch, *s, *m, *f_conv=NULL;
   unsigned int i;
   char buf[256];

   if ((f == NULL) || (*f == 0))
     return -1;

   if (Header_Showing == NULL)
     return -1;

   if (slrn_test_and_convert_string(f, &f_conv, Slrn_Editor_Charset, Slrn_Display_Charset) == -1)
	return -1;

   if (f_conv != NULL)
	f=f_conv;

   while ((ch = *f++) != 0)
     {
	if (ch != '%')
	  {
	     putc (ch, fp); /* charset ok*/
	     continue;
	  }
	s = m = NULL;
	ch = *f++;
	if (ch == 0) break;

	switch (ch)
	  {
	   case 's':
	     s = Header_Showing->subject;
	     break;
	   case 'm':
	     s = Header_Showing->msgid;
	     break;
	   case 'r':
	     s = Header_Showing->realname;
	     break;
	   case 'R':
	     for (i=0; i <= strlen (Header_Showing->realname); i++)
	       {
		  if (Header_Showing->realname[i] == ' ')
		       break;
	       }
	     m = slrn_strnmalloc (Header_Showing->realname,i,1);
	     break;
	   case 'f':
	     (void) parse_from (Header_Showing->from, buf, sizeof(buf));
	     s = buf;
	     break;
	   case 'n':
	     s = Slrn_Current_Group_Name;
	     break;
	   case 'd':
	     s = Header_Showing->date;
	     break;
	   case 'D':
	       {
		  char *fmtstr;

		  fmtstr = Slrn_Followup_Date_Format;
		  if (fmtstr == NULL)
		    fmtstr = _("%Y-%m-%d");

		  slrn_strftime(buf, sizeof(buf), fmtstr,
				Header_Showing->date,
				Slrn_Use_Localtime & 0x02);
		  s = buf;
	       }
	     break;
	   case '%':
	   default:
	     putc (ch, fp); /* charset ok*/
	  }

	if (m != NULL)
	  {
	     if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s", m) < 0)
	       {
		  slrn_free(m);
		  slrn_free(f_conv);
		  return -1;
	       }
	     slrn_free(m);
	  }
	if (s != NULL)
	  {
	     if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s", s) < 0)
	       {
		  slrn_free(f_conv);
		  return -1;
	       }
	  }
     } /* while ((ch = *f++) != 0) */

   slrn_free(f_conv);

   return 0;
}

/*}}}*/

static char *extract_reply_address (void)
{
   char *from;

   if ((NULL != (from = slrn_extract_header ("Mail-Reply-To: ", 15)))
       && (*from != 0))
     return from;

   if ((NULL != (from = slrn_extract_header ("Reply-To: ", 10)))
       && (*from != 0))
     return from;

   return slrn_extract_header ("From: ", 6);
}

/* This function is called after an article hook has been called to make
 * sure the user did not delete the article.
 */
static int check_for_current_article (void)
{
   if (SLang_get_error ())
     return -1;
   if (Slrn_Current_Article == NULL)
     {
	slrn_error (_("This operation requires an article"));
	return -1;
     }
   return 0;
}

static int run_article_hook (unsigned int hook)
{
   slrn_run_hooks (hook, 0);
   return check_for_current_article ();
}

/* This function strips the old subject included with "(was: <old sub>)" */
void slrn_subject_strip_was (char *subject) /*{{{*/
{
   SLRegexp_Type **r;
   unsigned char *was = NULL;
   unsigned int len = strlen (subject);

   r = Slrn_Strip_Was_Regexp;

   while (*r != NULL)
     {
	SLRegexp_Type *re;
	re = *r++;
	was = (unsigned char *)SLregexp_match (re, subject, len);
	if (NULL != was)
	  {
	     if (was == (unsigned char*) subject)
	       was = NULL;
	     else break;
	  }
     }

   if (was != NULL)
     *was = '\0';
}

/*}}}*/

static char *subject_skip_re (char *subject) /*{{{*/
{
   SLRegexp_Type **r;
   unsigned int len;

   while (1)
     {
	subject = slrn_skip_whitespace (subject);

	if (((*subject | 0x20) == 'r')
	    && ((*(subject + 1) | 0x20) == 'e')
	    && (*(subject + 2) == ':'))
	  {
	     subject = subject + 3;
	     continue;
	  }

	r = Slrn_Strip_Re_Regexp;
	len = strlen (subject);

	while (*r != NULL)
	  {
	     SLRegexp_Type *re = *r;
	     char *match_pos;

#if SLANG_VERSION < 20000
	     match_pos = SLang_regexp_match ((unsigned char*) subject, len, re);
#else
	     match_pos = SLregexp_match (re, subject, len);
#endif
	     if (subject == match_pos)
	       {
#if SLANG_VERSION < 20000
		  subject = subject + re->end_matches[0];
#else
		  unsigned int ofs, mlen;
		  (void) SLregexp_nth_match (re, 0, &ofs, &mlen);
		  subject += mlen;
#endif
		  break;
	       }
	     r++;
	  }
	if (*r == NULL)
	  break;
     }

   return subject;
}

/*}}}*/

/* If from != NULL, it's taken as the address to send the reply to, otherwise
 * the reply address is taken from Reply-To: or From: */
static void reply (char *from, int use_cc) /*{{{*/
{
   char *msgid, *subject, *from_t;
   Slrn_Article_Line_Type *l;
   FILE *fp;
   char file [SLRN_MAX_PATH_LEN];
   char from_buf[256];
   unsigned int n;
   int is_wrapped;
   char *quote_str;

   if ((-1 == slrn_check_batch ())
       || (-1 == select_affected_article ())
       || (-1 == run_article_hook (HOOK_REPLY))
      )
     return;

   /* Check for FQDN.  If it appear bogus, warn user */
   if (from == NULL) from = extract_reply_address ();
   from_t = parse_from (from, from_buf, sizeof(from_buf));

   if ((from_t == NULL)
       || ((strlen(from_buf) > 8) &&
	   !(slrn_case_strcmp(".invalid",
			      from_buf+strlen(from_buf)-8))))
     {
	if (0 == slrn_get_yesno (1, _("%s appears invalid.  Continue anyway"),
				 (*from_buf) ? from_buf : _("Email address")))
	  return;
     }

   if (Slrn_Use_Tmpdir)
     fp = slrn_open_tmpfile (file, sizeof (file));
   else fp = slrn_open_home_file (SLRN_LETTER_FILENAME, "w", file,
				  sizeof (file), 0);

   if (NULL == fp)
     {
	slrn_error (_("Unable to open %s for writing."), file);
	return;
     }

   /* parse header */
   msgid = slrn_extract_header ("Message-ID: ", 12);
   subject = slrn_extract_header ("Subject: ", 9);

   n = 0;

   if (from != NULL)
     {
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "To: %s\n", from) < 0)
	     return;
     }
   else
	fputs ("To: \n", fp);
   n++;

   if (use_cc)
     {
	char *cc, *f;

	f = slrn_extract_header ("To: ", 4);
	cc = slrn_extract_header ("Cc: ", 4);
	if ((f != NULL) || (cc != NULL))
	  {
	     fputs ("Cc: ", fp);
	     if (f != NULL)
	       {
		  fputs (f, fp);
		  if (cc != NULL)
		    fputs (", ", fp);
	       }
	     if (cc != NULL)
	       fputs (cc, fp);
	     fputs ("\n", fp);
	     n++;
	  }
     }

   if (Slrn_Generate_Email_From)
     {
	char *fromstr = slrn_make_from_header ();
	if (fromstr == NULL) return;
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s\n", fromstr)< 0)
	  {
	     slrn_free(fromstr);
	     return;
	  }
	slrn_free(fromstr);
	n += 1;
     }

   if (subject == NULL) subject = "";
   else subject = subject_skip_re (subject);

   /* We need a copy of subject as slrn_subject_strip_was() might change it */
   if (NULL == (subject = slrn_safe_strmalloc (subject)))
     return;

   slrn_subject_strip_was (subject);

   if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "Subject: Re: %s\n", subject) < 0)
     {
	slrn_free(subject);
	return;
     }
   fprintf (fp, "In-Reply-To: %s\n", (msgid == NULL ? "" : msgid));
   n += 2;

   if ((msgid != NULL) && (*msgid != 0))
     {
	char *refs = slrn_extract_header("References: ", 12);
	if ((refs == NULL) || (*refs == 0))
	  fprintf (fp, "References: %s\n", msgid);
	else
	  fprintf (fp, "References: %s %s\n", refs, msgid);
	n++;
     }

   if (0 != *Slrn_User_Info.replyto)
     {
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "Reply-To: %s\n", Slrn_User_Info.replyto) < 0)
	  {
	     slrn_free (subject);
	     return;
	  }
	n += 1;
     }

   n += slrn_add_custom_headers (fp, Slrn_Reply_Custom_Headers, insert_followup_format);
   fputs ("\n", fp);

   insert_followup_format (Slrn_User_Info.reply_string, fp);
   fputs ("\n", fp);

   n += 2;

   is_wrapped = Slrn_Current_Article->is_wrapped;
   (void) _slrn_art_unwrap_article (Slrn_Current_Article);

   l = Slrn_Current_Article->lines;
   if ((Slrn_Prefix_Arg_Ptr == NULL) || ((*Slrn_Prefix_Arg_Ptr & 1) == 0))
     {
	while ((l != NULL) && (*l->buf != 0)) l = l->next;
	if (l != NULL) l = l->next;
     }

   if (NULL == (quote_str = Slrn_Quote_String))
     quote_str = ">";

   while (l != NULL)
     {
	int smart_space = (Slrn_Smart_Quote & 0x01) && ! (l->flags & QUOTE_LINE)
	  && (*l->buf != 0);
	if ((*l->buf == 0) && (Slrn_Smart_Quote & 0x02))
	  fputc ('\n', fp);
	else
	  {
	     if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s%s%s\n", quote_str, (smart_space)? " " : "" , l->buf) < 0)
	       {
		  slrn_free (subject);
		  return;
	       }
	  }
	l = l->next;
     }

   if (is_wrapped)
     (void) _slrn_art_wrap_article (Slrn_Current_Article);

   slrn_add_signature (fp);
   slrn_fclose (fp);

   slrn_mail_file (file, 1, n, from_buf, subject);
   slrn_free (subject);
   if (Slrn_Use_Tmpdir) (void) slrn_delete_file (file);
}

/*}}}*/

static void reply_cmd (void) /*{{{*/
{
   if (-1 == slrn_check_batch ())
     return;

   select_affected_article ();
   slrn_update_screen ();

   if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_POST)
       && (0 == slrn_get_yesno (1, _("Are you sure you want to reply"))))
     return;

   reply (NULL, (Slrn_Prefix_Arg_Ptr != NULL) ? (*Slrn_Prefix_Arg_Ptr & 2) : 0);
}

/*}}}*/

static void forward_article (void) /*{{{*/
{
   char *subject;
   Slrn_Article_Line_Type *l;
   FILE *fp;
   char file [SLRN_MAX_PATH_LEN];
   char to[SLRL_DISPLAY_BUFFER_SIZE];
   char *charset=NULL;
   int edit, is_wrapped, full = 0;
   unsigned int n;

   if (Slrn_Prefix_Arg_Ptr != NULL)
     {
	full = *Slrn_Prefix_Arg_Ptr;
	Slrn_Prefix_Arg_Ptr = NULL;
     }

   if ((-1 == slrn_check_batch ()) ||
       (-1 == select_affected_article ()))
     return;

   *to = 0;
   if (slrn_read_input (_("Forward to (^G aborts): "), NULL, to, 1, 0) <= 0)
     {
	slrn_error (_("Aborted.  An email address is required."));
	return;
     }

   if (-1 == (edit = slrn_get_yesno_cancel (1,"%s",_("Edit the message before sending"))))
     return;

   if (!edit)
     {
	charset=Slrn_Editor_Charset;
	Slrn_Editor_Charset=NULL;
     }

   if (-1 == run_article_hook (HOOK_FORWARD))
     return;

   if (Slrn_Use_Tmpdir)
     {
	fp = slrn_open_tmpfile (file, sizeof (file));
     }
   else fp = slrn_open_home_file (SLRN_LETTER_FILENAME, "w", file,
				  sizeof (file), 0);

   if (fp == NULL)
     {
	slrn_error (_("Unable to open %s for writing."), file);
	return;
     }

   subject = slrn_extract_header ("Subject: ", 9);

   if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "To: %s\n", to) <0)
	return;
   n = 4;

   if (Slrn_Generate_Email_From)
     {
	char *from = slrn_make_from_header ();
	if (from == NULL) return;
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s\n", from) < 0)
	  {
	     slrn_free(from);
	     return;
	  }
	slrn_free(from);
	n++;
     }

   if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset,"Subject: Fwd: %s\n", subject == NULL ? "" : subject) < 0)
	return;

   if (0 != *Slrn_User_Info.replyto)
     {
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "Reply-To: %s\n", Slrn_User_Info.replyto)  < 0)
	     return;
	n++;
     }
   putc ('\n', fp);

   is_wrapped = Slrn_Current_Article->is_wrapped;
   (void) _slrn_art_unwrap_article (Slrn_Current_Article);

   l = Slrn_Current_Article->lines;
   while (l != NULL)
     {
	if (full || (0 == (l->flags & HEADER_LINE)) ||
	    (0 == (l->flags & HIDDEN_LINE)))
	  if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s\n", l->buf) < 0)
	        return;
	l = l->next;
     }
   slrn_fclose (fp);

   if (is_wrapped)
     (void) _slrn_art_wrap_article (Slrn_Current_Article);

   (void) slrn_mail_file (file, edit, n, to, subject);

   if (charset != NULL)
	Slrn_Editor_Charset=charset;

   if (Slrn_Use_Tmpdir) slrn_delete_file (file);
}

/*}}}*/

static void reply_to_mailing_list (void)
{
   /* Some mailing lists have a Mail-Followup-To header.  But do this if there
    * is no Newsgroups header.
    */
   char *mail_followupto;

   mail_followupto = slrn_extract_header ("Mail-Followup-To: ", 18);
   if ((mail_followupto == NULL) || (*mail_followupto == 0))
     reply (NULL, 1);
   else
     reply (mail_followupto, 0);
}

/* If prefix arg is 1, insert all headers.  If it is 2, insert all headers
 * but do not quote text nor attach signature.  2 is good for re-posting.
 */
static void followup (void) /*{{{*/
{
   char *msgid, *newsgroups, *subject, *xref, *quote_str;
   char *cc_address, *cc_address_t;
   char cc_address_buf[256];
   char *followupto = NULL;
   Slrn_Article_Line_Type *l;
   FILE *fp;
   char file [SLRN_MAX_PATH_LEN];
   unsigned int n;
   int prefix_arg;
   int perform_cc;
   int free_cc_string = 0;
   int strip_sig, wrap;
   char *newsgroups_hdr;

   /* The perform_cc testing is ugly.  Is there an easier way?? */

   if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_POST)
       && (slrn_get_yesno (1, _("Are you sure you want to followup")) == 0))
     return;

   if ((-1 == slrn_check_batch ()) ||
       (-1 == select_affected_article ()))
     return;

   if ((NULL == (newsgroups_hdr = slrn_extract_header ("Newsgroups: ", 12)))
       || (*newsgroups_hdr == 0))
     {
	/* Must be a mailing list */
	reply_to_mailing_list ();
	return;
     }

   if (Slrn_Prefix_Arg_Ptr != NULL)
     {
	prefix_arg = *Slrn_Prefix_Arg_Ptr;
	Slrn_Prefix_Arg_Ptr = NULL;
     }
   else prefix_arg = -1;

   if (Slrn_Post_Obj->po_can_post == 0)
     {
	slrn_error (_("Posting not allowed by server"));
	return;
     }

   strip_sig = ((prefix_arg == -1) && Slrn_Followup_Strip_Sig);

   if (-1 == run_article_hook (HOOK_FOLLOWUP))
     return;

   /* Here is the logic:
    * If followup-to contains an email address, use that as a CC.
    * If followup-to contains 'poster', use poster's email address.
    * Otherwise, check for mail-copies-to header.  If its value is 'never'
    *  do not add cc header.  If it is 'always', add it.  If neither of these,
    *  assume it is an email address and use that.
    * Otherwise, use email addresss.
    */
   if (Slrn_Auto_CC_To_Poster) perform_cc = -1;
   else perform_cc = 0;
   cc_address = NULL;
   cc_address_t = NULL;

   if ((NULL != (newsgroups = slrn_extract_header ("Followup-To: ", 13))) &&
       (0 != *newsgroups))
     {
	newsgroups = slrn_skip_whitespace (newsgroups);
	cc_address = newsgroups;
	cc_address_t = parse_from (cc_address, cc_address_buf,
				   sizeof(cc_address_buf));
	if (cc_address != NULL)
	  {
	     int is_poster;
	     is_poster = (0 == slrn_case_strcmp ( cc_address,
						  "poster"));
	     if (is_poster
		 || (NULL != slrn_strbyte (cc_address, '@')))
	       /* The GNU newsgroups appear to have email addresses in the
		* Followup-To header.  Yuk.
		*/
	       {
		  if (is_poster) cc_address = extract_reply_address ();

		  if ((Slrn_Warn_Followup_To == 0) ||
		      slrn_get_yesno (1, _("Do you want to reply to POSTER as poster prefers")))
		    {
		       reply (cc_address, 0);
		       return;
		    }
		  newsgroups = NULL;
	       }
	  } /* if (cc_address != NULL) */
	/* There's a Followup-To to a normal Newsgroup */
	if (Slrn_Warn_Followup_To && (newsgroups != NULL))
	  {
	     int warn = 0;
	     if (Slrn_Warn_Followup_To == 1)
	       /* Warn if "Followup-To:" does not include current newsgroup */
	       {
		  char* lpos;
		  char* rpos = newsgroups;
		  char* epos = rpos + strlen(rpos);
		  warn = 1;
		  while (rpos < epos)
		    {
		       unsigned int len;

		       lpos = rpos;
		       rpos = slrn_strbyte(lpos, ',');
		       if (rpos == NULL)
			 rpos = epos;

		       len = (unsigned int) (rpos - lpos);
		       if ((len == strlen(Slrn_Current_Group_Name))
			   && (0 == strncmp(lpos, Slrn_Current_Group_Name, len)))
			 {
			    warn = 0;
			    break;
			 }
		       rpos++;
		    }
	       }
	     else
	       warn = 1;

	     if (warn &&
		 (slrn_get_yesno (1, _("Followup to %s as poster prefers"), newsgroups) == 0))
	       newsgroups = NULL;
	  } /* if (Slrn_Warn_Followup_To) */
     }

   if ((newsgroups == NULL)
       /* Hmm..  I have also seen an empty Followup-To: header on a GNU
	* newsgroup.
	*/
       || (*newsgroups == 0))
     {
	newsgroups = newsgroups_hdr;
	if (newsgroups == NULL)
	  newsgroups = "";
     }

   if (Slrn_Netiquette_Warnings
       && (NULL != slrn_strbyte(newsgroups, ',')))
     {
	/* Note to translators: Here, "fF" means "Followup-To", "aA" is for
	 * "all groups", "tT" is "this group only" and "cC" means "cancel".
	 * Do not change the length of the string! You cannot use any of the
	 * default characters for other fields than they originally stood for.
	 */
        char *responses=_("fFaAtTcC");
	char rsp;
	if (strlen (responses) != 8)
	  responses = "";
	rsp = slrn_get_response ("fFaAtTcC", responses,
				 _("Crosspost using: (\001F)ollowup-To, (\001A)ll groups, (\001T)his group, (\001C)ancel ?"));
	rsp = slrn_map_translated_char ("fFaAtTcC", responses, rsp) | 0x20;
	switch (rsp)
	  {
	   case 'a':
	     break;

	   case 'f':
	     followupto = Slrn_Current_Group_Name;
	     break;

	   case 't':
	     newsgroups = Slrn_Current_Group_Name;
	     break;

	   case 'c':
	     return;
	  }
     }

   if (perform_cc)
     {
	if (((NULL != (cc_address = slrn_extract_header ("X-Mail-Copies-To: ", 18))) &&
	     (0 != *cc_address)) ||
	    ((NULL != (cc_address = slrn_extract_header ("Mail-Copies-To: ", 16))) &&
	     (0 != *cc_address)))
	  {
	     /* Original poster has requested a certain cc-ing behaviour
	      * which should override whatever default the user has set */
	     perform_cc = 1;
	     if ((0 == slrn_case_strcmp ( cc_address,
					  "always"))
		 || (0 == slrn_case_strcmp ( cc_address,
					     "poster")))
	       {
		  cc_address = NULL;
	       }
	     else if ((0 == slrn_case_strcmp ( cc_address,
					       "never"))
		      || (0 == slrn_case_strcmp ( cc_address,
						  "nobody")))
	       {
		  perform_cc = 0;
		  cc_address = NULL;
	       }
	     else if (NULL == (cc_address_t = parse_from (cc_address,
							  cc_address_buf,
							  sizeof(cc_address_buf))))
	       cc_address = NULL; /* do CC, but use "From" / "Reply-To:" address */

	  }

	if (prefix_arg == 2)
	  perform_cc = 0;
     }

   if (cc_address == NULL)
     {
	cc_address = extract_reply_address ();
	cc_address_t = parse_from (cc_address, cc_address_buf,
				   sizeof(cc_address_buf));
     }

   if ((perform_cc != 0)
       && (cc_address_t != NULL))
     {
	int cc_hook_status;

	if (-1 == (cc_hook_status = slrn_run_hooks (HOOK_CC, 1, cc_address)))
	  return;
	if (-1 == check_for_current_article ())
	  return;
	if (cc_hook_status == 1)
	  {
	     if (-1 == SLang_pop_slstring (&cc_address))
	       return;
	     cc_address_t = parse_from (cc_address, cc_address_buf,
					sizeof(cc_address_buf));
	     free_cc_string = 1;
	     if (*cc_address == 0)
	       perform_cc = 0;
	  }
	if ((perform_cc == 1) && (Slrn_Auto_CC_To_Poster & 0x01))
	  {
	     if (-1 == (perform_cc = slrn_get_yesno_cancel (1,"%s",_("Cc message as requested by poster"))))
	       goto free_and_return;
	  }
	else if (perform_cc == -1)
	  {
	     if (Slrn_Auto_CC_To_Poster < 3) perform_cc = 0;
	     else if (-1 == (perform_cc = slrn_get_yesno_cancel (1,"%s",_("Cc message to poster"))))
	       goto free_and_return;
	  }

	if (perform_cc)
	  {
	     if ((NULL == cc_address_t)
		 || ((strlen(cc_address_buf) > 8) &&
		     !(slrn_case_strcmp(".invalid",
					cc_address_buf+
					strlen(cc_address_buf)-8))))
	       {
		  perform_cc = slrn_get_yesno_cancel (1,_("%s appears invalid.  CC anyway"), *cc_address_buf ? cc_address_buf : _("Email address"));
		  if (perform_cc < 0)
		     goto free_and_return;
	       }
	  }
     }

   msgid = slrn_extract_header ("Message-ID: ", 12);

   if (NULL != (subject = slrn_extract_header ("Subject: ", 9)))
     subject = subject_skip_re (subject);
   else subject = "";

   if (Slrn_Use_Tmpdir)
     fp = slrn_open_tmpfile (file, sizeof (file));
   else fp = slrn_open_home_file (SLRN_FOLLOWUP_FILENAME, "w", file,
				  sizeof (file), 0);

   if (fp == NULL)
     {
	slrn_error (_("Unable to open %s for writing."), file);
	goto free_and_return;
     }

   fprintf (fp, "Newsgroups: %s\n", newsgroups);  n = 3;

#if ! SLRN_HAS_STRICT_FROM
     {
	char *from = slrn_make_from_header ();
	if (from == NULL) return;
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s\n", from) < 0)
	  {
	     slrn_free(from);
	     goto free_and_return;
	  }
	slrn_free(from);
	n++;
     }
#endif

   if (NULL == (subject = slrn_safe_strmalloc (subject)))
     goto free_and_return;

   slrn_subject_strip_was (subject);
   if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "Subject: Re: %s\n", subject) < 0)
     {
	slrn_free (subject);
	goto free_and_return;
     }
   slrn_free (subject);

   xref = slrn_extract_header("References: ", 12);
   if ((msgid != NULL) && (*msgid != 0))
     {
	if ((xref == NULL) || (*xref == 0))
	  fprintf (fp, "References: %s\n", msgid);
	else
	  fprintf (fp, "References: %s %s\n", xref, msgid);
	n++;
     }

   if (Slrn_User_Info.org != NULL)
     {
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "Organization: %s\n", Slrn_User_Info.org) < 0)
	  goto free_and_return;
	n++;
     }

   if (perform_cc
       && (cc_address_t != NULL))
     {
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset,  "Cc: %s\n", cc_address) < 0)
	     goto free_and_return;
	n++;
     }

   if (0 != *Slrn_User_Info.replyto)
     {
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "Reply-To: %s\n", Slrn_User_Info.replyto) < 0)
	     goto free_and_return;
	n++;
     }

   fputs("Followup-To: ", fp);
   if (followupto != NULL)
     fputs(followupto, fp);
   fputs("\n", fp);
   n++;

   n += slrn_add_custom_headers (fp, Slrn_Followup_Custom_Headers, insert_followup_format);

   fputs ("\n", fp);

   if (prefix_arg != 2)
     {
	if ((followupto != NULL) && (*Slrn_User_Info.followupto_string != 0))
	  {
	     insert_followup_format (Slrn_User_Info.followupto_string, fp);
	     fputs ("\n", fp);
	  }
	insert_followup_format (Slrn_User_Info.followup_string, fp);
	fputs ("\n", fp);
     }
   n += 1;			       /* by having + 1, the cursor will be
					* placed on the first line of message.
					*/

   wrap = Slrn_Current_Article->is_wrapped;
   (void) _slrn_art_unwrap_article (Slrn_Current_Article);

   /* skip header */
   l = Slrn_Current_Article->lines;
   if (prefix_arg == -1)
     {
	while ((l != NULL) && (*l->buf != 0)) l = l->next;
	if (l != NULL) l = l->next;
     }

   if (prefix_arg == 2) quote_str = "";
   else if (NULL == (quote_str = Slrn_Quote_String))
     quote_str = ">";

   while (l != NULL)
     {
	int smart_space = (Slrn_Smart_Quote & 0x01) && ! (l->flags & QUOTE_LINE)
	  && (prefix_arg != 2) && (*l->buf != 0);

	if (strip_sig
	    && (l->flags & SIGNATURE_LINE))
	  break;

	if (strip_sig && (l->flags & PGP_SIGNATURE_LINE))
	  {
	     l = l->next;
	     continue;
	  }

	if ((*l->buf == 0) && (Slrn_Smart_Quote & 0x02))
	  fputc ('\n', fp);
	else
	  {
	     if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s%s%s\n", quote_str, (smart_space)? " " : "" , l->buf) < 0)
		  goto free_and_return;
	  }

	l = l->next;
     }

   if (wrap)
     (void) _slrn_art_wrap_article (Slrn_Current_Article);

   if (prefix_arg != 2) slrn_add_signature (fp);
   slrn_fclose (fp);

   if (slrn_edit_file (Slrn_Editor_Post, file, n, 1) >= 0)
     {
	(void) slrn_post_file (file, cc_address, 0);
     }

   if (Slrn_Use_Tmpdir) (void) slrn_delete_file (file);

   free_and_return:
   if (free_cc_string && (cc_address != NULL))
     SLang_free_slstring (cc_address);
}

/*}}}*/

#if SLRN_HAS_CANLOCK
/* Generate a key needed for canceling and superseding messages when
 * cancel-locking is used. Returns a malloced string or NULL on failure. */
static char* gen_cancel_key (char* msgid) /*{{{*/
{
   FILE *cansecret;
   char *buf, *cankey;
   unsigned int filelen;
   char canfile[SLRN_MAX_PATH_LEN];

   if (0 == *Slrn_User_Info.cancelsecret)
     return NULL;

   if ((cansecret = slrn_open_home_file (Slrn_User_Info.cancelsecret, "r",
					 canfile, SLRN_MAX_PATH_LEN, 0)) == NULL)
     {
	slrn_error (_("Cannot open file: %s"), Slrn_User_Info.cancelsecret);
	return NULL;
     }

   (void) fseek (cansecret, 0, SEEK_END);
   if ((filelen = ftell(cansecret)) == 0)
     {
        slrn_error (_("Zero length file: %s"), Slrn_User_Info.cancelsecret);
	fclose (cansecret);
        return NULL;
     }

   if (NULL == (buf = slrn_malloc (filelen+1, 0, 1)))
     {
	fclose (cansecret);
	return NULL;
     }
   (void) fseek (cansecret, 0, SEEK_SET);
   (void) fread (buf, filelen, 1, cansecret);

# if 0
   cankey = md5_key (buf, filelen, msgid, strlen(msgid));
# else /* by default we use SHA-1 */
   cankey = sha_key ((unsigned char *) buf, filelen, (unsigned char *)msgid, strlen(msgid));
# endif

   fclose (cansecret);
   slrn_free (buf);
   return cankey;
}
/*}}}*/
#endif /* CANCEL_LOCKS */

/* Copy a message, adding a "Supersedes: " header for the message it replaces.
 * Not all headers of original are preserved; notably Cc is discarded.
 */
static void supersede (void) /*{{{*/
{
   char *followupto, *msgid, *newsgroups, *subject, *xref;
   Slrn_Article_Line_Type *l;
   FILE *fp;
   char file[SLRN_MAX_PATH_LEN], from_buf[512];
   unsigned int n;
   int wrap;
   char *from, *me;
   char me_buf[512];

   if ((-1 == slrn_check_batch ()) ||
       (-1 == select_affected_article ()))
     return;

   if (Slrn_Post_Obj->po_can_post == 0)
     {
	slrn_error (_("Posting not allowed by server"));
	return;
     }

   from = slrn_extract_header ("From: ", 6);
   if (from != NULL)
      (void) parse_from (from, from_buf, sizeof(from_buf));
   else
      from_buf[0] = '\0';
   if (NULL == (me = slrn_make_from_header())) return;
   (void) parse_from (me+6, me_buf, sizeof(me_buf));

   if (slrn_case_strcmp ( from_buf,  me_buf))
     {
        slrn_error (_("Failed: Your name: '%s' is not '%s'"), me_buf, from_buf);
        return;
     }

   if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_POST)
       && (slrn_get_yesno (1, _("Are you sure you want to supersede")) == 0))
     return;

   if (-1 == run_article_hook (HOOK_SUPERSEDE))
     return;

   if (NULL == (newsgroups = slrn_extract_header ("Newsgroups: ", 12)))
     newsgroups = "";
   if (NULL == (followupto = slrn_extract_header ("Followup-To: ", 12)))
     followupto = "";
   if (NULL == (subject = slrn_extract_header ("Subject: ", 9)))
     subject = "";
   if (NULL == (msgid = slrn_extract_header ("Message-ID: ", 12)))
     msgid = "";
   xref = slrn_extract_header("References: ", 12);

   if (Slrn_Use_Tmpdir)
     fp = slrn_open_tmpfile (file, sizeof (file));
   else fp = slrn_open_home_file (SLRN_FOLLOWUP_FILENAME, "w", file,
				  sizeof (file), 0);

   if (fp == NULL)
     {
	slrn_error (_("Unable to open %s for writing."), file);
	slrn_free(me);
	return;
     }

   fprintf (fp, "Newsgroups: %s\n", newsgroups); n = 5;
#if ! SLRN_HAS_STRICT_FROM
   if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s\n", me) < 0)
     {
	slrn_free(me);
	return;
     }
   n++;
#endif
   slrn_free(me);

   if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "Subject: %s\n", subject) < 0)
	return;

   fprintf (fp, "Supersedes: %s\nFollowup-To: %s\n",
	    msgid, followupto);
#if SLRN_HAS_CANLOCK
   /* Abuse 'from', we don't need its content anymore */
   if (NULL != (from = gen_cancel_key(msgid)))
     {
	fprintf (fp, "Cancel-Key: %s\n", from);
	slrn_free (from);
     }
#endif

   if ((xref != NULL) && (*xref != 0))
     {
	fprintf (fp, "References: %s\n", xref);
	n++;
     }

   if (Slrn_User_Info.org != NULL)
     {
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "Organization: %s\n", Slrn_User_Info.org) < 0)
	     return;
	n++;
     }

   if (0 != *Slrn_User_Info.replyto)
     {
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset,  "Reply-To: %s\n", Slrn_User_Info.replyto) < 0)
	     return;
	n++;
     }

   n += slrn_add_custom_headers (fp, Slrn_Supersedes_Custom_Headers, insert_followup_format);

   fputs ("\n", fp);
   n += 1;

   wrap = Slrn_Current_Article->is_wrapped;
   (void) _slrn_art_unwrap_article (Slrn_Current_Article);

   /* skip header */
   l = Slrn_Current_Article->lines;
   while ((l != NULL) && (*l->buf != 0)) l = l->next;
   if (l != NULL) l = l->next;

   while (l != NULL)
     {
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s\n", l->buf) < 0)
	     return;

	l = l->next;
     }

   if (wrap)
     (void) _slrn_art_wrap_article (Slrn_Current_Article);

   slrn_fclose (fp);

   if (slrn_edit_file (Slrn_Editor_Post, file, n, 1) >= 0)
     {
	(void) slrn_post_file (file, NULL, 0);
     }
   if (Slrn_Use_Tmpdir) (void) slrn_delete_file (file);
}

/*}}}*/

/*}}}*/

/*{{{ header movement functions */

int slrn_header_cursor_pos (void)
{
   return Last_Cursor_Row;
}

unsigned int slrn_header_down_n (unsigned int n, int err) /*{{{*/
{
   unsigned int m;

   m = SLscroll_next_n (&Slrn_Header_Window, n);
   Slrn_Current_Header = (Slrn_Header_Type *) Slrn_Header_Window.current_line;

   if (err && (m != n))
     slrn_error (_("End of buffer."));

   return m;
}
/*}}}*/

static void header_down (void) /*{{{*/
{
   slrn_header_down_n (1, 1);
}

/*}}}*/

unsigned int slrn_header_up_n (unsigned int n, int err) /*{{{*/
{
   unsigned int m;

   m = SLscroll_prev_n (&Slrn_Header_Window, n);
   Slrn_Current_Header = (Slrn_Header_Type *) Slrn_Header_Window.current_line;

   if (err && (m != n))
     slrn_error (_("Top of buffer."));

   return m;
}

/*}}}*/

static void header_up (void) /*{{{*/
{
   slrn_header_up_n (1, 1);
}

/*}}}*/

static void header_pageup (void) /*{{{*/
{
   Slrn_Full_Screen_Update = 1;
   if (-1 == SLscroll_pageup (&Slrn_Header_Window))
     slrn_error (_("Top of buffer."));
   Slrn_Current_Header = (Slrn_Header_Type *) Slrn_Header_Window.current_line;
}

/*}}}*/

static void header_pagedn (void) /*{{{*/
{
   Slrn_Full_Screen_Update = 1;
   if (-1 == SLscroll_pagedown (&Slrn_Header_Window))
     slrn_error (_("End of buffer."));
   Slrn_Current_Header = (Slrn_Header_Type *) Slrn_Header_Window.current_line;
}

/*}}}*/

static void header_bob (void) /*{{{*/
{
   Slrn_Current_Header = _art_Headers;
   _art_find_header_line_num ();
}

/*}}}*/

static void header_eob (void) /*{{{*/
{
   while (0xFFFF == slrn_header_down_n (0xFFFF, 0));
}

/*}}}*/

static int prev_unread (void) /*{{{*/
{
   Slrn_Header_Type *h;

   h = Slrn_Current_Header -> prev;

   while (h != NULL)
     {
	if (0 == (h->flags & (HEADER_READ|HEADER_WITHOUT_BODY))) break;
	h = h->prev;
     }

   if (h == NULL)
     {
	slrn_message (_("No previous unread articles."));
	return 0;
     }

   Slrn_Current_Header = h;

   if (h->flags & HEADER_HIDDEN)
     slrn_uncollapse_this_thread (h, 0);

   _art_find_header_line_num ();
   return 1;
}

/*}}}*/

static void goto_last_read (void) /*{{{*/
{
   if (Last_Read_Header == NULL) return;
   slrn_goto_header (Last_Read_Header, 1);
}

/*}}}*/

static void art_prev_unread (void) /*{{{*/
{
   if (prev_unread () && Article_Visible) art_pagedn  ();
}

/*}}}*/

int slrn_next_unread_header (int skip_without_body) /*{{{*/
{
   Slrn_Header_Type *h;

   h = Slrn_Current_Header->next;

   while (h != NULL)
     {
	if ((0 == (h->flags & HEADER_READ)) &&
	    (!skip_without_body || (0 == (h->flags & HEADER_WITHOUT_BODY))))
	  break;
	h = h->next;
     }

   if (h == NULL)
     {
	slrn_message (_("No following unread articles."));
	return 0;
     }

   Slrn_Current_Header = h;
   if (h->flags & HEADER_HIDDEN)
     slrn_uncollapse_this_thread (h, 0);

   _art_find_header_line_num ();

   return 1;
}

/*}}}*/

static void art_next_unread (void) /*{{{*/
{
   char ch;
   char *keyseq;

   if (slrn_next_unread_header (1))
     {
	if (Article_Visible) art_pagedn  ();
	return;
     }

   if (Slrn_Query_Next_Group == 0)
     {
	skip_to_next_group ();
	return;
     }

   if (Slrn_Batch) return;

   keyseq = slrn_help_keyseq_from_function ("next", Slrn_Article_Keymap);
   if (keyseq != NULL)
     {
	SLang_Key_Type *key;
	char *keystr = slrn_help_keyseq_to_string(keyseq+1,*keyseq-1);
	if (keystr == NULL)
	  keystr = SLang_make_keystring((unsigned char*)keyseq);
	slrn_message_now (_("No following unread articles.  Press %s for next group."),
			  keystr);
	key = SLang_do_key (Slrn_Article_Keymap, slrn_getkey);
	if (key == NULL) return;
	if ((key->type != SLKEY_F_INTRINSIC) ||
	    (key->f.f != (FVOID_STAR) art_next_unread))
	  {
	     unsigned int len = key->str[0];
	     if (len != 0)
	       SLang_ungetkey_string (key->str+1, len-1);
	     return;
	  }
     }
   else /* next is unbound */
     {
	slrn_message_now (_("No following unread articles.  Press %s for next group."), "n");
#if 0
	ch = SLang_getkey ();
#else
	ch = slrn_getkey ();
#endif
	if (ch != 'n')
	  {
	     SLang_ungetkey (ch);
	     return;
	  }
     }

   skip_to_next_group ();
}

/*}}}*/

static void next_high_score (void) /*{{{*/
{
   Slrn_Header_Type *l;

   l = Slrn_Current_Header->next;

   while (l != NULL)
     {
	if (l->flags & HEADER_HIGH_SCORE)
	  {
	     break;
	  }
	l = l->next;
     }

   if (l == NULL)
     {
	slrn_error (_("No more high scoring articles."));
	return;
     }

   if (l->flags & HEADER_HIDDEN) slrn_uncollapse_this_thread (l, 0);

   Slrn_Current_Header = l;
   _art_find_header_line_num ();

   if (Article_Visible)
     {
	if (Header_Showing != Slrn_Current_Header)
	  art_pagedn ();
     }
}

/*}}}*/

static Slrn_Header_Type *Same_Subject_Start_Header;
static void next_header_same_subject (void) /*{{{*/
{
#if SLANG_VERSION < 20000
   SLsearch_Type st;
#else
   SLsearch_Type *st;
   unsigned int flags = SLSEARCH_CASELESS;
#endif
   Slrn_Header_Type *l;
   static char same_subject[SLRL_DISPLAY_BUFFER_SIZE];

   if ((Same_Subject_Start_Header == NULL)
       || (Slrn_Prefix_Arg_Ptr != NULL))
     {
	Slrn_Prefix_Arg_Ptr = NULL;
	if (slrn_read_input (_("Subject: "), same_subject, NULL, 0, 0) <= 0) return;
	Same_Subject_Start_Header = Slrn_Current_Header;
     }
#if SLANG_VERSION < 20000
   SLsearch_init (same_subject, 1, 0, &st);
#else
   if (Slrn_UTF8_Mode) flags |= SLSEARCH_UTF8;
   if (NULL == (st = SLsearch_new ((SLuchar_Type *)same_subject, flags)))
     return;
#endif
   l = Slrn_Current_Header->next;

   while (l != NULL)
     {
	if (
#if 0
	    /* Do we want to do this?? */
	    ((l->flags & HEADER_READ) == 0) &&
#endif
	    (l->subject != NULL)
#if SLANG_VERSION < 20000
	    && (NULL != SLsearch ((unsigned char *) l->subject,
				  (unsigned char *) l->subject + strlen (l->subject),
				  &st))
#else
	    && (NULL != SLsearch_forward (st, (SLuchar_Type *)l->subject,
					  (SLuchar_Type *)l->subject + strlen (l->subject)))
#endif
	    )
	  break;

	l = l->next;
     }

   if (l == NULL)
     {
	slrn_error (_("No more articles on that subject."));
	l = Same_Subject_Start_Header;
	Same_Subject_Start_Header = NULL;
     }

   if (l->flags & HEADER_HIDDEN) slrn_uncollapse_this_thread (l, 0);
   Slrn_Current_Header = l;
   _art_find_header_line_num ();
   if ((Same_Subject_Start_Header != NULL)
       && (Article_Visible))
     {
	art_pagedn ();
     }
#if SLANG_VERSION >= 20000
   SLsearch_delete (st);
#endif
}

/*}}}*/

static void goto_header_number (void) /*{{{*/
{
   int diff, i, ich;

   if (Slrn_Batch) return;

   i = 0;
   ich = SLang_Last_Key_Char;
   do
     {
	i = i * 10 + (ich - '0');
	if (10 * i > Largest_Header_Number)
	  {
	     ich = '\r';
	     break;
	  }
	slrn_message_now (_("Goto Header: %d"), i);
     }
#if 0
   while ((ich = SLang_getkey ()), (ich <= '9') && (ich >= '0'));
#else
   while ((ich = slrn_getkey ()), (ich <= '9') && (ich >= '0'));
#endif

   if (SLKeyBoard_Quit) return;

   if (ich != '\r')
     SLang_ungetkey (ich);

   diff = i - Last_Cursor_Row;
   if (diff > 0) slrn_header_down_n (diff, 0); else slrn_header_up_n (-diff, 0);
   slrn_run_hooks (HOOK_HEADER_NUMBER, 0);
   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

/*}}}*/

/*{{{ article save/decode functions */

static int write_article_line (Slrn_Article_Line_Type *l, FILE *fp, int convert)
{
   while (l != NULL)
     {
	char *buf;
	Slrn_Article_Line_Type *next = l->next;

	buf = l->buf;
	if (l->flags & WRAPPED_LINE) buf++;   /* skip space */

	if (convert)
	  {
	     if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s", buf)< 0)
	       return -1;
	  }
	else
	  {
	     if (EOF == fputs (buf, fp))
		  return -1;
	  }

	if ((next == NULL) || (0 == (next->flags & WRAPPED_LINE)))
	  {
	     if (EOF == putc ('\n', fp))
	       return -1;
	  }
	l = next;
     }

   return 0;
}

/* returns the header that should be affected by interactive commands. */
static Slrn_Header_Type *affected_header (void) /*{{{*/
{
   if ((Header_Showing != NULL) && Article_Visible)
     return Header_Showing;
   else
     return Slrn_Current_Header;
}

/*}}}*/

int slrn_save_current_article (char *file) /*{{{*/
{
   FILE *fp = NULL;
   Slrn_Header_Type *h;
   Slrn_Article_Line_Type *lines;
   int retval = 0;

   /* We're saving raw_lines, this saves the re-encoding later */
   if (NULL == (h = affected_header ()) ||
       select_header (h, Slrn_Del_Article_Upon_Read) < 0)
     return -1;

   lines = Slrn_Current_Article->raw_lines;

   fp = fopen (file, "w");

   if (fp == NULL)
     {
	slrn_error (_("Unable to open %s."), file);
	retval = -1;
     }
   else
     {
	if (-1 == (retval = write_article_line (lines, fp, 0)))
	  slrn_error (_("Error writing to %s."), file);
	fclose (fp);
     }

   return retval;
}

/*}}}*/

static int save_article_as_unix_mail (Slrn_Header_Type *h, FILE *fp) /*{{{*/
{
   int is_wrapped = 0;
   Slrn_Article_Line_Type *l = NULL;
   Slrn_Article_Type *a = Slrn_Current_Article;
   char *from;
   char from_buf[256];
   time_t now;

   if ((Header_Showing != h) || (a == NULL))
     {
	if (NULL == (a = read_article (h, Slrn_Del_Article_Upon_Read)))
	  return -1;
     }

   l = a->raw_lines;
   from = h->from;
   if (from != NULL) from = parse_from (from, from_buf, sizeof(from_buf));
   else from_buf[0] = '\0';
   if ((from == NULL) || (*from_buf == 0))
      strcpy (from_buf, "nobody@nowhere"); /* safe */

   time (&now);
   fprintf (fp, "From %s %s", from_buf, ctime(&now));

   while (l != NULL)
     {
	if ((*l->buf == 'F')
	    && !strncmp ("From", l->buf, 4)
	    && ((unsigned char)(l->buf[4]) <= ' '))
	  {
	     putc ('>', fp);
	  }

	fputs (l->buf, fp);
	putc ('\n', fp);
	l = l->next;
     }

   fputs ("\n", fp); /* one empty line as a separator */

   if (a != Slrn_Current_Article) slrn_art_free_article (a);
   else if (is_wrapped) _slrn_art_wrap_article (Slrn_Current_Article);

   return 0;
}

/*}}}*/

static char *save_article_to_file (char *defdir, int for_decoding) /*{{{*/
{
   char file[SLRL_DISPLAY_BUFFER_SIZE];
   char name[SLRN_MAX_PATH_LEN];
   char *input_string;
   int save_tagged = 0;
   int save_thread = 0;
   int save_simple;
   FILE *fp;

   if (-1 == slrn_check_batch ())
     return NULL;

   if (Num_Tag_List.len)
     {
	save_tagged = slrn_get_yesno_cancel (1,"%s",_("Save tagged articles"));
	if (save_tagged < 0) return NULL;
     }

   if ((save_tagged == 0)
       && (Slrn_Current_Header->child != NULL)
       && (Slrn_Current_Header->child->flags & HEADER_HIDDEN))
     {
	save_thread = slrn_get_yesno_cancel (1,"%s",_("Save this thread"));
	if (save_thread == -1) return NULL;
     }

   save_simple = !(save_tagged || save_thread);

   if (*Output_Filename == 0)
     {
#ifdef VMS
	char *p;
#endif
	char *filename = Slrn_Current_Group_Name;
	unsigned int defdir_len;
	if (defdir == NULL) defdir = "News";

	slrn_make_home_dirname (defdir, file, sizeof (file));
	defdir_len = strlen (file);

	switch (slrn_file_exists (file))
	  {
	   case 0:
	     if (slrn_get_yesno (1, _("Do you want to create directory %s"), file) &&
		 (-1 == slrn_mkdir (file)))
	       {
		  slrn_error_now (2, _("Unable to create directory. (errno = %d)"), errno);
		  slrn_clear_message ();
	       }
	     break;
	   case 1:
	     slrn_error_now (2, _("Warning: %s is a regular file."), file);
	     slrn_clear_message ();
	  }

#ifdef VMS
	slrn_snprintf (name, sizeof (name) - 5, "%s/%s", file,
		       Slrn_Current_Group_Name);
	p = name + defdir_len + 1;
	while (*p != 0)
	  {
	     if (*p == '.') *p = '_';
	     p++;
	  }
	strcpy (p, ".txt"); /* safe */
#else
	if ((slrn_run_hooks (HOOK_MAKE_SAVE_FILENAME, 0) <= 0)
	    || (0 != SLang_pop_slstring (&filename))
	    || (*filename == 0))
	  filename = Slrn_Current_Group_Name;
	if ((filename == Slrn_Current_Group_Name) ||
	    (0 == slrn_is_absolute_path(filename)))
	  slrn_snprintf (name, sizeof (name), "%s/%s", file,
			 filename);
	else
	  slrn_strncpy (name, filename, sizeof (name));
#endif

#if !defined(VMS) && !defined(IBMPC_SYSTEM)
	if (filename == Slrn_Current_Group_Name)
	  {
	     /* Uppercase first letter and see if it exists. */
	     name[defdir_len + 1] = UPPER_CASE(name[defdir_len + 1]);
	  }
#endif
	slrn_make_home_filename (name, file, sizeof (file));

	if (filename == Slrn_Current_Group_Name)
	  {
#if !defined(VMS) && !defined(IBMPC_SYSTEM)
	     if (1 != slrn_file_exists (file))
	       {
		  /* Uppercase version does not exist so use lowercase form. */
		  name[defdir_len + 1] = LOWER_CASE(name[defdir_len + 1]);
		  slrn_make_home_filename (name, file, sizeof (file));
	       }
#endif
	  }
	else
	  SLang_free_slstring (filename);
     }
   else slrn_strncpy (file, Output_Filename, sizeof (file));

   if (for_decoding) input_string = _("Temporary file (^G aborts): ");
   else input_string = _("Save to file (^G aborts): ");
   if (slrn_read_filename (input_string, NULL, file, 1, 1) <= 0)
     {
	slrn_error (_("Aborted."));
	return NULL;
     }

   if (NULL == (fp = fopen (file, "a")))
     {
	slrn_error (_("Unable to open %s"), file);
	return NULL;
     }

   slrn_strncpy (Output_Filename, file, sizeof (Output_Filename));

   if (save_simple)
     {
	Slrn_Header_Type *h;

	if (NULL != (h = affected_header ()))
	  save_article_as_unix_mail (h, fp);
     }
   else if (save_tagged)
     {
	unsigned int i;
	unsigned int num_saved = 0;

	for (i = 0; i < Num_Tag_List.len; i++)
	  {
	     if (-1 == save_article_as_unix_mail (Num_Tag_List.headers[i], fp))
	       {
		  slrn_smg_refresh ();
		  if (SLang_get_error() == SL_USER_BREAK)
		    break;

		  SLang_set_error (0);
		  (void) SLang_input_pending (5);   /* half second delay */
		  slrn_clear_message ();
	       }
	     else num_saved++;
	  }
	if (num_saved == 0)
	  {
	     slrn_fclose (fp);
	     return NULL;
	  }
     }
   else
     {
	Slrn_Header_Type *h = Slrn_Current_Header;
	unsigned int num_saved = 0;
	do
	  {
	     if (-1 == save_article_as_unix_mail (h, fp))
	       {
		  slrn_smg_refresh ();
		  SLang_set_error (0);
		  (void) SLang_input_pending (5);   /* half second delay */
	       }
	     else num_saved++;

	     h = h->next;
	  }
	while ((h != NULL) && (h->parent != NULL));
	if (num_saved == 0)
	  {
	     slrn_fclose (fp);
	     return NULL;
	  }
     }
   slrn_fclose (fp);

   if (SLang_get_error ()) return NULL;

   return Output_Filename;
}

/*}}}*/

static void save_article (void) /*{{{*/
{
   (void) save_article_to_file (Slrn_Save_Directory, 0);
}

/*}}}*/

#if SLRN_HAS_DECODE
#if SLRN_HAS_UUDEVIEW
static int the_uudeview_busy_callback (void *param, uuprogress *progress)/*{{{*/
{
   char stuff[26];
   unsigned int count, count_max;
   int pcts;
   char *ptr;

   if (progress->action != UUACT_DECODING)
     return 0;

   pcts = (int)((100 * progress->partno + progress->percent - 100) / progress->numparts);

   count_max = sizeof (stuff) - 1;

   for (count = 0; count < count_max; count++)
     stuff[count] = (count < pcts/4) ? '#' : '.';

   stuff [count_max] = 0;

   slrn_message_now (_("decoding %10s (%3d/%3d) %s"),
		     progress->curfile,
		     progress->partno, progress->numparts,
		     stuff);
   return 0;
}

/*}}}*/

static int do_slrn_uudeview (char *uu_dir, char *file)/*{{{*/
{
   uulist *item;
   char where [SLRN_MAX_PATH_LEN];
   int i, ret;

   slrn_make_home_dirname (uu_dir, where, sizeof (where));
   /* this is expecting a '/' at the end...so we put one there */
   if (strlen (where) + 1 >= sizeof (where))
     slrn_error (_("Filename buffer not large enough."));

   strcat (where, "/"); /* safe */

   slrn_message_now (_("Calling uudeview ..."));
   ret = UUInitialize ();
   ret = UUSetBusyCallback (NULL, the_uudeview_busy_callback, 100);
   ret = UUSetOption (UUOPT_DESPERATE, 1, NULL);
   ret = UUSetOption (UUOPT_SAVEPATH, 0, where);
   if (UURET_OK != (ret = UULoadFile (file, NULL, 0)))
     {
	/* Not all systems have strerror... */
	if (ret == UURET_IOERR)
	  slrn_error (_("could not load %s: errno = %d"),
		      file, UUGetOption (UUOPT_ERRNO, NULL, NULL, 0));
	else
	  slrn_error (_("could not load %s: %s"),  file, UUstrerror (ret));
     }

   i = 0;
   while (NULL != (item = UUGetFileListItem (i)))
     {
	i++;

	/* When decoding yEnc messages, uudeview inserts bogus items into the
	 * file list. Processing them should not trigger an error, so we need
	 * to ignore errors where no data was found. */
	ret = UUDecodeFile (item, NULL);
	if ((ret != UURET_OK) && (ret != UURET_NODATA))
	  {
	     char *f;
	     char *err;

	     if (NULL == (f = item->filename)) f = "oops";

	     if (ret == UURET_IOERR) err = _("I/O error.");
	     else err = UUstrerror (ret);

	     slrn_error (_("error decoding %s: %s"), f, err);
	  }
     }

   UUCleanUp ();
}

/*}}}*/
#endif

static void decode_article (void) /*{{{*/
{
   char *uu_dir;
   char *file;

   if (NULL == (uu_dir = Slrn_Decode_Directory))
     {
	if (NULL == (uu_dir = Slrn_Save_Directory))
	  uu_dir = "News";
     }
   else *Output_Filename = 0;	       /* force it to use this directory */

   file = save_article_to_file(uu_dir, 1);

   if (file == NULL) return;

   if (1 == slrn_get_yesno (1, _("Decode %s"), file))
     {
# if SLRN_HAS_UUDEVIEW
	if (Slrn_Use_Uudeview)
	  (void) do_slrn_uudeview (uu_dir, file);
	else
# endif
	(void) slrn_uudecode_file (file, NULL, 0, NULL);

	if (0 == SLang_get_error ())
	  {
	     if (1 == slrn_get_yesno (1, _("Delete %s"), file))
	       {
		  if (-1 == slrn_delete_file (file))
		    slrn_error (_("Unable to delete %s"), file);
	       }
	  }
     }
   /* Since we have a decode directory, do not bother saving this */
   if (NULL != Slrn_Decode_Directory)
     *Output_Filename = 0;
}

/*}}}*/
#endif  /* SLRN_HAS_DECODE */

/*}}}*/
/*{{{ pipe_article functions */

int slrn_pipe_article_to_cmd (char *cmd) /*{{{*/
{
#if SLRN_HAS_PIPING
   FILE *fp;
   int retval;
   int convert=0;
   Slrn_Article_Line_Type *lines;

   if (-1 == (retval = select_affected_article ()))
     return -1;
   else if (retval == 1)
     select_header (Header_Showing, 0);

   switch (Slrn_Pipe_Type)
     {
      default:
      case PIPE_RAW:
	lines = Slrn_Current_Article->raw_lines;
	convert=0;
	break;
      case PIPE_DECODED:
	return -1;

      case PIPE_CONVERTED:
	lines = Slrn_Current_Article->lines;
	convert = 1;
	break;
     }

   if (NULL == (fp = slrn_popen (cmd, "w")))
     {
	slrn_error (_("Unable to open pipe to %s"), cmd);
	retval = -1;
     }
   else
     {
	retval = write_article_line (lines, fp, convert);
	slrn_pclose (fp);
     }

   return retval;
#else
   slrn_error (_("Piping not implemented on this system."));
   return -1;
#endif
}

/*}}}*/

static void pipe_article (void) /*{{{*/
{
#if SLRN_HAS_PIPING
   static char cmd[SLRL_DISPLAY_BUFFER_SIZE];

   if (slrn_read_filename (_("Pipe to command: "), NULL, cmd, 1, 1) <= 0)
     {
	slrn_error (_("Aborted.  Command name is required."));
	return;
     }

   if (-1 == slrn_pipe_article_to_cmd (cmd))
     slrn_message (_("Error piping to %s."), cmd);
#else
   slrn_error (_("Piping not implemented on this system."));
#endif
}

/*}}}*/

static int print_article (int full) /*{{{*/
{
   Slrn_Print_Type *p;
   Slrn_Article_Line_Type *l;
   int was_wrapped = 0;

   if (-1 == select_affected_article ()) return -1;

   slrn_message_now (_("Printing article..."));

   p = slrn_open_printer ();
   if (p == NULL)
     return -1;

   if (full && (was_wrapped = (Slrn_Wrap_Method && Slrn_Current_Article->is_wrapped)))
     _slrn_art_unwrap_article (Slrn_Current_Article);
   l = Slrn_Current_Article->lines;

   while (l != NULL)
     {
	if ((full || (0 == (l->flags & HIDDEN_LINE))) &&
	    ((-1 == slrn_write_to_printer (p, l->buf, strlen (l->buf)))
	     || (-1 == slrn_write_to_printer (p, "\n", 1))))
	  {
	     if (was_wrapped)
	       _slrn_art_wrap_article (Slrn_Current_Article);
	     slrn_close_printer (p);
	     return -1;
	  }

	l = l->next;
     }

   if (was_wrapped)
     _slrn_art_wrap_article (Slrn_Current_Article);

   if (-1 == slrn_close_printer (p))
     return -1;

   slrn_message_now (_("Printing article...done"));
   return 0;
}

/*}}}*/

static void print_article_cmd (void) /*{{{*/
{
   int prefix = 0;
   if (Slrn_Prefix_Arg_Ptr != NULL)
     {
	prefix = *Slrn_Prefix_Arg_Ptr;
	Slrn_Prefix_Arg_Ptr = NULL;
     }

   if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_PRINT)
       && (slrn_get_yesno (1, _("Are you sure you want to print the article")) == 0))
     return;

   (void) print_article (prefix);
}

/*}}}*/

/*}}}*/

/*{{{ Thread related functions */

static void find_non_hidden_header (void) /*{{{*/
{
   Slrn_Header_Type *h = Slrn_Current_Header;

   while ((h != NULL) && (h->flags & HEADER_HIDDEN))
     h = h->prev;

   if (h == NULL)
     {
	h = Slrn_Current_Header;
	while ((h != NULL) && (h->flags & HEADER_HIDDEN))
	  h = h->next;
     }

   Slrn_Current_Header = h;
}

/*}}}*/

/* This function cannot depend upon routines which call SLscroll functions if
 * sync_now is non-zero.
 */
void slrn_collapse_threads (int sync_now) /*{{{*/
{
   Slrn_Header_Type *h = Slrn_First_Header;

   if ((h == NULL)
       || (_art_Threads_Collapsed == 1))
     return;

   while (h != NULL)
     {
	if (h->parent != NULL) h->flags |= HEADER_HIDDEN;
	else
	  {
	     h->flags &= ~HEADER_HIDDEN;
	  }
	h = h->real_next;
     }

   find_non_hidden_header ();

   if (sync_now) _art_find_header_line_num ();

   Slrn_Full_Screen_Update = 1;
   _art_Threads_Collapsed = 1;
}

/*}}}*/

void slrn_uncollapse_threads (int sync_now) /*{{{*/
{
   Slrn_Header_Type *h = Slrn_First_Header;

   if ((h == NULL)
       || (0 == _art_Threads_Collapsed))
     return;

   while (h != NULL)
     {
	h->flags &= ~HEADER_HIDDEN;
	h = h->real_next;
     }
   Slrn_Full_Screen_Update = 1;
   _art_Threads_Collapsed = 0;
   if (sync_now) _art_find_header_line_num ();
}

/*}}}*/

static void uncollapse_header (Slrn_Header_Type *h) /*{{{*/
{
   h->flags &= ~HEADER_HIDDEN;
}

/*}}}*/

static void collapse_header (Slrn_Header_Type *h) /*{{{*/
{
   h->flags |= HEADER_HIDDEN;
}

/*}}}*/

static void for_this_tree (Slrn_Header_Type *h, void (*f)(Slrn_Header_Type *)) /*{{{*/
{
   Slrn_Header_Type *child = h->child;
   while (child != NULL)
     {
	for_this_tree (child, f);
	child = child->sister;
     }
   (*f) (h);
   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

static void for_this_family (Slrn_Header_Type *h, void (*f)(Slrn_Header_Type *)) /*{{{*/
{
   while (h != NULL)
     {
	for_this_tree (h, f);
	h = h->sister;
     }
}

/*}}}*/

/* For efficiency, these functions only sync their own changes if sync_linenum
 * is non-zero, so do not expect them to clean up a "dirty" window */
void slrn_uncollapse_this_thread (Slrn_Header_Type *h, int sync_linenum) /*{{{*/
{
   Slrn_Header_Type *child;
   int back = 0;

   /* if (_art_Threads_Collapsed == 0) return; */

   while ((h->parent != NULL) && (h->prev != NULL))
     {
	h = h->prev;
	back++;
     }
   if ((child = h->child) == NULL) return;
   if (0 == (child->flags & HEADER_HIDDEN)) return;

   for_this_family (child, uncollapse_header);

   if (sync_linenum)
     {
	Slrn_Full_Screen_Update = 1;
	Slrn_Current_Header = h;
	Slrn_Header_Window.current_line = (SLscroll_Type *) h;
	/* SLscroll_find_line_num scales poorly on large groups,
	 * so we update this information ourselves: */
	Slrn_Header_Window.line_num -= back;
	Slrn_Header_Window.num_lines += h->num_children;
     }

   _art_Threads_Collapsed = -1;	       /* uncertain */
}

/*}}}*/

void slrn_collapse_this_thread (Slrn_Header_Type *h, int sync_linenum) /*{{{*/
{
   Slrn_Header_Type *child;
   int back = 0;

   /* if (_art_Threads_Collapsed == 1) return; */

   while ((h->parent != NULL) && (h->prev != NULL))
     {
	h = h->prev;
	back++;
     }

   if ((child = h->child) == NULL) return;
   if (child->flags & HEADER_HIDDEN) return;

   for_this_family (child, collapse_header);

   if (sync_linenum)
     {
	Slrn_Full_Screen_Update = 1;
	Slrn_Current_Header = h;
	Slrn_Header_Window.current_line = (SLscroll_Type *) h;
	Slrn_Header_Window.line_num -= back;
	Slrn_Header_Window.num_lines -= h->num_children;
     }

   _art_Threads_Collapsed = -1;	       /* uncertain */
}

/*}}}*/

static void toggle_collapse_threads (void) /*{{{*/
{
   if (Slrn_Prefix_Arg_Ptr != NULL)
     {
	if (_art_Threads_Collapsed == 1)
	  {
	     slrn_uncollapse_threads (0);
	  }
	else slrn_collapse_threads (0);
	Slrn_Prefix_Arg_Ptr = NULL;
     }
   else
     {
	if (0 == slrn_is_thread_collapsed (Slrn_Current_Header))
	  slrn_collapse_this_thread (Slrn_Current_Header, 0);
	else
	  slrn_uncollapse_this_thread (Slrn_Current_Header, 0);

	find_non_hidden_header ();
     }
   _art_find_header_line_num ();
}

/*}}}*/

unsigned int slrn_thread_size (Slrn_Header_Type *h)
{
   if (h == NULL) return 0;
   return 1 + h->num_children;
}

int slrn_is_thread_collapsed (Slrn_Header_Type *h)
{
   if (h == NULL) return 1;
   while (h->parent != NULL) h = h->parent;
   if (h->child == NULL) return 0;
   return (h->child->flags & HEADER_HIDDEN);
}

/*}}}*/

/*{{{ select_article */

/* returns 0 if article selected, -1 if something went wrong or 1 if article
 * already selected.
 */
static int select_article (int check_mime) /*{{{*/
{
   int ret = 1;

   slrn_uncollapse_this_thread (Slrn_Current_Header, 1);

   if (Slrn_Current_Header != Header_Showing)
     ret = 0;

   if (((ret == 0) || check_mime) &&
       select_header (Slrn_Current_Header, Slrn_Del_Article_Upon_Read) < 0)
     return -1;

   if ((0 == ret) && Slrn_Current_Article->mime.needs_metamail)
     {
	if (slrn_mime_call_metamail ())
	  return -1;
     }

   /* The article gets synced here */
   set_article_visibility (1);
   return ret;
}

/*}}}*/

/*}}}*/

/*{{{ mark_spot and exchange_mark */

static void mark_spot (void) /*{{{*/
{
   Mark_Header = Slrn_Current_Header;
   slrn_message (_("Mark set."));
}

/*}}}*/

static void exchange_mark (void) /*{{{*/
{
   Slrn_Header_Type *h;

   if ((h = Mark_Header) == NULL)
     {
	slrn_error (_("Mark not set."));
	return;
     }

   mark_spot ();
   slrn_goto_header (h, 0);
}

/*}}}*/

/*}}}*/
/*{{{ subject/author header searching commands */

static void header_generic_search (int dir, int type) /*{{{*/
{
   static char search_str[SLRL_DISPLAY_BUFFER_SIZE];
#if SLANG_VERSION < 20000
   SLsearch_Type st;
#else
   SLsearch_Type *st;
   unsigned int flags = SLSEARCH_CASELESS;
#endif
   Slrn_Header_Type *l;
   char* prompt;
   int ret;

   prompt = slrn_strdup_strcat ((type == 's' ? _("Subject search ") : _("Author search ")),
				(dir > 0 ? _("(forward)") : _("(backward)")), ": ",
				NULL);

   ret = slrn_read_input (prompt, search_str, NULL, 0, 0);
   slrn_free (prompt);
   if (ret <= 0) return;

#if SLANG_VERSION < 20000
   SLsearch_init (search_str, 1, 0, &st);
#else
   if (Slrn_UTF8_Mode) flags |= SLSEARCH_UTF8;
   st = SLsearch_new ((SLuchar_Type *)search_str, flags);
   if (NULL == st)
     return;
#endif

   if (dir > 0) l = Slrn_Current_Header->next;
   else l = Slrn_Current_Header->prev;

   while (l != NULL)
     {
	if (type == 's')
	  {
	     if ((l->subject != NULL)
#if SLANG_VERSION < 20000
		 && (NULL != SLsearch ((unsigned char *) l->subject,
				       (unsigned char *) l->subject + strlen (l->subject),
				       &st))
#else
		 && (NULL != SLsearch_forward (st, (SLuchar_Type *) l->subject,
					       (SLuchar_Type *) l->subject + strlen (l->subject)))
#endif
		     )
	       break;
	  }
	else if ((l->from != NULL)
#if SLANG_VERSION < 20000
		 && (NULL != SLsearch ((unsigned char *) l->from,
				       (unsigned char *) l->from + strlen (l->from),
				       &st))
#else
		 && (NULL != SLsearch_forward (st, (SLuchar_Type *) l->from,
					       (SLuchar_Type *) l->from + strlen (l->from)))
#endif
		     )
	  break;

	if (dir > 0) l = l->next; else l = l->prev;
     }

#if SLANG_VERSION >= 20000
   SLsearch_delete (st);
#endif
   if (l == NULL)
     {
	slrn_error (_("Not found."));
	return;
     }

   if (l->flags & HEADER_HIDDEN) slrn_uncollapse_this_thread (l, 0);
   Slrn_Current_Header = l;
   _art_find_header_line_num ();
}

/*}}}*/

static void subject_search_forward (void) /*{{{*/
{
   header_generic_search (1, 's');
}

/*}}}*/

static void subject_search_backward (void) /*{{{*/
{
   header_generic_search (-1, 's');
}

/*}}}*/

static void author_search_forward (void) /*{{{*/
{
   header_generic_search (1, 'a');
}

/*}}}*/

static void author_search_backward (void) /*{{{*/
{
   header_generic_search (-1, 'a');
}

/*}}}*/

/*}}}*/

/*{{{ score header support */

/*{{{ kill list functions */

typedef struct Kill_List_Type /*{{{*/
{
#define MAX_DKILLS 50
   NNTP_Artnum_Type range_min[MAX_DKILLS];
   NNTP_Artnum_Type range_max[MAX_DKILLS];
   unsigned int active_index;
   struct Kill_List_Type *next;
}

/*}}}*/

Kill_List_Type;

static Kill_List_Type *Kill_List;
static Kill_List_Type *Missing_Article_List;

static Kill_List_Type *add_to_specified_kill_list (NNTP_Artnum_Type num, Kill_List_Type *root) /*{{{*/
{
   unsigned int active_index;

   if (num < 0) return root;

   if (root != NULL)
     {
	active_index = root->active_index;
	if (num == root->range_max[active_index] + 1)
	  {
	     root->range_max[active_index] = num;   /* extend range */
	     return root;
	  }
	active_index++;		       /* need new range */
     }
   else active_index = MAX_DKILLS;

   if (active_index == MAX_DKILLS)
     {
	Kill_List_Type *k;
	k = (Kill_List_Type *) SLMALLOC (sizeof (Kill_List_Type));
	if (k == NULL) return root;
	k->next = root;
	root = k;
	active_index = 0;
     }

   root->active_index = active_index;
   root->range_min[active_index] = root->range_max[active_index] = num;
   return root;
}

/*}}}*/

static void add_to_kill_list (NNTP_Artnum_Type num) /*{{{*/
{
   Kill_List = add_to_specified_kill_list (num, Kill_List);
   Number_Killed++;
}

/*}}}*/

static void add_to_missing_article_list (NNTP_Artnum_Type num) /*{{{*/
{
   Missing_Article_List = add_to_specified_kill_list (num, Missing_Article_List);
}

/*}}}*/

static void free_specific_kill_list_and_update (Kill_List_Type *k) /*{{{*/
{
   while (k != NULL)
     {
	Kill_List_Type *next = k->next;
	unsigned int i, imax = k->active_index;
	NNTP_Artnum_Type *mins = k->range_min;
	NNTP_Artnum_Type *maxs = k->range_max;

	if (User_Aborted_Group_Read == 0)
	  {
	     for (i = 0; i <= imax; i++)
	       {
		  slrn_mark_articles_as_read (NULL, mins[i], maxs[i]);
	       }
	  }
	SLFREE (k);
	k = next;
     }
}

/*}}}*/

static void free_kill_lists_and_update (void) /*{{{*/
{
   free_specific_kill_list_and_update (Kill_List);
   Kill_List = NULL;
   Number_Killed = 0;
   free_specific_kill_list_and_update (Missing_Article_List);
   Missing_Article_List = NULL;
}

/*}}}*/

/*}}}*/

Slrn_Header_Type *slrn_set_header_score (Slrn_Header_Type *h,
					 int score, int apply_kill,
					 Slrn_Score_Debug_Info_Type *sdi)
{
   if (h == NULL) return NULL;

   if (h->flags & HEADER_HIGH_SCORE)
     Number_High_Scored--;
   if (h->flags & HEADER_LOW_SCORE)
     Number_Low_Scored--;

   h->flags &= ~(HEADER_HIGH_SCORE|HEADER_LOW_SCORE);

   if (score >= Slrn_High_Score_Min)
     {
	h->flags |= HEADER_HIGH_SCORE;
	Number_High_Scored++;
     }
   else if (score < Slrn_Low_Score_Max)
     {
	if ((score <= Slrn_Kill_Score_Max) && apply_kill)
	  {
	     NNTP_Artnum_Type number = h->number;
	     if (Slrn_Kill_Log_FP != NULL)
	       {
		  Slrn_Score_Debug_Info_Type *hlp = sdi;
		  fprintf (Slrn_Kill_Log_FP, _("Score %d killed article %s\n"), score, h->msgid);
		  while (hlp != NULL)
		    {
		       if (hlp->description [0] != 0)
			 fprintf (Slrn_Kill_Log_FP, _(" Score %c%5i: %s (%s:%i)\n"),
				  (hlp->stop_here ? '=' : ' '), hlp->score,
				  hlp->description, hlp->filename, hlp->linenumber);
		       else
			 fprintf (Slrn_Kill_Log_FP, _(" Score %c%5i: %s:%i\n"),
				  (hlp->stop_here ? '=' : ' '), hlp->score,
				  hlp->filename, hlp->linenumber);
		       hlp = hlp->next;
		    }
		  fprintf (Slrn_Kill_Log_FP, _("  Newsgroup: %s\n  From: %s\n  Subject: %s\n\n"),
			   Slrn_Current_Group_Name, h->from, h->subject);
	       }
	     free_killed_header (h);
	     add_to_kill_list (number);
	     Number_Score_Killed++;
	     h = NULL;
	     goto free_and_return;
	  }

	if (0 == (h->flags & HEADER_DONT_DELETE_MASK))
	  {
	     if (0 == (h->flags & HEADER_READ))
	       Number_Read++;
	     h->flags |= (HEADER_READ | HEADER_LOW_SCORE);
	  }
	else
	  h->flags |= HEADER_LOW_SCORE;
	Number_Low_Scored++;
	/* The next line should be made configurable */
	kill_cross_references (h);
     }
   h->thread_score = h->score = score;

   free_and_return:
   while (sdi != NULL)
     {
	Slrn_Score_Debug_Info_Type *hlp = sdi->next;
	slrn_free ((char*)sdi);
	sdi = hlp;
     }
   return h;
}

/*{{{ apply_score */
static Slrn_Header_Type *apply_score (Slrn_Header_Type *h, int apply_kill) /*{{{*/
{
   int score;
   Slrn_Score_Debug_Info_Type *sdi = NULL;

   if ((h == NULL) || (-1 == h->number)) return h;

   if (Slrn_Apply_Score && Perform_Scoring)
     score = slrn_score_header (h, Slrn_Current_Group_Name,
				((Slrn_Kill_Log_FP != NULL) &&
				 apply_kill) ? &sdi : NULL);
   else score = 0;

   return slrn_set_header_score (h, score, apply_kill, sdi);
}

/*}}}*/

/*}}}*/

/*{{{ score_headers */
static void score_headers (int apply_kill) /*{{{*/
{
   Slrn_Header_Type *h = Slrn_First_Header;
   int percent, last_percent, delta_percent;
   int num;

   if ((h == NULL) || (Slrn_Apply_Score == 0)) return;

   /* slrn_set_suspension (1); */

   percent = num = 0;
   delta_percent = (30 * 100) / Total_Num_Headers + 1;
   last_percent = -delta_percent;

   slrn_message_now (_("Scoring articles ..."));

   while ((h != NULL) && (SLang_get_error() != USER_BREAK))
     {
	Slrn_Header_Type *prev, *next;
	prev = h->real_prev;
	next = h->real_next;

	num++;
	h = apply_score (h, apply_kill);
	percent = (100 * num) / Total_Num_Headers;
	if (percent >= last_percent + delta_percent)
	  {
	     slrn_message_now (_("Scoring articles: %2d%%, Killed: %u, High: %u, Low: %u"),
			       percent, Number_Score_Killed, Number_High_Scored, Number_Low_Scored);
	     last_percent = percent;
	  }

	if (h == NULL)
	  {
	     num--;
	     if (prev == NULL)
	       Slrn_First_Header = next;
	     else
	       prev->next = prev->real_next = next;

	     if (next != NULL)
	       next->prev = next->real_prev = prev;
	  }
	h = next;
     }
   if (SLang_get_error () == USER_BREAK)
     {
	slrn_error ("Scoring aborted.");
	SLang_set_error (0);
     }
   if (apply_kill)
     Slrn_Current_Header = _art_Headers = Slrn_First_Header;
   /* slrn_set_suspension (0); */
}

/*}}}*/

/*}}}*/

static void init_scoring (void)
{
   Number_Score_Killed = Number_High_Scored = Number_Low_Scored = 0;
}

void slrn_apply_scores (int apply_now) /*{{{*/
{
   if ((apply_now == -1) &&
       (1 != slrn_get_yesno (1, _("Apply scorefile now"))))
     return;

   slrn_close_score ();

   if (-1 == slrn_open_score (Slrn_Current_Group_Name))
     return;

   /* high / low scoring counters get updated in slrn_set_header_score */
   Number_Score_Killed = 0;
   Slrn_Apply_Score = 1;	       /* force even if there are no scores */
   Perform_Scoring = 1;
   get_missing_headers ();
   score_headers (0);
   slrn_sort_headers ();
   slrn_goto_header (Slrn_Current_Header, 0);
}

/*}}}*/

static void create_score (void)
{
   Slrn_Header_Type *h;

   if (NULL == (h = affected_header ()))
     return;

   if ((Slrn_Batch == 0) &&
       (-1 != slrn_edit_score (h, Slrn_Current_Group_Name)))
     slrn_apply_scores (-1);
}

/*}}}*/

/*{{{ view_scores */
static void view_scores (void) /*{{{*/
{
   int selection, i;
   unsigned int scorenum = 0, homelen;
   char line [1024], file[SLRN_MAX_PATH_LEN], *home;
   char **scores;

   Slrn_Score_Debug_Info_Type *sdi = NULL, *hlp;

   if (!Perform_Scoring)
     {
	slrn_message(_("No scorefile loaded."));
	return;
     }

   slrn_uncollapse_this_thread (affected_header (), 1);
   slrn_score_header (affected_header (), Slrn_Current_Group_Name, &sdi);

   if ((hlp = sdi) == NULL)
     {
	slrn_message (_("This article is not matched by any scorefile entries."));
	return;
     }

   while (hlp != NULL)
     {
	scorenum++;
	hlp = hlp->next;
     }

   hlp = sdi;
   if ((NULL == (home = getenv ("SLRNHOME"))) &&
       (NULL == (home = getenv ("HOME"))))
     {
	home = "";
	homelen = 1;
     }
   else
     homelen = strlen (home);

   scores = (char **)slrn_safe_malloc
     ((unsigned int)(sizeof (char *) * scorenum));

   i = 0;
   while (hlp != NULL)
     {
	if (!strncmp (hlp->filename, home, homelen))
	  {
	     *file = '~';
	     slrn_strncpy (file+1, hlp->filename + homelen, sizeof (file) - 1);
	  }
	else
	  slrn_strncpy (file, hlp->filename, sizeof (file));
	if (hlp->description [0] != 0)
	  slrn_snprintf (line, sizeof (line), "%c%5i: %s (%s:%i)",
			 (hlp->stop_here ? '=' : ' '), hlp->score,
			 hlp->description, file, hlp->linenumber);
	else
	  slrn_snprintf (line, sizeof (line), "%c%5i: %s:%i",
			 (hlp->stop_here ? '=' : ' '), hlp->score,
			 file, hlp->linenumber);
	scores [i] = slrn_safe_strmalloc (line);
	i++;
	hlp = hlp->next;
     }

   selection = slrn_select_list_mode (_("This article is matched by the following scores"),
				      scorenum, scores, 0, 0, NULL);

   if (selection >= 0)
     {
	hlp = sdi;
	for (i = 0; i < selection; i++)
	  hlp = hlp->next;

	if (slrn_edit_file (Slrn_Editor_Score, (char *) hlp->filename,
			    hlp->linenumber, 0) >= 0)
	  {
	     slrn_make_home_filename (Slrn_Score_File, file, sizeof (file));

	     if (Slrn_Scorefile_Open != NULL)
	       slrn_free (Slrn_Scorefile_Open);
	     Slrn_Scorefile_Open = slrn_safe_strmalloc (file);

	     slrn_apply_scores (-1);
	  }
     }

   i = 0;
   while (sdi != NULL)
     {
	hlp = sdi->next;
	slrn_free ((char *)scores [i++]);
	slrn_free ((char *)sdi);
	sdi = hlp;
     }
   slrn_free ((char *)scores);
}
/*}}}*/

/*}}}*/

/*{{{ get headers from server and process_xover */

static Slrn_Header_Type *process_xover (Slrn_XOver_Type *xov)
{
   Slrn_Header_Type *h;
   int err;

   h = (Slrn_Header_Type *) slrn_safe_malloc (sizeof (Slrn_Header_Type));

   slrn_map_xover_to_header (xov, h, 1);
   Number_Total++;

   err = SLang_get_error ();

   if ((-1 == slrn_rfc1522_decode_header ("Subject", &h->subject))
       && (err == 0))
     {
	h->flags |= HEADER_HAS_PARSE_PROBLEMS;
	slrn_clear_error ();
     }

   if ((-1 == slrn_rfc1522_decode_header ("From", &h->from))
       && (err == 0))
     {
	h->flags |= HEADER_HAS_PARSE_PROBLEMS;
	slrn_clear_error ();
     }

   get_header_real_name (h);

#if SLRN_HAS_GROUPLENS
   if (Slrn_Use_Group_Lens)
     {
	h->gl_rating = h->gl_pred = -1;
     }
#endif
   return h;
}

/*}}}*/

/*{{{ get_headers from server */
static int get_add_headers (NNTP_Artnum_Type min, NNTP_Artnum_Type max) /*{{{*/
{
   char *meter_chars = "|/-\\";
   static unsigned int last_meter_char;
   int reads_per_update, num = 0;

   if ((reads_per_update = Slrn_Reads_Per_Update) < 5)
     reads_per_update = 50;

   while (slrn_open_add_xover (min, max) == 1)
     {
	Slrn_Header_Type *h;
	Slrn_Header_Line_Type *l;
	NNTP_Artnum_Type this_num;

	h = Slrn_First_Header;

	while (-1 != slrn_read_add_xover(&l, &this_num))
	  {
	     if (SLang_get_error () == USER_BREAK)
	       {
		  if (Slrn_Server_Obj->sv_reset != NULL)
		    Slrn_Server_Obj->sv_reset ();
		  return -1;
	       }

	     if (1 == (++num % reads_per_update))
	       {
		  if (meter_chars[last_meter_char] == 0)
		    last_meter_char = 0;

		  slrn_message_now (_("%s: receiving additional headers...[%c]"),
				    Slrn_Current_Group_Name,
				    meter_chars[last_meter_char++]);
	       }

	     while ((NULL != h) && (this_num > h->number))
	       h = h->real_next;

	     if ((NULL != h) && (this_num == h->number))
	       slrn_append_add_xover_to_header (h, l);
	  }
     }
   slrn_close_add_xover (0);

   return 0;
}

/*}}}*/
/* gets the headers of article number min-max, decrementing *totalp for each
 * downloaded article */
static int get_headers (NNTP_Artnum_Type min, NNTP_Artnum_Type max, NNTP_Artnum_Type *totalp) /*{{{*/
{
   Slrn_Header_Type *h;
   /* int percent, last_percent, dpercent, */
   NNTP_Artnum_Type expected_num;
   NNTP_Artnum_Type total = *totalp;
   int reads_per_update;
   int num_processed;
   int num, err;
   Slrn_XOver_Type xov;

   if (total == 0)
     return 0;

   if (SLang_get_error () == USER_BREAK)
     return -1;

   if ((reads_per_update = Slrn_Reads_Per_Update) < 5)
     reads_per_update = 50;

   /* slrn_set_suspension (1); */

   err = slrn_open_xover (min, max);
   if (err != OK_XOVER)
     {
	if ((err == ERR_NOCRNT) || /* no articles in the range */
	    (err == ERR_NOARTIG))  /* this one is not RFC 2980 compliant */
	  return 0;

	return -1;
     }

   num_processed = 0;
   expected_num = min;
   num = Total_Num_Headers + Number_Killed;
   while (slrn_read_xover(&xov) > 0)
     {
	NNTP_Artnum_Type this_num;

	if (SLang_get_error () == USER_BREAK)
	  {
	     slrn_free_xover_data (&xov);
	     if (Slrn_Server_Obj->sv_reset != NULL)
	       Slrn_Server_Obj->sv_reset ();
	     return -1;
	  }

	this_num = xov.id;

	if (expected_num != this_num)
	  {
	     NNTP_Artnum_Type bad_num;

	     total -= (this_num - expected_num);

	     for (bad_num = expected_num; bad_num < this_num; bad_num++)
	       add_to_missing_article_list (bad_num);
	  }

	expected_num = this_num + 1;
	num++;
	h = process_xover (&xov);      /* steals the xover data -- nothing to free */

	if ((1 == (num % reads_per_update))
	    && (SLang_get_error () == 0))
	  {
	     slrn_message_now (_("%s: headers received: %2d/" NNTP_FMT_ARTNUM),
			       Slrn_Current_Group_Name, num, total);
	  }

	if (Slrn_First_Header == NULL)
	  Slrn_First_Header = _art_Headers = h;
	else
	  {
	     h->real_next = Slrn_Current_Header->real_next;
	     h->real_prev = Slrn_Current_Header;
	     Slrn_Current_Header->real_next = h;

	     if (h->real_next != NULL)
	       {
		  h->real_next->real_prev = h;
	       }
	  }

	Slrn_Current_Header = h;
	num_processed++;
     }

   slrn_close_xover ();

   if (expected_num != max + 1)
     {
	NNTP_Artnum_Type bad_num;

	total -= (max - expected_num) + 1;

	for (bad_num = expected_num; bad_num <= max; bad_num++)
	  add_to_missing_article_list (bad_num);
     }

   if (-1 == get_add_headers (min, max))
     return -1;

   /* slrn_set_suspension (0); */
   *totalp = total;

   Total_Num_Headers += num_processed;

   return (int) num_processed;
}

/*}}}*/
static void get_missing_headers () /*{{{*/
{
   Slrn_Header_Type *h;
   NNTP_Artnum_Type min, max;

   if (0 == slrn_add_xover_missing())
     return;

   h = Slrn_First_Header;

   while ((NULL != h) && (-1 == h->number))
     h = h->real_next;

   while (NULL != h)
     {
	max = min = h->number;
	while ((NULL != (h = h->real_next)) && (h->number == max + 1))
	  max++;
	get_add_headers (min, max);
     }

   slrn_close_add_xover (1);
}

/*}}}*/

/*}}}*/

/*}}}*/
/*{{{ get parent/children headers, etc... */

/* Nothing is synced by this routine.  It is up to the calling routine. */
static void insert_header (Slrn_Header_Type *ref) /*{{{*/
{
   NNTP_Artnum_Type n, id;
   Slrn_Header_Type *h;

   ref->hash_next = Header_Table[ref->hash % HEADER_TABLE_SIZE];
   Header_Table[ref->hash % HEADER_TABLE_SIZE] = ref;

   n = ref->number;
   h = Slrn_First_Header;
   while (h != NULL)
     {
	if (h->number >= n)
	  {
	     ref->real_next = h;
	     ref->real_prev = h->real_prev;
	     if (h->real_prev != NULL) h->real_prev->real_next = ref;
	     h->real_prev = ref;

	     if (h == Slrn_First_Header) Slrn_First_Header = ref;
	     if (h == _art_Headers) _art_Headers = ref;

	     break;
	  }
	h = h->real_next;
     }

   if (h == NULL)
     {
	h = Slrn_First_Header;
	while (h->real_next != NULL) h = h->real_next;

	ref->real_next = NULL;
	ref->real_prev = h;
	h->real_next = ref;
     }

   if ((id = ref->number) <= 0) return;

   /* Set the flags for this guy. */
   if (!(ref->flags & HEADER_REQUEST_BODY) &&
       slrn_ranges_is_member (Current_Group->range.next, id))
     {
	if (!(ref->flags & HEADER_READ))
	  {
	     ref->flags |= HEADER_READ;
	     Number_Read++;
	  }
     }
   else if (ref->flags & HEADER_READ)
     {
	ref->flags &= ~HEADER_READ;
	Number_Read--;
     }
}

/*}}}*/

/* line number is not synced. */
static int get_header_by_message_id (char *msgid,
				     int reconstruct_thread,
				     int query_server,
				     Slrn_Range_Type *no_body) /*{{{*/
{
   Slrn_Header_Type *ref;
   Slrn_XOver_Type xov;

   if ((msgid == NULL) || (*msgid == 0)) return -1;

   ref = _art_find_header_from_msgid (msgid, msgid + strlen (msgid));
   if (ref != NULL)
     {
	Slrn_Current_Header = ref;
	if (reconstruct_thread == 0)
	  _art_find_header_line_num ();
	Slrn_Full_Screen_Update = 1;
	return 0;
     }

   if (query_server == 0)
     return -1;

   slrn_message_now (_("Retrieving %s from server..."), msgid);

   /* Try reading it from the server */
   if (-1 == slrn_xover_for_msgid (msgid, &xov))
     return 1;

   ref = process_xover (&xov);

   if ((ref->number!=0) &&
       (slrn_ranges_is_member (no_body, ref->number)))
     {
	ref->flags |= HEADER_WITHOUT_BODY;
	if (slrn_ranges_is_member (Current_Group->requests, ref->number))
	  ref->flags |= HEADER_REQUEST_BODY;
     }

   ref = apply_score (ref, 0);

   if (ref == NULL) return -1;

   insert_header (ref);

   Slrn_Current_Header = ref;
   if (reconstruct_thread == 0)
     {
	slrn_sort_headers ();
     }
   return 0;
}

/*}}}*/

/* returns -1 if not implemented or the number of children returned from
 * the server.  It does not sync line number.
 */
static int find_children_headers (Slrn_Header_Type *parent, /*{{{*/
				  Slrn_Range_Type *no_body,
				  unsigned int *numidsp)
{
   char buf[NNTP_BUFFER_SIZE];
   NNTP_Artnum_Type *id_array;
   NNTP_Artnum_Type id;
   unsigned int i, num_ids, id_array_size;
   char *fmt = _("Retrieving children from server...[%c]");
   char *meter_chars = "|/-\\";
   static unsigned int last_meter_char;

   if (OK_HEAD != Slrn_Server_Obj->sv_xpat_cmd ("References",
						Slrn_Server_Min, Slrn_Server_Max,
						parent->msgid))
     {
	slrn_error (_("Your server does not provide support for this feature."));
	return -1;
     }

   if (meter_chars[last_meter_char] == 0)
     last_meter_char = 0;

   slrn_message_now (fmt, meter_chars[last_meter_char]);
   last_meter_char++;

   num_ids = 0;
   id_array_size = 0;
   id_array = NULL;

   while (1)
     {
	int status;
	char *p;

	status = Slrn_Server_Obj->sv_read_line (buf, sizeof (buf) - 1);
	if (status <= 0)
	  break;

	if (meter_chars[last_meter_char] == 0)
	  last_meter_char = 0;

	slrn_message_now (fmt, meter_chars[last_meter_char]);
	last_meter_char++;

	id = NNTP_STR_TO_ARTNUM (buf);
	if (id <= 0) continue;

	p = buf;
	while ((*p != 0) && (*p != ' '))
	  p++;
	if ((*p == 0) || (*(p+1) == 0))
	  continue; /* work around a bug in Typhoon servers */

	if (NULL != find_header_from_serverid (id))
	  continue;

	if (num_ids == id_array_size)
	  {
	     NNTP_Artnum_Type *new_ids;
	     unsigned int new_size = 2*(id_array_size + 1);
	     new_ids = (NNTP_Artnum_Type *)slrn_realloc ((char *)id_array, new_size*sizeof(NNTP_Artnum_Type), 1);
	     if (new_ids == NULL)
	       break;
	     id_array = new_ids;
	     id_array_size = new_size;
	  }
	id_array[num_ids] = id;
	num_ids++;
     }

   for (i = 0; i < num_ids; i++)
     {
	Slrn_XOver_Type xov;
	Slrn_Header_Type *h = NULL;

	id = id_array[i];

	if (OK_XOVER != slrn_open_xover (id, id))
	  break;

	/* This will loop once. */
	while (slrn_read_xover (&xov) > 0)
	  {
	     Slrn_Header_Type *bad_h;

	     h = process_xover (&xov);

	     if ((h->number!=0) &&
		 (slrn_ranges_is_member (no_body, h->number)))
	       {
		  h->flags |= HEADER_WITHOUT_BODY;
		  if (slrn_ranges_is_member (Current_Group->requests, h->number))
		    h->flags |= HEADER_REQUEST_BODY;
	       }

	     h = apply_score (h, 0);
	     if (h == NULL) continue;

	     /* We may already have this header.  How is this possible?
	      * If the header was retrieved sometime earlier via HEAD
	      * <msgid>, the server may not have returned the article
	      * number.  As a result, that header may have a number
	      * of -1.  Here, we really have the correct article
	      * number since the previous while loop made sure of that.
	      * So, before inserting it, check to see whether or not we
	      * have it and if so, fixup the id.
	      */

	     bad_h = slrn_find_header_with_msgid (h->msgid);
	     if (bad_h != NULL)
	       {
		  bad_h->number = h->number;
		  free_killed_header (h);
		  h = bad_h;
		  continue;
	       }
	     insert_header (h);
	  }
	slrn_close_xover ();

	if (h == NULL) continue;

	slrn_open_all_add_xover ();
	while (slrn_open_add_xover (id, id) == 1)
	  {
	     Slrn_Header_Line_Type *l;
	     NNTP_Artnum_Type unused;

	     while (-1 != slrn_read_add_xover (&l, &unused)) /* loops once */
	       slrn_append_add_xover_to_header (h, l);
	  }
	slrn_close_add_xover (0);
     }
   if (numidsp != NULL)
     *numidsp = num_ids;
   slrn_free ((char *) id_array);
   return 0;
}

/*}}}*/

/* Line number not synced. */
static void get_children_headers_1 (Slrn_Header_Type *h, /*{{{*/
				    Slrn_Range_Type *no_body)
{
   while (h != NULL)
     {
	(void) find_children_headers (h, no_body, NULL);
	if (h->child != NULL)
	  {
	     get_children_headers_1 (h->child, no_body);
	  }
	h = h->sister;
     }
}

/*}}}*/

static void get_children_headers (void) /*{{{*/
{
   Slrn_Header_Type *h;
   int thorough_search;
   Slrn_Range_Type *no_body = NULL;

#if SLRN_HAS_SPOOL_SUPPORT
   if (Slrn_Server_Id == SLRN_SERVER_ID_SPOOL)
     no_body = slrn_spool_get_no_body_ranges (Slrn_Current_Group_Name);
#endif

   if (Slrn_Prefix_Arg_Ptr == NULL) thorough_search = 1;
   else
     {
	Slrn_Prefix_Arg_Ptr = NULL;
	thorough_search = 0;
     }

   /* slrn_set_suspension (1); */

   if (find_children_headers (Slrn_Current_Header, no_body, NULL) < 0)
     {
	/* slrn_set_suspension (0); */
	slrn_ranges_free (no_body);
	return;
     }

   slrn_sort_headers ();

   h = Slrn_Current_Header->child;
   if ((h != NULL) && thorough_search)
     {
	/* Now walk the tree getting children headers.  For efficiency,
	 * only children currently threaded will be searched.  Hopefully the
	 * above attempt got everything.  If other newsreaders did not chop off
	 * headers, this would be unnecessary!
	 */
	get_children_headers_1 (h, no_body);
	slrn_sort_headers ();
     }
   slrn_uncollapse_this_thread (Slrn_Current_Header, 1);
   slrn_ranges_free (no_body);

   /* slrn_set_suspension (0); */
}

/*}}}*/

static void mark_headers_unprocessed (void)
{
   Slrn_Header_Type *h = Slrn_First_Header;

   while (h != NULL)
     {
	h->flags &= ~HEADER_PROCESSED;
	h = h->real_next;
     }
}

static void reference_loop_error (void)
{
   slrn_error (_("Header is part of a reference loop"));
   mark_headers_unprocessed ();
}

static void get_parent_header (void) /*{{{*/
{
   char *r1, *r0, *rmin;
   unsigned int len;
   char buf[512];
   int no_error_no_thread;
   Slrn_Header_Type *last_header;
   Slrn_Range_Type *no_body = NULL;

   if (Slrn_Current_Header == NULL) return;

   if (Slrn_Prefix_Arg_Ptr == NULL) no_error_no_thread = 0;
   else
     {
	if (*Slrn_Prefix_Arg_Ptr != 2) Slrn_Prefix_Arg_Ptr = NULL;
	/* else: leave it for get_children_headers() */
	no_error_no_thread = 1;
     }

   last_header = NULL;
   r1 = rmin = NULL;
#if SLRN_HAS_SPOOL_SUPPORT
   if (Slrn_Server_Id == SLRN_SERVER_ID_SPOOL)
     no_body = slrn_spool_get_no_body_ranges (Slrn_Current_Group_Name);
#endif
   do
     {
	rmin = Slrn_Current_Header->refs;
	if (rmin == NULL) break;

	if (last_header != Slrn_Current_Header)
	  {
	     if (Slrn_Current_Header->flags & HEADER_PROCESSED)
	       {
		  reference_loop_error ();
		  slrn_ranges_free (no_body);
		  return;
	       }
	     last_header = Slrn_Current_Header;
	     last_header->flags |= HEADER_PROCESSED;
	     r1 = rmin + strlen (rmin);
	  }

	while ((r1 > rmin) && (*r1 != '>')) r1--;
	r0 = r1 - 1;
	while ((r0 >= rmin) && (*r0 != '<')) r0--;

	if ((r0 < rmin) || (r1 == rmin))
	  {
	     if (no_error_no_thread) break;
	     slrn_error (_("Article has no parent reference."));
	     mark_headers_unprocessed ();
	     slrn_ranges_free (no_body);
	     return;
	  }

	len = (unsigned int) ((r1 + 1) - r0);
	strncpy (buf, r0, len);
	buf[len] = 0;

	r1 = r0;
     }
   while (no_error_no_thread
	  && (get_header_by_message_id (buf, 1, 1, no_body) >= 0));

   mark_headers_unprocessed ();
   slrn_ranges_free (no_body);

   if (no_error_no_thread)
     {
	slrn_sort_headers ();
	if (SLKeyBoard_Quit == 0) get_children_headers ();
	else Slrn_Prefix_Arg_Ptr = NULL;
     }
   else (void) slrn_locate_header_by_msgid (buf, 0, 1); /* syncs the line */

   slrn_uncollapse_this_thread (Slrn_Current_Header, 1);
}

/*}}}*/

int slrn_locate_header_by_msgid (char *msgid, int no_error, int query_server)
{
   Slrn_Range_Type *no_body = NULL;
#if SLRN_HAS_SPOOL_SUPPORT
   if (Slrn_Server_Id == SLRN_SERVER_ID_SPOOL)
     no_body = slrn_spool_get_no_body_ranges (Slrn_Current_Group_Name);
#endif

   if (0 == get_header_by_message_id (msgid, 0, query_server, no_body))
     {
	/* The actual header might be part of a collapsed thread.  If so, then
	 * the current header may not be the one we are seeking.
	 * Check the message-id and retry with the thread uncollapsed
	 * if this is the case.
	 */
	if ((Slrn_Current_Header->msgid == NULL)
	    || (0 != strcmp (Slrn_Current_Header->msgid, msgid)))
	  {
	     slrn_uncollapse_this_thread (Slrn_Current_Header, 1);
	     (void) get_header_by_message_id (msgid, 0, 0, no_body);
	  }
	slrn_ranges_free (no_body);
	return 0;
     }
   slrn_ranges_free (no_body);
   if (0 == no_error)
     slrn_error (_("Article %s not available."), msgid);
   return -1;
}

static void locate_header_by_msgid (void) /*{{{*/
{
   char buf[SLRL_DISPLAY_BUFFER_SIZE+2];
   char *msgid = buf + 1;
   *buf = 0;
   *msgid = 0;

   if (slrn_read_input (_("Enter Message-ID: "), NULL, msgid, 1, 0) <= 0) return;

   if (*msgid != '<')
     {
	*buf = '<';
	msgid = buf;
     }
   if (!strncmp (msgid+1, "news:", 5))
     {
	msgid += 5;
	*msgid = '<';
     }

   if (msgid [strlen(msgid) - 1] != '>')
     strcat (msgid, ">"); /* safe */

   (void) slrn_locate_header_by_msgid (msgid, 0, 1);
}

/*}}}*/

/*}}}*/

/*{{{ article window display modes */

static void hide_article (void) /*{{{*/
{
   Slrn_Full_Screen_Update = 1;
   if (Article_Visible == 0)
     {
	select_article (1);
	return;
     }

   set_article_visibility (0);
   Article_Window_HScroll = 0;
}

/*}}}*/

#define ZOOM_OFFSET 4
static void zoom_article_window (void)
{
   static int old_rows;
   int zoomed_rows;

   if (Article_Visible == 0)
     hide_article ();

   if (Article_Visible == 0)
     return;

   zoomed_rows = SLtt_Screen_Rows - ZOOM_OFFSET;
   if (zoomed_rows == Article_Window_Nrows)
     /* already zoomed.  Unzoom */
     Article_Window_Nrows = old_rows;
   else
     {
	old_rows = Article_Window_Nrows;
	Article_Window_Nrows = zoomed_rows;
     }

   art_winch ();
}

int slrn_is_article_win_zoomed (void)
{
   return (Article_Window_Nrows == SLtt_Screen_Rows - ZOOM_OFFSET);
}

static void art_left (void) /*{{{*/
{
   if ((Article_Visible == 0)
       || (Article_Window_HScroll == 0))
     {
	if (Header_Window_HScroll == 0) return;
	Header_Window_HScroll -= SLtt_Screen_Cols / 5;
	if (Header_Window_HScroll < 0) Header_Window_HScroll = 0;
     }
   else
     {
	if (Article_Window_HScroll == 0) return;
	Article_Window_HScroll -= (SLtt_Screen_Cols * 2) / 3;
	if (Article_Window_HScroll < 0) Article_Window_HScroll = 0;
     }
   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

static void art_right (void) /*{{{*/
{
   if (Article_Visible == 0)
     Header_Window_HScroll += SLtt_Screen_Cols / 5;
   else
     Article_Window_HScroll += (SLtt_Screen_Cols * 2) / 3;

   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

/*{{{ rot13 and spoilers */
static void toggle_rot13 (void) /*{{{*/
{
   Do_Rot13 = !Do_Rot13;
   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

#if SLRN_HAS_SPOILERS
static void show_spoilers (void) /*{{{*/
{
   Slrn_Article_Line_Type *l1 = NULL;
   Slrn_Article_Line_Type *l;

   if (Slrn_Current_Article == NULL)
     return;

   l = Slrn_Current_Article->lines;

   /* find the first spoiler-ed line */
   while ((l != NULL) && (0 == (l->flags & SPOILER_LINE)))
     l = l->next;
   if (NULL == (l1 = l))
     return;

   /* Prefix arg means un-spoiler the whole article */
   if ((Slrn_Prefix_Arg_Ptr != NULL) || (Slrn_Spoiler_Display_Mode & 2))
     {
	Slrn_Prefix_Arg_Ptr = NULL;
	while (l != NULL)
	  {
	     l->flags &= ~SPOILER_LINE;
	     l = l->next;
	  }
     }
   else
     {
	char *s, n;

	s = l->buf;
	n = Num_Spoilers_Visible + 1;

	while (n-- != 0)
	  {
	     if (NULL == (s = slrn_strbyte (s, 12)))
	       break;
	     else
	       s++;
	  }

	if (NULL != s)
	  Num_Spoilers_Visible++;
	else
	  {
	     Num_Spoilers_Visible = 1;
	     /* un-spoiler until we hit another formfeed */
	     do
	       {
		  l->flags &= ~SPOILER_LINE;
		  l = l->next;
	       }
	     while ((l != NULL) && (NULL == slrn_strbyte (l->buf, 12)));
	  }
     }

   if (Slrn_Spoiler_Display_Mode & 1)
     {
	Slrn_Current_Article->cline = l1;
	find_article_line_num ();
     }

   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

#endif

/*}}}*/

/*{{{ hide/toggle quotes */

/* This function does not update the line number */
static void hide_or_unhide_quotes (void) /*{{{*/
{
   if (Slrn_Current_Article == NULL)
     return;

   if (Slrn_Current_Article->quotes_hidden)
     _slrn_art_hide_quotes (Slrn_Current_Article, 1);
   else
     _slrn_art_unhide_quotes (Slrn_Current_Article);
}

/*}}}*/

static void toggle_quotes (void) /*{{{*/
{
   Slrn_Article_Type *a = Slrn_Current_Article;

   if (a == NULL)
     return;

   if (Slrn_Prefix_Arg_Ptr != NULL)
     {
	Slrn_Quotes_Hidden_Mode = *Slrn_Prefix_Arg_Ptr + 1;
	Slrn_Prefix_Arg_Ptr = NULL;
	_slrn_art_hide_quotes (Slrn_Current_Article, 0);
     }
   else if (a->quotes_hidden)
     _slrn_art_unhide_quotes (a);
   else
     _slrn_art_hide_quotes (a, 1);

   Slrn_Quotes_Hidden_Mode = Slrn_Current_Article->quotes_hidden;

   find_article_line_num ();
}

/*}}}*/

/*}}}*/

int slrn_is_hidden_headers_mode ()
{
   return Headers_Hidden_Mode;
}

static void toggle_headers (void) /*{{{*/
{
   Slrn_Article_Type *a = Slrn_Current_Article;

   if (a == NULL)
     return;

   if (a->headers_hidden)
     {
	_slrn_art_unhide_headers (a);
	a->cline = a->lines;
	find_article_line_num ();
	Headers_Hidden_Mode = 0;
	return;
     }

   _slrn_art_hide_headers (a);
   Headers_Hidden_Mode = 1;
}

/*}}}*/

static void toggle_signature (void) /*{{{*/
{
   Slrn_Article_Type *a = Slrn_Current_Article;

   if (a == NULL)
     return;

   if (a->signature_hidden)
     _slrn_art_unhide_signature (a);
   else _slrn_art_hide_signature (a);
   find_article_line_num ();

   Slrn_Signature_Hidden = a->signature_hidden;
}

/*}}}*/

static void toggle_pgp_signature (void) /*{{{*/
{
   Slrn_Article_Type *a = Slrn_Current_Article;

   if (a == NULL)
     return;

   if (a->pgp_signature_hidden)
     _slrn_art_unhide_pgp_signature (a);
   else _slrn_art_hide_pgp_signature (a);
   find_article_line_num ();

   Slrn_Pgp_Signature_Hidden = a->pgp_signature_hidden;
}

/*}}}*/

static void toggle_verbatim_marks (void) /*{{{*/
{
   Slrn_Article_Type *a = Slrn_Current_Article;

   if (a == NULL)
     return;

   a->verbatim_marks_hidden = !a->verbatim_marks_hidden;
   Slrn_Verbatim_Marks_Hidden = a->verbatim_marks_hidden;
}
/*}}}*/

static void toggle_verbatim (void) /*{{{*/
{
   Slrn_Article_Type *a = Slrn_Current_Article;

   if (a == NULL)
     return;

   if (a->verbatim_hidden)
     _slrn_art_unhide_verbatim (a);
   else
     _slrn_art_hide_verbatim (a);

   find_article_line_num();
   Slrn_Verbatim_Hidden = a->verbatim_hidden;
}
/*}}}*/
/*}}}*/

/*{{{ leave/suspend article mode and support functions */
static void update_ranges (void) /*{{{*/
{
   NNTP_Artnum_Type bmin, bmax;
   unsigned int is_read;
   Slrn_Range_Type *r_new;
   Slrn_Header_Type *h;

   if (User_Aborted_Group_Read) return;

   h = Slrn_First_Header;
   /* skip articles for which the numeric id was not available */
   while ((h != NULL) && (h->number < 0)) h = h->real_next;
   if (h == NULL) return;

   /* We do our work on a copy of the ranges first; this way, we can
    * find out whether we need to mark the group dirty later. */
   r_new = slrn_ranges_clone (Current_Group->range.next);

   /* Mark old (unavailable) articles as read */
   if (Slrn_Server_Min > 1)
     r_new = slrn_ranges_add (r_new, 1, Slrn_Server_Min - 1);

   /* Now, mark blocks of articles read / unread */
   is_read = h->flags & HEADER_READ;
   bmin = bmax = h->number;
   while (h != NULL)
     {
	h = h->real_next;
	if ((h==NULL) || (h->number > bmax+1) ||
	    (is_read != (h->flags & HEADER_READ)))
	  {
	     if (is_read)
	       r_new = slrn_ranges_add (r_new, bmin, bmax);
	     else
	       r_new = slrn_ranges_remove (r_new, bmin, bmax);
	     if (h!=NULL)
	       {
		  bmin = bmax = h->number;
		  is_read = h->flags & HEADER_READ;
	       }
	  }
	else
	  bmax++;
     }

   if (slrn_ranges_compare(r_new, Current_Group->range.next))
     Slrn_Groups_Dirty = 1;

   /* Finally delete the old ranges and replace them with the new one */
   slrn_ranges_free (Current_Group->range.next);
   Current_Group->range.next = r_new;
   slrn_group_recount_unread (Current_Group);
}
/*}}}*/

static void update_requests (void) /*{{{*/
{
#if SLRN_HAS_SPOOL_SUPPORT
   NNTP_Artnum_Type bmin, bmax;
   unsigned int is_req;
   Slrn_Range_Type *r = Current_Group->requests;
   Slrn_Header_Type *h;

   if ((User_Aborted_Group_Read)||
       (Slrn_Server_Id != SLRN_SERVER_ID_SPOOL))
     return;

   h = Slrn_First_Header;
   /* skip articles for which the numeric id was not available */
   while ((h != NULL) && (h->number < 0)) h = h->real_next;
   if (h == NULL) return;

   /* Mark old (unavailable) articles as unrequested */
   r = slrn_ranges_remove (r, 1, Slrn_Server_Min - 1);

   /* Update the requested article ranges list. */
   is_req = h->flags & HEADER_REQUEST_BODY;
   bmin = bmax = h->number;
   while (h != NULL)
     {
	h = h->real_next;
	if ((h==NULL) || (h->number > bmax+1) ||
	    (is_req != (h->flags & HEADER_REQUEST_BODY)))
	  {
	     if (is_req)
	       r = slrn_ranges_add (r, bmin, bmax);
	     else
	       r = slrn_ranges_remove (r, bmin, bmax);
	     if (h!=NULL)
	       {
		  bmin = bmax = h->number;
		  is_req = h->flags & HEADER_REQUEST_BODY;
	       }
	  }
	else
	  bmax++;
     }

   if ((-1 == slrn_spool_set_requested_ranges (Slrn_Current_Group_Name, r)) &&
       (r != NULL)) /* if r == NULL, don't bother user */
     slrn_error_now (2, _("Warning: Could not save list of requested bodies."));
   Current_Group->requests = r;
#endif
}
/*}}}*/

/*{{{ art_quit */
static void art_quit (void) /*{{{*/
{
   Slrn_Header_Type *h = _art_Headers;

   (void) slrn_run_hooks (HOOK_ARTICLE_MODE_QUIT, 0);

   slrn_init_hangup_signals (0);

#if SLRN_HAS_GROUPLENS
   if (Slrn_Use_Group_Lens) slrn_put_grouplens_scores ();
#endif

   free_article ();

   free_kill_lists_and_update ();
   free_tag_list ();

   slrn_close_score ();
   slrn_clear_requested_headers ();

   if (h != NULL)
     {
	update_ranges ();
	update_requests ();
	free_all_headers ();
     }

   Slrn_First_Header = _art_Headers = Slrn_Current_Header = NULL;
   SLMEMSET ((char *) &Slrn_Header_Window, 0, sizeof (SLscroll_Window_Type));
   Total_Num_Headers = 0;

   Current_Group = NULL;
   Last_Read_Header = NULL;
   *Output_Filename = 0;

   Same_Subject_Start_Header = NULL;
   Slrn_Current_Group_Name = NULL;

   /* Since this function may get called before the mode is pushed,
    * only pop it if the mode is really article mode.
    */
   if ((Slrn_Current_Mode != NULL)
       && (Slrn_Current_Mode->mode == SLRN_ARTICLE_MODE))
     slrn_pop_mode ();

   slrn_write_newsrc (1); /* calls slrn_init_hangup_signals (1);*/
}

/*}}}*/

/*}}}*/

static void skip_to_next_group_1 (void)
{
   art_quit ();
   slrn_select_next_group ();
}

static void skip_to_next_group (void) /*{{{*/
{
   int tmp = Slrn_Startup_With_Article;
   Slrn_Startup_With_Article = 0;

   art_quit ();
   slrn_select_next_group ();
   if ((Slrn_Current_Mode != NULL) &&
       (Slrn_Current_Mode->mode == SLRN_ARTICLE_MODE))
     {
	if ((Slrn_Current_Header->flags & (HEADER_READ|HEADER_WITHOUT_BODY)) &&
	    (0 == slrn_next_unread_header (1)))
	  slrn_clear_message ();
	else if (tmp)
	  art_pagedn ();
     }

   Slrn_Startup_With_Article = tmp;
}

/*}}}*/

static void skip_to_prev_group (void) /*{{{*/
{
   art_quit ();
   slrn_select_prev_group ();
}

/*}}}*/

static void fast_quit (void) /*{{{*/
{
   art_quit ();
   slrn_group_quit ();
}

/*}}}*/

static void art_suspend_cmd (void) /*{{{*/
{
   int rows = SLtt_Screen_Rows;
   slrn_suspend_cmd ();
   if (rows != SLtt_Screen_Rows)
     art_winch_sig (rows, -1);
}

/*}}}*/

/*}}}*/
/*{{{ art_xpunge */
static void art_xpunge (void) /*{{{*/
{
   Slrn_Header_Type *save, *next, *h;

   free_article ();
   free_kill_lists_and_update ();

   save = _art_Headers;
   if (_art_Headers != NULL)
     {
	update_ranges ();
     }

   /* If there are no unread headers, quit to group mode. */
   while (_art_Headers != NULL)
     {
	if ((0 == (_art_Headers->flags & HEADER_READ)) ||
	    (_art_Headers->flags & HEADER_DONT_DELETE_MASK))
	  break;
	_art_Headers = _art_Headers->next;
     }

   if (_art_Headers == NULL)
     {
	_art_Headers = save;
	art_quit ();
	return;
     }

   /* Remove numerical tags from all read headers. */
   if ((Num_Tag_List.len != 0)
       && (Num_Tag_List.headers != NULL))
     {
	unsigned int i, j;
	Slrn_Header_Type *th;

	j = 0;
	for (i = 0; i < Num_Tag_List.len; i++)
	  {
	     th = Num_Tag_List.headers[i];
	     if ((th->flags & HEADER_READ) &&
		 (0 == (th->flags & HEADER_DONT_DELETE_MASK)))
	       {
		  th->tag_number = 0;
		  th->flags &= ~HEADER_NTAGGED;
		  continue;
	       }

	     Num_Tag_List.headers [j] = th;
	     j++;
	     th->tag_number = j;
	  }
	Num_Tag_List.len = j;
     }

   /* Check if Last_Read_Header and Mark_Header are unread */
   if ((Last_Read_Header != NULL) &&
       (Last_Read_Header->flags & HEADER_READ) &&
       (0 == (Last_Read_Header->flags & HEADER_DONT_DELETE_MASK)))
     Last_Read_Header = NULL;

   if ((Mark_Header != NULL) &&
       (Mark_Header->flags & HEADER_READ) &&
       (0 == (Mark_Header->flags & HEADER_DONT_DELETE_MASK)))
     Mark_Header = NULL;

   /* Find an unread message for Slrn_Current_Header; we made sure that
    * at least one unread message exists, so this can never fail. */
   next = Slrn_Current_Header;
   while (next != NULL)
     {
	if ((0 == (next->flags & HEADER_READ)) ||
	    (next->flags & HEADER_DONT_DELETE_MASK))
	  break;
	next = next->next;
     }

   if (next == NULL)
     {
	next = Slrn_Current_Header;
	while (next != NULL)
	  {
	     if ((0 == (next->flags & HEADER_READ)) ||
		 (next->flags & HEADER_DONT_DELETE_MASK))
	       break;
	     next = next->prev;
	  }
     }

   Slrn_Current_Header = next; /* cannot be NULL (see above) */

   /* Free all headers up to the first unread one; set Slrn_First_Header
    * to it. */
   h = Slrn_First_Header; /* Slrn_First_Header != NULL */
   while (1)
     {
	next = h->real_next;
	if ((0 == (h->flags & HEADER_READ)) ||
	    (h->flags & HEADER_DONT_DELETE_MASK))
	  break;
	free_killed_header (h);
	h = next;
     }
   Slrn_First_Header = h;
   h->real_prev = NULL;

   /* Free the rest of the read headers, linking up the unread ones. */
   while (h != NULL)
     {
	Slrn_Header_Type *next_next;

	next = h->real_next;
	while (next != NULL)
	  {
	     next_next = next->real_next;
	     if ((0 == (next->flags & HEADER_READ)) ||
		 (next->flags & HEADER_DONT_DELETE_MASK))
	       break;
	     free_killed_header (next);
	     next = next_next;
	  }
	h->real_next = next;
	if (next != NULL)
	  next->real_prev = h;
	h = next;
     }

   /* Fix the prev / next linking */
   h = _art_Headers = Slrn_First_Header;
   _art_Headers->prev = NULL;

   while (h != NULL)
     {
	h->next = next = h->real_next;
	if (next != NULL)
	  {
	     next->prev = h;
	  }
	h = next;
     }

   slrn_sort_headers ();
   slrn_write_newsrc (1);

   Slrn_Full_Screen_Update = 1;
}
/*}}}*/

/*}}}*/
/*{{{ cancel_article */

static void cancel_article (void) /*{{{*/
{
   char *me, *msgid, *newsgroups, *dist;
#if SLRN_HAS_CANLOCK
   char *can_key;
#endif
   char from_buf[512], me_buf[512], *from;
   Slrn_Mime_Error_Obj *err;

   if ((-1 == slrn_check_batch ()) ||
       (-1 == select_affected_article ()))
     return;
   slrn_update_screen ();

   /* TO cancel, we post a cancel message with a 'control' header.  First, check to
    * see if this is really the owner of the message.
    */

   from = slrn_extract_header ("From: ", 6);
   if (from != NULL) (void) parse_from (from, from_buf, sizeof(from_buf));
   else from[0] = '\0';

   if (NULL == (me = slrn_make_from_header ())) return;
   (void) parse_from (me+6, me_buf, sizeof(me_buf));

   if (slrn_case_strcmp ( from_buf,  me_buf))
     {
        slrn_error (_("Failed: Your name: '%s' is not '%s'"), me_buf, from_buf);
        slrn_free(me);
	return;
     }

   if (slrn_get_yesno (0, _("Are you sure that you want to cancel this article")) <= 0)
     {
	slrn_free(me);
	return;
     }

   slrn_message_now (_("Cancelling..."));

   if ((err = slrn_mime_header_encode(&me, Slrn_Display_Charset)) != NULL)
     {
	slrn_error (_("error during From: encoding: %s"), err->msg);
	slrn_free(me);
	slrn_free_mime_error(err);
	return;
     }

   if (NULL == (newsgroups = slrn_extract_header ("Newsgroups: ", 12)))
     newsgroups = "";

   if ((NULL == (msgid = slrn_extract_header ("Message-ID: ", 12))) ||
       (0 == *msgid))
     {
	slrn_error (_("No message id."));
	slrn_free(me);
	return;
     }

   dist = slrn_extract_header("Distribution: ", 14);

   if (Slrn_Post_Obj->po_start () < 0) return;

   Slrn_Post_Obj->po_printf ("%s\nNewsgroups: %s\nSubject: cmsg cancel %s\nControl: cancel %s\n",
			     me, newsgroups, msgid, msgid);

   slrn_free(me);

   if ((dist != NULL) && (*dist != 0))
     {
	Slrn_Post_Obj->po_printf ("Distribution: %s\n", dist);
     }

#if SLRN_HAS_CANLOCK
   if (NULL != (can_key = gen_cancel_key(msgid)))
     {
	Slrn_Post_Obj->po_printf ("Cancel-Key: %s\n", can_key);
	SLFREE (can_key);
     }
#endif

   Slrn_Post_Obj->po_printf("\nignore\nArticle cancelled by slrn %s\n", Slrn_Version_String);

   if (0 == Slrn_Post_Obj->po_end ())
     {
	slrn_message (_("Done."));
     }
}

/*}}}*/

/*}}}*/

/*{{{ header/thread (un)deletion/(un)catchup */
static void delete_header (Slrn_Header_Type *h) /*{{{*/
{
   if (h->flags & HEADER_DONT_DELETE_MASK) return;
   if (0 == (h->flags & HEADER_READ))
     {
	kill_cross_references (h);
	h->flags |= HEADER_READ;
	Number_Read++;
     }
}

/*}}}*/

static void undelete_header (Slrn_Header_Type *h) /*{{{*/
{
   if (h->flags & HEADER_READ)
     {
	h->flags &= ~HEADER_READ;
	Number_Read--;
     }
}

/*}}}*/

void slrn_request_header (Slrn_Header_Type *h) /*{{{*/
{
   if (0 == (h->flags & HEADER_WITHOUT_BODY)) return;
   if (h->number < 0)
     {
	slrn_error (_("Warning: Can only request article bodies from this group."));
	return;
     }
   h->flags |= HEADER_REQUEST_BODY;
   undelete_header (h);
}
/*}}}*/

void slrn_unrequest_header (Slrn_Header_Type *h) /*{{{*/
{
   h->flags &= ~HEADER_REQUEST_BODY;
}
/*}}}*/

static void catch_up_all (void) /*{{{*/
{
   if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_CATCHUP)
       && slrn_get_yesno (1, _("Mark all articles as read")) <= 0)
     return;
   for_all_headers (delete_header, 1);
}

/*}}}*/

static void un_catch_up_all (void) /*{{{*/
{
   if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_CATCHUP)
       && slrn_get_yesno (1, _("Mark all articles as unread")) <= 0)
     return;
   for_all_headers (undelete_header, 1);
}

/*}}}*/

static void catch_up_to_here (void) /*{{{*/
{
   if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_CATCHUP)
       && slrn_get_yesno (1, _("Mark all articles up to here as read")) <= 0)
     return;
   for_all_headers (delete_header, 0);
}

/*}}}*/

static void un_catch_up_to_here (void) /*{{{*/
{
   if ((Slrn_User_Wants_Confirmation & SLRN_CONFIRM_CATCHUP)
       && slrn_get_yesno (1, _("Mark all articles up to here as unread")) <= 0)
     return;
   for_all_headers (undelete_header, 0);
}

/*}}}*/

static void undelete_header_cmd (void) /*{{{*/
{
   if ((Slrn_Current_Header->parent != NULL)/* in middle of thread */
       || (Slrn_Current_Header->child == NULL)/* At top with no child */
       /* or at top with child showing */
       || (0 == (Slrn_Current_Header->child->flags & HEADER_HIDDEN)))
     {
	undelete_header (Slrn_Current_Header);
     }
   else
     {
	for_this_tree (Slrn_Current_Header, undelete_header);
     }
   slrn_header_down_n (1, 0);
   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

static void delete_header_cmd (void) /*{{{*/
{
   if ((Slrn_Current_Header->parent != NULL)/* in middle of thread */
       || (Slrn_Current_Header->child == NULL)/* At top with no child */
       /* or at top with child showing */
       || (0 == (Slrn_Current_Header->child->flags & HEADER_HIDDEN)))
     {
	delete_header (Slrn_Current_Header);
     }
   else
     {
	for_this_tree (Slrn_Current_Header, delete_header);
     }
   slrn_next_unread_header (0);
   Slrn_Full_Screen_Update = 1;
}

/*}}}*/

static void request_header_cmd (void) /*{{{*/
{
   if ((Slrn_Current_Header->parent != NULL)/* in middle of thread */
       || (Slrn_Current_Header->child == NULL)/* At top with no child */
       /* or at top with child showing */
       || (0 == (Slrn_Current_Header->child->flags & HEADER_HIDDEN)))
     {
	if (Slrn_Current_Header->flags & HEADER_REQUEST_BODY)
	  slrn_unrequest_header (Slrn_Current_Header);
	else
	  slrn_request_header (Slrn_Current_Header);
     }
   else
     {
	/* Unrequest only if all bodies in the thread were requested */
	Slrn_Header_Type *h=Slrn_Current_Header, *next;
	next = h->sister;
	while (h != next)
	  {
	     if ((h->flags & HEADER_WITHOUT_BODY) &&
		 !(h->flags & HEADER_REQUEST_BODY))
	       {
		  for_this_tree (Slrn_Current_Header, slrn_request_header);
		  break;
	       }
	     h = h->next;
	  }
	if (h == next)
	  for_this_tree (Slrn_Current_Header, slrn_unrequest_header);
     }
   slrn_header_down_n (1, 0);
   Slrn_Full_Screen_Update = 1;
}
/*}}}*/

static void thread_delete_cmd (void) /*{{{*/
{
   for_this_tree (Slrn_Current_Header, delete_header);
   delete_header_cmd ();
}

/*}}}*/

/*}}}*/

/*{{{ group_lens functions */
#if SLRN_HAS_GROUPLENS
static void grouplens_rate_article (void) /*{{{*/
{
   int ch;

   if ((Slrn_Current_Header == NULL)
       || (Num_GroupLens_Rated == -1))
     return;

   slrn_message_now (_("Rate article (1-5):"));
#if 0
   ch = SLang_getkey ();
#else
   ch = slrn_getkey ();
#endif
   if ((ch < '1') || (ch > '5'))
     {
	slrn_error (_("Rating must be in range 1 to 5."));
	return;
     }

   slrn_group_lens_rate_article (Slrn_Current_Header, ch - '0',
				 (Article_Visible && (Header_Showing == Slrn_Current_Header)));
}

/*}}}*/

#endif
/*}}}*/
/*{{{ mouse commands */

/* actions for different regions:
 *	- top status line (help)
 *	- header status line
 *	- above header status line
 *	- below header status line
 *	- bottom status line
 */
static void art_mouse (void (*top_status)(void), /*{{{*/
		       void (*header_status)(void),
		       void (*bot_status)(void),
		       void (*normal_region)(void)
		       )
{
   int r, c;
   slrn_get_mouse_rc (&r, &c);

   /* take top status line into account */
   if (r == 1)
     {
	if (Slrn_Use_Mouse)
	  (void) slrn_execute_menu (c);
	else if (NULL != top_status) (*top_status) ();
 	return;
     }

   if (r >= SLtt_Screen_Rows)
     return;

   /* On header status line */
   if (r - 2 == Header_Window_Nrows)
     {
	if (NULL != header_status) (*header_status) ();
 	return;
     }

   /* bottom status line */
   if (r == SLtt_Screen_Rows - 1)
     {
	if (NULL != bot_status) (*bot_status) ();
	return;
     }

   if (r - 2 > Header_Window_Nrows)
     {
	if (Slrn_Highlight_Urls && (c < 255))
	  {
	     SLsmg_Char_Type buf[256];
	     char line[512];
	     char *url = NULL;
	     unsigned int len, i;

	     SLsmg_gotorc (r - 1, 0);
	     len = SLsmg_read_raw (buf, 256);

	     i = (unsigned int) c;
	     while (c && (SLSMG_EXTRACT_COLOR(buf[--c]) == URL_COLOR))
	       {
		  line[c] = (char) SLSMG_EXTRACT_CHAR(buf[c]);
		  url = line + c;
	       }
	     while ((i < len) && (SLSMG_EXTRACT_COLOR(buf[i]) == URL_COLOR))
	       {
		  line[i] = (char) SLSMG_EXTRACT_CHAR(buf[i]);
		  i++;
	       }

	     line[i] = '\0';

	     if (url != NULL)
	       {
		  launch_url (url, (normal_region != hide_article));
		  /* Hack: Don't ask when middle mouse key is used */
		  return;
	       }
	  }

	if (NULL != normal_region) (*normal_region) ();
 	return;
     }

   r -= (1 + Last_Cursor_Row);
   if (r < 0)
     {
	r = -r;
	if (r != (int) slrn_header_up_n (r, 0)) return;
     }
   else if (r != (int) slrn_header_down_n (r, 0)) return;

   select_article (0);
   /* if (NULL != normal_region) (*normal_region) (); */

}

/*}}}*/

static void art_mouse_left (void) /*{{{*/
{
   art_mouse (slrn_article_help, header_pagedn,
	      art_next_unread, art_pagedn);
}

/*}}}*/

static void art_mouse_middle (void) /*{{{*/
{
   art_mouse (toggle_header_formats, hide_article,
	      toggle_quotes, hide_article);
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

/*}}}*/

static void art_mouse_right (void) /*{{{*/
{
   art_mouse (slrn_article_help, header_pageup,
	      art_prev_unread, art_pageup);
}

/*}}}*/

/*}}}*/

/*{{{ slrn_init_article_mode */

#define A_KEY(s, f)  {s, (int (*)(void)) f}

static SLKeymap_Function_Type Art_Functions [] = /*{{{*/
{
   A_KEY("article_bob", art_bob),
   A_KEY("article_eob", art_eob),
   A_KEY("article_left", art_left),
   A_KEY("article_line_down", art_linedn),
   A_KEY("article_line_up", art_lineup),
   A_KEY("article_page_down", art_pagedn),
   A_KEY("article_page_up", art_pageup),
   A_KEY("article_right", art_right),
   A_KEY("article_search", article_search),
   A_KEY("author_search_backward", author_search_backward),
   A_KEY("author_search_forward", author_search_forward),
   A_KEY("browse_url", browse_url),
   A_KEY("cancel", cancel_article),
   A_KEY("catchup", catch_up_to_here),
   A_KEY("catchup_all", catch_up_all),
   A_KEY("create_score", create_score),
#if SLRN_HAS_DECODE
   A_KEY("decode", decode_article),
#endif
   A_KEY("delete", delete_header_cmd),
   A_KEY("delete_thread", thread_delete_cmd),
   A_KEY("digit_arg", slrn_digit_arg),
   A_KEY("enlarge_article_window", enlarge_window),
   A_KEY("evaluate_cmd", slrn_evaluate_cmd),
   A_KEY("exchange_mark", exchange_mark),
   A_KEY("expunge", art_xpunge),
   A_KEY("fast_quit", fast_quit),
   A_KEY("followup", followup),
   A_KEY("forward", forward_article),
   A_KEY("forward_digest", skip_digest_forward),
   A_KEY("get_children_headers", get_children_headers),
   A_KEY("get_parent_header", get_parent_header),
#if SLRN_HAS_GROUPLENS
   A_KEY("grouplens_rate_article", grouplens_rate_article),
#endif
   A_KEY("goto_article", goto_article),
   A_KEY("goto_last_read", goto_last_read),
   A_KEY("header_bob", header_bob),
   A_KEY("header_eob", header_eob),
   A_KEY("header_line_down", header_down),
   A_KEY("header_line_up", header_up),
   A_KEY("header_page_down", header_pagedn),
   A_KEY("header_page_up", header_pageup),
   A_KEY("help", slrn_article_help),
   A_KEY("hide_article", hide_article),
   A_KEY("locate_article", locate_header_by_msgid),
   A_KEY("mark_spot", mark_spot),
   A_KEY("next", art_next_unread),
   A_KEY("next_high_score", next_high_score),
   A_KEY("next_same_subject", next_header_same_subject),
   A_KEY("pipe", pipe_article),
   A_KEY("post", slrn_post_cmd),
   A_KEY("post_postponed", slrn_post_postponed),
   A_KEY("previous", art_prev_unread),
   A_KEY("print", print_article_cmd),
   A_KEY("quit", art_quit),
   A_KEY("redraw", slrn_redraw),
   A_KEY("repeat_last_key", slrn_repeat_last_key),
   A_KEY("reply", reply_cmd),
   A_KEY("request", request_header_cmd),
   A_KEY("save", save_article),
#if SLRN_HAS_SPOILERS
   A_KEY("show_spoilers", show_spoilers),
#endif
   A_KEY("shrink_article_window", shrink_window),
   A_KEY("skip_quotes", skip_quoted_text),
   A_KEY("skip_to_next_group", skip_to_next_group_1),
   A_KEY("skip_to_previous_group", skip_to_prev_group),
   A_KEY("subject_search_backward", subject_search_backward),
   A_KEY("subject_search_forward", subject_search_forward),
   A_KEY("supersede", supersede),
   A_KEY("suspend", art_suspend_cmd),
   A_KEY("tag_header", num_tag_header),
   A_KEY("toggle_collapse_threads", toggle_collapse_threads),
   A_KEY("toggle_header_formats", toggle_header_formats),
   A_KEY("toggle_header_tag", toggle_header_tag),
   A_KEY("toggle_headers", toggle_headers),
   A_KEY("toggle_pgpsignature", toggle_pgp_signature),
   A_KEY("toggle_quotes", toggle_quotes),
   A_KEY("toggle_rot13", toggle_rot13),
   A_KEY("toggle_signature", toggle_signature),
   A_KEY("toggle_verbatim_text", toggle_verbatim),
   A_KEY("toggle_verbatim_marks", toggle_verbatim_marks),
   A_KEY("uncatchup", un_catch_up_to_here),
   A_KEY("uncatchup_all", un_catch_up_all),
   A_KEY("undelete", undelete_header_cmd),
   A_KEY("untag_headers", num_untag_headers),
   A_KEY("wrap_article", toggle_wrap_article),
   A_KEY("zoom_article_window", zoom_article_window),
   A_KEY("view_scores", view_scores),
#if 1 /* FIXME: These ones are going to be deleted before 1.0 */
   A_KEY("art_bob", art_bob),
   A_KEY("art_eob", art_eob),
   A_KEY("art_xpunge", art_xpunge),
   A_KEY("article_linedn", art_linedn),
   A_KEY("article_lineup", art_lineup),
   A_KEY("article_pagedn", art_pagedn),
   A_KEY("article_pageup", art_pageup),
   A_KEY("down", header_down),
   A_KEY("enlarge_window", enlarge_window),
   A_KEY("goto_beginning", art_bob),
   A_KEY("goto_end", art_eob),
   A_KEY("left", art_left),
   A_KEY("locate_header_by_msgid", locate_header_by_msgid),
   A_KEY("pagedn", header_pagedn),
   A_KEY("pageup", header_pageup),
   A_KEY("pipe_article", pipe_article),
   A_KEY("prev", art_prev_unread),
   A_KEY("print_article", print_article_cmd),
   A_KEY("right", art_right),
   A_KEY("scroll_dn", art_pagedn),
   A_KEY("scroll_up", art_pageup),
   A_KEY("shrink_window", shrink_window),
   A_KEY("skip_to_prev_group", skip_to_prev_group),
   A_KEY("toggle_show_author", toggle_header_formats),
   A_KEY("toggle_sort", _art_toggle_sort),
   A_KEY("up", header_up),
#endif
   A_KEY(NULL, NULL)
};

/*}}}*/

static Slrn_Mode_Type Art_Mode_Cap = /*{{{*/
{
   NULL,			       /* keymap */
   art_update_screen,
   art_winch_sig,		       /* sigwinch_fun */
   slrn_art_hangup,
   NULL,			       /* enter_mode_hook */
   SLRN_ARTICLE_MODE,
};

/*}}}*/

void slrn_init_article_mode (void) /*{{{*/
{
   char  *err = _("Unable to create Article keymap!");
   char numbuf[2];
   char ch;

   if (NULL == (Slrn_Article_Keymap = SLang_create_keymap ("article", NULL)))
     slrn_exit_error ("%s", err);

   Art_Mode_Cap.keymap = Slrn_Article_Keymap;

   Slrn_Article_Keymap->functions = Art_Functions;

   numbuf[1] = 0;

   for (ch = '0'; ch <= '9'; ch++)
     {
	numbuf[0] = ch;
	SLkm_define_key (numbuf, (FVOID_STAR) goto_header_number, Slrn_Article_Keymap);
     }
#if SLRN_HAS_GROUPLENS
   numbuf[0] = '0';
   /* Steal '0' for use as a prefix for rating. */
   SLkm_define_key  (numbuf, (FVOID_STAR) grouplens_rate_article, Slrn_Article_Keymap);
#endif

   SLkm_define_key  ("\033l", (FVOID_STAR) locate_header_by_msgid, Slrn_Article_Keymap);
   SLkm_define_key ("\0331", (FVOID_STAR) slrn_digit_arg, Slrn_Article_Keymap);
   SLkm_define_key ("\0332", (FVOID_STAR) slrn_digit_arg, Slrn_Article_Keymap);
   SLkm_define_key ("\0333", (FVOID_STAR) slrn_digit_arg, Slrn_Article_Keymap);
   SLkm_define_key ("\0334", (FVOID_STAR) slrn_digit_arg, Slrn_Article_Keymap);
   SLkm_define_key ("\0335", (FVOID_STAR) slrn_digit_arg, Slrn_Article_Keymap);
   SLkm_define_key ("\0336", (FVOID_STAR) slrn_digit_arg, Slrn_Article_Keymap);
   SLkm_define_key ("\0337", (FVOID_STAR) slrn_digit_arg, Slrn_Article_Keymap);
   SLkm_define_key ("\0338", (FVOID_STAR) slrn_digit_arg, Slrn_Article_Keymap);
   SLkm_define_key ("\0339", (FVOID_STAR) slrn_digit_arg, Slrn_Article_Keymap);
   SLkm_define_key ("\0330", (FVOID_STAR) slrn_digit_arg, Slrn_Article_Keymap);
   SLkm_define_key  ("*", (FVOID_STAR) toggle_header_tag, Slrn_Article_Keymap);
   SLkm_define_key  ("#", (FVOID_STAR) num_tag_header, Slrn_Article_Keymap);
   SLkm_define_key  ("\033#", (FVOID_STAR) num_untag_headers, Slrn_Article_Keymap);
#if SLRN_HAS_DECODE
   SLkm_define_key  (":", (FVOID_STAR) decode_article, Slrn_Article_Keymap);
#endif
   SLkm_define_key  (" ", (FVOID_STAR) art_pagedn, Slrn_Article_Keymap);
   SLkm_define_key  ("!", (FVOID_STAR) next_high_score, Slrn_Article_Keymap);
   SLkm_define_key  (",", (FVOID_STAR) exchange_mark, Slrn_Article_Keymap);
   SLkm_define_key  (".", (FVOID_STAR) slrn_repeat_last_key, Slrn_Article_Keymap);
   SLkm_define_key  ("/", (FVOID_STAR) article_search, Slrn_Article_Keymap);
   SLkm_define_key  ("\\", (FVOID_STAR) toggle_signature, Slrn_Article_Keymap);
   SLkm_define_key  ("{", (FVOID_STAR) toggle_verbatim, Slrn_Article_Keymap);
   SLkm_define_key  ("[", (FVOID_STAR) toggle_verbatim_marks, Slrn_Article_Keymap);
   SLkm_define_key  ("]", (FVOID_STAR) toggle_pgp_signature, Slrn_Article_Keymap);
   SLkm_define_key  (";", (FVOID_STAR) mark_spot, Slrn_Article_Keymap);
   SLkm_define_key  ("<", (FVOID_STAR) art_bob, Slrn_Article_Keymap);
   SLkm_define_key  ("=", (FVOID_STAR) next_header_same_subject, Slrn_Article_Keymap);
   SLkm_define_key  (">", (FVOID_STAR) art_eob, Slrn_Article_Keymap);
   SLkm_define_key  ("?", (FVOID_STAR) slrn_article_help, Slrn_Article_Keymap);
   SLkm_define_key  ("A", (FVOID_STAR) author_search_backward, Slrn_Article_Keymap);
   SLkm_define_key  ("F", (FVOID_STAR) forward_article, Slrn_Article_Keymap);
   SLkm_define_key  ("H", (FVOID_STAR) hide_article, Slrn_Article_Keymap);
   SLkm_define_key  ("K", (FVOID_STAR) create_score, Slrn_Article_Keymap);
   SLkm_define_key  ("L", (FVOID_STAR) goto_last_read, Slrn_Article_Keymap);
   SLkm_define_key  ("N", (FVOID_STAR) skip_to_next_group_1, Slrn_Article_Keymap);
   SLkm_define_key  ("P", (FVOID_STAR) slrn_post_cmd, Slrn_Article_Keymap);
   SLkm_define_key  ("Q", (FVOID_STAR) fast_quit, Slrn_Article_Keymap);
   SLkm_define_key  ("S", (FVOID_STAR) subject_search_backward, Slrn_Article_Keymap);
   SLkm_define_key  ("T", (FVOID_STAR) toggle_quotes, Slrn_Article_Keymap);
   SLkm_define_key  ("U", (FVOID_STAR) browse_url, Slrn_Article_Keymap);
   SLkm_define_key  ("W", (FVOID_STAR) toggle_wrap_article, Slrn_Article_Keymap);
   SLkm_define_key  ("\033^C", (FVOID_STAR) cancel_article, Slrn_Article_Keymap);
   SLkm_define_key  ("\033^P", (FVOID_STAR) get_children_headers, Slrn_Article_Keymap);
   SLkm_define_key  ("\033^S", (FVOID_STAR) supersede, Slrn_Article_Keymap);
   SLkm_define_key  ("\033a", (FVOID_STAR) toggle_header_formats, Slrn_Article_Keymap);
   SLkm_define_key  ("\033d", (FVOID_STAR) thread_delete_cmd, Slrn_Article_Keymap);
   SLkm_define_key  ("\033p", (FVOID_STAR) get_parent_header, Slrn_Article_Keymap);
   SLkm_define_key  ("\033S", (FVOID_STAR) _art_toggle_sort, Slrn_Article_Keymap);
   SLkm_define_key  ("\033t", (FVOID_STAR) toggle_collapse_threads, Slrn_Article_Keymap);
   SLkm_define_key  ("\r", (FVOID_STAR) art_linedn, Slrn_Article_Keymap);
   SLkm_define_key  ("\t", (FVOID_STAR) skip_quoted_text, Slrn_Article_Keymap);
   SLkm_define_key  ("^L", (FVOID_STAR) slrn_redraw, Slrn_Article_Keymap);
   SLkm_define_key  ("^P", (FVOID_STAR) header_up, Slrn_Article_Keymap);
   SLkm_define_key  ("^R", (FVOID_STAR) slrn_redraw, Slrn_Article_Keymap);
   SLkm_define_key  ("^X^[", (FVOID_STAR) slrn_evaluate_cmd, Slrn_Article_Keymap);
   SLkm_define_key  ("^Z", (FVOID_STAR) art_suspend_cmd, Slrn_Article_Keymap);
   SLkm_define_key  ("a", (FVOID_STAR) author_search_forward, Slrn_Article_Keymap);
   SLkm_define_key  ("b", (FVOID_STAR) art_pageup, Slrn_Article_Keymap);
   SLkm_define_key  ("d", (FVOID_STAR) delete_header_cmd, Slrn_Article_Keymap);
   SLkm_define_key  ("f", (FVOID_STAR) followup, Slrn_Article_Keymap);
   SLkm_define_key  ("g", (FVOID_STAR) skip_digest_forward, Slrn_Article_Keymap);
   SLkm_define_key  ("j", (FVOID_STAR) goto_article, Slrn_Article_Keymap);
   SLkm_define_key  ("m", (FVOID_STAR) request_header_cmd, Slrn_Article_Keymap);
   SLkm_define_key  ("n", (FVOID_STAR) art_next_unread, Slrn_Article_Keymap);
   SLkm_define_key  ("o", (FVOID_STAR) save_article, Slrn_Article_Keymap);
   SLkm_define_key  ("p", (FVOID_STAR) art_prev_unread, Slrn_Article_Keymap);
   SLkm_define_key  ("q", (FVOID_STAR) art_quit, Slrn_Article_Keymap);
   SLkm_define_key  ("r", (FVOID_STAR) reply_cmd, Slrn_Article_Keymap);
   SLkm_define_key  ("s", (FVOID_STAR) subject_search_forward, Slrn_Article_Keymap);
   SLkm_define_key  ("t", (FVOID_STAR) toggle_headers, Slrn_Article_Keymap);
   SLkm_define_key  ("u", (FVOID_STAR) undelete_header_cmd, Slrn_Article_Keymap);
   SLkm_define_key  ("v", (FVOID_STAR) view_scores, Slrn_Article_Keymap);
   SLkm_define_key  ("x", (FVOID_STAR) art_xpunge, Slrn_Article_Keymap);
   SLkm_define_key  ("y", (FVOID_STAR) print_article_cmd, Slrn_Article_Keymap);
   SLkm_define_key  ("|", (FVOID_STAR) pipe_article, Slrn_Article_Keymap);
#if defined(IBMPC_SYSTEM)
   SLkm_define_key  ("^@S", (FVOID_STAR) art_pageup, Slrn_Article_Keymap);
   SLkm_define_key  ("\xE0S", (FVOID_STAR) art_pageup, Slrn_Article_Keymap);
#else
   SLkm_define_key  ("^?", (FVOID_STAR) art_pageup, Slrn_Article_Keymap);
#endif
#if defined(IBMPC_SYSTEM)
   SLkm_define_key  ("\033^@H", (FVOID_STAR) art_lineup, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\xE0H", (FVOID_STAR) art_lineup, Slrn_Article_Keymap);
   SLkm_define_key  ("\033^@P", (FVOID_STAR) art_linedn, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\xE0P", (FVOID_STAR) art_linedn, Slrn_Article_Keymap);
   SLkm_define_key  ("\033^@M", (FVOID_STAR) skip_to_next_group_1, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\xE0M", (FVOID_STAR) skip_to_next_group_1, Slrn_Article_Keymap);
   SLkm_define_key  ("\033^@K", (FVOID_STAR) skip_to_prev_group, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\xE0K", (FVOID_STAR) skip_to_prev_group, Slrn_Article_Keymap);
   SLkm_define_key  ("^@H", (FVOID_STAR) header_up, Slrn_Article_Keymap);
   SLkm_define_key  ("\xE0H", (FVOID_STAR) header_up, Slrn_Article_Keymap);
   SLkm_define_key  ("^@P", (FVOID_STAR) header_down, Slrn_Article_Keymap);
   SLkm_define_key  ("\xE0P", (FVOID_STAR) header_down, Slrn_Article_Keymap);
   SLkm_define_key  ("^@M", (FVOID_STAR) art_right, Slrn_Article_Keymap);
   SLkm_define_key  ("\xE0M", (FVOID_STAR) art_right, Slrn_Article_Keymap);
   SLkm_define_key  ("^@K", (FVOID_STAR) art_left, Slrn_Article_Keymap);
   SLkm_define_key  ("\xE0K", (FVOID_STAR) art_left, Slrn_Article_Keymap);
#else
   SLkm_define_key  ("\033\033[A", (FVOID_STAR) art_lineup, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\033OA", (FVOID_STAR) art_lineup, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\033[B", (FVOID_STAR) art_linedn, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\033OB", (FVOID_STAR) art_linedn, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\033[C", (FVOID_STAR) skip_to_next_group_1, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\033OC", (FVOID_STAR) skip_to_next_group_1, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\033[D", (FVOID_STAR) skip_to_prev_group, Slrn_Article_Keymap);
   SLkm_define_key  ("\033\033OD", (FVOID_STAR) skip_to_prev_group, Slrn_Article_Keymap);
   SLkm_define_key  ("\033[A", (FVOID_STAR) header_up, Slrn_Article_Keymap);
   SLkm_define_key  ("\033OA", (FVOID_STAR) header_up, Slrn_Article_Keymap);
   SLkm_define_key  ("\033[B", (FVOID_STAR) header_down, Slrn_Article_Keymap);
   SLkm_define_key  ("\033OB", (FVOID_STAR) header_down, Slrn_Article_Keymap);
   SLkm_define_key  ("\033[C", (FVOID_STAR) art_right, Slrn_Article_Keymap);
   SLkm_define_key  ("\033OC", (FVOID_STAR) art_right, Slrn_Article_Keymap);
   SLkm_define_key  ("\033[D", (FVOID_STAR) art_left, Slrn_Article_Keymap);
   SLkm_define_key  ("\033OD", (FVOID_STAR) art_left, Slrn_Article_Keymap);
#endif
   SLkm_define_key  ("^U", (FVOID_STAR) header_pageup, Slrn_Article_Keymap);
   SLkm_define_key  ("\033V", (FVOID_STAR) header_pageup, Slrn_Article_Keymap);
#if defined(IBMPC_SYSTEM)
   SLkm_define_key  ("^@I", (FVOID_STAR) header_pageup, Slrn_Article_Keymap);
   SLkm_define_key  ("\xE0I", (FVOID_STAR) header_pageup, Slrn_Article_Keymap);
   SLkm_define_key  ("^@Q", (FVOID_STAR) header_pagedn, Slrn_Article_Keymap);
   SLkm_define_key  ("\xE0Q", (FVOID_STAR) header_pagedn, Slrn_Article_Keymap);
#else
   SLkm_define_key  ("\033[5~", (FVOID_STAR) header_pageup, Slrn_Article_Keymap);
   SLkm_define_key  ("\033[I", (FVOID_STAR) header_pageup, Slrn_Article_Keymap);
   SLkm_define_key  ("\033[6~", (FVOID_STAR) header_pagedn, Slrn_Article_Keymap);
   SLkm_define_key  ("\033[G", (FVOID_STAR) header_pagedn, Slrn_Article_Keymap);
#endif
   SLkm_define_key  ("^D", (FVOID_STAR) header_pagedn, Slrn_Article_Keymap);
   SLkm_define_key  ("^V", (FVOID_STAR) header_pagedn, Slrn_Article_Keymap);
   SLkm_define_key  ("\033>", (FVOID_STAR) header_eob, Slrn_Article_Keymap);
   SLkm_define_key  ("\033<", (FVOID_STAR) header_bob, Slrn_Article_Keymap);
   SLkm_define_key  ("c", (FVOID_STAR) catch_up_all, Slrn_Article_Keymap);
   SLkm_define_key  ("\033c", (FVOID_STAR) catch_up_all, Slrn_Article_Keymap);
   SLkm_define_key  ("\033u", (FVOID_STAR) un_catch_up_all, Slrn_Article_Keymap);
   SLkm_define_key  ("\033C", (FVOID_STAR) catch_up_to_here, Slrn_Article_Keymap);
   SLkm_define_key  ("C", (FVOID_STAR) catch_up_to_here, Slrn_Article_Keymap);
   SLkm_define_key  ("\033U", (FVOID_STAR) un_catch_up_to_here, Slrn_Article_Keymap);
   SLkm_define_key  ("\033R", (FVOID_STAR) toggle_rot13, Slrn_Article_Keymap);
#if 1
#if SLRN_HAS_SPOILERS
   SLkm_define_key  ("\033?", (FVOID_STAR) show_spoilers, Slrn_Article_Keymap);
#endif
#endif
   SLkm_define_key  ("^N", (FVOID_STAR) header_down, Slrn_Article_Keymap);
   SLkm_define_key  ("^", (FVOID_STAR) enlarge_window, Slrn_Article_Keymap);
   SLkm_define_key  ("^^", (FVOID_STAR) shrink_window, Slrn_Article_Keymap);
   SLkm_define_key  ("\033P", (FVOID_STAR) slrn_post_postponed, Slrn_Article_Keymap);

   /* mouse (left/middle/right) */
   SLkm_define_key  ("\033[M\040", (FVOID_STAR) art_mouse_left, Slrn_Article_Keymap);
   SLkm_define_key  ("\033[M\041", (FVOID_STAR) art_mouse_middle, Slrn_Article_Keymap);
   SLkm_define_key  ("\033[M\042", (FVOID_STAR) art_mouse_right, Slrn_Article_Keymap);

   SLkm_define_key  ("z", (FVOID_STAR) zoom_article_window, Slrn_Article_Keymap);

   if (SLang_get_error ()) slrn_exit_error ("%s", err);
}

/*}}}*/

/*}}}*/
/*{{{ slrn_article_mode and support functions */

static void slrn_art_hangup (int sig) /*{{{*/
{
   (void) sig;
   if (Slrn_Current_Header != NULL)
     undelete_header_cmd ();		       /* in case we are reading one */
   art_quit ();
}

/*}}}*/

static void mark_ranges (Slrn_Range_Type *r, int flag) /*{{{*/
{
   Slrn_Header_Type *h = Slrn_First_Header;

   while ((r != NULL) && (h != NULL))
     {
	NNTP_Artnum_Type min = r->min;
	NNTP_Artnum_Type max = r->max;
	while (h != NULL)
	  {
	     if (h->number < min)
	       {
		  h = h->real_next;
		  continue;
	       }

	     if (h->number > max)
	       {
		  break;
	       }

	     if ((flag != HEADER_REQUEST_BODY) ||
		 (h->flags & HEADER_WITHOUT_BODY))
	       h->flags |= flag;
	     if (flag == HEADER_READ)
	       Number_Read++;
	     h = h->real_next;
	  }
	r = r->next;
     }
}

/*}}}*/

void slrn_set_header_flags (Slrn_Header_Type *h, unsigned int flags) /*{{{*/
{
   if (h->flags & HEADER_READ) Number_Read--;
   if (flags & HEADER_READ) Number_Read++;
   h->flags &= ~HEADER_HARMLESS_FLAGS_MASK;
   h->flags |= (flags & HEADER_HARMLESS_FLAGS_MASK);
}
/*}}}*/

/* If all > 0, get last 'all' headers from server independent of whether
 *             they have been read or not.
 * If all < 0, and this is not the first time this group has been accessed,
 *             either during this session or during previous sessions, get
 *             that last 'all' UNREAD articles.
 * Otherwise,  fetch ALL UNREAD headers from the server.
 */
int slrn_select_article_mode (Slrn_Group_Type *g, NNTP_Artnum_Type all, int score) /*{{{*/
{
   NNTP_Artnum_Type min, max;
   NNTP_Artnum_Type smin, smax;
   Slrn_Range_Type *r;
   int status;

   slrn_init_graphic_chars ();

   Header_Window_HScroll = 0;
   User_Aborted_Group_Read = 0;
   _art_Headers = Slrn_First_Header = NULL;
   _art_Threads_Collapsed = 0;
   Same_Subject_Start_Header = NULL;
   Number_Read = 0;
   Number_Total = 0;

   init_scoring ();
   delete_hash_table ();
   Number_Killed = 0;

   Current_Group = g;
   r = &g->range;
   Slrn_Current_Group_Name = g->group_name;

   if (Slrn_Server_Obj->sv_reset_has_xover)
     Slrn_Server_Obj->sv_has_xover = 1;

   slrn_run_hooks (HOOK_PRE_ARTICLE_MODE, 0);
   if (SLang_get_error ())
     return -1;

   if (score && (1 == slrn_open_score (Slrn_Current_Group_Name)))
     Perform_Scoring = 1;
   else Perform_Scoring = 0;

   Slrn_Server_Min = r->min;
   Slrn_Server_Max = r->max;
   r = r->next;
   /* Now r points to ranges already read.  */

   status = 0;

   if (all > 0)
     {
	min = Slrn_Server_Max - all + 1;
	if (min < Slrn_Server_Min) min = Slrn_Server_Min;
	status = get_headers (min, Slrn_Server_Max, &all);
	if (status != -1)
	  mark_ranges (r, HEADER_READ);
     }
   else
     {
	if ((all < 0) && (r != NULL))
	  {
	     NNTP_Artnum_Type unread;

	     /* This condition will occur when the user wants to read unread
	      * articles that occur in a gap, i.e., RRRUUUUURRRUUUUUUU and
	      * we need to dig back far enough below the last group of read
	      * ones until we have retrieved abs(all) articles.
	      *
	      * The problem with this is that some articles may not be
	      * available on the server which means that the number to
	      * go back will be under estimated.
	      */
	     all = -all;

	     while (r->next != NULL) r = r->next;
	     /* Go back through previously read articles counting unread.
	      * If number unread becomes greater than the number that we
	      * intend to read, then we know where to start querying
	      * the server.
	      */
	     unread = 0;
	     max = Slrn_Server_Max;
	     while (r->prev != NULL)
	       {
		  unread += max - r->max;
		  if (unread >= all) break;
		  max = r->min - 1;
		  r = r->prev;
	       }
	     if (r->prev == NULL)
	       unread += max;

	     if (unread >= all)
	       {
		  /* This may be problematic if some articles are missing on
		   * the server.  If that is the case, smin will be to high
		   * and we will fall short of the goal.
		   */
		  if (r->prev != NULL)
		    smin = r->max + (unread - all) + 1;
		  else
		    smin = unread - all + 1;
	       }
	     else smin = Slrn_Server_Min;
	     smax = Slrn_Server_Max;
	     r = r->next;
	  }
	else
	  {
	     /* all == 0, or no previously read articles. */
	     smin = Slrn_Server_Min;
	     smax = Slrn_Server_Max;
	     if (r != NULL)
	       {
		  Slrn_Range_Type *r1;

		  /* Estimate how many are available to read */
		  all = smax - r->max;
#if 0				       /* is this correct?? */
		  all++;
#endif

		  /* Now subtract the ones that we have already read. */
		  r1 = r->next;
		  while (r1 != NULL)
		    {
		       all -= (r1->max - r1->min) + 1;
		       r1 = r1->next;
		    }
		  /* This condition should never arise */
		  if (all == 0) all = smax - smin + 1;
	       }
	     else all = smax - smin + 1;
	  }

	while (r != NULL)
	  {
	     if (r->min > smin)
	       {
		  min = smin;
		  max = r->min - 1;

		  status = get_headers (min, max, &all);

		  if (status == -1)
		    break;

		  if (status == 0)
		    {
		       Slrn_Groups_Dirty = 1;
		       r->min = min;
		    }

		  smin = r->max + 1;
	       }
	     else
	       {
		  smin = r->max + 1;
	       }
	     r = r->next;
	  }

	if (smin <= smax)
	  {
	     status = get_headers (smin, smax, &all);
	  }
     }

   slrn_close_add_xover (1);

#if SLRN_HAS_SPOOL_SUPPORT
   if (Slrn_Server_Id == SLRN_SERVER_ID_SPOOL)
     {
	r = slrn_spool_get_no_body_ranges (Slrn_Current_Group_Name);
	mark_ranges (r, HEADER_WITHOUT_BODY);
	slrn_ranges_free (r);

	slrn_ranges_free (g->requests);
	r = slrn_spool_get_requested_ranges (Slrn_Current_Group_Name);
	mark_ranges (r, HEADER_REQUEST_BODY);
	g->requests = r;
	g->requests_loaded = 1;
     }
#endif

   if ((status == -1) || SLKeyBoard_Quit)
     {
	if (SLang_get_error () == USER_BREAK)
	  slrn_error_now (0, _("Group transfer aborted."));
	else
	  slrn_error_now (0, _("Server read failed."));

	/* This means that we cannot update ranges for this group because
	 * the user aborted and update_ranges assumes that all articles
	 * upto server max are present.
	 */
	User_Aborted_Group_Read = 1;
	art_quit ();
     }
   else if (Perform_Scoring)
     score_headers (1);

   if (_art_Headers == NULL)
     {
	slrn_close_score ();
	slrn_clear_requested_headers ();
	if (Number_Killed)
	  slrn_error (_("No unread articles found. (%d killed)"), Number_Killed);
	else
	  slrn_error (_("No unread articles found."));

	free_kill_lists_and_update ();
	Slrn_Current_Group_Name = NULL;
	if ((SLang_get_error () == USER_BREAK) || (all != 0)) return -1;
	else return -2;
     }

   make_hash_table ();

   /* This must go here to fix up the next/prev pointers */
   _art_sort_by_server_number ();

   slrn_push_mode (&Art_Mode_Cap);

   Slrn_Current_Header = _art_Headers;
   Last_Cursor_Row = 0;
   Mark_Header = NULL;
   init_header_window_struct ();

   At_End_Of_Article = NULL;
   Header_Showing = NULL;
   SLMEMSET ((char *) &Slrn_Article_Window, 0, sizeof (SLscroll_Window_Type));

   set_article_visibility (0);

#if SLRN_HAS_GROUPLENS
   /* slrn_set_suspension (1); */
   Num_GroupLens_Rated = slrn_get_grouplens_scores ();
   /* slrn_set_suspension (0); */
#endif

   (void) slrn_run_hooks (HOOK_ARTICLE_MODE, 0);
   slrn_sort_headers ();
   header_bob ();

   quick_help ();

   (void) slrn_run_hooks (HOOK_ARTICLE_MODE_STARTUP, 0);

   if (Slrn_Startup_With_Article) art_pagedn ();
   if (SLang_get_error () == 0)
     {
	if (Perform_Scoring
	    /* && (Number_Killed || Number_High_Scored) */
	    )
	  {
#if SLRN_HAS_GROUPLENS
	     if (Num_GroupLens_Rated != -1)
	       {
		  slrn_message (_("Num Killed: %u, Num High: %u, Num Low: %u, Num GroupLens Rated: %d"),
				Number_Score_Killed, Number_High_Scored, Number_Low_Scored,
				Num_GroupLens_Rated);
	       }
	     else
#endif
	       slrn_message (_("Num Killed: %u, Num High: %u, Num Low: %u"),
			     Number_Score_Killed, Number_High_Scored, Number_Low_Scored);
	  }

	else slrn_clear_message ();
     }

/*   Number_Low_Scored = Number_Score_Killed = Number_High_Scored = 0;*/
   return 0;
}

/*}}}*/

/*}}}*/

/*{{{ screen update functions */

static char *Header_Display_Formats [SLRN_MAX_DISPLAY_FORMATS];
static unsigned int Header_Format_Number;
static char *Default_Header_Format = "%F%B%-5S%G%-5l:[%12r]%t%s";

int slrn_set_header_format (unsigned int num, char *fmt)
{
   return slrn_set_display_format (Header_Display_Formats, num, fmt);
}

char *slrn_get_header_format (int num)
{
   char *fmt;

   if ((num < 0) || (num >= SLRN_MAX_DISPLAY_FORMATS))
     num = (int) Header_Format_Number;

   fmt = Header_Display_Formats[num];
   if ((fmt == NULL) || (*fmt == 0))
     fmt = Default_Header_Format;

   return fmt;
}

static void toggle_header_formats (void)
{
   Header_Format_Number = slrn_toggle_format (Header_Display_Formats,
					      Header_Format_Number);
}

static void smg_write_string (char *s)
{
   slrn_write_nbytes (s, strlen (s));
}

static void smg_write_byte (char c)
{
   slrn_write_nbytes (&c, 1);
}

#if SLRN_HAS_EMPHASIZED_TEXT
static void smg_write_emphasized_text (char *str, int color0)
{
   char *s, *url = NULL;
   char ch;
   int url_len = Slrn_Highlight_Urls ? 0 : -1;

   if ((Slrn_Emphasized_Text_Mode == 0) && (Slrn_Highlight_Urls == 0))
     {
	smg_write_string (str);
	return;
     }

   s = str;

   while ((ch = *s) != 0)
     {
	char *s0;
	int color;
	char ch0;

	if ((url_len != -1) && (url < str) &&
	    (NULL == (url = find_url (s, (unsigned int*) &url_len))))
	  url_len = -1;

	if ((url != NULL) && (s >= url))
	  {
	     slrn_write_nbytes (str, (unsigned int) (url - str));
	     slrn_set_color (URL_COLOR);
	     slrn_write_nbytes (url, url_len);
	     slrn_set_color (color0);
	     s = str = url + url_len;
	     url = NULL;
	     continue;
	  }

	if (Slrn_Emphasized_Text_Mode == 0)
	  {
	     if (url_len == -1)
	       {
		  s = str + strlen (str);
		  break;
	       }
	     else
	       {
		  s++;
		  continue;
	       }
	  }

	if ((ch != '_') && (ch != '*') && (ch != '/'))
	  {
	     s++;
	     continue;
	  }
	/*To handle these cases:
	 *   bla bla _bla bla_ bla
	 *   bla bla (_bla_) bla.
	 * So, all these possibilities:
	 *
	 *      [^a-z0-9_]_[a-z0-9] ... [a-z0-9,]_[^a-z0-9]
	 */

	if (s > str)
	  {
	     ch0 = s[-1];
	     if (isalnum (ch0)
		 || (ch0 == ch))
	       {
		  s++;
		  continue;
	       }
	  }
	ch0 = s[1];
	if (0 == isalnum (ch0))
	  {
	     s++;
	     continue;
	  }

	/* Now look for the end */

	s0 = s;
	ch0 = ch;
	s++;
	while ((ch = *s) != 0)
	  {
	     if ((ch == '_') || (ch == '*') || (ch == '/'))
	       break;

	     s++;
	  }

	if (ch0 != ch)
	  continue;

	/* We have a possible match. */
	ch0 = s[-1];
	if ((0 == isalnum (ch0))
	    && (0 == ispunct (ch0)))
	  continue;

	ch0 = s[1];
	if (isalnum (ch0)
	    || (ch0 == ch))
	  continue;

	/* Now, we have s0 at _bla bla and s at _ whatever */

	slrn_write_nbytes (str, (unsigned int) (s0 - str));
	str = s + 1;

	if (ch == '_')
	  color = UNDERLINE_COLOR;
	else if (ch == '*')
	  color = BOLD_COLOR;
	else /* if (ch == '/') */
	  color = ITALICS_COLOR;

	switch (Slrn_Emphasized_Text_Mode)
	  {
	   case 1:  /* Strip _ characters */
	     slrn_set_color (color);
	     s0++;
	     slrn_write_nbytes (s0, (unsigned int) (s-s0));
	     slrn_set_color (color0);
	     break;

	   case 2:		       /* substitute space */
	     ch = ' ';
	     slrn_write_nbytes (&ch, 1);
	     slrn_set_color (color);
	     s0++;
	     slrn_write_nbytes (s0, (unsigned int) (s-s0));
	     slrn_set_color (color0);
	     slrn_write_nbytes (&ch, 1);
	     break;

	   case 3:		       /* write as is */
	   default:
	     slrn_set_color (color);
	     s++;
	     slrn_write_nbytes (s0, (unsigned int)(s-s0));
	     slrn_set_color (color0);
	  }

	s = str;
     }
   slrn_write_nbytes (str, (unsigned int)(s - str));
}
#endif				       /* SLRN_HAS_EMPHASIZED_TEXT */

static void quick_help (void) /*{{{*/
{
   char *msg;

   if (Slrn_Batch) return;

   if (Article_Visible == 0)
     {
	msg = _("SPC:Select  Ctrl-D:PgDn  Ctrl-U:PgUp  d:Mark-as-Read  n:Next  p:Prev  q:Quit");
	if (Slrn_Header_Help_Line != NULL) msg = Slrn_Header_Help_Line;
     }
   else
     {
	msg = _("SPC:Pgdn  B:PgUp  u:Un-Mark-as-Read  f:Followup  n:Next  p:Prev  q:Quit");
	if (Slrn_Art_Help_Line != NULL) msg = Slrn_Art_Help_Line;
     }

   if (0 == slrn_message ("%s", msg))
     Slrn_Message_Present = 0;
}

/*}}}*/

static char Rot13buf[256];

static void init_rot13 (void) /*{{{*/
{
   static int is_initialized;
   if (is_initialized == 0)
     {
	int i;
	is_initialized = 1;
	for (i = 0; i < 256; i++)
	  {
	     Rot13buf[i] = i;
	  }

	for (i = 'A'; i <= 'M'; i++)
	  {
	     Rot13buf[i] = i + 13;
	     /* Now take care of lower case ones */
	     Rot13buf[i + 32] =  i + 32 + 13;
	  }

	for (i = 'N'; i <= 'Z'; i++)
	  {
	     Rot13buf[i] = i - 13;
	     /* Now take care of lower case ones */
	     Rot13buf[i + 32] =  i + 32 - 13;
	  }
     }
}
/*}}}*/

static void write_rot13 (unsigned char *buf, int color, int use_emph) /*{{{*/
{
   char *buf_rot13;

   init_rot13();

   if (NULL == (buf_rot13 = slrn_strmalloc ((char *)buf, 1)))
     return;

   decode_rot13 ((unsigned char *)buf_rot13);
#if SLRN_HAS_EMPHASIZED_TEXT
   if (use_emph)
     smg_write_emphasized_text (buf_rot13, color);
   else
#endif
     smg_write_string (buf_rot13);

   slrn_free(buf_rot13);
}
/*}}}*/

/* rot13-"decodes" buf (overwriting its content) */
static void decode_rot13 (unsigned char *buf) /*{{{*/
{
   init_rot13();

   while (*buf != 0)
     {
	*buf = Rot13buf[*buf];
	buf++;
     }
}
/*}}}*/

/*{{{ utility routines */

#if SLRN_HAS_SPOILERS
/* write out the line, replacing all printable chars with '*' */
static void write_spoiler (char *buf, int first_line) /*{{{*/
{
   char ch;

   Spoilers_Visible = Header_Showing;

   if (Slrn_Spoiler_Char == ' ')
     return;

   if (first_line)
     {
	char *s, n;

	n = Num_Spoilers_Visible;
	s = buf;

	while (n-- != 0)
	  {
	     if (NULL == (s = slrn_strbyte (s, 12)))
	       break;
	     else
	       s++;
	  }

	if (NULL == s)
	  {
	     smg_write_string (buf);
	     return;
	  }

	slrn_write_nbytes (buf, s - buf);
	buf = s;
     }

   while ((ch = *buf++) != 0)
     {
	if (!isspace(ch)) ch = Slrn_Spoiler_Char;
	slrn_write_nbytes (&ch, 1);
     }
}
/*}}}*/
#endif

static void draw_tree (Slrn_Header_Type *h) /*{{{*/
{
   SLwchar_Type buf[2];

#if !defined(IBMPC_SYSTEM)
   if (Graphic_Chars_Mode == 0)
     {
	if (h->tree_ptr != NULL) smg_write_string (h->tree_ptr);
	smg_write_string ("  ");
	return;
     }
   if (Graphic_Chars_Mode == ALT_CHAR_SET_MODE)
     SLsmg_set_char_set (1);
#endif

   slrn_set_color (TREE_COLOR);

   if (h->tree_ptr != NULL) smg_write_string (h->tree_ptr);

   if (h->flags & FAKE_CHILDREN)
     {
	buf[0] = Graphic_UTee_Char;
	buf[1] = Graphic_HLine_Char;
	SLsmg_forward (-1);
	SLsmg_write_char (Graphic_ULCorn_Char);
     }
   else if ((h->sister == NULL) ||
	    ((h->sister->flags & FAKE_PARENT) && ((h->flags & FAKE_PARENT) == 0)))
     {
	buf[0] = Graphic_LLCorn_Char;
	buf[1] = Graphic_HLine_Char;
     }
   else
     {
	buf[0] = Graphic_LTee_Char;
	buf[1] = Graphic_HLine_Char;
     }
   SLsmg_write_char(buf[0]);
   SLsmg_write_char(buf[1]);

#if !defined(IBMPC_SYSTEM)
   if (Graphic_Chars_Mode == ALT_CHAR_SET_MODE) SLsmg_set_char_set (0);
#endif
}

/*}}}*/

/*{{{ check_subject */

/*
 * This checks if subjects should be printed (correctly I hope)
 * hacked: articles in a tree shouldn't display their subject if the
 *          subject is already displayed (i.e. at top)
 * To add: take more Re:'s into account (currently, one is allowed)
 */
int Slrn_Show_Thread_Subject = 0;
static int check_subject (Slrn_Header_Type *h) /*{{{*/
{
   char *psubj, *subj;

   subj = h->subject;
   psubj = h->prev->subject;	       /* used to be: h->parent->subject */
   if ((subj == NULL) || (psubj == NULL)) return 1;

   return _art_subject_cmp (subj, psubj);
}

/*}}}*/

/*}}}*/

#if SLRN_HAS_END_OF_THREAD
static int display_end_of_thread (Slrn_Header_Type *h) /*{{{*/
{
   Slrn_Header_Type *parent, *next_parent;

   if ((h == NULL)
       || (h->parent == NULL)
       || (h->child != NULL)
       || (h->next == NULL))
     return -1;

   parent = h->parent;
   while (parent->parent != NULL) parent = parent->parent;

   next_parent = h->next;
   while (next_parent->parent != NULL)
     next_parent = next_parent->parent;

   if (next_parent != parent)
     {
	if ((Header_Window_Nrows == 0)
	    && (next_parent != NULL)
	    && (next_parent->subject != NULL))
	  slrn_message (_("End of Thread.  Next: %s"), next_parent->subject);
	else
	  slrn_message (_("End of Thread."));
	return 0;
     }

   if ((h->sister == NULL)
       || (h->parent != h->next->parent)
       || (FAKE_PARENT & (h->next->flags ^ h->flags)))
     {
	/* The last test involving ^ is necessary because the two can be
	 * sisters except that one can have a fake parent.  If this is the
	 * case, we are at the end of a subthread.
	 */
	slrn_message (_("End of Sub-Thread"));
	return 0;
     }

   return -1;
}
/*}}}*/
#endif

static void disp_write_flags (Slrn_Header_Type *h)
{
   unsigned int flags = h->flags;
   int row = SLsmg_get_row ();

   /* Do not write header numbers if we are displaying at bottom of the
    * screen in the display area.  When this happens, the header window is
    * not visible.
    */
   if (row + 1 != SLtt_Screen_Rows)
     {
	if (Slrn_Use_Header_Numbers)
	  {
	     slrn_set_color (HEADER_NUMBER_COLOR);
	     SLsmg_printf ("%2d", row);
	     if (row > Largest_Header_Number) Largest_Header_Number = row;
	  }
	else smg_write_string ("  ");
     }

   if (flags & HEADER_NTAGGED)
     {
	slrn_set_color (HIGH_SCORE_COLOR);
	SLsmg_printf ("%2d",
		      h->tag_number);
     }
   else
     {
	if ((flags & HEADER_HIGH_SCORE)
	    || ((flags & FAKE_HEADER_HIGH_SCORE)
		&& (h->child != NULL)
		&& (h->child->flags & HEADER_HIDDEN)))
	  {
	     slrn_set_color (HIGH_SCORE_COLOR);
	     SLsmg_printf ("!%c",
			   ((flags & HEADER_TAGGED) ? '*': ' '));
	  }
	else
	  {
	     slrn_set_color (0);
	     SLsmg_printf (" %c",
			   ((flags & HEADER_TAGGED) ? '*': ' '));
	  }
     }
   slrn_set_color (0);

    if ((h->parent == NULL)
	&& (h->child != NULL)
	&& (h->child->flags & HEADER_HIDDEN))
      {
	 Slrn_Header_Type *next;
	 unsigned int num, num_read;

	 num = 0;
	 num_read = 0;

	 next = h->sister;
	 while (h != next)
	   {
	      num++;
	      if (h->flags & HEADER_READ)
		num_read++;
	      h = h->next;
	   }
	 if (num == num_read)
	   SLsmg_write_char ('D');
	 else if (num_read == 0)
	   SLsmg_write_char ('-');
	 else
	   SLsmg_write_char ('%');
      }
    else SLsmg_write_char ((flags & HEADER_READ) ? 'D': '-');
}

#if SLRN_HAS_SPOOL_SUPPORT
static char get_body_status (Slrn_Header_Type *h) /*{{{*/
{
   if ((h->parent == NULL) && (h->child != NULL)
       && (h->child->flags & HEADER_HIDDEN))
     {
	Slrn_Header_Type *next;
	unsigned int num=0, num_without=0, num_request=0;

	next = h->sister;
	while (h != next)
	  {
	     num++;
	     if (h->flags & HEADER_WITHOUT_BODY)
	       num_without++;
	     if (h->flags & HEADER_REQUEST_BODY)
	       num_request++;
	     h = h->next;
	  }

	if (num == num_request) return 'M';
	if (num_request) return 'm';
	if (num == num_without) return 'H';
	if (num_without) return 'h';
	return ' ';
     }
   if (h->flags & HEADER_REQUEST_BODY) return 'M';
   if (h->flags & HEADER_WITHOUT_BODY) return 'H';
   return ' ';
}
/*}}}*/
#endif

#if SLRN_HAS_GROUPLENS
# define SLRN_GROUPLENS_DISPLAY_WIDTH 5
static void disp_write_grplens (Slrn_Header_Type *h)
{
   char buf [SLRN_GROUPLENS_DISPLAY_WIDTH+1], *b, *bmax;
   int pred;

   if (Num_GroupLens_Rated == -1)
     return;

   b = buf;
   bmax = b + SLRN_GROUPLENS_DISPLAY_WIDTH;
   while (b < bmax) *b++ = ' ';
   *b = 0;

   pred = h->gl_pred;
   if (pred < 0)
     buf [SLRN_GROUPLENS_DISPLAY_WIDTH / 2] = '?';
   else
     {
	b = buf;
	while ((pred > 0) && (b < bmax))
	  {
	     pred--;
	     *b++ = '*';
	  }
     }

   slrn_set_color (GROUPLENS_DISPLAY_COLOR);
   SLsmg_write_nstring (buf, SLRN_GROUPLENS_DISPLAY_WIDTH);
   slrn_set_color (0);
}
#endif

/*}}}*/

/*}}}*/

static char *disp_get_header_subject (Slrn_Header_Type *h)
{
   int row = SLsmg_get_row ();

   if ((Slrn_Show_Thread_Subject)
       /* || (0 == h->num_children) */
       || (h->parent == NULL)
       || (row == 1)
       || (row + 1 == SLtt_Screen_Rows)/* at bottom of screen */
       || check_subject (h))
     return h->subject;

   return ">";
}

static int color_by_score (int score) /*{{{*/
{
   if (score >= Slrn_High_Score_Min) return HIGH_SCORE_COLOR;
   else if (score)
     {
	if (score > 0) return POS_SCORE_COLOR;
	else return NEG_SCORE_COLOR;
     }
   return SUBJECT_COLOR;
}
/*}}}*/

static char *display_header_cb (char ch, void *data, int *len, int *color) /*{{{*/
{
   Slrn_Header_Type *h = (Slrn_Header_Type *) data;
   static char buf[256];
   char *retval = NULL;
   int score;

   switch (ch)
     {
      case 'B':
#if SLRN_HAS_SPOOL_SUPPORT
	if (Slrn_Server_Id == SLRN_SERVER_ID_SPOOL)
	  {
	     retval = buf;
	     retval[0] = get_body_status (h);
	     retval[1] = 0;
	     *len = 1;
	  }
	else
#endif
	  retval = "";
	break;

      case 'C':
	if ((h->next != NULL) && (h->next->flags & HEADER_HIDDEN))
	  retval = "C";
	else
	  retval = " ";
	*len = 1;
	break;

      case 'F':
	disp_write_flags (h);
	break;

      case 'P':
	if (h->parent != NULL) retval = "P";
	else retval = " ";
	*len = 1;
	break;

      case 'S':
	score = ((h->child != NULL) && (h->child->flags & HEADER_HIDDEN)
		 ? h->thread_score : h->score);

	if ((color != NULL) && (Slrn_Color_By_Score & 0x1))
	  *color = color_by_score (score);
	retval = buf;
	if (score)
#ifdef HAVE_ANSI_SPRINTF
	  *len =
#endif
	  sprintf (retval, "%d", score); /* safe */
	else
	  *retval = 0;
	break;

      case 'T':
	if (((h->next == NULL) || (0 == (h->next->flags & HEADER_HIDDEN))) &&
	    ((h->parent != NULL) || (h->flags & FAKE_CHILDREN)))
	  draw_tree (h); /* field_len is ignored */
	break;

      case 'b':
	retval = buf;
	if (h->bytes < 1000)
#ifdef HAVE_ANSI_SPRINTF
	  *len =
#endif
	  sprintf (retval, "%d ", h->bytes); /* safe */
	else
	  {
	     float size = h->bytes / 1024.0;
	     char factor = 'k';
	     if (h->bytes >= 1024000)
	       {
		  size = size/1024.0;
		  factor = 'M';
	       }
	     if (size < 10)
#ifdef HAVE_ANSI_SPRINTF
	       *len =
#endif
	       sprintf (retval, "%.1f%c", size, factor); /* safe */
	     else
#ifdef HAVE_ANSI_SPRINTF
	       *len =
#endif
	       sprintf (retval, "%.0f%c", size, factor); /* safe */
	  }
	break;

      case 'c':
	if (color != NULL) *color = THREAD_NUM_COLOR;
	if (h->num_children)
	  {
	     retval = buf;
#ifdef HAVE_ANSI_SPRINTF
	     *len =
#endif
	     sprintf (buf, "%d", 1 + h->num_children); /* safe */
	  }
	else
	  {
	     retval = " ";
	     *len = 1;
	  }
	break;

      case 'l':
	retval = buf;
#ifdef HAVE_ANSI_SPRINTF
	*len =
#endif
	sprintf (retval, "%d", h->lines); /* safe */
	break;

      case 't':
	if (!_art_Headers_Threaded)
	  {
	     retval = " ";
	     *len = 1;
	  }
	else if ((h->next != NULL) && (h->next->flags & HEADER_HIDDEN))
	  {
	     retval = buf;
	     if (color != NULL) *color = THREAD_NUM_COLOR;
#ifdef HAVE_ANSI_SPRINTF
	     *len =
#endif
	     sprintf (buf, "%3d ", 1 + h->num_children); /* safe */
	  }
	else
	  {
	     smg_write_string ("    ");
	     if ((h->parent != NULL) || (h->flags & FAKE_CHILDREN))
	       draw_tree (h);
	  }
	break;

      case 'f':
	retval = h->from;
	if (retval == NULL) retval = "";
	if (color != NULL)
	  {
	     if (strcmp (Slrn_User_Info.realname,
			 (h->realname != NULL) ? h->realname : ""))
	       *color = AUTHOR_COLOR;
	     else
	       *color = FROM_MYSELF_COLOR;
	  }
	break;

      case 'r':
	retval = h->realname;
	if (retval == NULL) retval = "";
	if (color != NULL)
	  {
	     if (strcmp (Slrn_User_Info.realname, retval))
	       *color = AUTHOR_COLOR;
	     else
	       *color = FROM_MYSELF_COLOR;
	  }
	break;

      case 's':
	if (color == NULL) /* we're called for the status line */
	  {
	     retval = h->subject;
	     break;
	  }

	if ((Slrn_Color_By_Score & 0x2) &&
	    ((Slrn_Highlight_Unread != 2) || !(h->flags & HEADER_READ)))
	  {
	     score = ((h->child != NULL) && (h->child->flags & HEADER_HIDDEN)
		      ? h->thread_score : h->score);
	     *color = color_by_score (score);
	  }
	else *color = SUBJECT_COLOR;

	if (Slrn_Highlight_Unread)
	  {
	     Slrn_Header_Type *t = h, *next;

	     if ((h->parent == NULL) && (h->child != NULL)
		 && (h->child->flags & HEADER_HIDDEN))
	       next = h->sister;
	     else
	       next = h->next;

	     while (t != next)
	       {
		  if (!(t->flags & HEADER_READ))
		    {
		       if (Slrn_Highlight_Unread != 2)
			 *color += 5; /* cf. slrn.h */
		       else if (*color == SUBJECT_COLOR)
			 *color = UNREAD_SUBJECT_COLOR;
		       break;
		    }
		  t = t->next;
	       }
	  }

	retval = disp_get_header_subject (h);
	break;
#if SLRN_HAS_GROUPLENS
      case 'G':
	disp_write_grplens (h);
	break;
#endif
      case 'd':
	if (NULL == (retval = h->date))
	  retval = "";
	if (color != NULL)
	  *color = DATE_COLOR;
	break;

      case 'D':
	if (NULL == (retval = h->date))
	  retval = "";
	else
	  {
	     char *fmtstr;

	     fmtstr = Slrn_Overview_Date_Format;
	     if (fmtstr == NULL)
	       fmtstr = "%d %b %y %H:%M";

	     slrn_strftime (buf, sizeof(buf), fmtstr, retval,
			    Slrn_Use_Localtime & 0x01);
	     retval = buf;
	  }
	if (color != NULL)
	  *color = DATE_COLOR;
	break;

      case 'n':
	retval = buf;
#ifdef HAVE_ANSI_SPRINTF
	*len =
#endif
	sprintf (retval, NNTP_FMT_ARTNUM, h->number); /* safe */
	break;
     }
   return retval;
}
/*}}}*/

static void display_header_line (Slrn_Header_Type *h, int row)
{
   char *fmt = Header_Display_Formats[Header_Format_Number];

   if ((fmt == NULL) || (*fmt == 0))
     fmt = Default_Header_Format;

   slrn_custom_printf (fmt, display_header_cb, (void *) h, row, 0);
}

static void display_article_line (Slrn_Article_Line_Type *l)
{
   char *lbuf = l->buf;
   int color;
   int use_emph_mask = 0;
   int use_rot13 = 0;

   switch (l->flags & LINE_TYPE_MASK)
     {
      case HEADER_LINE:
	if ((unsigned char)*lbuf > (unsigned char) ' ')
	  {
	     lbuf = slrn_strbyte (lbuf, ':');
	     if (lbuf != NULL)
	       {
		  lbuf++;
		  slrn_set_color (SLRN_HEADER_KEYWORD_COLOR);
		  slrn_write_nbytes (l->buf, lbuf - l->buf);
	       }
	     else lbuf = l->buf;
	  }
	use_emph_mask = EMPHASIZE_HEADER;
	color = HEADER_COLOR;
	break;

      case QUOTE_LINE:
	color = QUOTE_COLOR + ((l->v.quote_level-1) % MAX_QUOTE_COLORS);
	use_emph_mask = EMPHASIZE_QUOTES;
	use_rot13 = 1;
	break;

      case SIGNATURE_LINE:
	use_emph_mask = EMPHASIZE_SIGNATURE;
	color = SIGNATURE_COLOR;
	use_rot13 = 1;
	break;

      case PGP_SIGNATURE_LINE:
	color = PGP_SIGNATURE_COLOR;
	break;
      case VERBATIM_MARK_LINE|VERBATIM_LINE:
	if (Slrn_Verbatim_Marks_Hidden)
	  lbuf = "";
	/* drop */
      case VERBATIM_LINE:
	color = VERBATIM_COLOR;
	break;
      default:
	color = ARTICLE_COLOR;
	use_emph_mask = EMPHASIZE_ARTICLE;
	use_rot13 = 1;
     }

   slrn_set_color (color);

#if SLRN_HAS_SPOILERS
   if (l->flags & SPOILER_LINE)
     {
	write_spoiler (lbuf, ((l->prev == NULL) ||
			      !(l->prev->flags & SPOILER_LINE)));
	return;
     }
#endif

   use_emph_mask &= Slrn_Emphasized_Text_Mask;

   if (Do_Rot13 && use_rot13)
     {
	write_rot13 ((unsigned char *) lbuf, color, use_emph_mask);
	return;
     }

   if (use_emph_mask)
     {
#if SLRN_HAS_EMPHASIZED_TEXT
	smg_write_emphasized_text (lbuf, color);
	return;
#endif
     }
   smg_write_string (lbuf);
}

static char *header_status_line_cb (char ch, void *data, int *len, int *color) /*{{{*/
{
   static char buf[66];
   char *retval = buf;

   *buf = 0;
   (void) data; (void) color; /* currently unused */

   switch (ch)
     {
      case 'L':
	retval = slrn_print_percent (buf, &Slrn_Header_Window, 1);
	break;
      case 'P':
	retval = slrn_print_percent (buf, &Slrn_Header_Window, 0);
	break;
      case 'T':
	if (Slrn_Current_Header != NULL)
#ifdef HAVE_ANSI_SPRINTF
	  *len =
#endif
	  sprintf (buf, "%u", 1 + Slrn_Current_Header->num_children); /* safe */
	break;
      case 'h':
#ifdef HAVE_ANSI_SPRINTF
	*len =
#endif
	  sprintf (buf, "%u", Number_High_Scored); /* safe */
	break;
      case 'k':
#ifdef HAVE_ANSI_SPRINTF
	*len =
#endif
	  sprintf (buf, "%u", Number_Score_Killed); /* safe */
	break;
      case 'l':
#ifdef HAVE_ANSI_SPRINTF
	*len =
#endif
	  sprintf (buf, "%u", Number_Low_Scored); /* safe */
	break;
      case 'n':
	retval = Slrn_Current_Group_Name;
	break;
      case 'p':
	if (Header_Window_HScroll) strcpy (buf, "<"); /* safe */
	else strcpy (buf, " "); /* safe */
	*len = 1;
	break;
      case 'r':
#ifdef HAVE_ANSI_SPRINTF
	*len =
#endif
	  sprintf (buf, "%u", Number_Read); /* safe */
	break;
      case 't':
#ifdef HAVE_ANSI_SPRINTF
	*len =
#endif
	  sprintf (buf, "%u", Number_Total); /* safe */
	break;
      case 'u':
#ifdef HAVE_ANSI_SPRINTF
	*len =
#endif
	  sprintf (buf, "%u", Number_Total - Number_Read); /* safe */
	break;
     }
   return retval;
}
/*}}}*/

int slrn_art_get_unread (void)
{
   return Number_Total - Number_Read;
}

static void update_header_window (void)
{
   Slrn_Header_Type *h;
   char *fmt = Slrn_Header_Status_Line;
   int height;
   int row;
   int last_cursor_row;
   int c0;

   height = Slrn_Header_Window.nrows;

   h = (Slrn_Header_Type *) Slrn_Header_Window.top_window_line;
   SLscroll_find_top (&Slrn_Header_Window);
   if (h != (Slrn_Header_Type *) Slrn_Header_Window.top_window_line)
     {
	Slrn_Full_Screen_Update = 1;
	h = (Slrn_Header_Type *) Slrn_Header_Window.top_window_line;
     }

   last_cursor_row = Last_Cursor_Row;

   if ((fmt == NULL) || (*fmt == 0))
     fmt = _("%p[%u/%t unread] Group: %n%-20g -- %L (%P)");
   slrn_custom_printf (fmt, header_status_line_cb, NULL, height + 1,
		       STATUS_COLOR);

   c0 = Header_Window_HScroll;
   SLsmg_set_screen_start (NULL, &c0);

   for (row = 1; row <= height; row++)
     {
	while ((h != NULL) && (h->flags & HEADER_HIDDEN))
	  h = h->next;

	if (Slrn_Full_Screen_Update
	    || (row == last_cursor_row))
	  {
	     if (h == NULL)
	       {
		  SLsmg_gotorc (row, 0);
		  slrn_set_color (0);
		  SLsmg_erase_eol ();
		  continue;
	       }

	     display_header_line (h, row);
	  }
	h = h->next;
     }
   SLsmg_set_screen_start (NULL, NULL);
}

static char *article_status_line_cb (char ch, void *data, int *len, int *color) /*{{{*/
{
   static char buf[66];
   char *retval = buf;

   (void) data; (void) color; /* currently unused */
   *buf = ' '; *(buf + 1) = '\0';

   switch (ch)
     {
      case 'H':
	if (!Slrn_Current_Article->headers_hidden) *buf = 'H';
	*len = 1;
	break;
      case 'I':
	if (!Slrn_Current_Article->pgp_signature_hidden) *buf = 'P';
	*len = 1;
	break;
      case 'L':
	retval = slrn_print_percent (buf, &Slrn_Article_Window, 1);
	break;
      case 'P':
	retval = slrn_print_percent (buf, &Slrn_Article_Window, 0);
	break;
      case 'Q':
	if (!Slrn_Current_Article->quotes_hidden) *buf = 'Q';
	*len = 1;
	break;
      case 'T':
	if (!Slrn_Current_Article->signature_hidden) *buf = 'S';
	*len = 1;
	break;
      case 'V':
	if (!Slrn_Current_Article->verbatim_hidden) *buf = 'V';
	*len = 1;
	break;
      case 'W':
	if (Slrn_Current_Article->is_wrapped) *buf = 'W';
	*len = 1;
	break;
      case 'p':
	if (Article_Window_HScroll) *buf = '<';
	*len = 1;
	break;
      case 'v':
	if (!Slrn_Current_Article->verbatim_marks_hidden) *buf = 'v';
	*len = 1;
	break;
      default:
	retval = display_header_cb (ch, (void *) Header_Showing, len, NULL);
	break;
     }

   return retval;
}
/*}}}*/

static void update_article_window (void)
{
   Slrn_Article_Line_Type *l;
   Slrn_Article_Type *a;

   if (Article_Visible == 0)
     return;

   if (NULL == (a = Slrn_Current_Article))
     return;

   if (Slrn_Current_Article->needs_sync)
     slrn_art_sync_article (Slrn_Current_Article);

   l = (Slrn_Article_Line_Type *) Slrn_Article_Window.top_window_line;

   SLscroll_find_top (&Slrn_Article_Window);

   if (Slrn_Full_Screen_Update
       || (l != a->cline)
       || (l != (Slrn_Article_Line_Type *) Slrn_Article_Window.top_window_line))
     {
	char *fmt = Slrn_Art_Status_Line;
	int row;
	int c0;
	int height;

	height = SLtt_Screen_Rows - 2;

	if ((fmt == NULL) || (*fmt == 0))
	  fmt = "%p%n : %s %-20g -- %L (%P)";

	slrn_custom_printf (fmt, article_status_line_cb, NULL, height,
			    STATUS_COLOR);

	c0 = Article_Window_HScroll;
	SLsmg_set_screen_start (NULL, &c0);
	l = a->cline;

	row = Slrn_Header_Window.nrows + 2;
	if (row == 2) row--;	       /* header window not visible */

	while (row < height)
	  {
	     SLsmg_gotorc (row, 0);

	     if (l != NULL)
	       {
		  if (l->flags & HIDDEN_LINE)
		    {
		       l = l->next;
		       continue;
		    }

		  display_article_line (l);
		  l = l->next;
	       }
	     else if (Slrn_Use_Tildes)
	       {
		  slrn_set_color (SLRN_TILDE_COLOR);
		  smg_write_byte ('~');
	       }

	     slrn_set_color (0);
	     SLsmg_erase_eol ();

	     row++;
	  }
#if 0
	if (((l == NULL)
	     || ((l->flags & SIGNATURE_LINE) && Slrn_Sig_Is_End_Of_Article))
	    && (Slrn_Current_Header == Header_Showing))
	  At_End_Of_Article = Slrn_Current_Header;
#else
	if ((l == NULL)
	    || ((l->flags & SIGNATURE_LINE) && Slrn_Sig_Is_End_Of_Article))
	  At_End_Of_Article = Header_Showing;
#endif
	SLsmg_set_screen_start (NULL, NULL);
     }
}

static void art_update_screen (void) /*{{{*/
{
   At_End_Of_Article = NULL;
#if SLRN_HAS_SPOILERS
   Spoilers_Visible = NULL;
#endif
   Largest_Header_Number = 0;

   update_header_window ();
   update_article_window ();

   if (Slrn_Use_Mouse) slrn_update_article_menu ();
   else
     slrn_update_top_status_line ();

   if (Slrn_Message_Present == 0)
     {
#if SLRN_HAS_SPOILERS
	if (Spoilers_Visible != NULL)
	  slrn_message (_("Spoilers visible!"));
	else
#endif
#if SLRN_HAS_END_OF_THREAD
	  if (Article_Visible
	      && (-1 != display_end_of_thread (Slrn_Current_Header)))
	    /* do nothing */ ;
	else
#endif
	  quick_help ();
     }

   Last_Cursor_Row = 1 + (int) Slrn_Header_Window.window_row;

   if ((Header_Window_Nrows == 0)
       && Article_Visible)
     {
	if (Slrn_Message_Present == 0)
	  display_header_line (Slrn_Current_Header, SLtt_Screen_Rows - 1);

	SLsmg_gotorc (SLtt_Screen_Rows - 1, 0);
	Slrn_Full_Screen_Update = 0;
	return;
     }

   if (Header_Window_HScroll)
     {
	int c0 = Header_Window_HScroll;
	SLsmg_set_screen_start (NULL, &c0);
     }
   display_header_line (Slrn_Current_Header, Last_Cursor_Row);
   SLsmg_set_screen_start (NULL, NULL);

   SLsmg_gotorc (Last_Cursor_Row, 0);

   slrn_set_color (CURSOR_COLOR);
#if SLANG_VERSION > 10003
   if (Slrn_Display_Cursor_Bar)
     SLsmg_set_color_in_region (CURSOR_COLOR, Last_Cursor_Row, 0, 1, SLtt_Screen_Cols);
   else
#endif
   smg_write_string ("->");

   slrn_set_color (0);

   Slrn_Full_Screen_Update = 0;
}

/*}}}*/

/*}}}*/
