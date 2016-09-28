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

#ifndef SLRNPULL_CODE
# include "slrnfeat.h"
#endif

#include <stdio.h>
#include <string.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include "util.h"
#include "ttymsg.h"
#include "hash.h"

#ifndef SLRNPULL_CODE
# include "group.h"
# include "art.h"
# include "mime.h"
#endif /* NOT SLRNPULL_CODE */

#include "xover.h"
#include "strutil.h"
#include "common.h"

#ifndef SLRNPULL_CODE
# include "server.h"

static int extract_id_from_xref (char *, NNTP_Artnum_Type *);
static void rearrange_add_headers (void);
static int Suspend_XOver_For_Kill = 0;
#endif

static Slrn_Header_Line_Type *copy_add_headers (Slrn_Header_Line_Type*, char);
static Slrn_Header_Line_Type *Unretrieved_Headers = NULL;
static Slrn_Header_Line_Type *Current_Add_Header = NULL;
static Slrn_Header_Line_Type *Additional_Headers = NULL;
#if SLRN_HAS_FAKE_REFS
static char *In_Reply_To;
#endif

static Slrn_Header_Line_Type Xover_Headers [] = /*{{{*/
{
#define SUBJECT_OFFSET  0
     {"Subject", 7, NULL, NULL},
#define FROM_OFFSET     1
     {"From", 4, NULL, NULL},
#define DATE_OFFSET     2
     {"Date", 4, NULL, NULL},
#define MSGID_OFFSET    3
     {"Message-ID", 10, NULL, NULL},
#define REFS_OFFSET     4
     {"References", 10, NULL, NULL},
#define BYTES_OFFSET    5
     {"Bytes", 5, NULL, NULL},
#define LINES_OFFSET    6
     {"Lines", 5, NULL, NULL},
};

/*}}}*/

static char *Xref;

/* The pointers in the above structure will point to the following buffer. */
static char *Malloced_Headers;

static void parse_headers (void) /*{{{*/
{
   Slrn_Header_Line_Type *addh;
   unsigned int i;
   char *h, ch;

   /* reset all pointers */

   for (i = 0; i < 7; i++)
     Xover_Headers[i].value = NULL;
   Xref = NULL;

   for (addh = Additional_Headers; addh != NULL; addh = addh->next)
     addh->value = "";

#if SLRN_HAS_FAKE_REFS
   In_Reply_To = NULL;
#endif

   h = Malloced_Headers;

   while (*h != 0)
     {
	char *colon;
	unsigned int len;

	colon = h;
	while (*colon && (*colon != ':')) colon++;

	if (*colon != ':')
	  break;

	*colon = 0;
	len = (unsigned int) (colon - h);

	colon++;
	if (*colon == ' ') colon++;  /* space is required to be there */

	for (i = 0; i < 7; i++)
	  {
	     if ((Xover_Headers[i].value == NULL)
		 && (len == Xover_Headers[i].name_len)
		 && (!slrn_case_strcmp (h,
					 Xover_Headers[i].name)))
	       {
		  Xover_Headers[i].value = colon;
		  break;
	       }
	     else if ((Xref == NULL) && (len == 4) &&
		      (!slrn_case_strcmp (h, "Xref")))
	       {
		  Xref = colon;
		  break;
	       }
	  }

	for (addh = Additional_Headers; addh != NULL; addh = addh->next)
	  if ((len == addh->name_len)
	      && (0 == slrn_case_strcmp (h,
					  addh->name)))
	    {
	       addh->value = colon;
	       break;
	    }

#if SLRN_HAS_FAKE_REFS
	if ((In_Reply_To == NULL) && (len == 11) &&
	    (0 == slrn_case_strcmp (h,
				     "In-Reply-To")))
	  In_Reply_To = colon;
#endif

	/* Now skip to next header line and take care of continuation if
	 * present.
	 */

	h = colon;
	while (0 != (ch = *h))
	  {
	     if (ch == '\n')
	       {
		  ch = *(h + 1);
		  if ((ch == ' ') || (ch == '\t'))
		    {
		       *h++ = ' ';
		    }
		  else
		    {
		       *h++ = 0;
		       break;
		    }
	       }

	     if (ch == '\t') *h = ' ';

	     h++;
	  }
     }
}

/*}}}*/

#if SLRN_HAS_FAKE_REFS
static char *fake_refs_from_inreply_to (char *buf, unsigned int buflen)
{
   char *p, *p1;
   unsigned int len;

   if (NULL == (p = In_Reply_To))
     return NULL;

   /* The In-Reply-To header seems to obey no well-defined format.  I have
    * seen things like:
    *   In-Reply-To: Your message <msg-id>
    *   In-Reply-To: A message from you <foo@bar> on date <msg-id>
    *   In-Reply-To: <msgid> from <foo@bar>
    *   In-Reply-To: A message from <foo@bar>
    *
    * Here is the plan:  If the first non-whitespace character is a < then
    * assume that starts the message id.
    * Otherwise, if the last character is >, then assume that is the
    * message id provided that it is not preceeded by from
    */

   p = slrn_skip_whitespace (p);
   if (*p != '<')
     {
	p1 = p;
	while ((p1 = slrn_strbyte (p1, '<')) != NULL)
	  {
	     p = p1;
	     p1++;
	  }

	if (*p != '<')
	  return NULL;
     }

   /* found a message-id */
   if (NULL == (p1 = slrn_strbyte (p, '>')))
     return NULL;

   len = 1 + (p1 - p); /* include the '>' */

   if (len >= buflen)
     return NULL;

   strncpy (buf, p, len);
   buf [len] = '\0';
   return buf;
}
#endif

static int parsed_headers_to_xover (NNTP_Artnum_Type id, Slrn_XOver_Type *xov) /*{{{*/
{
   unsigned int len;
   char *subj, *from, *date, *msgid, *refs, *bytes, *lines, *xref;
   char *buf;
#if SLRN_HAS_FAKE_REFS
   char fake_ref_buf [512];
#endif

   if (NULL == (subj = Xover_Headers[SUBJECT_OFFSET].value))
     subj = "";
   if (NULL == (from = Xover_Headers[FROM_OFFSET].value))
     from = "";
   if (NULL == (date = Xover_Headers[DATE_OFFSET].value))
     date = "";
   if (NULL == (msgid = Xover_Headers[MSGID_OFFSET].value))
     msgid = "";

   if ((NULL == (refs = Xover_Headers[REFS_OFFSET].value))
#if SLRN_HAS_FAKE_REFS
       && (NULL == (refs = fake_refs_from_inreply_to (fake_ref_buf, sizeof (fake_ref_buf))))
#endif
       )
     refs = "";

   if (NULL == (xref = Xref))
     xref = "";
   if (NULL == (lines = Xover_Headers[LINES_OFFSET].value))
     lines = "";
   if (NULL == (bytes = Xover_Headers[BYTES_OFFSET].value))
     bytes = "";

   len = strlen (subj) + strlen (from) + 2;
   buf = slrn_malloc (len, 0, 1);
   if (buf == NULL) return -1;

   xov->subject_malloced = buf;
   strcpy (buf, subj); /* safe */
   buf += strlen (subj) + 1;

   xov->from = buf;
   strcpy (buf, from); /* safe */

   len = strlen (date) + strlen (msgid) + strlen (refs) + strlen (xref) + 4;

   buf = slrn_malloc (len, 0, 1);
   if (buf == NULL)
     {
	slrn_free (xov->subject_malloced);
	xov->subject_malloced = NULL;
	return -1;
     }

   xov->date_malloced = buf;
   strcpy (buf, date); /* safe */
   buf += strlen (date) + 1;

   xov->message_id = buf;
   strcpy (buf, msgid); /* safe */
   buf += strlen (msgid) + 1;

   xov->references = buf;
   strcpy (buf, refs); /* safe */
   buf += strlen (refs) + 1;

   xov->xref = buf;
   strcpy (buf, xref); /* safe */

   xov->bytes = atoi (bytes);
   xov->lines = atoi (lines);

#ifndef SLRNPULL_CODE
   if ((id == -1)
       && (-1 == extract_id_from_xref (xov->xref, &id)))
     id = -1;

# if SLRN_HAS_SPOOL_SUPPORT
   if (Slrn_Spool_Check_Up_On_Nov && (xov->bytes == 0) && (id != -1))
     {
	int size = Slrn_Server_Obj->sv_get_article_size (id);
	if (size != -1) xov->bytes = size;
     }
# endif
#endif

   xov->add_hdrs = copy_add_headers (Additional_Headers, 0);

   xov->id = id;

   return 0;
}

/*}}}*/

char *slrn_extract_add_header (Slrn_Header_Type *h, char *hdr) /*{{{*/
{
   unsigned int len;
   Slrn_Header_Line_Type *l;

   len = strlen (hdr);
   if ((len > 2) && (hdr[len-1] == ' ') && (hdr[len-2] == ':'))
     len -= 2;
   for (l = h->add_hdrs; l != NULL; l = l->next)
     {
	if ((l->name_len == len) &&
	    (0 == slrn_case_strncmp ( l->name,
				      hdr, len)))
	  return l->value;
     }

   return NULL;
}

/*}}}*/

#ifdef SLRNPULL_CODE
static int xover_parse_head (NNTP_Artnum_Type id, char *headers, Slrn_XOver_Type *xov) /*{{{*/
{
   slrn_free (Malloced_Headers);

   if (NULL == (Malloced_Headers = slrn_strmalloc (headers, 1)))
     return -1;

   parse_headers ();

   return parsed_headers_to_xover (id, xov);
}
/*}}}*/
#endif

void slrn_free_xover_data (Slrn_XOver_Type *xov)
{
   slrn_free (xov->subject_malloced);
   slrn_free (xov->date_malloced);
   slrn_free_additional_headers (xov->add_hdrs);

   xov->subject_malloced = NULL;
   xov->date_malloced = NULL;
   xov->add_hdrs = NULL;
}

void slrn_map_xover_to_header (Slrn_XOver_Type *xov, Slrn_Header_Type *h, int free_xover)
{
   char *m;

   h->number = xov->id;
   h->subject = slrn_safe_strmalloc (xov->subject_malloced);
   h->from = slrn_safe_strmalloc(xov->from);
   h->date = slrn_safe_strmalloc (xov->date_malloced);
   h->refs = slrn_safe_strmalloc (xov->references);
   h->xref = slrn_safe_strmalloc (xov->xref);

   h->lines = xov->lines;
   h->bytes = xov->bytes;
   h->add_hdrs = xov->add_hdrs; xov->add_hdrs = NULL;

   /* Since the strings have been malloced, the message_id pointer can be changed.
    */
   m = xov->message_id;
   while ((*m != '<') && (*m != 0))
     m++;

   if (*m != 0)
     {
	h->msgid = m;
	m = slrn_strbyte (m, '>');
	if (m != NULL) *(m + 1) = 0;
     }
   else h->msgid = xov->message_id;

   h->msgid = slrn_safe_strmalloc (h->msgid);

   h->hash = slrn_compute_hash ( (unsigned char *)h->msgid,
				 (unsigned char *)h->msgid + strlen (h->msgid));

   if (free_xover)
     slrn_free_xover_data (xov);
}

typedef struct Overview_Fmt_Type
{
   char *name;
   char full;
   char **value;
   struct Overview_Fmt_Type *next;
}
Overview_Fmt_Type;

Overview_Fmt_Type *Overview_Fmt;

/* returns zero if OVERVIEW.FMT does not contain the standard headers */
int slrn_read_overview_fmt (void) /*{{{*/
{
#ifdef SLRNPULL_CODE
   return 1;
#else
   char line[NNTP_BUFFER_SIZE];
   Overview_Fmt_Type *tmp;

   while (Overview_Fmt != NULL)
     {
	tmp = Overview_Fmt->next;
	slrn_free (Overview_Fmt->name);
	slrn_free ((char *) Overview_Fmt);
	Overview_Fmt = tmp;
     }

   if (OK_GROUPS == Slrn_Server_Obj->sv_list ("OVERVIEW.FMT"))
     {
	int len, i = 0;

	while ((i < 7) && (Slrn_Server_Obj->sv_read_line (line, sizeof (line)) == 1))
	  {
	     if ((len = strlen (line)) && (line[len-1] == ':'))
	       line[len-1] = 0;
	     if (slrn_case_strcmp ( line,
				    Xover_Headers[i++].name))
	       {
		  while (Slrn_Server_Obj->sv_read_line (line, sizeof (line)) == 1)
		    continue; /* discard remaining output */
		  return 0;
	       }
	  }
	if (i == 0)
	  /* This is probably due to a broken firewall configuration;
	   * assume XOVER is working anyway. */
	  return 1;
	else if (i != 7)
	  return 0;

	tmp = NULL;
	while (Slrn_Server_Obj->sv_read_line (line, sizeof (line)) == 1)
	  {
	     char *p;
	     Overview_Fmt_Type* new_entry;

	     if (NULL == (p = slrn_strbyte (line, ':')))
	       p = "";
	     else
	       *p++ = 0;

	     if (NULL == (new_entry = (Overview_Fmt_Type*)
			  slrn_malloc (sizeof (Overview_Fmt_Type), 1, 0)))
	       break;

	     if (NULL == (new_entry->name = slrn_strmalloc (line, 0)))
	       {
		  slrn_free ((char*)new_entry);
		  break;
	       }

	     if (!slrn_case_strcmp ( line, "Xref"))
	       new_entry->value = &Xref;
	     if (!strcmp (p, "full"))
	       new_entry->full = 1;

	     if (tmp == NULL)
	       Overview_Fmt = new_entry;
	     else
	       tmp->next = new_entry;
	     tmp = new_entry;
	  }
     }
   return 1;
#endif /* NOT SLRNPULL_CODE */
}
/*}}}*/

void slrn_free_additional_headers (Slrn_Header_Line_Type *h) /*{{{*/
{
   Slrn_Header_Line_Type *tmp;
   while (h != NULL)
     {
	tmp = h->next;
	slrn_free (h->name);
	slrn_free ((char *)h);
	h = tmp;
     }
}

/*}}}*/

void slrn_clear_requested_headers (void) /*{{{*/
{
   Overview_Fmt_Type *o;

   slrn_free_additional_headers (Additional_Headers);
   Additional_Headers = NULL;
   Unretrieved_Headers = NULL;
   Current_Add_Header = NULL;

   for (o = Overview_Fmt; o != NULL; o = o->next)
     {
	if (o->value != &Xref)
	  o->value = NULL;
     }
}

/*}}}*/

void slrn_request_additional_header (char *hdr, int expensive) /*{{{*/
{
   Slrn_Header_Line_Type *h, **hh;
   Overview_Fmt_Type *o;
   unsigned int len;

   h = Additional_Headers;
   hh = &Additional_Headers;
   len = strlen (hdr);

   while (h != NULL)
     {
	if ((h->name_len == len) &&
	    (0 == slrn_case_strcmp ( hdr,
				     h->name)))
	  return; /* This one was already requested. */
	hh = &h->next;
	h = h->next;
     }

   if (NULL == (h = (Slrn_Header_Line_Type*)
		slrn_malloc (sizeof (Slrn_Header_Line_Type), 0, 0)))
     return;

   for (o = Overview_Fmt; o != NULL; o = o->next)
     {
	if ((o->value == NULL) &&
	    !slrn_case_strcmp (hdr, o->name))
	  {
	     o->value = &h->value;
	     break;
	  }
     }

   if (((o == NULL) && (expensive == 0))
       || (NULL == (h->name = slrn_strmalloc (hdr, 0))))
     {
	slrn_free ((char *)h);
	return;
     }

   h->name_len = len;
   h->value = NULL;
   h->next = NULL;
   *hh = h;

   if (Unretrieved_Headers == NULL)
     {
	Unretrieved_Headers = h;
	Current_Add_Header = h;
     }
}

/*}}}*/

/* now also takes care of the RFC1522 decoding */
static Slrn_Header_Line_Type *copy_add_headers (Slrn_Header_Line_Type *l, /*{{{*/
						char all)
{
   Slrn_Header_Line_Type *retval = NULL, *old = NULL, *copy;
   unsigned int len;
   char *name, *value, *buf;

   while (l != NULL)
     {
	name = l->name;
	if (NULL == (value = l->value))
	    {
	       if (all) value = "";
	       else break;
	    }

	value = slrn_safe_strmalloc(value);
#ifndef SLRNPULL_CODE /* FIXME */
	(void) slrn_rfc1522_decode_header (name, &value);
#endif

	if (NULL == (copy = (Slrn_Header_Line_Type*) slrn_malloc
		     (sizeof (Slrn_Header_Line_Type), 0, 1)))
	  {
	     slrn_free(value);
	     continue;
	  }

	len = strlen (name) + strlen (value) + 2;

	if (NULL == (buf = slrn_malloc (len, 0, 1)))
	  {
	     slrn_free ((char *) copy);
	     slrn_free (value);
	     continue;
	  }

	copy->name = buf;
	strcpy (buf, name); /* safe */
	buf += strlen (name) + 1;

	copy->value = buf;
	strcpy (buf, value); /* safe */

	slrn_free(value);

	copy->name_len = l->name_len;
	copy->next = NULL;

	if (NULL != old)
	  old->next = copy;
	else
	  retval = copy;

	old = copy;
	l = l->next;
     }

   return retval;
}

/*}}}*/

#ifndef SLRNPULL_CODE
static int XOver_Done;
static NNTP_Artnum_Type XOver_Min, XOver_Max, XOver_Next;

static int extract_id_from_xref (char *xref, NNTP_Artnum_Type *idp) /*{{{*/
{
   unsigned int group_len;
   char ch;

   if (*xref == 0)
     return -1;

   group_len = strlen (Slrn_Current_Group_Name);

   while ((ch = *xref) != 0)
     {
	if (ch == ' ')
	  {
	     xref++;
	     continue;
	  }

	if (0 == strncmp (xref, Slrn_Current_Group_Name, group_len))
	  {
	     xref += group_len;
	     if (*xref == ':')
	       {
		  *idp = NNTP_STR_TO_ARTNUM (xref + 1);
		  return 0;
	       }
	  }

	/* skip to next space */
	while (((ch = *xref) != 0) && (ch != ' ')) xref++;
     }
   return -1;
}

/*}}}*/

static char *server_read_and_malloc (void) /*{{{*/
{
   char line [NNTP_BUFFER_SIZE];
   char *mbuf;
   unsigned int buffer_len, buffer_len_max;
   int failed;

   mbuf = NULL;
   buffer_len_max = buffer_len = 0;
   failed = 0;

   while (1)
     {
	unsigned int len;
	int status;

	status = Slrn_Server_Obj->sv_read_line (line, sizeof(line));
	if (status == -1)
	  {
	     slrn_free (mbuf);
	     return NULL;
	  }
	if (status == 0)
	  break;

	if (failed) continue;

	len = strlen (line);

	if (len + buffer_len + 4 > buffer_len_max)
	  {
	     char *new_mbuf;

	     buffer_len_max += 4096 + len;
	     new_mbuf = slrn_realloc (mbuf, buffer_len_max, 0);

	     if (new_mbuf == NULL)
	       {
		  slrn_free (mbuf);
		  failed = 1;
		  continue;
	       }
	     mbuf = new_mbuf;
	  }

	strcpy (mbuf + buffer_len, line); /* safe */
	buffer_len += len;
	mbuf [buffer_len++] = '\n';
	mbuf [buffer_len] = 0;
     }

   if (failed) return NULL;

   return mbuf;
}

/*}}}*/

static int read_head_into_xover (NNTP_Artnum_Type id, Slrn_XOver_Type *xov) /*{{{*/
{
   slrn_free (Malloced_Headers);

   if (NULL == (Malloced_Headers = server_read_and_malloc ()))
     return -1;

   parse_headers ();

   return parsed_headers_to_xover (id, xov);
}
/*}}}*/

extern int Slrn_Prefer_Head;

int slrn_open_xover (NNTP_Artnum_Type min, NNTP_Artnum_Type max) /*{{{*/
{
   NNTP_Artnum_Type id;
   int status = -1;
   XOver_Done = 1;

   if (Slrn_Server_Obj->sv_has_xover && !Suspend_XOver_For_Kill &&
       (Slrn_Prefer_Head != 2))
     {
	rearrange_add_headers ();
	status = Slrn_Server_Obj->sv_nntp_xover (min, max);
	if (status == OK_XOVER)
	  {
	     XOver_Next = XOver_Min = min;
	     XOver_Max = max;
	     XOver_Done = 0;
	     return OK_XOVER;
	  }
	if (status != ERR_FAULT)
	  return status;

	Slrn_Server_Obj->sv_has_xover = 0;
	Slrn_Server_Obj->sv_reset_has_xover = 1;
     }

   /* The rest of this function applies when not using XOVER.
    * It is complicated by the fact that the server may contain huge ranges
    * of missing articles.  In particular, the first article in the desired
    * xover range may be missing.  The following code determines the first
    * article in this range.  If the range is large an no articles are present
    * in the range, it may be rather slow.
    */
   for (id = min; id <= max; id++)
     {
	status = (*Slrn_Server_Obj->sv_nntp_head)(id, NULL, NULL);

	if (status == -1)
	  return -1;

	if (status == OK_HEAD)
	  break;
     }

   if (id > max)
     return status;

   XOver_Next = XOver_Min = id;
   XOver_Max = max;
   XOver_Done = 0;

   return OK_XOVER;
}

/*}}}*/

/* The line consists of:
 * id|subj|from|date|msgid|refs|bytes|line|misc stuff
 * Here '|' is a TAB.  The following code parses this.
 */
static int parse_xover_line (char *buf, Slrn_XOver_Type *xov) /*{{{*/
{
   char *b;
   int i;
   NNTP_Artnum_Type id;
   Slrn_Header_Line_Type *addh;
   Overview_Fmt_Type *t = Overview_Fmt;

   for (addh = Additional_Headers; addh != NULL; addh = addh->next)
     addh->value = NULL;

   b = buf;

   while (*b && (*b != '\t')) b++;
   if (*b) *b++ = 0;
   id = NNTP_STR_TO_ARTNUM (buf);

   for (i = 0; i < 7; ++i)
     {
	Xover_Headers[i].value = b;
	while (*b && (*b != '\t')) b++;
	if (*b) *b++ = 0;
     }

   if (t == NULL)
     /* we don't know OVERVIEW.FMT, so just look for Xref */
     {
	while (*b != 0)
	  {
	     char *xb = b;

	     /* skip to next field. */
	     while (*b && (*b != '\t')) b++;
	     if (*b) *b++ = 0;

	     if (0 == slrn_case_strncmp ( xb, "Xref: ", 6))
	       {
		  Xref = xb + 6;
		  break;
	       }
	  }
     }
   else
     {
	while (t != NULL)
	  {
	     char *xb = b;

	     while (*b && (*b != '\t')) b++;
	     if (*b) *b++ = 0;

	     if (t->value != NULL)
	       {
		  if (t->full)
		    {
		       if ((NULL != (xb = slrn_strbyte (xb, ':'))) && (0 != *(++xb)))
			 *t->value = ++xb;
		       else
			 *t->value = "";
		    }
		  else
		    *t->value = xb;
	       }

	     t = t->next;
	  }
     }

   return parsed_headers_to_xover (id, xov);
}

/*}}}*/

/* Returns -1 upon error, 0, if done, and 1 upon success */
int slrn_read_xover (Slrn_XOver_Type *xov) /*{{{*/
{
   char buf [NNTP_BUFFER_SIZE];
   NNTP_Artnum_Type id;
   int status;

   if (XOver_Done)
     {
	if (!Slrn_Server_Obj->sv_has_xover || Suspend_XOver_For_Kill ||
	    (Slrn_Prefer_Head == 2))
	  {
	     Unretrieved_Headers = NULL;
	     Current_Add_Header = NULL;
	  }
	return 0;
     }

   if (Slrn_Server_Obj->sv_has_xover && !Suspend_XOver_For_Kill &&
       (Slrn_Prefer_Head != 2))
     {
	int bytes = -1;
	while (1)
	  {
	     status = Slrn_Server_Obj->sv_read_line (buf, sizeof (buf));
	     if (status == -1)
	       return -1;
	     if (status == 0)
	       {
		  XOver_Done = 1;
		  return 0;
	       }

	     id = NNTP_STR_TO_ARTNUM (buf);
# if SLRN_HAS_SPOOL_SUPPORT
	     if (Slrn_Spool_Check_Up_On_Nov)
	       if (-1 == (bytes = Slrn_Server_Obj->sv_get_article_size (id)))
		 continue; /* skip nonexisting article */
# endif

	     if ((id >= XOver_Min) && (id <= XOver_Max))
	       break;
	     /* else server screwed up and gave bad response.  Ignore it. */
	  }

	if (-1 == parse_xover_line (buf, xov))
	  return -1;

	if ((xov->bytes == 0) && (bytes > 0))
	  xov->bytes = bytes;
	return 1;
     }

   /* For non-XOVER, the HEAD command has already been sent. */
   if (-1 == read_head_into_xover (XOver_Next, xov))
     {
	XOver_Done = 1;
	return -1;
     }

   if (XOver_Next == XOver_Max)
     {
	XOver_Done = 1;
	return 1;
     }

   XOver_Next++;

   while (1)
     {
	/* Get head of next article ready for next call of this function. */
	status = Slrn_Server_Obj->sv_nntp_head (XOver_Next, NULL, NULL);
	if (status == OK_HEAD)
	  break;

	/* Looks like the head is not available for that article.  Do NEXT to
	 * next available article in the range.
	 */
	if ((status == -1)
	    || (-1 == (status = Slrn_Server_Obj->sv_nntp_next (&id))))
	  {
	     if (SLKeyBoard_Quit)
	       {
		  XOver_Done = 1;
		  break;
	       }

	     slrn_exit_error (_("Server closed connection.  Cannot recover."));
	  }

	if ((status != OK_NEXT) || (id > XOver_Max))
	  {
	     XOver_Done = 1;
	     break;
	  }

	XOver_Next = id;
     }

   return 1;
}

/*}}}*/
void slrn_close_xover (void) /*{{{*/
{
   XOver_Done = 1;
}

/*}}}*/

void slrn_open_suspend_xover (void) /*{{{*/
{
   Suspend_XOver_For_Kill |= 1;
}

/*}}}*/
void slrn_close_suspend_xover (void) /*{{{*/
{
   Suspend_XOver_For_Kill &= ~1;
}

/*}}}*/
int slrn_xover_for_msgid (char *msgid, Slrn_XOver_Type *xov) /*{{{*/
{
   NNTP_Artnum_Type id;
   int status, retval;

   if ((msgid == NULL) || (*msgid == 0)) return -1;

   status = Slrn_Server_Obj->sv_nntp_head (-1, msgid, &id);

   if (OK_HEAD != status)
     {
	if (ERR_FAULT == status)
	  slrn_error (_("Server does not provide this capability."));
	return -1;
     }

   /* As Leafnode sends article numbers from different groups,
    * we always need to set this to -1. Sigh. */
   /*if (id == 0)*/ id = -1;
   retval = read_head_into_xover (id, xov);

   return retval;
}

/*}}}*/

void slrn_open_all_add_xover (void) /*{{{*/
{
   if (!Slrn_Server_Obj->sv_has_xover || Suspend_XOver_For_Kill ||
       (Slrn_Prefer_Head == 2))
     Current_Add_Header = NULL;
   else
     Current_Add_Header = Additional_Headers;
}

/*}}}*/

/* Returns -1 upon error, 0 if done, 1 upon success */
int slrn_open_add_xover (NNTP_Artnum_Type min, NNTP_Artnum_Type max) /*{{{*/
{
   int status = -1;

   if (NULL == Current_Add_Header)
     return 0;

   if (Slrn_Server_Obj->sv_has_xhdr == -1)
     Slrn_Server_Obj->sv_has_xhdr = Slrn_Server_Obj->sv_has_cmd ("XHDR Path");

   if (Slrn_Server_Obj->sv_has_xhdr == 1)
     {
	status = Slrn_Server_Obj->sv_nntp_xhdr (Current_Add_Header->name,
						min, max);
	if (status == OK_HEAD)
	  {
	     XOver_Next = XOver_Min = min;
	     XOver_Max = max;
	     return 1;
	  }
	return -1;
     }

   /* Server does not support xhdr -- we need to get full HEADers (slow!) */
   Suspend_XOver_For_Kill |= 2; /* quick hack */
   status = slrn_open_xover (min, max);

   return (status == OK_XOVER) ? 1 : -1;
}

/*}}}*/
/* Returns -1 when done and on errors, 0 and article number otherwise */
int slrn_read_add_xover (Slrn_Header_Line_Type **l, NNTP_Artnum_Type *idp) /*{{{*/
{
   static char valbuf [NNTP_BUFFER_SIZE];
   static Slrn_Header_Line_Type hlbuf;
   Slrn_XOver_Type xov;
   char *b;
   int status;

   if (Slrn_Server_Obj->sv_has_xhdr == 1)
     {
	NNTP_Artnum_Type id;

	while (1)
	  {
	     status = Slrn_Server_Obj->sv_read_line (valbuf, sizeof (valbuf));
	     if (status == -1)
	       {
		  Current_Add_Header = NULL;
		  return -1;
	       }
	     if (status == 0)
	       {
		  Current_Add_Header = Current_Add_Header->next;
		  return -1;
	       }

	     b = valbuf;
	     while (*b && (*b != ' ')) b++;
	     if (*b) *b++ = 0;

	     id = NNTP_STR_TO_ARTNUM (valbuf);
	     if ((id >= XOver_Min) && (id <= XOver_Max))
	       break;
	     /* else ignore server response */
	  }

	hlbuf.name = Current_Add_Header->name;
	hlbuf.name_len = Current_Add_Header->name_len;
	if (strcmp (b, "(none)") &&
	    strcmp (b, "(null)")) /* some servers return this */
	  hlbuf.value = b;
	else
	  hlbuf.value = NULL;
	hlbuf.next = NULL;
	*l = &hlbuf;
	*idp = id;
	return 0;
     }

   /* Server does not support xhdr -- we need to get full HEADers (slow!) */
   status = slrn_read_xover (&xov); /* This is a quick hack. */
   *l = Current_Add_Header;
   if (status == 1)
     {
	slrn_free_xover_data (&xov);
	*idp = xov.id;
	return 0;
     }
   return -1;
}
/*}}}*/

void slrn_close_add_xover (int done) /*{{{*/
{
   Suspend_XOver_For_Kill &= ~2;
   if (done)
     Current_Add_Header = Unretrieved_Headers = NULL;
   else
     Current_Add_Header = Unretrieved_Headers;
}

/*}}}*/

int slrn_add_xover_missing (void)
{
   return (Current_Add_Header != NULL);
}

/* makes sure that headers found in the XOVER output are listed first */
static void rearrange_add_headers (void) /*{{{*/
{
   Slrn_Header_Line_Type *nonnov = NULL, *last = NULL, *l = Additional_Headers;
   Overview_Fmt_Type *o = Overview_Fmt;

   while (l != NULL)
     {
	l->value = NULL;
	l = l->next;
     }

   while (o != NULL)
     {
	if (o->value != NULL)
	  *o->value = "";
	o = o->next;
     }

   l = Additional_Headers;
   Additional_Headers = NULL;

   while (l != NULL)
     {
	Slrn_Header_Line_Type *next = l->next;
	if (l->value == NULL)
	  {
	     l->next = nonnov;
	     nonnov = l;
	  }
	else
	  {
	     l->next = Additional_Headers;
	     Additional_Headers = l;
	     if (last == NULL)
	       last = l;
	  }
	l = next;
     }

   if (last != NULL)
     last->next = nonnov;
   else
     Additional_Headers = nonnov;

   Unretrieved_Headers = Current_Add_Header = nonnov;
}

/*}}}*/

void slrn_append_add_xover_to_header (Slrn_Header_Type *h, /*{{{*/
				      Slrn_Header_Line_Type *l)
{
   Slrn_Header_Line_Type *pos;

   if (NULL != (pos = h->add_hdrs))
     {
	while (pos->next != NULL)
	  pos = pos->next;
	pos->next = copy_add_headers (l, 1);
     }
   else
     h->add_hdrs = copy_add_headers (l, 1);
}

/*}}}*/
#endif				       /* NOT SLRNPULL_CODE */
