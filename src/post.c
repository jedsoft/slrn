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

/* post an article */

#include "config.h"
#include "slrnfeat.h"

/*{{{ Include Files */
#include <stdio.h>
#include <string.h>
#include <errno.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <sys/types.h>
#include <time.h>
#include <slang.h>
#include "jdmacros.h"

#ifdef VMS
# include "vms.h"
#endif
#ifdef __MINGW32__
# include <process.h>
#endif

#if SLRN_HAS_CANLOCK
# include <canlock.h>
#endif

#include "slrn.h"
#include "util.h"
#include "server.h"
#include "misc.h"
#include "group.h"
#include "art.h"
#include "post.h"
#include "decode.h"
#include "snprintf.h"
#include "charset.h"
#include "menu.h"
#include "version.h"
#include "mime.h"
#include "hooks.h"
#include "strutil.h"
#include "common.h"
#include "hdrutils.h"

/*}}}*/

#define MAX_LINE_BUFLEN	2048

/*{{{ Public Global Variables */
char *Slrn_CC_Post_Message = NULL;
char *Slrn_Failed_Post_Filename;
char *Slrn_Post_Custom_Headers;
char *Slrn_Postpone_Dir;
char *Slrn_Save_Posts_File;
char *Slrn_Save_Replies_File;
char *Slrn_Signoff_String;

int Slrn_Generate_Date_Header = 0;
int Slrn_Generate_Message_Id = 1;
int Slrn_Netiquette_Warnings = 1;
int Slrn_Reject_Long_Lines = 2;
int Slrn_Use_Recom_Id = 0;
/*}}}*/

/*{{{ Forward Function Declarations */
static int postpone_file (char *);
/*}}}*/

/* This function needs to be called directly after po_start! */
static int create_message_id (char **msgidp)/*{{{*/
{
   char *t, *msgid;
#if SLRN_HAS_GEN_MSGID
   unsigned long pid, now;
   char baseid[64];
   char *b, tmp[32];
   char *chars32 = "0123456789abcdefghijklmnopqrstuv";
   static unsigned long last_now;
   size_t malloc_len;
#endif

   *msgidp = NULL;

   /* Try to find the Id the server recommends*/
   if (Slrn_Use_Recom_Id
       && (NULL != (*msgidp = Slrn_Post_Obj->po_get_recom_id ())))
     return 0;

#if !SLRN_HAS_GEN_MSGID
   return 0;
#else

   if (Slrn_Generate_Message_Id == 0)
     return 0;

   while (1)
     {
	if ((Slrn_User_Info.posting_host == NULL)
	    || ((time_t) -1 == time ((time_t *)&now)))
	  return 0;

	if (now != last_now) break;
	slrn_sleep (1);
     }
   last_now = now;

   pid = (unsigned long) getpid ();
   now -= 0x28000000;

   b = baseid;
   t = tmp;
   while (now)
     {
	*t++ = chars32[now & 0x1F];
	now = now >> 5;
     }
   while (t > tmp)
     {
	t--;
	*b++ = *t;
     }
   *b++ = '.';

   t = tmp;
   while (pid)
     {
	*t++ = chars32[pid & 0x1F];
	pid = pid >> 5;
     }
   while (t > tmp)
     {
	t--;
	*b++ = *t;
     }

   t = Slrn_User_Info.username;
   if (t != NULL && *t != 0)
     {
	unsigned char ch = 0;

	*b++ = '.';
	while ((*t != 0) && (b < baseid + sizeof(baseid) - 2))
	  {
	     if (((ch = *t) > ' ') && ((ch & 0x80) == 0)
		 && (NULL == slrn_strbyte ("<>()@,;:\\\"[]", ch)))
	       *b++ = ch;
	     t++;
	  }
	if (*(b-1) == '.') /* may not be last character, so append '_' */
	  *b++ = '_';
     }
   *b = 0;

   malloc_len=(strlen(baseid)+strlen(Slrn_User_Info.posting_host)+8);
   if (NULL == (msgid=slrn_malloc (malloc_len,0,1)))
     return -1;

   (void) SLsnprintf (msgid, malloc_len, "<slrn%s@%s>", baseid, Slrn_User_Info.posting_host);

   *msgidp = msgid;
   return 0;
#endif /*SLRN_HAS_GEN_MSGID*/
}
/*}}}*/

/*{{{ slrn_add_*() */

const char *Weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char *Months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", "ERR" };

char *slrn_gen_date_header () /*{{{*/
{
   time_t now;
   struct tm *t;
   long int tz = 0;

   time (&now);
   t = localtime (&now);

#ifdef HAVE_TIMEZONE
   tz = - timezone / 60;
   if (t->tm_isdst == 1) tz += 60;
#else
# ifdef HAVE_TM_GMTOFF
   tz = t->tm_gmtoff / 60;
# endif
#endif

   return slrn_strdup_printf(
		  "Date: %s, %d %s %d %02d:%02d:%02d %+03d%02d",
		  Weekdays[t->tm_wday], t->tm_mday, Months[t->tm_mon],
		  t->tm_year + 1900, t->tm_hour, t->tm_min, t->tm_sec,
		  (int) tz / 60, (int) labs (tz) % 60);
}
/*}}}*/

int slrn_add_signature (FILE *fp) /*{{{*/
{
   FILE *sfp;
   char file[SLRN_MAX_PATH_LEN];
   char buf [256];

   if ((NULL != Slrn_Signoff_String) && (*Slrn_Signoff_String != 0))
     {
        fputs ("\n", fp);
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s", Slrn_Signoff_String) < 0)
	     return -1;
     }

   if ((Slrn_User_Info.signature == NULL)
       || (Slrn_User_Info.signature[0] == 0))
     return 0;

   if ((sfp = slrn_open_home_file (Slrn_User_Info.signature, "r", file,
				   sizeof (file), 0)) != NULL)
     {
	if (! Slrn_Signoff_String)
	  fputs ("\n", fp);

	/* If signature file already has -- \n, do not add it. */
	if (NULL == fgets (buf, sizeof (buf), sfp))
	     return 0;

	if (0 == strcmp (buf, "-- \n"))
	  {
	     if (NULL == fgets (buf, sizeof (buf), sfp))
	       {
		  slrn_fclose(sfp);
		  return 0;
	       }

	  }
	/* Apparantly some RFC suggests the -- \n. */
        fputs ("\n-- \n", fp);

	do
	  {

	     if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s", buf) < 0)
	       {
		  slrn_fclose(sfp);
		  return -1;
	       }
	  } while (NULL != fgets (buf, sizeof(buf), sfp));
        slrn_fclose(sfp);
     }
   return 0;
}
/*}}}*/

int slrn_add_custom_headers (FILE *fp, char *headers, int (*write_fun)(char *, FILE *)) /*{{{*/
{
   int n;
   char *s, *s1, ch, last_ch, *tmp=NULL;

   if (headers == NULL) return 0;

   s = slrn_skip_whitespace (headers);
   if (*s == 0) return 0;

   s1 = s;
   n = 0;
   last_ch = 0;
   while ((ch = *s1) != 0)
     {
	if (ch == '\n')
	  {
	     n++;
	     while (s1[1] == '\n') s1++;   /* skip multiple newlines */
	  }
	last_ch = ch;
	s1++;
     }

   if (write_fun != NULL)
     (*write_fun)(s, fp);
   else fputs (s, fp);

   if (last_ch != '\n')
     {
	fputc ('\n', fp);
	n++;
     }

   if (tmp != NULL)
	slrn_free(tmp);

   return n;
}
/*}}}*/

/* Returns a malloced string */
char *slrn_trim_references_header (char *line) /*{{{*/
{
#define GNKSA_LENGTH 986 /* 998 - strlen ("References: ") */
   char *buf;
   char *p, *l, *r, *nextid, *tmp;
   unsigned int len = 0, extra_whitespaces=0;

   /* Make sure line does not end in whitespace */
   (void) slrn_trim_string (line);

   if ((NULL == (l = slrn_strbyte (line, '<'))) ||
       (NULL == (r = slrn_strbyte (l+1, '>'))))
     return NULL;
   while ((NULL != (nextid = slrn_strbyte (l+1, '<'))) &&
	  (nextid < r))
     l = nextid;

   len = r - l + 1;
   if (nextid != NULL) /* Skip enough IDs to fit into our limit */
     {
	tmp=nextid;
	while (NULL != (tmp = slrn_strbyte (tmp+1, '>')))
	/* make sure that we have enough space for missing whitespaces
	 * between Message-Ids */
	  {
	     if (*(tmp+1) != ' ') extra_whitespaces++;
	  }
        nextid--;
	while (NULL != (nextid = slrn_strbyte (nextid + 1, '<')))
	  {
	     if (strlen (nextid) + extra_whitespaces + len < GNKSA_LENGTH)
	       break;
	     else
	       {
		  tmp = slrn_strbyte (nextid, '>');
		  if (*(tmp+1) != ' ') extra_whitespaces--;
	       }
	  }
     }

   /* I'd rather violate GNKSA's limit than omitting the first or last ID */
   if ((nextid == NULL) || (len >= GNKSA_LENGTH))
     {
	return slrn_safe_strmalloc(line);
     }
   buf = slrn_safe_malloc(GNKSA_LENGTH + 12 + 2);
   strncpy (buf, "References: ", 12);
   p = buf + 12;
   strncpy (p, l, len);
   p += len;
   l = nextid;

   /* Copy the rest, dropping chopped IDs */
   while (l != NULL)
     {
	if (NULL == (r = slrn_strbyte (l+1, '>')))
	  break;
	while ((NULL != (nextid = slrn_strbyte (l+1, '<'))) &&
	       (nextid < r))
	  l = nextid;
	len = r - l + 1;
	*p = ' ';
	strncpy (p+1, l, len);
	p += len + 1;
	l = nextid;
     }

   return buf;
}
/*}}}*/

/*}}}*/

static int is_empty_header (char *line) /*{{{*/
{
   char *b;

   if ((*line == ' ') || (*line == '\t')) return 0;

   b = slrn_strbyte (line, ':');
   if (b == NULL) return 0;

   b = slrn_skip_whitespace (b + 1);
   return (*b == 0);
}
/*}}}*/

static void insert_cc_post_message (char *newsgroups, FILE* ofile) /*{{{*/
{
   char *percnt, *message = Slrn_CC_Post_Message;
   if (newsgroups == NULL) return;

   do
     {
	percnt = slrn_strbyte (message, '%');
	if (percnt == NULL)
	  fputs (message, ofile);
	else if (percnt[1] == 0)
	  percnt = NULL;
	else
	  {
	     fwrite (message, 1, percnt-message, ofile);
	     if (percnt[1] == 'n')
	       fputs (newsgroups, ofile);
	     else
	       fputc (percnt[1], ofile);
	     message = percnt+2;
	  }
     } while (percnt != NULL);
   fputc ('\n', ofile);
}
/*}}}*/

static int cc_article (Slrn_Article_Type *a) /*{{{*/
{
#if defined(VMS) || !SLRN_HAS_PIPING
   return -1;
#else
#if defined(IBMPC_SYSTEM)
   char outfile [SLRN_MAX_PATH_LEN];
   char cmdbuf [2*SLRN_MAX_PATH_LEN];
#endif
   FILE *pp;
   unsigned int cc_line = 0;
   unsigned int linenum =0;
   char *l, *newsgroups = NULL;

   a->cline=a->lines;
   /* Look for CC line */
   while ((NULL != a->cline) && a->cline->flags == HEADER_LINE)
     {
	linenum++;
	if (0 == slrn_case_strncmp ( a->cline->buf,
				     "Cc: ", 4))
	  {
	     l = slrn_skip_whitespace (a->cline->buf + 4);
	     if (*l && (*l != ',')) cc_line = linenum;
	     break;
	  }
	a->cline=a->cline->next;
     }

   /* At this point, if all has gone well a->cline->buf contains the cc information */

   if (cc_line == 0)
     {
	return -1;
     }

#if defined(IBMPC_SYSTEM)
   pp = slrn_open_tmpfile (outfile, sizeof (outfile));
#else
   pp = slrn_popen (Slrn_SendMail_Command, "w");
#endif
   if (pp == NULL)
     {
	return -1;
     }

   /* FIXME: fputs can (and eventually will) fail */

   (void) fputs ("To: ", pp);
   (void) fputs (a->cline->buf + 4, pp);
   (void) fputs ("\n", pp);

   /* rewind */
   a->cline=a->lines;
   linenum = 0;

   while ((NULL != a->cline) && (a->cline->flags == HEADER_LINE  ))
     {
	linenum++;
	if (linenum == cc_line)
	  {
	     a->cline=a->cline->next;
	     continue;
	  }
	if (0 == slrn_case_strncmp ( a->cline->buf,
				     "To: ", 4))
	  {
	     a->cline=a->cline->next;
	     continue;
	  }
	if ((Slrn_Generate_Email_From == 0) &&
	    (0 == slrn_case_strncmp ( a->cline->buf,
				      "From: ", 6)))
	  {
	     a->cline=a->cline->next;
	     continue;
	  }

	/* There is some discussion of this extension to mail headers.  For
	 * now, assume that this extension will be adopted.
	 *
	 * I think it is a bad idea.  What distinguishes a mail header
	 * from a Usenet header?  --JED
	 */
	if (0 == slrn_case_strncmp ( a->cline->buf,
				     "Newsgroups: ", 12))
	  {
	     (void) fputs ("X-Posted-To: ", pp);
	     (void) fputs (a->cline->buf + 12, pp);
	     (void) fputs ("\n", pp);
	     if (newsgroups == NULL)
	       newsgroups = slrn_strmalloc (a->cline->buf + 12, 1);   /* NULL ok */

	     a->cline = a->cline->next;
	     continue;
	  }
	(void) fputs (a->cline->buf, pp);
	(void) fputs ("\n", pp);
	a->cline=a->cline->next;
     }

   (void) fputs ("\n", pp);
   a->cline=a->cline->next;

   if (newsgroups != NULL)
     {
	insert_cc_post_message (newsgroups, pp);
	slrn_free (newsgroups);
     }

   while (NULL != a->cline)
     {
	(void) fputs (a->cline->buf, pp);
	(void) fputs ("\n", pp);
	a->cline=a->cline->next;
     }
# if defined(IBMPC_SYSTEM)
   slrn_fclose (pp);
   slrn_snprintf (cmdbuf, sizeof (cmdbuf), "%s %s", Slrn_SendMail_Command, outfile);
   slrn_posix_system (cmdbuf, 0);
# else
   slrn_pclose (pp);
# endif
   return 0;
#endif /* NOT VMS */
}
/*}}}*/

/* This assumes that s is malloed and is positioned after the header colon.
 * It removes all whitespace and collapses commas.
 */
static void process_newsgroups_field (char *s, int *num_valuesp)
{
   char *p, ch;
   int num_commas = 0;
   int value_seen;
   int has_pending_comma;

   if (*s != 0)			       /* skip the leading space */
     s++;

   p = s;

   value_seen = 0;
   has_pending_comma = 0;
   while (0 != (ch = *s++))
     {
	if (isspace (ch))
	  continue;

	if (ch == ',')
	  {
	     while (*s == ',') s++;
	     if (value_seen == 0)
	       continue;

	     has_pending_comma = 1;
	     continue;
	  }

	value_seen = 1;
	if (has_pending_comma)
	  {
	     has_pending_comma = 0;
	     num_commas++;
	     *p++ = ',';
	  }
	*p++ = ch;
     }
   *num_valuesp = value_seen + num_commas;
   *p = 0;
}

/* Returns 0 if at EOF or end of header, -1 upon error, and 1 if something read */
static int vread_header_line (VFILE *vp, char **bufp, unsigned int *linenump)
{
   unsigned int vlen;
   char *buf = NULL;
   char *vline;

   *bufp = NULL;
   vline = vgets (vp, &vlen);
   if (vline == NULL)
     return 0;

   *linenump = *linenump + 1;

   if (vlen && (vline[vlen-1] == '\n'))
     vlen--;

   if (vlen == 0)
     return 0;			       /* end of header */

   if (NULL == (buf = slrn_strnmalloc (vline, vlen, 1)))
     return -1;

   while (NULL != (vline = vgets (vp, &vlen)))
     {
	char *tmp;

	if (vline[vlen-1] == '\n')
	  vlen--;

	if ((vlen == 0)
	    || ((vline[0] != ' ') && (vline [0] != '\t')))
	  break;

	/* Continuation line */
	*linenump = *linenump + 1;

	tmp = slrn_substrjoin (buf, NULL, vline, vline+vlen, "");
	slrn_free (buf);
	if (tmp == NULL)
	  return -1;

	buf = tmp;
     }

   vungets (vp);
   *bufp = buf;
   return 1;
}

static int prepare_header (VFILE *vp, unsigned int *linenum, Slrn_Article_Type *a, char *to, char *from_charset, int mail,
			   Slrn_Mime_Error_Obj **errp) /*{{{*/
{
   unsigned int lineno;
   int newsgroups_found=0, subject_found=0, followupto_found=0;
   Slrn_Mime_Error_Obj *err=NULL;
   Slrn_Mime_Error_Obj *mime_err;
   char *tmp, *system_os_name;

   system_os_name = slrn_get_os_name ();

#if SLRN_HAS_STRICT_FROM
   if (NULL != (tmp = slrn_make_from_header ()))
     {
	if (NULL != (err = slrn_mime_header_encode (&tmp, from_charset)))
	  err->lineno = 0;

	if (NULL == slrn_append_to_header (a, tmp, 0))
	  {
	     slrn_add_mime_error (err, _("Out of memory."), tmp, 0, MIME_ERROR_CRIT);
	     slrn_free (tmp);
	     goto return_error;
	  }
     }
   else
     err = slrn_add_mime_error (err, _("Could not generate From header."), NULL, 0, MIME_ERROR_CRIT);
#endif

   lineno = 0;

   while (1)
     {
	char *line, *colon;
	int status;

	status = vread_header_line (vp, &line, &lineno);
	if (status == -1)
	  {
	     err = slrn_add_mime_error (err, _("Error reading file."), line, lineno, MIME_ERROR_CRIT);
	     goto return_error;
	  }

	if (status == 0)	       /* end of header */
	  {
	     if (lineno == 1)
	       err = slrn_add_mime_error (err, _("The first line must begin with a header."), line, lineno, MIME_ERROR_CRIT);
	     break;
	  }

	if (NULL == (colon = slrn_strbyte (line, ':')))
	  err = slrn_add_mime_error(err, _("Expecting a header. This is not a header line."), line, lineno, MIME_ERROR_CRIT);

	if (!slrn_case_strncmp ( line,  "Subject:", 8))
	  {
	     if (is_empty_header (line))
	       err = slrn_add_mime_error(err, _("The Subject: header is not allowed be to empty."), line, lineno, MIME_ERROR_CRIT);
	     subject_found = 1;
	  }

	if ((!mail) && (!slrn_case_strncmp ( line,  "Newsgroups:", 11)))
	  {
	     process_newsgroups_field (line+11, &newsgroups_found);

	     if (newsgroups_found == 0)
	       err = slrn_add_mime_error(err, _("The Newsgroups header is not allowed be to empty."), line, lineno, MIME_ERROR_CRIT);
	  }

	if ((!mail) && (!slrn_case_strncmp ( line,  "Followup-To:", 12)))
	  {
	     process_newsgroups_field (line+12, &followupto_found);
	  }

	/* Remove empty header */
	if (is_empty_header (line))
	  {
	     slrn_free(line);
	     continue;
	  }

	if ((colon != NULL)
	    && (*(colon + 1) != ' '))
	  {
	     err = slrn_add_mime_error(err, _("A space must follow the ':' in a header."), line, lineno, MIME_ERROR_CRIT);
	  }

#if SLRN_HAS_STRICT_FROM
	if (!slrn_case_strncmp ( line,  "From:", 5))
	  err = slrn_add_mime_error (err, _("This news reader will not accept user generated From lines."), line, lineno, MIME_ERROR_CRIT);
#endif
#if ! SLRN_HAS_GEN_MSGID
	if (!slrn_case_strncmp ( line,  "Message-Id:", 11))
	  err = slrn_add_mime_error (err, _("This news reader will not accept user generated Message-IDs."), line, lineno, MIME_ERROR_CRIT);
#endif

	/* Check the references header.  Rumor has it that many
	 * servers choke on long references lines.  In fact, to be
	 * GNKSA compliant, references headers cannot be longer than
	 * 998 characters.  Sigh.
	 */
	if (0 == slrn_case_strncmp (line, "References: ", 12))
	  {
	     if ((tmp=slrn_trim_references_header (line)) == NULL)
	       err = slrn_add_mime_error (err, _("Error trimming References header."), line, lineno, MIME_ERROR_CRIT);
	     else
	       {
		  slrn_free (line);
		  line = tmp;
	       }
	  }

	/* prepare Cc: header:
	 * This line consists of a comma separated list of addresses.  In
	 * particular, "poster" will be replaced by 'to'. */
	if ((to != NULL)
	    && (0 == slrn_case_strncmp (line, "Cc: ", 4)))
	  {
	     unsigned int  nth = 0;
	     char *l = line + 4;
	     char *t, *buf;
	     unsigned int buflen;

	     buflen = strlen (line) + 1;   /* is big enough for any substring of line */
	     buf = slrn_safe_malloc (buflen);
	     t = tmp = slrn_safe_malloc (buflen + strlen(to) + 1);

	     strncpy(t, "Cc: ", 4);
	     t +=4;
	     while (0 == SLextract_list_element (l, nth, ',', buf, buflen))
	       {
		  char *b;

		  /* Knock whitespace from ends of string */
		  b = slrn_skip_whitespace (buf);
		  slrn_trim_string (b);

		  if (nth != 0)
		    *t++ = ',';

		  if (0 == slrn_case_strcmp (b, "poster"))
		    b = to;

		  strcpy (t, b); /*safe*/
		  t += strlen(b);
		  nth++;
	       }
	     *t = 0;
	     slrn_free(buf);
	     slrn_free (line);
	     line = tmp;
	  }

	/* encode and put into a */

	if (NULL != (mime_err = slrn_mime_header_encode (&line, from_charset)))
	  err = slrn_mime_concat_errors (err, mime_err);

	if (NULL == slrn_append_to_header (a, line, 0))
	  {
	     err = slrn_add_mime_error (err, _("Out of memory."), line, 0, MIME_ERROR_CRIT);
	     slrn_free (line);
	     goto return_error;
	  }
     }

   if ((NULL == (tmp = slrn_gen_date_header ()))
       || (NULL == slrn_append_to_header (a, tmp, 1))
       || (NULL == (tmp = slrn_strdup_printf("User-Agent: slrn/%s (%s)", Slrn_Version_String, system_os_name)))
       || (NULL == slrn_append_to_header (a, tmp, 1))
       || (NULL == slrn_append_to_header (a, NULL,0)))   /* separator */
     {
	err = slrn_add_mime_error (err, _("Out of memory."), "Headers", 0, MIME_ERROR_CRIT);
	goto return_error;
     }

   if (subject_found == 0)
     {
	err = slrn_add_mime_error (err, _("Subject header is required."), NULL, 0, MIME_ERROR_NET);
     }

   if ((!mail) && (newsgroups_found == 0))
     {
	err = slrn_add_mime_error (err, _("Newsgroups header is required."), NULL, 0, MIME_ERROR_NET);
     }

   if ((!mail) && (newsgroups_found > 4))
     err = slrn_add_mime_error (err,
				_("Please re-consider if posting to a large number of groups is necessary."),
				NULL, 0, MIME_ERROR_NET);

   if ((!mail) && (followupto_found > 1))
     err = slrn_add_mime_error (err,
				_("In most cases, setting a \"Followup-To:\" multiple groups is a bad idea."),
				NULL, 0, MIME_ERROR_NET);

   if ((!mail) && (newsgroups_found > 1) && (followupto_found == 0))
     err = slrn_add_mime_error(err,
			       _("Setting a \"Followup-To:\" is recommended when crossposting."),
			       NULL, 0, MIME_ERROR_NET);

   *linenum=lineno;

   *errp = err;
   return 0;

return_error:
   *errp = err;
   return -1;
}

/*}}}*/

static Slrn_Mime_Error_Obj *
  prepare_body (VFILE *vp, unsigned int *linenum, /*{{{*/
		Slrn_Article_Type *a, char *from_charset, int mail)
{
   char *vline, *line;
   unsigned int vlen;
   unsigned int lineno=*linenum;
   char *qs = Slrn_Quote_String;
   int qlen, not_quoted=0, sig_lines=-1, longline=0, hibin=0;
   int verbatim = 0, check_verbatim = 1;
   Slrn_Mime_Error_Obj *err = NULL;
   Slrn_Article_Line_Type *raw_line=a->raw_lines;

   if (qs == NULL) qs = ">";
   qlen = strlen (qs);

   /* put body into a->raw_lines */
   while (NULL != (vline = vgets (vp, &vlen)))
     {
	Slrn_Article_Line_Type *tmp;
	/*remove trailing \n*/
	if (vline[vlen-1] == '\n')
	  vlen--;

	line = slrn_safe_strnmalloc (vline, vlen);
	lineno++;

	if (!hibin && slrn_string_nonascii(line))
	  hibin=1;

	if (check_verbatim)
	  {
	     if (0 == strcmp (line, "#v+"))
	       verbatim++;
	     else if (0 == strcmp (line, "#v-"))
	       {
		  verbatim--;
		  if (verbatim < 0)
		    {
		       err = slrn_add_mime_error(err,
						 _("Unbalanced #v+/- verbatim marks"),
						 line, lineno, MIME_ERROR_WARN);
		       check_verbatim = 0;
		       verbatim = 0;
		    }
	       }
	  }

	if (verbatim == 0)
	  {
	     if (sig_lines != -1) sig_lines++;

	     if (0 == strcmp (line, "-- "))
	       sig_lines = 0;

	     if ((!not_quoted) &&(strncmp (line, qs, qlen)))
	       not_quoted=1;

	     if (Slrn_Reject_Long_Lines
		 && (longline == 0)
		 && (slrn_charset_strlen (line, from_charset) > 80))
	       {
		  int severity = MIME_ERROR_WARN;

		  if (Slrn_Reject_Long_Lines == 1)
		    severity = MIME_ERROR_CRIT;

		  longline = 1;
		  err = slrn_add_mime_error(err,
					    _("Please wrap lines with more than 80 characters (only first one is shown)"),
					    line, lineno, severity);
	       }
	  }

	tmp = (Slrn_Article_Line_Type *) slrn_safe_malloc(sizeof(Slrn_Article_Line_Type));

	tmp->prev = raw_line;
	if (raw_line == NULL)
	  a->raw_lines = tmp;
	else
	  raw_line->next = tmp;
	tmp->next = NULL;
	tmp->flags = 0;
	tmp->buf = line;

	raw_line = tmp;
     }

   if (sig_lines > 4)
     err = slrn_add_mime_error(err,
			       _("Please keep your signature short. 4 lines is a commonly accepted limit."),
			       NULL, lineno, MIME_ERROR_NET);
   *linenum=lineno;
   return err;
}

/*}}}*/

static void display_errors (Slrn_Mime_Error_Obj *err, int type,
			    unsigned int *linenop, int *rowp, int max_row)
{
   int row = *rowp;

   while ((err != NULL) && (row < max_row))
     {
	if (err->critical != type)
	  {
	     err = err->next;
	     continue;
	  }

	slrn_set_color (ERROR_COLOR);
	SLsmg_gotorc (row, 4);
	SLsmg_write_string (err->msg);
	row += 2;
	if (err->lineno > 0)
	  {
	     if (*linenop != 0)
	       *linenop = err->lineno;

	     SLsmg_gotorc (row, 0);
	     row += 2;
	     slrn_set_color (SUBJECT_COLOR);
	     SLsmg_printf (_("This message was generated while looking at line %d%c"), err->lineno,
			   (err->err_str == NULL) ? '.' : ':');
	  }
	else
	  {
	     if (err->err_str != NULL)
	       {
		  SLsmg_gotorc (row, 0);
		  row +=2;
		  slrn_set_color (SUBJECT_COLOR);
		  SLsmg_printf (_("This message was generated while looking at the following line"));
	       }
	  }
	if (err->err_str != NULL)
	  {
	     SLsmg_gotorc (row, 0);
	     row += 3;
	     slrn_set_color (QUOTE_COLOR);
	     SLsmg_write_string (err->err_str);
	  }
	err=err->next;
     }
   *rowp = row;
}

/* Returns -1 upon error, 0, if ok, 1 if needs repair, 2 is warning is issued */
int slrn_prepare_file_for_posting (char *file, unsigned int *line, Slrn_Article_Type *a, char *to, int mail) /*{{{*/
{
   VFILE *vp;
   Slrn_Mime_Error_Obj *mime_errors, *tmp;
   char *from_charset;
   int ret, row, has_crit, has_warn, has_net;
   int status;

   if (1 == slrn_is_hook_defined (HOOK_POST_FILE))
     {
	if ((-1 == slrn_run_hooks (HOOK_POST_FILE, 1, file))
	    || (0 != SLang_get_error ()))
	  return -1;
     }

   if (Slrn_Editor_Charset != NULL)
     from_charset = Slrn_Editor_Charset;
   else
     from_charset = Slrn_Display_Charset;

   if (NULL == (vp = vopen (file, 4096, 0)))
     {
	slrn_error (_("Unable to open %s."), file);
	return -1;
     }

   if (0 == (status = prepare_header (vp, line, a, to, from_charset, mail, &mime_errors)))
     {
	tmp = prepare_body (vp, line, a, from_charset, mail);
	if (tmp != NULL)
	  mime_errors = slrn_mime_concat_errors (mime_errors, tmp);
	tmp = slrn_mime_encode_article(a, from_charset);
	if (tmp != NULL)
	  mime_errors = slrn_mime_concat_errors (mime_errors, tmp);
     }
   vclose (vp);

   if (mime_errors == NULL)
     {
	*line = 0;
	return status;
     }

   has_crit = has_warn = has_net = 0;

   tmp = mime_errors;
   while (tmp != NULL)
     {
	switch (tmp->critical)
	  {
	   case MIME_ERROR_NET:
	     if (Slrn_Netiquette_Warnings)
	       has_net = 1;
	     break;
	   case MIME_ERROR_CRIT:
	     has_crit = 1;
	     break;
	   case MIME_ERROR_WARN:
	     has_warn = 1;
	     break;
	  }
	tmp = tmp->next;
     }

   ret = 0;
   *line = 0;

   if ((has_crit == 0) && (has_warn == 0) && (has_net == 0))
     goto free_and_return;

   if (Slrn_Batch)
     {
	if (has_crit)
	  {
	     slrn_error (_("Message is not acceptable."));
	     slrn_error (_("Reason(s):"));
	     tmp = mime_errors;
	     while (tmp != NULL)
	       {
		  if (tmp->critical == MIME_ERROR_CRIT)
		    {
		       slrn_error ("%s", tmp->msg);
		       tmp=tmp->next;
		    }
	       }
	     ret = -1;
	  }
	goto free_and_return;
     }

   Slrn_Full_Screen_Update = 1;
   slrn_set_color (0);
   SLsmg_cls ();
   row = 1;
   SLsmg_gotorc (row, 0);
   slrn_set_color (SUBJECT_COLOR);
   SLsmg_write_string (_("Your message is not acceptable for the following reason(s):"));

   if (has_crit)
     {
	ret = 1;
	row += 2;
	display_errors (mime_errors, MIME_ERROR_CRIT, line, &row, SLtt_Screen_Rows-11);

	slrn_set_color (ARTICLE_COLOR);
	SLsmg_gotorc (row, 0);
	SLsmg_write_string (_("Perhaps this error was generated because you did not separate the header"));

	row++;
	SLsmg_gotorc (row, 0);
	SLsmg_write_string (_("section from the body by a BLANK line."));
     }

   if (has_warn)
     {
	if (ret == 0)
	  ret = 2;

	row += 2;
	display_errors (mime_errors, MIME_ERROR_WARN, line, &row, SLtt_Screen_Rows-9);
     }

   if (has_net)
     {
	if (ret == 0)
	  ret = 2;

	row += 2;
	SLsmg_gotorc (row, 0);
	SLsmg_write_string (_("Your message breaks the following netiquette guidelines:"));
	row += 2;
	display_errors (mime_errors, MIME_ERROR_NET, line, &row, SLtt_Screen_Rows-9);
     }

free_and_return:

   slrn_free_mime_error (mime_errors);
   return ret;
}
/*}}}*/

int slrn_save_article_to_mail_file (Slrn_Article_Type *a, char *save_file) /*{{{*/
{
   FILE *outfp;
   time_t now;
   char save_post_file[SLRN_MAX_PATH_LEN];

   if ((save_file == NULL) || (*save_file == 0))
     return 0;

   if (NULL == (outfp = slrn_open_home_file (save_file, "a", save_post_file,
					     sizeof (save_post_file), 1)))
     {
	slrn_message_now (_("***Warning: error saving to %s"), save_post_file);
	SLang_input_pending (30);
	SLang_flush_input ();
	return 0;
     }

   time (&now);
   if ((*Slrn_User_Info.username == 0) || (*Slrn_User_Info.hostname == 0))
     fprintf (outfp, "From nobody@nowhere %s", ctime(&now));
   else
     fprintf (outfp, "From %s@%s %s", Slrn_User_Info.username, Slrn_User_Info.hostname, ctime(&now));

   a->cline=a->lines;
   while (a->cline != NULL)
     {
	if ((*(a->cline->buf) == 'F')
	    && !strncmp ("From", a->cline->buf, 4)
	    && ((unsigned char)a->cline->buf[4] <= ' '))
	  putc ('>', outfp);

	(void) fputs (a->cline->buf, outfp);
	putc ('\n', outfp);
	a->cline=a->cline->next;
     }
   putc ('\n', outfp);	       /* separator */
   return slrn_fclose (outfp);
}
/*}}}*/

static int save_failed_post (char *file, char *msg) /*{{{*/
{
   FILE *fp, *outfp;
   char filebuf[SLRN_MAX_PATH_LEN];
   char line[256];
   char *save_file;

   if (NULL == (save_file = Slrn_Failed_Post_Filename))
     save_file = SLRN_FAILED_POST_FILE;
   if (*save_file == 0)
     return 0;

   if (NULL == (fp = fopen (file, "r")))
     return -1;

   if (NULL == (outfp = slrn_open_home_file (save_file, "a", filebuf,
					     sizeof (filebuf), 1)))
     {
	slrn_message_now (_("***Warning: error writing to %s"), filebuf);
	fclose (fp);
	return -1;
     }

   slrn_error (_("%s Failed post saved in %s"),
	       (msg != NULL) ? msg : _("Posting not allowed."), filebuf);

   (void) fputs ("\n\n", outfp);

   while (NULL != (fgets (line, sizeof (line), fp)))
     {
	if (EOF == fputs (line, outfp))
	  break;
     }

   fclose (fp);
   fclose (outfp);
   return 0;
}
/*}}}*/

/*{{{ Post functions*/

/* Does not yet accept any percent escapes */
static int insert_custom_header (char *fmt, FILE *fp) /*{{{*/
{
   char ch, *fmt_conv=NULL;

   if ((fmt == NULL) || (*fmt == 0))
     return -1;

   if (slrn_test_and_convert_string(fmt, &fmt_conv, Slrn_Editor_Charset, Slrn_Display_Charset) == -1)
	return -1;

   if (fmt_conv != NULL)
	fmt = fmt_conv;

   while ((ch = *fmt) != 0)
     {
	char *s;

	if (ch != '%')
	  {
	     if (NULL == (s = slrn_strbyte (fmt, '%')))
	       return fputs (fmt, fp);
	     else
	       {
		  if (fwrite (fmt, 1, (unsigned int) (s - fmt), fp) <
		      (unsigned int) (s-fmt))
		    return -1;
		  fmt = s;
		  continue;
	       }
	  }

	fmt++;
	ch = *fmt++;

	switch (ch)
	  {
	   case '%':
	   case '\n':
	     if (EOF == fputc (ch, fp))
	       return -1;
	     break;
	  }
     }
   slrn_free(fmt_conv);

   return 0;
}
/*}}}*/

#if SLRN_HAS_CANLOCK
/* This function returns a malloced string or NULL on failure. */
static char *gen_cancel_lock (char *msgid, char *file) /*{{{*/
{
   FILE *cansecret;
   char *buf, *canlock;
   unsigned int filelen;
   char canfile[SLRN_MAX_PATH_LEN];

   cansecret = slrn_open_home_file (file, "r", canfile, SLRN_MAX_PATH_LEN, 0);
   if (cansecret == NULL)
     {
	slrn_error (_("Cannot open file: %s"), file);
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
   (void) fclose(cansecret);

# if 0
   canlock = md5_lock(buf, filelen, msgid, strlen(msgid));
# else /* by default we use SHA-1 */
   canlock = sha_lock ((unsigned char *) buf, filelen, (unsigned char *)msgid, strlen(msgid));
# endif
   slrn_free (buf);
   return canlock;
}
/*}}}*/

static Slrn_Article_Line_Type *add_cancel_lock_to_header (Slrn_Article_Type *a, char *msgid, char *file)
{
   char *canlock;
   Slrn_Article_Line_Type *l;

   if (NULL == (canlock = gen_cancel_lock (msgid, file)))
     return NULL;

   l = slrn_append_header_keyval (a, "Cancel-Lock", canlock);
   slrn_free (canlock);

   return l;
}
#endif

static Slrn_Article_Line_Type *next_header_line (Slrn_Article_Line_Type *line)
{
   line = line->next;
   while ((line != NULL)
	  && (line->flags & HEADER_LINE)
	  && ((line->buf[0] == ' ') || (line->buf[0] == '\t')))
     line = line->next;
   return line;
}

static Slrn_Article_Line_Type *find_header_line (Slrn_Article_Type *a, char *key)
{
   Slrn_Article_Line_Type *line;
   unsigned int len;

   len = strlen (key);
   line = a->lines;
   while ((line != NULL) && (line->flags & HEADER_LINE))
     {
	if (0 == slrn_case_strncmp (line->buf, key, len))
	  return line;
	line = line->next;
     }
   return NULL;
}

static int post_line (Slrn_Article_Line_Type *line)
{
   if (line->buf[0] == '.')
     Slrn_Post_Obj->po_puts("."); /* double leading dots for posting */

   (void) Slrn_Post_Obj->po_puts(line->buf);
   (void) Slrn_Post_Obj->po_puts("\n");
   return 0;
}

/* Returns -1 upon fatal error, 0 if not posted, or 1 if ok */
static int post_article (Slrn_Article_Type *a, char **errmsgp)
{
   Slrn_Article_Line_Type *line;
   int has_msgid = 0;
   char *msgid = NULL;
   int status;

   *errmsgp = NULL;

   slrn_message_now (_("Posting ..."));

   line = find_header_line (a, "Message-ID: ");
   if (line != NULL)
     {
	msgid = slrn_strmalloc (line->buf+12, 0);
	if (msgid == NULL)
	  {
	     *errmsgp = _("Out of memory.");
	     return -1;
	  }
	has_msgid = 1;
     }

   status = Slrn_Post_Obj->po_start ();
   if (status != CONT_POST)
     {
	slrn_free (msgid);	       /* NULL ok */
	*errmsgp = (status != -1)
	  ?  _("Posting not allowed.") : _("Could not reach server.");
	return -1;
     }

   if (has_msgid == 0)
     {
	/* Generate the message-id only after po_start has been called */
	if (-1 == create_message_id (&msgid))
	  {
	     *errmsgp = _("Could not generate Message-ID.");
	     return -1;
	  }

	if ((msgid != NULL)
	    && (NULL == slrn_append_header_keyval (a, "Message-ID", msgid)))
	  {
	     slrn_free (msgid);	       /* NULL ok */
	     *errmsgp = _("Unable to append a header line");
	     return -1;
	  }
     }

   /* Note: msgid could be NULL if it is to be provided by the server */
#if SLRN_HAS_CANLOCK
   if ((msgid != NULL)
       && (0 != *Slrn_User_Info.cancelsecret)
       && (NULL == add_cancel_lock_to_header (a, msgid, Slrn_User_Info.cancelsecret)))
     {
	slrn_free (msgid);	       /* NULL ok */
	*errmsgp = _("Failed to add cancel-lock header");
	return -1;
     }
#endif /* SLRN_HAS_CANLOCK */

   slrn_free (msgid);/* NULL ok */
   msgid = NULL;

   line = a->lines;
   while ((line != NULL)
	  && (line->flags & HEADER_LINE))
     {
	if (!Slrn_Generate_Date_Header
	    && (0 == slrn_case_strncmp ("Date: ", line->buf, 4)))
	  {
	     /* skip generated date header for posting */
	     line = next_header_line (line);
	     continue;
	  }

	if (0 == slrn_case_strncmp ("Cc: ", line->buf, 4))
	  {
	     line = next_header_line (line);
	     continue;
	  }

	(void) post_line (line);
	line = line->next;
     }

   /* Now the body */
   if ((line != NULL)
       && (line->buf[0] != 0))
     {
	*errmsgp = "Internal Error: Expected to see header-separation line";
	return -1;
     }

   while (line != NULL)
     {
	(void) post_line (line);
	line = line->next;
     }

   if (0 == Slrn_Post_Obj->po_end ())
     {
	slrn_message (_("Posting...done."));
	return 0;
     }

   return -1;
}

#define POST_CONFIRM_ERROR     -1
#define POST_CONFIRM_DELETE	1
#define POST_CONFIRM_POST	2
#define POST_CONFIRM_NOPOST	3
#define POST_CONFIRM_POSTPONE	4

/* This function returns:
 * -1 => user does not want to post article
 *  0 => user wants to delete postponed article
 *  1 => user postponed the article
 *  2 => user wants to post article */
/* Be warned: This function now also transfers "file" into "a", which will be
 * used for the actual posting process later on. */
static int post_user_confirm (char *file, int is_postponed, unsigned int linenum) /*{{{*/
{
   int rsp;
   char *responses;
   int filter_hook;

   if (Slrn_Batch)
     return POST_CONFIRM_POST;

   filter_hook = slrn_is_hook_defined (HOOK_POST_FILTER);

   while (1)
     {
	if (filter_hook != 0)
	  {
	     if (is_postponed)
	       {
  /* Note to translators:
   * In the following strings, "yY" will mean "yes", "nN" is for "no",
   * "eE" for "edit", "sS" for "postpone", "dD" for "delete" and "fF"
   * for "filter". Please use the same letter for the same field everywhere,
   * or your users will get unexpected results. As always, you can't use the
   * default letters for other fields than those they originally stood for.
   * Be careful not to change the length of these strings!
   */
		  responses = _("yYnNeEsSdDfF");
		  if (strlen (responses) != 12)
		       responses = "";
		  rsp = slrn_get_response ("yYnNeEsSDdFf", responses, _("Post the message? \001Yes, \001No, \001Edit, po\001Stpone, \001Delete, \001Filter"));
	       }
	     else
	       {
		  responses = _("yYnNeEsSfF");
		  if (strlen (responses) != 10)
		       responses = "";
		  rsp = slrn_get_response ("yYnNeEsSFf", responses, _("Post the message? \001Yes, \001No, \001Edit, po\001Stpone, \001Filter"));
	       }
	  }
	else
	  {
	     if (is_postponed)
	       {
		  responses = _("yYnNeEsSdD");
		  if (strlen (responses) != 10)
		    responses = "";
		  rsp = slrn_get_response ("yYnNeEsSDd", responses, _("Post the message? \001Yes, \001No, \001Edit, po\001Stpone, \001Delete"));
	       }
	     else
	       {
		  responses = _("yYnNeEsS");
		  if (strlen (responses) != 8)
		    responses = "";
		  rsp = slrn_get_response ("yYnNeEsS", responses, _("Post the message? \001Yes, \001No, \001Edit, po\001Stpone"));
	       }
	  }

	rsp = slrn_map_translated_char ("yYnNeEsSdDfF", _("yYnNeEsSdDfF"), rsp);

	switch (rsp | 0x20)
	  {
	   case 'n':
	     return POST_CONFIRM_NOPOST;

	   case 's':
	     if (is_postponed
		 || (0 == postpone_file (file)))
	       return POST_CONFIRM_POSTPONE;

	     /* Instead of returning, let user have another go at it */
	     slrn_sleep (1);
	     break;

	   case 'd':
	     rsp = slrn_get_yesno_cancel (1, "%s", _("Sure you want to delete it"));
	     if (rsp == 0)
	       continue;
	     if (rsp == -1)
	       return POST_CONFIRM_POSTPONE;
	     /* Calling routine will delete it. */
	     return POST_CONFIRM_DELETE;

	   case 'y':
	     return POST_CONFIRM_POST;

	   case 'e':
	     if (slrn_edit_file (Slrn_Editor_Post, file, linenum, 0) < 0)
	       return POST_CONFIRM_ERROR;
	     break;

	   case 'f':
	     if (filter_hook != 0)
	       {
                  slrn_run_hooks (HOOK_POST_FILTER, 1, file);
		  if (SLang_get_error ())
		    filter_hook = 0;
	       }
	  }
     }
}
/*}}}*/

/* Returns -1 upon error, 0 if not edit, 1 is edit */
static int request_user_edit (char *file, unsigned int line, int is_serious)
{
   char *responses;
   int rsp;

   SLtt_beep ();

   if (is_serious)
     {
	/* Note to translators:
	 * In the next two strings, "yY" is "yes", "eE" is "edit", "nN" is "no",
	 * "cC" is "cancel" and "fF" means "force". The usual rules apply.
	 */
	responses = _("yYeEnNcC");
	if (strlen (responses) != 8)
	  responses = "";
	rsp = slrn_get_response ("yYEenNcC\007", responses, _("re-\001Edit,  or \001Cancel"));
     }
   else
     {
	responses = _("yYeEnNcCfF");
	if (strlen (responses) != 10)
	  responses = "";
	rsp = slrn_get_response ("EeyYnNcC\007Ff", responses, _("re-\001Edit, \001Cancel, or \001Force the posting (not recommended)"));
     }

   rsp = slrn_map_translated_char ("yYeEnNcCfF", _("yYeEnNcCfF"), rsp);

   switch (rsp)
     {
      case 'f': case 'F':
	return 0;

      case 'y': case 'Y':
      case 'e': case 'E':
	if (slrn_edit_file (Slrn_Editor_Post, file, line, 0) < 0)
	  return -1;
	return 1;

      default:
	break;
     }
   return -1;
}

/* This function returns 1 if postponed, 0 upon sucess, -1 upon error,
 * and 2 if the user declined to post it.
 */
int slrn_post_file (char *file, char *to, int is_postponed) /*{{{*/
{
   Slrn_Article_Type *a = NULL;
   int rsp;
   int status;
   char *errmsg = NULL;
   char *responses;

   while (1)
     {
	unsigned int linenum = 1;
	errmsg = NULL;

	if (a != NULL)
	  {
	     slrn_art_free_article (a);
	     a = NULL;
	  }

	switch (post_user_confirm (file, is_postponed, linenum))
	  {
	   case POST_CONFIRM_POST:
	     break;

	   case POST_CONFIRM_DELETE:
	     (void) slrn_delete_file (file);
	     return 0;

	   case POST_CONFIRM_POSTPONE:
	     return 1;

	   case POST_CONFIRM_NOPOST:
	     return 2;

	   case POST_CONFIRM_ERROR:
	   default:
	     goto return_error;
	  }

	a = (Slrn_Article_Type*) slrn_malloc (sizeof(Slrn_Article_Type), 1, 1);
	if (a == NULL)
	  goto return_error;

	status = slrn_prepare_file_for_posting (file, &linenum, a, to, 0);

	if (status == -1)
	  goto return_error;

	if (status != 0)
	  {
	     status = request_user_edit (file, linenum, status == 1);
	     if (status == -1)
	       goto return_error;

	     if (status == 1)
	       continue;

	     /* forced -- drop */
	  }

	if (0 == post_article (a, &errmsg))
	  break;

	if (Slrn_Batch)
	  goto return_error;

	slrn_smg_refresh ();
	slrn_sleep (2);
	slrn_clear_message ();
	SLang_set_error (0);

	/* Note to translators: Here, "rR" is "repost", "eE" is "edit" and
	 * "cC" is "cancel". Don't change the length of the string; you
	 * cannot re-use any of the default characters for different fields. */
	responses = _("rReEcC");
	if (strlen (responses) != 6)
	  responses = "";

	rsp = slrn_get_response ("rReEcC\007", responses, _("Select one: \001Repost, \001Edit, \001Cancel"));
	if (rsp == 7)
	  rsp = 'c';

	switch (slrn_map_translated_char ("rReEcC", responses, rsp))
	  {
	   default:
	     goto return_error;

	   case 'e': case 'E':
	     if (slrn_edit_file (Slrn_Editor_Post, file, 1, 0) < 0)
	       goto return_error;
	     break;

	   case 'r': case 'R':
	     break;		       /* try again */
	  }
     }

   /* get here if it posted ok */

   if (NULL != find_header_line (a, "Cc: "))
     (void) cc_article (a);

   if (-1 == slrn_save_article_to_mail_file (a, Slrn_Save_Posts_File))
     {
	slrn_art_free_article(a);
	return -1;
     }

   slrn_art_free_article(a);
   return 0;

return_error:
   (void) save_failed_post (file, errmsg);
   slrn_art_free_article(a);
   return -1;
}

/*}}}*/

/* Returns -1 upon error, 0 if posted, 1 if postponed, 2 if user
 * declined to post the file
 */
int slrn_post (char *newsgroup, char *followupto, char *subj) /*{{{*/
{
   FILE *fp;
   char file[SLRN_MAX_PATH_LEN];
   unsigned int header_lines;
   int ret;

   if (Slrn_Use_Tmpdir)
     fp = slrn_open_tmpfile (file, sizeof (file));
   else fp = slrn_open_home_file (SLRN_ARTICLE_FILENAME, "w", file,
				  sizeof (file), 0);

   if (fp == NULL)
     {
	slrn_error (_("Unable to create %s."), file);
	return -1;
     }

   header_lines = 8;
   fprintf (fp, "Newsgroups: %s\n", newsgroup);
#if ! SLRN_HAS_STRICT_FROM
     {
	char *from;

	if (NULL == (from = slrn_make_from_header ()))
	  return -1;
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s\n", from) <0)
	  {
	     slrn_free(from);
	     return -1;
	  }
	slrn_free(from);
	header_lines++;
     }
#endif
   if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "Subject: %s\n", subj)<0)
	return -1;

   if (Slrn_User_Info.org != NULL)
     {
	header_lines++;
	if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset,"Organization: %s\n",Slrn_User_Info.org) <0)
	     return -1;
     }

   if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "Reply-To: %s\n", Slrn_User_Info.replyto) < 0)
	return -1;
   fprintf (fp, "Followup-To: %s\nKeywords: \nSummary: \n", followupto);

   if ((ret = slrn_add_custom_headers (fp, Slrn_Post_Custom_Headers,
		       insert_custom_header)) == -1)
	return -1;
   else
	header_lines += ret;

   (void) fputs ("\n", fp);

   if (slrn_add_signature (fp) == -1)
	return -1;
   slrn_fclose (fp);

   if (slrn_edit_file (Slrn_Editor_Post, file, header_lines, 1) >= 0)
     ret = slrn_post_file (file, NULL, 0);
   else
     ret = -1;

   if (Slrn_Use_Tmpdir) (void) slrn_delete_file (file);
   return ret;
}
/*}}}*/

/*}}}*/

/*{{{ functions for postponing */

static int get_postpone_dir (char *dirbuf, size_t n) /*{{{*/
{
   char *dir;

   if (Slrn_Postpone_Dir != NULL)
     dir = Slrn_Postpone_Dir;
   else
     dir = "News/postponed";

   slrn_make_home_dirname (dir, dirbuf, n);
   switch (slrn_file_exists (dirbuf))
     {
      case 0:
	if (slrn_get_yesno (1, _("Do you want to create directory %s"), dirbuf))
	  {
	     if (-1 == slrn_mkdir (dirbuf))
	       {
		  slrn_error_now (2, _("Unable to create directory. (errno = %d)"), errno);
		  slrn_clear_message ();
		  return -1;
	       }
	  }
	else
	  {
	     slrn_error_now (1, _("Aborted on user request."));
	     slrn_clear_message ();
	     return -1;
	  }
	break;
      case 1:
	slrn_error_now (2, _("postpone_directory points to a regular file (not a directory)"));
	slrn_clear_message ();
	return -1;
     }

   return 0;
}
/*}}}*/

static int postpone_file (char *file) /*{{{*/
{
#ifdef VMS
   slrn_error (_("Not implemented yet. Sorry"));
   return -1;
#else
   char dir [SLRN_MAX_PATH_LEN];
   char dirfile [SLRL_DISPLAY_BUFFER_SIZE + 256];

   if (-1 == get_postpone_dir (dir, sizeof (dir)))
     return -1;

   while (1)
     {
	int status;

	if (-1 == slrn_dircat (dir, NULL, dirfile, sizeof (dirfile)))
	  return -1;

	if (-1 == slrn_read_filename (_("Save to: "), NULL, dirfile, 1, -1))
	  return -1;

	status = slrn_file_exists (dirfile);
	if (status == 1)
	  {
	     status = slrn_get_yesno_cancel (1, "%s", _("File exists, overwrite"));
	     if (status == 0)
	       continue;
	     if (status == 1)
	       break;

	     return -1;
	  }

	if (status == 0)
	  break;
	if (status == 2)
	  slrn_error_now (2, _("You must specify a name"));
	else
	  slrn_error_now (2, _("Illegal filename. Try again"));
     }

   if (-1 == slrn_move_file (file, dirfile))
     return -1;

   return 0;
#endif
}
/*}}}*/

void slrn_post_postponed (void) /*{{{*/
{
   char dir [SLRN_MAX_PATH_LEN];
   char *file;

   if (-1 == get_postpone_dir (dir, sizeof (dir)))
     return;

   file = slrn_browse_dir (dir);
   if (file == NULL)
     return;

   if (1 != slrn_file_exists (file))
     {
	slrn_error (_("%s: not a regular file"), file);
	return;
     }

   if (0 == slrn_post_file (file, NULL, 1))
     slrn_delete_file (file);

   slrn_free (file);
}
/*}}}*/

/*}}}*/
