/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999 John E. Davis <davis@space.mit.edu>
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
 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
static char *create_message_id (int *error)/*{{{*/
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

   *error=0; /* default: create_message_id() has success*/

   if (Slrn_Use_Recom_Id) /* Try to find the Id the server recommends*/
     {
	msgid = Slrn_Post_Obj->po_get_recom_id ();
	if (msgid != NULL)
	  return msgid;
     }

#if ! SLRN_HAS_GEN_MSGID
   return NULL;
#else
   if (Slrn_Generate_Message_Id == 0)
     return NULL;
   
   while (1)
     {
	if ((Slrn_User_Info.posting_host == NULL)
	    || ((time_t) -1 == time ((time_t *)&now)))
	  return NULL;

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
		 && (NULL == strchr ("<>()@,;:\\\"[]", ch)))
	       *b++ = ch;
	     t++;
	  }
	if (*(b-1) == '.') /* may not be last character, so append '_' */
	  *b++ = '_';
     }
   *b = 0;

   malloc_len=(strlen(baseid)+strlen(Slrn_User_Info.posting_host)+8);
   if ((msgid=slrn_malloc (malloc_len,0,1)) == NULL)
     {
	*error=1;
	return NULL;
     }
   slrn_snprintf(msgid, malloc_len,"<slrn%s@%s>", baseid, Slrn_User_Info.posting_host);

   return msgid;
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
		  (int) tz / 60, (int) abs (tz) % 60);
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

char *slrn_trim_references_header (char *line) /*{{{*/
{
#define GNKSA_LENGTH 986 /* 998 - strlen ("References: ") */
   char *buf;
   char *p, *l, *r, *nextid, *tmp;
   unsigned int len = 0, extra_whitespaces=0;

   /* Make sure line does not end in whitespace */
   (void) slrn_trim_string (line);

   if ((NULL == (l = slrn_strchr (line, '<'))) ||
       (NULL == (r = slrn_strchr (l+1, '>'))))
     return NULL;
   while ((NULL != (nextid = slrn_strchr (l+1, '<'))) &&
	  (nextid < r))
     l = nextid;


   len = r - l + 1;
   if (nextid != NULL) /* Skip enough IDs to fit into our limit */
     {
	tmp=nextid;
	while (NULL != (tmp = slrn_strchr (tmp+1, '>')))
	/* make sure that we have enough space for missing whitespaces
	 * between Message-Ids */
	  {
	     if (*(tmp+1) != ' ') extra_whitespaces++;
	  }
        nextid--;
	while (NULL != (nextid = slrn_strchr (nextid + 1, '<')))
	  {
	     if (strlen (nextid) + extra_whitespaces + len < GNKSA_LENGTH)
	       break;
	     else
	       {
		  tmp = slrn_strchr (nextid, '>');
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
	if (NULL == (r = slrn_strchr (l+1, '>')))
	  break;
	while ((NULL != (nextid = slrn_strchr (l+1, '<'))) &&
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

   b = slrn_strchr (line, ':');
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
	percnt = slrn_strchr (message, '%');
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
	if (0 == slrn_case_strncmp ((unsigned char *) a->cline->buf,
				    (unsigned char *) "Cc: ", 4))
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

   fputs ("To: ", pp);
   fputs (a->cline->buf + 4, pp);
   fputs ("\n", pp);
   
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
	if (0 == slrn_case_strncmp ((unsigned char *) a->cline->buf,
				    (unsigned char *) "To: ", 4))
	  {
	     a->cline=a->cline->next;
	     continue;
	  }
	if ((Slrn_Generate_Email_From == 0) &&
	    (0 == slrn_case_strncmp ((unsigned char *) a->cline->buf,
				     (unsigned char *) "From: ", 6)))
	  {
	     a->cline=a->cline->next;
	     continue;
	  }

	/* There is some discussion of this extension to mail headers.  For
	 * now, assume that this extension will be adopted.
	 */
	if (0 == slrn_case_strncmp ((unsigned char *) a->cline->buf,
				    (unsigned char *) "Newsgroups: ", 12))
	  {
	     fputs ("X-Posted-To: ", pp);
	     fputs (a->cline->buf + 12, pp);
	     fputs ("\n", pp);
	     if (newsgroups == NULL)
	       newsgroups = slrn_strmalloc (a->cline->buf + 12, 1);
	     a->cline=a->cline->next;
	     continue;
	  }
	fputs (a->cline->buf, pp);
	fputs ("\n", pp);
	a->cline=a->cline->next;
     }

   fputs ("\n", pp);
   a->cline=a->cline->next;

   insert_cc_post_message (newsgroups, pp);
   slrn_free (newsgroups);

   while (NULL != a->cline)
     {
	fputs (a->cline->buf, pp);
	fputs ("\n", pp);
	a->cline=a->cline->next;
     }
# if defined(IBMPC_SYSTEM)
   slrn_fclose (pp);
   slrn_snprintf (buf, sizeof (buf), "%s %s", Slrn_SendMail_Command, outfile);
   slrn_posix_system (buf, 0);
# else
   slrn_pclose (pp);
# endif
   return 0;
#endif /* NOT VMS */
}
/*}}}*/

/* Returns -1 upon error, 0, if ok, 1 if needs repair, 2 is warning is issued */
static Slrn_Mime_Error_Obj *prepare_header (VFILE *vp, int *linenum, Slrn_Article_Type *a, char *to, char *from_charset, int mail) /*{{{*/
{
   char *vline;
   unsigned int vlen;
   char *tmp=NULL;
   int lineno=0;
   int newsgroups_found=0, subject_found=0, followupto_found=0;
   Slrn_Mime_Error_Obj *err=NULL;
   Slrn_Mime_Error_Obj *mime_err;
   char ch;
   char *colon;
   char *system_os_name;

   system_os_name = slrn_get_os_name ();

#if SLRN_HAS_STRICT_FROM
   if (NULL != (tmp = slrn_make_from_string ()))
     {
	if ((err = slrn_mime_header_encode(&tmp, from_charset)) != NULL)
	  {
	     err->lineno=0;
	  }
	a->cline=a->lines = (Slrn_Article_Line_Type *) slrn_safe_malloc(sizeof(Slrn_Article_Line_Type),1,1);
	a->lines->prev = NULL;
	a->cline->flags=HEADER_LINE;
	a->cline->buf=tmp;
	tmp=NULL;
     }
   else
     {
	slrn_malloc_mime_error(&ret, &err, _("Could not generate From header."), NULL, 0, MIME_ERROR_CRIT);
     }
#endif
   /* scan the header */
   while (NULL != (vline = vgets (vp, &vlen)))
     {
	char *line;
	/* remove trailing \n */
	if (vline[vlen-1] == '\n')
	  vlen--;
	
	line = slrn_strnmalloc(vline, vlen, 1);
	
	ch = *line;
	lineno++;
	if ((ch == ' ') || (ch == '\t') || (ch == '\0'))
	  {
	     if (lineno == 1)
	       {
		  slrn_malloc_mime_error(&err, _("The first line must begin with a header."), line, lineno, MIME_ERROR_CRIT);
	       }
	     if (ch == '\0')
	       {
		  slrn_free(line);
		  break; /* header ends */
	       }
	     
	     slrn_free(line);
	     continue;
	  }

	if (NULL == (colon = slrn_strchr (line, ':')))
	  {
	     slrn_malloc_mime_error(&err, _("Expecting a header. This is not a header line."), line, lineno, MIME_ERROR_CRIT);
	  }

	if (!slrn_case_strncmp ((unsigned char *) line, (unsigned char *) "Subject:", 8))
	  {
	     if (is_empty_header (line))
	       {
		  slrn_malloc_mime_error(&err, _("The Subject: header is not allowed be to empty."), line, lineno, MIME_ERROR_CRIT);
	       }
	     subject_found = 1;
	  }

	if ((!mail) && (!slrn_case_strncmp ((unsigned char *) line, (unsigned char *) "Newsgroups:", 11)))
	  {
	     char *p = line;
	     if (is_empty_header (line))
	       {
		  slrn_malloc_mime_error(&err, _("The Newsgroups header is not allowed be to empty."), line, lineno, MIME_ERROR_CRIT);
	       }
	     newsgroups_found = 1;
	     while (*(++p))
	       {
		  if (*p == ',') newsgroups_found++;
	       }
	  }

	if ((!mail) && (!slrn_case_strncmp ((unsigned char *) line, (unsigned char *) "Followup-To:", 12))
	    && (!is_empty_header (line)))
	  {
	     char *p = line;
	     followupto_found = 1;
	     while (*(++p))
	       {
		  if (*p == ',') followupto_found++;
	       }
	  }

	/* Remove empty header */
	if (is_empty_header (line))
	  {
	     slrn_free(line);
	     continue;
	  }

	if (*(colon + 1) != ' ')
	  {
	     slrn_malloc_mime_error(&err, _("A space must follow the ':' in a header."), line, lineno, MIME_ERROR_CRIT);
	  }

#if SLRN_HAS_STRICT_FROM
	if (!slrn_case_strncmp ((unsigned char *) line, (unsigned char *) "From:", 5))
	  {
	     slrn_malloc_mime_error(&err, _("This news reader will not accept user generated From lines."), line, lineno, MIME_ERROR_CRIT);
	  }
#endif
#if ! SLRN_HAS_GEN_MSGID
	if (!slrn_case_strncmp ((unsigned char *) line, (unsigned char *) "Message-Id:", 11))
	  {
	     slrn_malloc_mime_error(&err, _("This news reader will not accept user generated Message-IDs."), line, lineno, MIME_ERROR_CRIT);
	  }
#endif
	/* Check the references header.  Rumor has it that many
	 * servers choke on long references lines.  In fact, to be
	 * GNKSA compliant, references headers cannot be longer than
	 * 998 characters.  Sigh.
	 */
	if (0 == slrn_case_strncmp ((unsigned char *) line,
				    (unsigned char *)"References: ",
					12))
	  {
	     if ((tmp=slrn_trim_references_header (line)) == NULL)
	       {
		  slrn_malloc_mime_error(&err, _("Error trimming References header."), line, lineno, MIME_ERROR_CRIT);
	       }
	  }
   
	/* prepare Cc: header:
	 * This line consists of a comma separated list of addresses.  In
	 * particular, "poster" will be replaced by 'to'. */
	if ((to != NULL) && (0 == slrn_case_strncmp ((unsigned char *) line,
						     (unsigned char *)"Cc: ",
						     4)))
	  {
	     unsigned int  nth = 0;
	     char *l = line + 4;
	     char *t, *buf;
	     
	     buf=slrn_safe_malloc(strlen(line));
	     t=tmp=slrn_safe_malloc(strlen(line) + strlen(to)+1);
	     strncpy(t, "Cc: ", 4);
	     t +=4;
	     while (0 == SLextract_list_element (l, nth, ',', buf, sizeof (buf)))
	       {
		  char *b;

		  /* Knock whitespace from ends of string */
		  b = slrn_skip_whitespace (buf);
		  slrn_trim_string (b);

		  if (nth != 0)
		       *(t++)= ',';

		  if (0 == slrn_case_strcmp ((unsigned char *)b, (unsigned char *)"poster"))
		    {
		       strcpy(t, to); /*safe*/
		       t += strlen(to);
		    }
		  else
		    {
		       strcpy(t, b);
		       t += strlen(b);
		    }
		  nth++;
	       }
	     *t='\0';
	     slrn_free(buf);
	  }
	/* encode and put into a */
	if (tmp == NULL)
	     tmp=line;
	else
	     slrn_free(line);
	
	if ((mime_err = slrn_mime_header_encode(&tmp, from_charset)) != NULL)
	  {
	     if (err == NULL)
		  err=mime_err;
	     else
	       {
		  err->next=mime_err;
		  mime_err->prev=err;
		  err=err->next;
		  err->lineno=lineno;
	       }
	  }
	
	if (a->lines == NULL)
	  {
	     a->cline=a->lines = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
	     a->lines->prev = NULL;
	  }
	else
	  {
	     a->cline->next = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
	     a->cline->next->prev = a->cline;
	     a->cline=a->cline->next;
	  }
	a->cline->flags=HEADER_LINE;
	a->cline->buf=tmp;
	tmp=NULL;
     } /*while (NULL != (vline = vgets (vp, &vlen)))*/

   /* Insert other headers */
   a->cline->next = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
   a->cline->next->prev = a->cline;
   a->cline=a->cline->next;
   a->cline->flags=HEADER_LINE;
   a->cline->buf=slrn_gen_date_header();
   
   a->cline->next = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
   a->cline->next->prev = a->cline;
   a->cline=a->cline->next;
   a->cline->flags=HEADER_LINE;
   a->cline->buf = slrn_strdup_printf("User-Agent: slrn/%s (%s)", Slrn_Version, system_os_name);
   
   /* Insert empty line between headers and body */
   a->raw_lines = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
   a->raw_lines->prev = NULL;
   a->raw_lines->buf=slrn_safe_strmalloc("");

   if (subject_found == 0)
     {
	slrn_malloc_mime_error(&err, _("Subject header is required."), NULL, 0, MIME_ERROR_NET);
     }
   if ((!mail) && (newsgroups_found == 0))
     {
	slrn_malloc_mime_error(&err, _("Newsgroups header is required."), NULL, 0, MIME_ERROR_NET);
     }
   if ((!mail) && (newsgroups_found > 4))
     slrn_malloc_mime_error(&err,
     _("Please re-consider if posting to a large number of groups is necessary."),
     NULL, 0, MIME_ERROR_NET);
   if ((!mail) && (followupto_found > 1))
     slrn_malloc_mime_error(&err,
	       _("In most cases, setting a \"Followup-To:\" multiple groups is a bad idea."),
		NULL, 0, MIME_ERROR_NET);
   if ((!mail) && (newsgroups_found > 1) && (followupto_found == 0))
     slrn_malloc_mime_error(&err,
	       _("Setting a \"Followup-To:\" is recommended when crossposting."),
      NULL, 0, MIME_ERROR_NET);

   *linenum=lineno;
   return err;
}

/*}}}*/

static Slrn_Mime_Error_Obj *prepare_body (VFILE *vp, int *linenum, /*{{{*/
	  Slrn_Article_Type *a, char *from_charset, int mail)
{
   char *vline, *line;
   unsigned int vlen;
   char *tmp=NULL;
   int lineno=*linenum;
   char *qs = Slrn_Quote_String;
   int qlen, not_quoted=0, sig_lines=-1, longline=0, hibin=0;
   Slrn_Mime_Error_Obj *err = NULL;
   Slrn_Mime_Error_Obj *mime_err;
   Slrn_Article_Line_Type *cline=a->raw_lines;


   if (qs == NULL) qs = ">";
   qlen = strlen (qs);

   /* put body into a->raw_lines */
   while (NULL != (vline = vgets (vp, &vlen)))
     {
	/*remove trailing \n*/
	if (vline[vlen-1] == '\n')
	  vlen--;
	
	line = slrn_strnmalloc(vline, vlen, 1);
	lineno++;
	
	if (!hibin && slrn_string_nonascii(line))
	     hibin=1;
	if (sig_lines != -1) sig_lines++;

	if ((!not_quoted) &&(strncmp (line, qs, qlen)))
	     not_quoted=1;

	if (0 == strcmp (line, "-- "))
	     sig_lines = 0;

	if ( (Slrn_Netiquette_Warnings != 0) || (Slrn_Reject_Long_Lines != 0))
	  {
	     if ( !longline && (strlen (line) > 80))
	       {
		   slrn_malloc_mime_error(&err,
			     _("Lines with more than 80 characters generally need to be wrapped. (only first one is shown)"),
			     NULL, lineno, MIME_ERROR_NET);
	       }
	  }


	cline->next = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
        cline->next->prev = cline;
	cline = cline->next;
	cline->buf = line;
     }
   if (sig_lines > 4)
	slrn_malloc_mime_error(&err,
		  _("Please keep your signature short. 4 lines is a commonly accepted limit."), NULL, lineno, MIME_ERROR_NET);

   if ((mime_err = slrn_mime_encode_article(a, &hibin, from_charset)) != NULL)
     {
	if (err == NULL)
	     err = mime_err;
	else
	  {
	     err->next= mime_err;
	     mime_err->prev=err;
	  }
     }
   *linenum=lineno;
   return err;
}

/*}}}*/

/* Returns -1 upon error, 0, if ok, 1 if needs repair, 2 is warning is issued */
int slrn_prepare_file_for_posting (char *file, int *linenum, Slrn_Article_Type *a, char *to, int mail) /*{{{*/
{
   VFILE *vp;
   int i=0;
   Slrn_Mime_Error_Obj *err=NULL, *warn=NULL, *net=NULL, *ret, *tmp, **e_ptr;
   char *from_charset;
   int iret;

   if (Slrn_Editor_Charset != NULL)
	from_charset = Slrn_Editor_Charset;
   else
	from_charset = Slrn_Display_Charset;
   
   if (NULL == (vp = vopen (file, 4096, 0)))
     {
	slrn_error (_("Unable to open %s."), file);
	return -1;
     }

   ret = prepare_header (vp, linenum, a, to, from_charset, mail);

   if (ret == NULL)
	ret = prepare_body (vp, linenum, a, from_charset, mail);
   else
     {
	while(ret->next != NULL) ret=ret->next;
	ret->next = prepare_body (vp, linenum, a, from_charset, mail);
     }
   vclose (vp);

   /* sort errors/warnings etc */
   if (ret != NULL)
     {
	while(ret->prev != NULL) ret=ret->prev; /*rewind*/
	while (ret != NULL)
	  {
	     tmp = ret;
	     ret = tmp->next;
	     tmp->next=NULL;
	     switch (tmp->critical)
	       {
		  case MIME_ERROR_WARN:
		        e_ptr = &warn;
			break;
		  case MIME_ERROR_NET:
			e_ptr = &net;
			break;
		  case MIME_ERROR_CRIT:
		  default:
			e_ptr = &err;
			break;
	       }
	     if (*e_ptr == NULL)
	       {
		  tmp->prev = NULL;
		  *e_ptr = tmp;
	       }
	     else
	       {
		  tmp->prev = *e_ptr;
		  (*e_ptr)->next = tmp;
		  *e_ptr = (*e_ptr)->next;
	       }
	  }
     }
		  
   if (Slrn_Batch && (err != NULL))
     {
	slrn_error (_("Message is not acceptable."));
	slrn_error (_("Reason(s):"));
	tmp=err;
	while (tmp != NULL)
	  {
	     slrn_error (tmp->msg);
	     tmp=tmp->next;
	  }
	slrn_free_mime_error(err);
	slrn_free_mime_error(warn);
	slrn_free_mime_error(net);
	return -1;
     }

   if ((err == NULL) && (warn == NULL))
     {
	if (net == NULL)
	  {
	     return 0;
	  }
	else
	  {
	     if (!Slrn_Netiquette_Warnings)
	       {
		  slrn_free_mime_error(net);
		  return 0;
	       }
	  }
     }

   if (err != NULL)
     {
	Slrn_Full_Screen_Update = 1;

	slrn_set_color (0);

	SLsmg_cls ();
	SLsmg_gotorc (2,0);
	slrn_set_color (SUBJECT_COLOR);
	i=4;
	*linenum=err->lineno;
	SLsmg_write_string (_("Your message is not acceptable for the following reason(s):"));

	tmp=err;
	while ((tmp != NULL) && ( i  < SLtt_Screen_Rows - 11))
	  {
	     slrn_set_color (ERROR_COLOR);
	     SLsmg_gotorc (i,4); 
	     SLsmg_write_string (tmp->msg);
	     i += 2;
	     if (tmp->lineno >= 0)
	       {
		  SLsmg_gotorc (i,0);
		  i += 2;
		  slrn_set_color (SUBJECT_COLOR);
		  SLsmg_printf (_("This message was generated while looking at line %d:"), tmp->lineno);
	       }
	     else
	       {
		  if (tmp->err_str != NULL)
		    {
		       SLsmg_gotorc (i,0);
		       i +=2;
		       slrn_set_color (SUBJECT_COLOR);
		       SLsmg_printf (_("This message was generated while looking at the following line"));
		    }
	       }
	     if (tmp->err_str != NULL)
	       {
		  SLsmg_gotorc (i,0);
		  i +=3;
		  slrn_set_color (QUOTE_COLOR);
		  SLsmg_write_string (tmp->err_str);
	       }
	     tmp=tmp->next;
	  }
	SLsmg_gotorc (i++, 0);
	slrn_set_color (ARTICLE_COLOR);
	SLsmg_write_string (_("Perhaps this error was generated because you did not separate the header"));
	SLsmg_gotorc (i, 0);
	SLsmg_write_string (_("section from the body by a BLANK line."));
	i += 2;
	iret = 1;
     }
   if ((warn != NULL) && ( i  < SLtt_Screen_Rows - 9))
     {
	if (err == NULL)
	  {
	     Slrn_Full_Screen_Update = 1;
	     
	     slrn_set_color (0);
	     
	     SLsmg_cls ();
	     SLsmg_gotorc (2,0);
	     slrn_set_color (SUBJECT_COLOR);
	     i=4;
	     *linenum=warn->lineno;
	     SLsmg_write_string (_("Your message is not acceptable for the following reason(s):"));
	  }
	tmp=warn;
	while ((tmp != NULL) && ( i  < SLtt_Screen_Rows - 9))
	  {
	     slrn_set_color (ERROR_COLOR);
	     SLsmg_gotorc (i,4); 
	     SLsmg_write_string (tmp->msg);
	     i += 2;
	     if (tmp->lineno >= 0)
	       {
		  SLsmg_gotorc (i,0);
		  i += 2;
		  slrn_set_color (SUBJECT_COLOR);
		  SLsmg_printf (_("This message was generated while looking at line %d:"), tmp->lineno);
	       }
	     else
	       {
		  if (tmp->err_str != NULL)
		    {
		       SLsmg_gotorc (i,0);
		       i += 2;
		       slrn_set_color (SUBJECT_COLOR);
		       SLsmg_printf (_("This message was generated while looking at the following line"));
		    }
	       }
	     if (tmp->err_str != NULL)
	       {
		  SLsmg_gotorc (i,0);
		  i +=3;
		  slrn_set_color (QUOTE_COLOR);
		  SLsmg_write_string (tmp->err_str);
	       }
	     tmp=tmp->next;
	  }
	if (ret == 0) iret=2;
     }
   if ((net != NULL) && ( i  < SLtt_Screen_Rows - 9))
     {
	if ((err == NULL) && (warn==NULL))
	  {
	     *linenum=net->lineno;
	     Slrn_Full_Screen_Update = 1;

	     slrn_set_color (0);

	     SLsmg_cls ();
	     SLsmg_gotorc (2,0);
	     slrn_set_color (SUBJECT_COLOR);
	     i=4;
	  }
	SLsmg_write_string (_("Your message breaks the following netiquette guidelines:"));
	slrn_set_color (ERROR_COLOR);

	tmp=net;
	while ((tmp != NULL) && ( i  < SLtt_Screen_Rows - 9))
	  {
	     SLsmg_gotorc (i++, 4);
	     slrn_set_color (ERROR_COLOR);
	     SLsmg_write_string (tmp->msg);
	     i += 2;
	     if (tmp->lineno >= 0)
	       {
		  SLsmg_gotorc (i,0);
		  i += 2;
		  slrn_set_color (SUBJECT_COLOR);
		  SLsmg_printf (_("This message was generated while looking at line %d:"), tmp->lineno);
	       }
	     else
	       {
		  if (tmp->err_str != NULL)
		    {
		       SLsmg_gotorc (i,0);
		       i += 2;
		       slrn_set_color (SUBJECT_COLOR);
		       SLsmg_printf (_("This message was generated while looking at the following line"));
		    }
	       }
	     if (tmp->err_str != NULL)
	       {
		  SLsmg_gotorc (i,0);
		  i += 3;
		  slrn_set_color (QUOTE_COLOR);
		  SLsmg_write_string (tmp->err_str);
	       }
	     tmp=tmp->next;
	  }
	if (ret == 0) iret=2;
     }
   return iret;
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

	  fputs (a->cline->buf, outfp);
	  putc ('\n', outfp);
	  a->cline=a->cline->next;
     }
   putc ('\n', outfp);	       /* separator */
   return slrn_fclose (outfp);
}
/*}}}*/

static int saved_failed_post (char *file, char *msg) /*{{{*/
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

#if SLRN_HAS_CANLOCK
/* This function returns a malloced string or NULL on failure. */
static char *gen_cancel_lock (char *msgid) /*{{{*/
{
   FILE *cansecret;
   unsigned char *buf, *canlock;
   long filelen;
   char canfile[SLRN_MAX_PATH_LEN];
   
   if (0 == *Slrn_User_Info.cancelsecret)
     return NULL;
   
   if ((cansecret = slrn_open_home_file(Slrn_User_Info.cancelsecret,
			"r", canfile, SLRN_MAX_PATH_LEN, 0)) == NULL)
     {
	slrn_error (_("Cannot open file: %s"), Slrn_User_Info.cancelsecret);
	return NULL;
     }
   
   fseek (cansecret, 0, SEEK_END);
   if ((filelen = ftell(cansecret)) == 0)
     {
	slrn_error (_("Zero length file: %s"), Slrn_User_Info.cancelsecret);
	fclose (cansecret);
	return NULL;
     }
   if (NULL == (buf = slrn_malloc (filelen, 0, 1)))
     {
	fclose (cansecret);
	return NULL;
     }
   
   (void) fseek (cansecret, 0, SEEK_SET);
   fread (buf, filelen, 1, cansecret);
   
# if 0
   canlock = md5_lock(buf, filelen, msgid, strlen(msgid));
# else /* by default we use SHA-1 */
   canlock = sha_lock(buf, filelen, msgid, strlen(msgid));
# endif
   fclose(cansecret);
   SLFREE(buf);
   return canlock;
}
/*}}}*/
#endif /* SLRN_HAS_CANLOCK */

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
	     if (NULL == (s = strchr (fmt, '%')))
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

/* This function returns:
 * -1 => user does not want to post article
 *  0 => user wants to delete postponed article
 *  1 => user postponed the article
 *  2 => user wants to post article */
/* Be warned: This function now also transfers "file" into "a", which will be
 * used for the actual posting process later on. */
static int post_user_confirm (Slrn_Article_Type *a, char *to, char *file, int is_postponed) /*{{{*/
{
   int rsp;
   int once_more=1;
   char *responses;
#if SLRN_HAS_SLANG
   int filter_hook;

    filter_hook = slrn_is_hook_defined (HOOK_POST_FILTER);
#endif

   while (once_more)
     {
	int linenum = 1;
#if SLRN_HAS_SLANG
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
#endif
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
#if SLRN_HAS_SLANG
	  }
#endif
	rsp = slrn_map_translated_char ("yYnNeEsSdDfF", _("yYnNeEsSdDfF"), rsp) | 0x20;

	switch (rsp)
	  {
	   case 'n':
	     return -1;

	   case 's':

	     if (is_postponed
		 || (0 == postpone_file (file)))
	       return 1;

	     /* Instead of returning, let user have another go at it */
	     slrn_sleep (1);
	     break;

	   case 'd':
	     rsp = slrn_get_yesno_cancel (_("Sure you want to delete it"));
	     if (rsp == 0)
	       continue;
	     if (rsp == -1)
	       return 1;
	     /* Calling routine will delete it. */
	     return 0;

	   case 'y':
	     if (0 == (rsp = slrn_prepare_file_for_posting (file, &linenum, a, to, 0)))
	       {
		  once_more = 0;
		  break;
	       }
	     if (rsp == -1)
	       return -1;

	     SLtt_beep ();
	     if (rsp == 1)
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

	     rsp = slrn_map_translated_char ("yYeEnNcCfF", _("yYeEnNcCfF"), rsp) | 0x20;

	     switch (rsp)
	       {
		case 'y': case 'e':
		  rsp = 'y'; break;

		case 'c': case 7:
		  return -1;

		case 'f':
		  once_more = 0;
	       }

	     if (rsp != 'y')
	       break;

	     /* Drop */

	   case 'e':
	     if (slrn_edit_file (Slrn_Editor_Post, file, linenum, 0) < 0)
	       return -1;
	     break;

#if SLRN_HAS_SLANG
	   case 'f':
	     if (filter_hook != 0)
	       {
                  slrn_run_hooks (HOOK_POST_FILTER, 1, file);
		  if (SLang_get_error ())
		    filter_hook = 0;
	       }
#endif
	  }
	if (once_more) 
	  {
	     slrn_mime_free(&a->mime);
	     slrn_art_free_article_lines(a);
	  }
     } /* while (once_more) */
   return 2;
}
/*}}}*/

/* This function returns 1 if postponed, 0 upon sucess, -1 upon error */
int slrn_post_file (char *file, char *to, int is_postponed) /*{{{*/
{
   Slrn_Article_Type *a = NULL;
   Slrn_Article_Line_Type *tmp;
   int header;
   int rsp;
   int perform_cc;
   int status;
   char *msgid = NULL;
   int has_messageid = 0;
   char *responses;

   try_again:

   perform_cc = 0;
   has_messageid == 0;
   if (msgid != NULL)
	slrn_free(msgid);
   if (a != NULL)
	slrn_art_free_article(a);
   a=(Slrn_Article_Type*) slrn_malloc (sizeof(Slrn_Article_Type), 1, 1);

   if (Slrn_Batch == 0)
     {
	if ((rsp=post_user_confirm(a, to, file, is_postponed)) != 2)
	  {
	     slrn_art_free_article(a);
	     return rsp;
	  }	
     }

   slrn_message_now (_("Posting ..."));

   /* XXX
#if SLRN_HAS_SLANG
   if (1 == slrn_is_hook_defined (HOOK_POST_FILE))
     {
	(void) slrn_run_hooks (HOOK_POST_FILE, 1, file);
	if (SLang_get_error ())
	  {
	     if (!Slrn_Editor_Uses_Mime_Charset)
	       slrn_chmap_fix_file (file, 1);
	     (void) saved_failed_post (file, _("post_file_hook returned error."));
	     return -1;
	  }

	slrn_message_now (_("Posting ..."));
     }
#endif
    */

   /* slrn_set_suspension (1); */
   status = Slrn_Post_Obj->po_start ();
   if (status != CONT_POST)
     {
	(void) saved_failed_post (file, (status != -1) ?
				  _("Posting not allowed.") :
				  _("Could not reach server."));
	slrn_art_free_article(a);
	return -1;
     }

    msgid = create_message_id (&rsp);
    if (rsp)
      {
	(void) saved_failed_post (file, _("Could not generate Message-ID."));
	slrn_free(msgid);
	slrn_art_free_article(a);
	return -1;
      }

   a->cline=a->lines;
   
   header=1;
   while (a->cline != NULL)
     {
	if (header)
	  {
	     if (a->cline->flags != HEADER_LINE) /* Header ends, body begins */
	       {
		  if (msgid != NULL)
		    {
#if SLRN_HAS_CANLOCK
		       char *canlock;
		       if (NULL != (canlock = gen_cancel_lock (msgid)))
			 {
			    a->cline=a->cline->prev;
			    tmp=a->cline->next;
			    a->cline->next=(Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
			    a->cline->next->prev=a->cline;
			    a->cline=a->cline->next;
			    a->cline->next=tmp;
			    a->cline->flags=HEADER_LINE;
			    a->cline->buf=slrn_safe_malloc(14+strlen(canlock));
			    sprintf(a->cline->buf, "Cancel-Lock: %s", canlock);
			    SLFREE (canlock);
			    Slrn_Post_Obj->po_puts(a->cline->buf);
			    Slrn_Post_Obj->po_puts("\n");
			    a->cline=a->cline->next;
			 }
#endif /* SLRN_HAS_CANLOCK */
		       if (has_messageid == 0)
			 {
			    a->cline=a->cline->prev;
			    tmp=a->cline->next;
			    a->cline->next=(Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
			    a->cline->next->prev=a->cline;
			    a->cline=a->cline->next;
			    a->cline->next=tmp;
			    a->cline->flags=HEADER_LINE;
			    a->cline->buf=slrn_safe_malloc(13+strlen(msgid));
			    sprintf(a->cline->buf, "Message-ID: %s", msgid);
			    SLFREE (msgid);
			    Slrn_Post_Obj->po_puts(a->cline->buf);
			    Slrn_Post_Obj->po_puts("\n");
			    a->cline=a->cline->next;
			 }
		    }
		  header = 0;
		  continue;
	       }

	     if (Slrn_Generate_Date_Header &&
		       (!slrn_case_strncmp ((unsigned char *)"Date: ",
					    (unsigned char *)a->cline->buf, 4)))
	       {
		  /* skip generated date header for posting */
		  a->cline=a->cline->next;
		  continue;
	       }
	     if (!slrn_case_strncmp ((unsigned char *)"Cc: ",
				     (unsigned char *)a->cline->buf, 4))
	       {
		  perform_cc = 1;
		  a->cline=a->cline->next; /* The 'Cc:' header is only needed in the cc-file.*/
		  continue;
	       }

#if SLRN_HAS_GEN_MSGID
	     if (!slrn_case_strncmp ((unsigned char *)"Message-ID: ",
				     (unsigned char *)a->cline->buf, 12))
	       {
		  slrn_free(msgid);
		  msgid = slrn_strmalloc (a->cline->buf+12, 0);
		  has_messageid = 1;
	       }
#endif
	  } /* if (header) */

	Slrn_Post_Obj->po_puts(a->cline->buf);
	Slrn_Post_Obj->po_puts("\n");
	a->cline=a->cline->next;
     } /*while (a->cline != NULL)*/

   if (0 == Slrn_Post_Obj->po_end ())
     {
	slrn_message (_("Posting...done."));
     }
   else
     {
	if (Slrn_Batch)
	  {
	     saved_failed_post (file, NULL);
	     slrn_art_free_article(a);
	     return -1;
	  }

	/* slrn_set_suspension (0); */
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
	if (rsp == 7) rsp = 'c';
	else rsp = slrn_map_translated_char ("rReEcC", responses, rsp) | 0x20;

	if ((rsp == 'c')
	    || ((rsp == 'e')
		&& (slrn_edit_file (Slrn_Editor_Post, file, 1, 0) < 0)))
	  {
	     (void) saved_failed_post (file, NULL);
	     slrn_art_free_article(a);
	     return -1;
	  }
	goto try_again;
     } /* (0 == Slrn_Post_Obj->po_end ()) */

   if (-1 == slrn_save_article_to_mail_file (a, Slrn_Save_Posts_File))
     {
	slrn_art_free_article(a);
	return -1;
     }

   if (perform_cc)
     {
	cc_article (a);
     }

   slrn_art_free_article(a);
   return 0;
}

/*}}}*/

int slrn_post (char *newsgroup, char *followupto, char *subj) /*{{{*/
{
   FILE *fp;
   char file[SLRN_MAX_PATH_LEN], *from, *tmp;
   unsigned int header_lines;
   int ret;
   int convert;

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
   if (NULL == (from = slrn_make_from_string ()))
	return -1;
   if (slrn_convert_fprintf(fp, Slrn_Editor_Charset, Slrn_Display_Charset, "%s\n", from) <0)
     {
	slrn_free(from);
	return -1;
     }
   slrn_free(from);
   header_lines++;
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

   fputs ("\n", fp);

   if (slrn_add_signature (fp) == -1)
	return -1;
   slrn_fclose (fp);

   if (slrn_edit_file (Slrn_Editor_Post, file, header_lines, 1) >= 0)
     ret = slrn_post_file (file, NULL, 0);
   else ret = -1;

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

	if (-1 == slrn_read_filename (_("Save to: "), NULL, dirfile, 1, 1))
	  return -1;

	status = slrn_file_exists (dirfile);
	if (status == 1)
	  {
	     status = slrn_get_yesno_cancel (_("File exists, overwrite"));
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
	slrn_error (_("%s: not a regular file"));
	return;
     }

   if (0 == slrn_post_file (file, NULL, 1))
     slrn_delete_file (file);

   slrn_free (file);
}
/*}}}*/

/*}}}*/
