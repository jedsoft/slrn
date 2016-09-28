/* -*- mode: C; mode: fold; -*- */
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

/*{{{ system include files */

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <ctype.h>
#include <slang.h>
#include "jdmacros.h"

#include "slrn.h"
#include "group.h"
#include "util.h"
#include "strutil.h"
#include "server.h"
#include "art.h"
#include "misc.h"
#include "post.h"
/* #include "clientlib.h" */
#include "startup.h"
#include "hash.h"
#include "score.h"
#include "menu.h"
#include "xover.h"
#include "print.h"

/*}}}*/

static int Art_Hide_Quote_Level = 1;
int Slrn_Wrap_Mode = 3;
int Slrn_Wrap_Method = 2;
int Slrn_Wrap_Width = -1;

static char *Super_Cite_Regexp = "^[^A-Za-z0-9]*\"\\([-_a-zA-Z/]+\\)\" == .+";

static void hide_article_lines (Slrn_Article_Type *a, unsigned int mask) /*{{{*/
{
   Slrn_Article_Line_Type *l;

   if (a == NULL)
     return;

   a->needs_sync = 1;
   l = a->lines;

   while (l != NULL)
     {
	if (l->flags & mask)
	  l->flags |= HIDDEN_LINE;

	l = l->next;
     }
}

/*}}}*/

static void unhide_article_lines (Slrn_Article_Type *a, unsigned int mask) /*{{{*/
{
   Slrn_Article_Line_Type *l;

   if (a == NULL)
     return;

   a->needs_sync = 1;
   l = a->lines;

   while (l != NULL)
     {
	if (l->flags & mask)
	  l->flags &= ~HIDDEN_LINE;

	l = l->next;
     }
}

/*}}}*/

/* The function that set the needs_sync flag article line number start with _
 */

int _slrn_art_unhide_quotes (Slrn_Article_Type *a) /*{{{*/
{
   if (a == NULL)
     return -1;

   unhide_article_lines (a, QUOTE_LINE);
   a->quotes_hidden = 0;
   return 0;
}

/*}}}*/

static unsigned char *is_matching_line (unsigned char *b, SLRegexp_Type **r) /*{{{*/
{
   unsigned int len;
   len = strlen ((char *) b);

   while (*r != NULL)
     {
	SLRegexp_Type *re;
	unsigned int match_len;

	re = *r++;
#if SLANG_VERSION < 20000
	if ((re->min_length > len)
	    || (b != SLang_regexp_match (b, len, re)))
	  continue;
	match_len = re->end_matches[0];
#else
	if (b != (unsigned char *) SLregexp_match (re, (char *) b, len))
	  continue;
	(void) SLregexp_nth_match (re, 0, NULL, &match_len);
#endif
	return b + match_len;
     }
   return NULL;
}

/*}}}*/

int _slrn_art_hide_quotes (Slrn_Article_Type *a, int reset) /*{{{*/
{
   Slrn_Article_Line_Type *l, *last;

   if (a == NULL)
     return -1;

   _slrn_art_unhide_quotes (a);

   a->needs_sync = 1;

   if (!reset)
     Art_Hide_Quote_Level = Slrn_Quotes_Hidden_Mode;

   l = a->lines;
   last = NULL;

   while (l != NULL)
     {
	if (l->flags & QUOTE_LINE)
	  {
	     if (Art_Hide_Quote_Level > 1)
	       {
		  if (l->v.quote_level + 1 >= (unsigned int) Art_Hide_Quote_Level)
		    l->flags |= HIDDEN_LINE;
	       }
	     /* Show first line of quoted material; try to pick one that
	      * actually has text on it! */
	     else
	       {
		  if (last != NULL)
		    l->flags |= HIDDEN_LINE;
		  else
		    {
		       last = l;
		       if ((l->next != NULL) && (l->next->flags & QUOTE_LINE))
			 {
			    unsigned char *str, *line = (unsigned char*)l->buf;
			    while (NULL != (str = is_matching_line
					    (line, Slrn_Ignore_Quote_Regexp)))
			      line = str;
			    if (0 == *(slrn_skip_whitespace((char*)line)))
			      {
				 l->flags |= HIDDEN_LINE;
				 last = NULL;
			      }
			 }
		    }
	       }
	  }
	else last = NULL;
	l = l->next;
     }
   a->quotes_hidden = Art_Hide_Quote_Level;
   return 0;
}

/*}}}*/

#if SLRN_HAS_SPOILERS
void slrn_art_mark_spoilers (Slrn_Article_Type *a) /*{{{*/
{
   Slrn_Article_Line_Type *l;

   if ((Slrn_Spoiler_Char == 0) || (a == NULL))
     return;

   l = a->lines;

   while ((l != NULL) && (l->flags & HEADER_LINE))
     l = l->next; /* skip header */

   while ((l != NULL) && (NULL == slrn_strbyte (l->buf, 12)))
     l = l->next; /* skip to first formfeed */

   while (l != NULL)
     {
	l->flags |= SPOILER_LINE;
	l = l->next;
     }
}

/*}}}*/
#endif

static int try_supercite (Slrn_Article_Line_Type *l) /*{{{*/
{
   Slrn_Article_Line_Type *last, *lsave;
#if SLANG_VERSION < 20000
   static unsigned char compiled_pattern_buf[256];
   static SLRegexp_Type re_buf;
   SLRegexp_Type *re;
#else
   static SLRegexp_Type *re = NULL;
#endif
   unsigned char *b;
   int count;
   char name[32];
   unsigned int len;
   unsigned int ofs;
   int ret;

#if SLANG_VERSION < 20000
   re = &re_buf;
   re->pat = (unsigned char *) Super_Cite_Regexp;
   re->buf = compiled_pattern_buf;
   re->case_sensitive = 1;
   re->buf_len = sizeof (compiled_pattern_buf);
   if ((*compiled_pattern_buf == 0) && SLang_regexp_compile (re))
     return -1;
#else
   if ((re == NULL)
       && (NULL == (re = SLregexp_compile (Super_Cite_Regexp, 0))))
     return -1;
#endif

   /* skip header --- I should look for Xnewsreader: gnus */
   while ((l != NULL) && (*l->buf != 0)) l = l->next;

   /* look at the first 15 lines on first attempt.
    * After that, scan the whole buffer looking for more citations */
   count = 15;
   lsave = l;
   ret = -1;
   while (1)
     {
	while (count && (l != NULL))
	  {
	     if ((l->flags & QUOTE_LINE) == 0)
	       {
		  if (NULL != slrn_regexp_match (re, l->buf))
		    {
		       l->flags |= QUOTE_LINE;
		       break;
		    }
	       }
	     l = l->next;
	     count--;
	  }

	if ((l == NULL) || (count == 0)) return ret;

	/* Now find out what is used for citing. */
	if (-1 == SLregexp_nth_match (re, 1, &ofs, &len))
	  return ret;

	b = (unsigned char *) l->buf + ofs;
	if (len > sizeof (name) - 2) return ret;

	ret = 0;
	strncpy (name, (char *) b, len); name[len] = 0;

	while (l != NULL)
	  {
	     unsigned char ch;

	     b = (unsigned char *) l->buf;
	     last = l;
	     l = l->next;
	     if (last->flags & QUOTE_LINE) continue;

	     b = (unsigned char *) slrn_skip_whitespace ((char *) b);

	     if (!strncmp ((char *) b, name, len)
		 && (((ch = b[len] | 0x20) < 'a')
		     || (ch > 'z')))
	       {
		  last->flags |= QUOTE_LINE;

		  while (l != NULL)
		    {
		       b = (unsigned char *) slrn_skip_whitespace (l->buf);
		       if (strncmp ((char *) b, name, len)
			   || (((ch = b[len] | 0x20) >= 'a')
			       && (ch <= 'z')))
			 break;
		       l->flags |= QUOTE_LINE;
		       l = l->next;
		    }
	       }
	  }
	count = -1;
	l = lsave;
     }
}

/*}}}*/

void slrn_art_mark_quotes (Slrn_Article_Type *a) /*{{{*/
{
   Slrn_Article_Line_Type *l;
   unsigned char *b;

   if (a == NULL)
     return;

   if (0 == try_supercite (a->lines))
     {
	/* return; */
     }

   if (Slrn_Ignore_Quote_Regexp[0] == NULL) return;

   /* skip header */
   l = a->lines;
   while ((l != NULL) && (l->flags == HEADER_LINE))
     l = l->next;

   while (l != NULL)
     {
	int level;
	b = (unsigned char *) l->buf;

	if (NULL == (b = is_matching_line (b, Slrn_Ignore_Quote_Regexp)))
	  {
	     l = l->next;
	     continue;
	  }
	l->flags |= QUOTE_LINE;
	level = 1;
	while (NULL != (b = is_matching_line (b, Slrn_Ignore_Quote_Regexp)))
	  level++;

	l->v.quote_level = level;
	l = l->next;
     }
   a->is_modified = 1;
}

/*}}}*/

void slrn_art_mark_signature (Slrn_Article_Type *a) /*{{{*/
{
   Slrn_Article_Line_Type *l;

   if (a == NULL)
     return;

   l = a->lines;
   if (l == NULL) return;
   /* go to end of article */
   while (l->next != NULL) l = l->next;

   /* skip back until a line matches the signature RegExp */

   while ((l != NULL) && (0 == (l->flags & HEADER_LINE))
	  && ((l->flags & VERBATIM_LINE) ||
	      (NULL == is_matching_line ((unsigned char *) l->buf,
					 Slrn_Strip_Sig_Regexp))))
     l = l->prev;

   if ((l == NULL) || (l->flags & HEADER_LINE)) return;

   while (l != NULL)
     {
        l->flags |= SIGNATURE_LINE;
        l->flags &= ~(
		      QUOTE_LINE  /* if in a signature, not a quote */
#if SLRN_HAS_SPOILERS
		      | SPOILER_LINE     /* not a spoiler */
#endif
		      );
	l = l->next;
     }
}

/*}}}*/

/* The input line is assumed to be the first wrapped portion of a line.  For
 * example, if a series of lines denoted as A/B is wrapped: A0/A1/A2/B0/B1,
 * then to unwrap A, A1 is passed and B0 is returned.
 */
static Slrn_Article_Line_Type *unwrap_line (Slrn_Article_Type *a, /*{{{*/
					    Slrn_Article_Line_Type *l) /*{{{*/
{
   char *b;
   Slrn_Article_Line_Type *next, *ll;

   a->needs_sync = 1;
   a->is_modified = 1;

   ll = l->prev;
   b = ll->buf;
   do
     {
	b += strlen (b);
	/* skip the space at beginning of the wrapped line: */
	strcpy (b, l->buf + 1); /* safe */
	next = l->next;
	slrn_free ((char *)l->buf);
	slrn_free ((char *)l);
	if (l == a->cline) a->cline = ll;
	l = next;
     }
   while ((l != NULL) && (l->flags & WRAPPED_LINE));

   ll->next = l;
   if (l != NULL) l->prev = ll;
   return l;
}

/*}}}*/

/*}}}*/

int _slrn_art_unwrap_article (Slrn_Article_Type *a) /*{{{*/
{
   Slrn_Article_Line_Type *l;

   if (a == NULL)
     return -1;

   a->needs_sync = 1;
   a->is_modified = 1;
   l = a->lines;

   while (l != NULL)
     {
	if (l->flags & WRAPPED_LINE)
	  l = unwrap_line (a, l);      /* cannot fail */
	else l = l->next;
     }
   a->is_wrapped = 0;
   return 0;
}

/*}}}*/

int _slrn_art_wrap_article (Slrn_Article_Type *a) /*{{{*/
{
   unsigned char *buf, ch;
   Slrn_Article_Line_Type *l;
   unsigned int wrap_mode = Slrn_Wrap_Mode;
   int wrap_width = Slrn_Wrap_Width;

   if ((wrap_width < 5) || (wrap_width > SLtt_Screen_Cols))
     wrap_width = SLtt_Screen_Cols;

   if (a == NULL)
     return -1;

   a->is_modified = 1;
   a->needs_sync = 1;

   if (-1 == _slrn_art_unwrap_article (a))
     return -1;

   l = a->lines;

#if SLANG_VERSION >= 20000
   SLsmg_gotorc(0,0); /* used by Slsmg_strbytes later on */
#endif

   while (l != NULL)
     {
	unsigned char header_char_delimiter = 0;

	if (l->flags & HEADER_LINE)
	  {
	     if ((wrap_mode & 1) == 0)
	       {
		  l = l->next;
		  continue;
	       }

	     if (0 == slrn_case_strncmp ("Path: ", l->buf, 6))
	       header_char_delimiter = '!';
	     else if (0 == slrn_case_strncmp ( "Newsgroups: ", l->buf, 12))
	       header_char_delimiter = ',';
	  }
	else if (l->flags & QUOTE_LINE)
	  {
	     if ((wrap_mode & 2) == 0)
	       {
		  l = l->next;
		  continue;
	       }
	  }

	buf = (unsigned char *) l->buf;
	ch = *buf;
	while (ch != 0)
	  {
	     unsigned int bytes = SLsmg_strbytes (buf, buf+strlen((char *)buf),
						  (unsigned int) wrap_width);
	     buf += bytes;
	     while ((*buf == ' ') || (*buf == '\t'))
	       buf++;
	     if (*buf) /* we need to wrap */
	       {
		  Slrn_Article_Line_Type *new_l;
		  unsigned char *buf0, *lbuf;

		  /* Try to break the line on a word boundary.
		   * For now, I will only break on space characters.
		   */
		  buf0 = buf;
		  lbuf = (unsigned char *) l->buf;

		  lbuf += 1;	       /* avoid space at beg of line */

		  if (Slrn_Wrap_Method == 0 || Slrn_Wrap_Method == 2)
		    {
		       while (buf0 > lbuf)
			 {
			    if ((*buf0 == ' ') || (*buf0 == '\t')
				|| (header_char_delimiter
				    && (*buf0 == header_char_delimiter)))
			      {
				 buf = buf0;
				 break;
			      }
			    buf0--;
			 }

		       if (buf0 == lbuf)
			 {
			    if (Slrn_Wrap_Method == 0)
			      {
		       /* Could not find a place to break the line.  Ok, so
			* we will not break this.  Perhaps it is a URL.
			* If not, it is a long word and who cares about it.
			*/
				 while (((ch = *buf) != 0)
					&& (ch != ' ') && (ch != '\t'))
				   buf++;

				 if (ch == 0)
				   continue;
			      }
			    else {
			       lbuf = (unsigned char *) l->buf;
			       lbuf += 1; /* avoid space at beg of line */
			    }
			 }
		    }

		  /* Start wrapped lines with a space.  To do this, I will
		   * _temporally_ modify the previous character for the purpose
		   * of creating the new space.
		   */
		  buf--;
		  ch = *buf;
		  *buf = ' ';

		  new_l = (Slrn_Article_Line_Type *) slrn_malloc (sizeof (Slrn_Article_Line_Type), 1, 1);
		  if (new_l == NULL)
		    return -1;

		  if (NULL == (new_l->buf = slrn_strmalloc ((char *)buf, 1)))
		    {
		       slrn_free ((char *) new_l);
		       return -1;
		    }

		  *buf++ = ch;
		  *buf = 0;

		  new_l->next = l->next;
		  new_l->prev = l;
		  l->next = new_l;
		  if (new_l->next != NULL) new_l->next->prev = new_l;

		  new_l->flags = l->flags | WRAPPED_LINE;

		  if (new_l->flags & QUOTE_LINE)
		    new_l->v.quote_level = l->v.quote_level;

		  l = new_l;
		  buf = (unsigned char *) new_l->buf;
		  a->is_wrapped = 1;
	       }
#if SLANG_VERSION < 20000
	     else buf++;
#endif

	     ch = *buf;
	  }
	l = l->next;
     }
   return 0;
}

/*}}}*/

static int is_blank_line (unsigned char *b) /*{{{*/
{
   b = (unsigned char *) slrn_skip_whitespace ((char *) b);
   return (*b == 0);
}

/*}}}*/

void _slrn_art_skip_quoted_text (Slrn_Article_Type *a) /*{{{*/
{
   Slrn_Article_Line_Type *l;

   if (a == NULL)
     return;

   l = a->cline;
   a->needs_sync = 1;

   /* look for a quoted line */
   while (l != NULL)
     {
	if ((l->flags & HIDDEN_LINE) == 0)
	  {
	     a->cline = l;
	     if (l->flags & QUOTE_LINE) break;
	  }
	l = l->next;
     }

   /* Now we are either at the end of the buffer or on a quote line. Skip
    * past other quote lines.
    */

   if (l == NULL)
     return;

   l = l->next;

   while (l != NULL)
     {
	if (l->flags & HIDDEN_LINE)
	  {
	     l = l->next;
	     continue;
	  }
	a->cline = l;
	if ((l->flags & QUOTE_LINE) == 0)
	  {
	     /* Check to see if it is blank */
	     if (is_blank_line ((unsigned char *) l->buf) == 0) break;
	  }
	l = l->next;
     }
}

/*}}}*/

int _slrn_art_skip_digest_forward (Slrn_Article_Type *a) /*{{{*/
{
   Slrn_Article_Line_Type *l;
   int num_passes;

   if (a == NULL)
     return -1;

   /* We are looking for:
    * <blank line>  (actually, most digests do not have this-- even the FAQ that suggests it!!)
    * ------------------------------
    * <blank line>
    * Subject: something
    *
    * In fact, most digests do not conform to this.  So, I will look for:
    * <blank line>
    * Subject: something
    *
    * Actually, most faqs, etc... do not support this.  So, look for any line
    * beginning with a number on second pass.  Sigh.
    */
   num_passes = 0;
   while (num_passes < 2)
     {
	l = a->cline->next;

	while (l != NULL)
	  {
	     char ch;
	     char *buf;

	     if ((l->flags & HIDDEN_LINE) || (l->flags & HEADER_LINE))
	       {
		  l = l->next;
		  continue;
	       }

	     buf = l->buf;
	     if (num_passes == 0)
	       {
		  if ((strncmp ("Subject:", buf, 8))
		      || (((ch = buf[8]) != ' ') && (ch != '\t')))
		    {
		       l = l->next;
		       continue;
		    }
	       }
	     else
	       {
		  ch = *buf;
		  if ((ch > '9') || (ch < '0'))
		    {
		       l = l->next;
		       continue;
		    }
	       }

	     a->cline = l;
	     a->needs_sync = 1;
	     return 0;
	  }
	num_passes++;
     }
   return -1;
}

/*}}}*/

char *slrn_art_extract_header (char *hdr, unsigned int len) /*{{{*/
{
   Slrn_Article_Line_Type *l;
   Slrn_Article_Type *a = Slrn_Current_Article;

   if (a == NULL)
     return NULL;

   l = a->lines;

   while ((l != NULL)
	  && (*l->buf != 0))
     {
	if (0 == slrn_case_strncmp ( hdr,
				     l->buf, len))
	  {
	     char *result;

	     if ((l->next != NULL) && (l->next->flags & WRAPPED_LINE))
	       {
		  (void) unwrap_line (a, l->next);
		  slrn_art_sync_article (a);
	       }

	     /* Return the data after the colon */
	     result = slrn_strbyte (l->buf, ':');
	     if (result == NULL)
	       result = l->buf + len;
	     else result += 1;

	     return slrn_skip_whitespace (result);
	  }
	l = l->next;
     }
   return NULL;
}

/*}}}*/

typedef struct _Visible_Header_Type /*{{{*/
{
   char *header;
   unsigned int len;
   struct _Visible_Header_Type *next;
}

/*}}}*/
Visible_Header_Type;

static Visible_Header_Type *Visible_Headers;

char *Slrn_Visible_Headers_String;     /* for interpreter */

static void free_visible_header_list (void) /*{{{*/
{
   while (Visible_Headers != NULL)
     {
	Visible_Header_Type *next;
	next = Visible_Headers->next;
	SLang_free_slstring (Visible_Headers->header);   /* NULL ok */
	SLfree ((char *) Visible_Headers);
	Visible_Headers = next;
     }
   SLang_free_slstring (Slrn_Visible_Headers_String);
   Slrn_Visible_Headers_String = NULL;
}

/*}}}*/

int slrn_set_visible_headers (char *headers) /*{{{*/
{
   char buf[256];
   unsigned int nth;

   free_visible_header_list ();

   Slrn_Visible_Headers_String = SLang_create_slstring (headers);
   if (Slrn_Visible_Headers_String == NULL)
     return -1;

   nth = 0;
   while (-1 != SLextract_list_element (headers, nth, ',', buf, sizeof(buf)))
     {
	Visible_Header_Type *next;

	next = (Visible_Header_Type *) SLmalloc (sizeof (Visible_Header_Type));
	if (next == NULL)
	  return -1;
	memset ((char *) next, 0, sizeof(Visible_Header_Type));
	if (NULL == (next->header = SLang_create_slstring (buf)))
	  {
	     SLfree ((char *) next);
	     return -1;
	  }
	next->len = strlen (buf);
	next->next = Visible_Headers;
	Visible_Headers = next;

	nth++;
     }
   if ((Slrn_Current_Article != NULL) &&
       (Slrn_Current_Article->headers_hidden))
     _slrn_art_hide_headers (Slrn_Current_Article); /* commit the changes */
   return 0;
}

/*}}}*/

void _slrn_art_hide_headers (Slrn_Article_Type *a) /*{{{*/
{
   Slrn_Article_Line_Type *l;
   char ch;

   if (a == NULL)
     return;

   _slrn_art_unhide_headers (a);

   a->needs_sync = 1;

   l = a->lines;

   while ((l != NULL) && (l->flags & HEADER_LINE))
     {
	int hide_header;
	Visible_Header_Type *v;

	ch = l->buf[0];
	ch |= 0x20;

	hide_header = 1;

	v = Visible_Headers;
	while (v != NULL)
	  {
	     int hide = (v->header[0] == '!') ? 1 : 0;
	     char chv = (0x20 | v->header[hide]);

	     if ((chv == ch)
		 && (0 == slrn_case_strncmp (l->buf,
					     v->header + hide,
					     (v->len) - hide)))
	       {
		  hide_header = hide;
		  break;
	       }

	     v = v->next;
	  }

	do
	  {
	     if (hide_header)
	       l->flags |= HIDDEN_LINE;

	     l = l->next;
	  }
	while ((l != NULL) && ((*l->buf == ' ') || (*l->buf == '\t')));
     }
   a->headers_hidden = 1;
}

/*}}}*/

void _slrn_art_unhide_headers (Slrn_Article_Type *a) /*{{{*/
{
   if (a == NULL)
     return;

   unhide_article_lines (a, HEADER_LINE);
   a->headers_hidden = 0;
}

/*}}}*/

int _slrn_art_unfold_header_lines (Slrn_Article_Type *a) /*{{{*/
{
   Slrn_Article_Line_Type *l;
   char ch;

   if (a == NULL)
     return -1;

   l = a->lines->next;
   a->is_modified = 1;
   a->needs_sync = 1;

   while ((l != NULL) && (l->flags & HEADER_LINE))
     {
	ch = l->buf[0];
	if ((ch == ' ') || (ch == '\t'))
	  {
	     unsigned int len0, len1;
	     Slrn_Article_Line_Type *prev;
	     char *new_buf;

	     l->buf[0] = ' ';

	     prev = l->prev;

	     len0 = strlen (prev->buf);
	     len1 = len0 + strlen (l->buf) + 1;

	     new_buf = slrn_realloc (prev->buf, len1, 1);
	     if (new_buf == NULL)
	       return -1;

	     prev->buf = new_buf;

	     strcpy (new_buf + len0, l->buf); /* safe */
	     prev->next = l->next;
	     if (l->next != NULL) l->next->prev = prev;
	     slrn_free ((char *)l->buf);
	     slrn_free ((char *) l);
	     l = prev;
	  }

	l = l->next;
     }
   return 0;
}

/*}}}*/

void slrn_mark_header_lines (Slrn_Article_Type *a) /*{{{*/
{
   Slrn_Article_Line_Type *l;

   if (a == NULL)
     return;
   l = a->lines;

   while ((l != NULL) && (l->buf[0] != 0))
     {
	l->flags = HEADER_LINE;
	l = l->next;
     }
}

/*}}}*/

void _slrn_art_hide_signature (Slrn_Article_Type *a) /*{{{*/
{
   if (a == NULL)
     return;

   hide_article_lines (a, SIGNATURE_LINE);
   a->signature_hidden = 1;
}

/*}}}*/

void _slrn_art_unhide_signature (Slrn_Article_Type *a) /*{{{*/
{
   if (a == NULL)
     return;

   unhide_article_lines (a, SIGNATURE_LINE);
   a->signature_hidden = 0;
}

/*}}}*/

void slrn_art_mark_pgp_signature (Slrn_Article_Type *a) /*{{{*/
{
   Slrn_Article_Line_Type *l;

   if (a == NULL)
     return;

   l = a->lines;
   while (l != NULL)
     {
	Slrn_Article_Line_Type *l0;
	int count;

	if ((l->buf[0] == '-')
	    && !strcmp (l->buf, "-----BEGIN PGP SIGNED MESSAGE-----"))
	  {
	     l->flags |= PGP_SIGNATURE_LINE;
	     l->flags &= ~QUOTE_LINE;

	     if ((NULL != (l = l->next)) &&
		 !strncmp (l->buf, "Hash: ", 6))
	       {
		  /* catch the `Hash: ... ' line */
		  l->flags |= PGP_SIGNATURE_LINE;
		  l->flags &= ~QUOTE_LINE;

		  l = l->next;
	       }
	     if ((NULL != l) &&
		 !strncmp (l->buf, "NotDashEscaped: ", 16))
	       {
		  /* the optional "NotDashEscaped: ..." line */
		  l->flags |= PGP_SIGNATURE_LINE;
		  l->flags &= ~QUOTE_LINE;

		  l = l->next;
	       }

	     continue;
	  }

	if ((l->buf[0] != '-')
	    || strcmp (l->buf, "-----BEGIN PGP SIGNATURE-----"))
	  {
	     l = l->next;
	     continue;
	  }
	l0 = l;
	l = l->next;

	count = 256;		       /* arbitrary */
	while ((l != NULL) && count)
	  {
	     if ((l->buf[0] != '-')
		 || strcmp (l->buf, "-----END PGP SIGNATURE-----"))
	       {
		  count--;
		  l = l->next;
		  continue;
	       }

	     l0->flags |= PGP_SIGNATURE_LINE;
	     l0->flags &= ~(QUOTE_LINE|SIGNATURE_LINE);
	     do
	       {
		  l0 = l0->next;
		  l0->flags |= PGP_SIGNATURE_LINE;
		  l0->flags &= ~(QUOTE_LINE|SIGNATURE_LINE);
	       }
	     while (l0 != l);
	     return;
	  }
	l = l0->next;
     }
}

/*}}}*/

void _slrn_art_hide_pgp_signature (Slrn_Article_Type *a)
{
   if (a == NULL)
     return;

   hide_article_lines (a, PGP_SIGNATURE_LINE);
   a->pgp_signature_hidden = 1;
}

/*}}}*/

void _slrn_art_unhide_pgp_signature (Slrn_Article_Type *a) /*{{{*/
{
   if (a == NULL)
     return;

   unhide_article_lines (a, PGP_SIGNATURE_LINE);
   a->pgp_signature_hidden = 0;
}

/*}}}*/

void slrn_art_mark_verbatim (Slrn_Article_Type *a)
{
   Slrn_Article_Line_Type *l;
   unsigned char chon, choff;
   unsigned int mask, next_mask;
   char *von, *voff;

   if (a == NULL)
     return;

   von = "#v+";
   voff = "#v-";

   chon = von[0];
   choff = voff[0];

   l = a->lines;
   next_mask = mask = 0;

   while (l != NULL)
     {
	if (mask == 0)
	  {
	     if ((l->buf[0] == chon)
		 && (0 == strcmp ((char *) l->buf, von)))
	       {
		  mask = VERBATIM_LINE|VERBATIM_MARK_LINE;
		  next_mask = VERBATIM_LINE;
	       }
	  }
	else if ((l->buf[0] == choff)
		 && (0 == strcmp ((char *) l->buf, voff)))
	  {
	     mask = VERBATIM_LINE|VERBATIM_MARK_LINE;
	     next_mask = 0;
	  }

	if (mask)
	  {
	     l->flags &= ~(QUOTE_LINE|SIGNATURE_LINE|PGP_SIGNATURE_LINE);
	     l->flags |= mask;
	     mask = next_mask;
	  }

	l = l->next;
     }
}

void _slrn_art_unhide_verbatim (Slrn_Article_Type *a) /*{{{*/
{
   if (a == NULL)
     return;

   unhide_article_lines (a, VERBATIM_LINE);
   a->verbatim_hidden = 0;
}

/*}}}*/

void _slrn_art_hide_verbatim (Slrn_Article_Type *a) /*{{{*/
{
   if (a == NULL)
     return;

   hide_article_lines (a, VERBATIM_LINE);
   a->verbatim_hidden = 1;
}

/*}}}*/
