/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999 John E. Davis <davis@space.mit.edu>
 Copyright (c) 2001 Thomas Schultz <tststs@gmx.de>

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

#include <stdio.h>
#include <string.h>


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

#include "slrn.h"
#include "util.h"
#include "server.h"
#include "misc.h"
#include "post.h"
#include "group.h"
#include "art.h"
#include "decode.h"
#include "snprintf.h"
#include "chmap.h"
#include "menu.h"
#include "version.h"
#include "mime.h"
#include "hooks.h"

#define MAX_LINE_BUFLEN	2048

char *Slrn_CC_Followup_Message = NULL;
char *Slrn_CC_Post_Message = NULL;
char *Slrn_Save_Posts_File;
char *Slrn_Save_Replies_File;
char *Slrn_Last_Message_Id;
char *Slrn_Post_Custom_Headers;
char *Slrn_Failed_Post_Filename;
char *Slrn_Signoff_String;

int Slrn_Reject_Long_Lines = 2;
int Slrn_Netiquette_Warnings = 1;
char *Slrn_Postpone_Dir;
int Slrn_Generate_Message_Id = 1;
int Slrn_Generate_Date_Header = 0;

static int postpone_file (char *);

#if SLRN_HAS_GEN_MSGID
static char *slrn_create_message_id (void)
{
   unsigned long pid, now;
   static unsigned char baseid[64];
   unsigned char *b, *t, tmp[32];
   char *chars32 = "0123456789abcdefghijklmnopqrstuv";
   static unsigned long last_now;
   
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
   
   t = (unsigned char *) Slrn_User_Info.username;
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
   
   return (char *) baseid;
}
#endif

const char *Weekdays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
const char *Months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec", "ERR" };

void slrn_add_date_header (FILE *fp)
{
   char buf[64];
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
   
   slrn_snprintf (buf, sizeof (buf),
		  "Date: %s, %d %s %d %02d:%02d:%02d %+03d%02d\n",
		  Weekdays[t->tm_wday], t->tm_mday, Months[t->tm_mon],
		  t->tm_year + 1900, t->tm_hour, t->tm_min, t->tm_sec,
		  (int) tz / 60, (int) abs (tz) % 60);
   
   if (fp != NULL)
     fputs (buf, fp);
   else if (Slrn_Generate_Date_Header)
     Slrn_Post_Obj->po_puts (buf);
}

void slrn_add_signature (FILE *fp)
{
   FILE *sfp;
   char file[SLRN_MAX_PATH_LEN];
   char buf [256];

   if ((NULL != Slrn_Signoff_String) && (*Slrn_Signoff_String != 0))
     {
        fputs ("\n", fp);
        fputs (Slrn_Signoff_String, fp);
     }
   
   if ((Slrn_User_Info.signature == NULL)
       || (Slrn_User_Info.signature[0] == 0))
     return;
   
   if ((sfp = slrn_open_home_file (Slrn_User_Info.signature, "r", file,
				   sizeof (file), 0)) != NULL)
     {
 	if (! Slrn_Signoff_String)
 	  fputs ("\n", fp);
	
	/* Apparantly some RFC suggests the -- \n. */
        fputs ("\n-- \n", fp);
	
	/* If signature file already has -- \n, do not add it. */
	if ((NULL != fgets (buf, sizeof (buf), sfp))
	    && (0 != strcmp (buf, "-- \n")))
	  fputs (buf, fp);
	  
        while (NULL != fgets (buf, sizeof(buf), sfp))
	  {
	     fputs (buf, fp);
	  }
        slrn_fclose(sfp);
     }
}

static int is_empty_header (char *line)
{
   char *b;
   
   if ((*line == ' ') || (*line == '\t')) return 0;
   
   b = slrn_strchr (line, ':');
   if (b == NULL) return 0;
   
   b = slrn_skip_whitespace (b + 1);
   return (*b == 0);
}


static int slrn_cc_file (char *file, char *to, char *msgid)
{
#if defined(VMS) || !SLRN_HAS_PIPING
   return -1;
#else
#if defined(IBMPC_SYSTEM)
   char outfile [SLRN_MAX_PATH_LEN];
#endif
   FILE *pp, *fp;
   char line[MAX_LINE_BUFLEN];
   unsigned int cc_line = 0;
   unsigned int linenum;
   char buf [MAX_LINE_BUFLEN];
   unsigned int nth;
   char *l, *ref = NULL;
   int reflen = 0, ret = -1;

   if (NULL == (fp = fopen (file, "r")))
     {
	slrn_error (_("Unable to open %s."));
	return -1;
     }
   
   /* Look for CC line */
   linenum = 0;
   while ((NULL != fgets (line, sizeof (line) - 1, fp)) && (*line != '\n'))
     {
	linenum++;
	if (0 == slrn_case_strncmp ((unsigned char *)line,
				    (unsigned char *) "Cc: ", 4))
	  {
	     l = slrn_skip_whitespace (line + 4);
	     if (*l && (*l != ',')) cc_line = linenum;
	     break;
	  }
     }
   
   /* At this point, if all has gone well line contains the cc information */
   
   if (cc_line == 0)
     {
	slrn_fclose (fp);
	return -1;
     }
   
#if defined(IBMPC_SYSTEM)
   pp = slrn_open_tmpfile (outfile, sizeof (outfile));
#else
   pp = slrn_popen (Slrn_SendMail_Command, "w");
#endif
   if (pp == NULL)
     {
	slrn_fclose (fp);
	return -1;
     }
   
   fputs ("To: ", pp);
   
   /* This line consists of a comma separated list of addresses.  In
    * particular, "poster" will be replaced by 'to'.
    */
   l = line + 4;
   nth = 0;
   while (0 == SLextract_list_element (l, nth, ',', buf, sizeof (buf)))
     {
	char *b;

	/* Knock whitespace from ends of string */
	b = slrn_skip_whitespace (buf);
	slrn_trim_string (b);

	if (nth != 0)
	  putc (',', pp);
	
	if ((0 == slrn_case_strcmp ((unsigned char *)b, (unsigned char *)"poster"))
	    && (to != NULL))
	  b = to;
	
	fputs (b, pp);
	nth++;
     }

   putc ('\n', pp);
   rewind (fp);

   linenum = 0;
   
   if (msgid != NULL)
     fprintf (pp, "Message-Id: <slrn%s@%s>\n", 
	      msgid, Slrn_User_Info.posting_host);
   
   
   while ((NULL != fgets (line, sizeof (line) - 1, fp)) && (*line != '\n'))
     {
	linenum++;
	if (linenum == cc_line) continue;
	if (is_empty_header (line)) continue;
	if (0 == slrn_case_strncmp ((unsigned char *)line,
				    (unsigned char *) "To: ", 4))
	  continue;
	if ((Slrn_Generate_Email_From == 0) &&
	    (0 == slrn_case_strncmp ((unsigned char *)line,
				     (unsigned char *) "From: ", 6)))
	  continue;
	if (0 == slrn_case_strncmp ((unsigned char *)line,
				    (unsigned char *) "References: ", 12))
	  {
	     if (NULL != (ref = strrchr (line, '<')))
	       {
		  reflen = strlen (ref);
		  if ((ref[reflen - 1] == '\n') &&
		      (ref[--reflen] == '\r'))
		    reflen--;
	       }
	  }
	
	/* There is some discussion of this extension to mail headers.  For
	 * now, assume that this extension will be adopted.
	 */
	if (0 == slrn_case_strncmp ((unsigned char *)line,
				    (unsigned char *) "Newsgroups: ", 12))
	  {
	     fputs ("Posted-To: ", pp);
	     fputs (line + 12, pp);
	  }
	else
	  fputs (line, pp);
     }

   slrn_add_date_header (pp);
# if SLRN_HAS_MIME
   if (Slrn_Use_Mime & MIME_DISPLAY) slrn_mime_add_headers (pp);
# endif
   
   fputs ("\n", pp);
   
   if ((Slrn_Current_Header != NULL) && (ref != NULL) &&
       (0 == strncmp ((unsigned char*)ref,
		      (unsigned char*)Slrn_Current_Header->msgid, reflen)))
     ret = slrn_insert_followup_format (Slrn_CC_Followup_Message, pp);
   else if ((NULL != Slrn_CC_Post_Message) && (*Slrn_CC_Post_Message))
     ret = fputs (Slrn_CC_Post_Message, pp);
   
   if (ret >= 0)
     ret = fputs ("\n", pp);
   
# if SLRN_HAS_MIME
   if (Slrn_Use_Mime & MIME_DISPLAY) fp = slrn_mime_encode (fp);
# endif
   
   while (NULL != fgets (line, sizeof (line) - 1, fp))
     {
	fputs (line, pp);
     }
   slrn_fclose (fp);
# if defined(IBMPC_SYSTEM)
   slrn_fclose (pp);
   slrn_snprintf (buf, sizeof (buf), "%s %s", Slrn_SendMail_Command, outfile);
   slrn_posix_system (buf, 0);
# else
   slrn_pclose (pp);
# endif
   return 0;
#endif				       /* NOT VMS */
}

/* Returns -1 upon error, 0, if ok, 1 if needs repair, 2 is warning is issued */
#define MAX_WARNINGS 10
static int check_file_for_posting (char *file)
{
   char line[MAX_LINE_BUFLEN], buf[MAX_LINE_BUFLEN], *the_line;
   FILE *fp;
   unsigned int num;
   char *err, *warnings[MAX_WARNINGS];
   int newsgroups_found, subject_found, followupto_found, warn;
   char ch;
   char *colon;

   for (warn = 0; warn < MAX_WARNINGS; warn++)
     warnings[warn] = NULL;
   warn = newsgroups_found = followupto_found = subject_found = 0;
   err = NULL;
   fp = fopen (file, "r");
   
   if (fp == NULL)
     {
	slrn_error (_("Unable to open %s"), file);
	return -1;
     }
   
   the_line = line;
   
   /* scan the header */
   num = 0;
   while (NULL != fgets (line, sizeof (line), fp))
     {
	ch = *line;	
	num++;
	if ((ch == ' ') || (ch == '\t') || (ch == '\n'))
	  {
	     if (num == 1) 
	       {
		  err = _("The first line must begin with a header.");
		  break;
	       }
	     if (ch == '\n') break;
	     
	     continue;
	  }
	
	if (NULL == (colon = slrn_strchr (line, ':')))
	  {
	     err = _("Expecting a header.  This is not a header line.");
	     break;
	  }
	
	if (!slrn_case_strncmp ((unsigned char *) line, (unsigned char *) "Subject:", 8))
	  {
	     if (is_empty_header (line))
	       {
		  err = _("The subject header is not allowed be to empty.");
		  break;
	       }
	     subject_found = 1;
	     continue;
	  }
	
	if (!slrn_case_strncmp ((unsigned char *) line, (unsigned char *) "Newsgroups:", 11))
	  {
	     char *p = line;
	     if (is_empty_header (line)) 
	       {
		  err = _("The Newsgroups header is not allowed be to empty.");
		  break;
	       }
	     newsgroups_found = 1;
	     while (*(++p))
	       {
		  if (*p == ',') newsgroups_found++;
	       }
	     continue;
	  }
	
	if ((!slrn_case_strncmp ((unsigned char *) line, (unsigned char *) "Followup-To:", 12))
	    && (!is_empty_header (line)))
	  {
	     char *p = line;
	     followupto_found = 1;
	     while (*(++p))
	       {
		  if (*p == ',') followupto_found++;
	       }
	     continue;
	  }
	
	/* slrn will remove it later if it is empty */
	if (is_empty_header (line)) continue;
	
	if (*(colon + 1) != ' ') 
	  {
	     err = _("A space must follow the ':' in a header");
	     break;
	  }
	
#if SLRN_HAS_STRICT_FROM
	if (!slrn_case_strncmp ((unsigned char *) line, (unsigned char *) "From:", 5))
	  {
	     err = _("This news reader will not accept user generated From lines.");
	     break;
	  }
#endif
#if ! SLRN_HAS_GEN_MSGID
	if (!slrn_case_strncmp ((unsigned char *) line, (unsigned char *) "Message-Id:", 11))
	  {
	     err = _("This news reader will not accept user generated Message-Ids.");
	     break;
	  }
#endif
     }

   if (err == NULL) 
     {
	if (subject_found == 0)
	  {
	     err = _("Subject header is required.");
	     num = 0;
	  }
	else if (newsgroups_found == 0)
	  {
	     err = _("Newsgroups header is required.");
	     num = 0;
	  }
	else
	  {
	     if (newsgroups_found > 4)
	       warnings[warn++ % MAX_WARNINGS] =
	       _("Please re-consider if posting to a large number of groups is necessary.");
	     if (followupto_found > 1)
	       warnings[warn++ % MAX_WARNINGS] =
	       _("In most cases, setting a \"Followup-To:\" multiple groups is a bad idea.");
	     else if ((newsgroups_found > 1) && (followupto_found == 0))
	       warnings[warn++ % MAX_WARNINGS] =
	       _("Setting a \"Followup-To:\" is recommended when crossposting.");
	  }
     }
   
   /* Now body.  Check for non-quoted lines. */
   if (err == NULL) 
     {
	char *qs = Slrn_Quote_String;
	unsigned int qlen;
	int sig_lines = -1;
#if SLRN_HAS_VERBATIM_MARKS
	int is_verbatim = 0;
#endif
	if (qs == NULL) qs = ">";
	qlen = strlen (qs);
	
	err = _("Your message does not appear to have any unquoted text.");
	the_line = NULL;
	while (NULL != fgets (line, sizeof (line), fp))
	  {
	     if (the_line == NULL) num++;
	     if (sig_lines != -1) sig_lines++;
	     
	     if (!strncmp (line, qs, qlen))
	       continue;
#if SLRN_HAS_VERBATIM_MARKS
	     if (0 == strncmp (line, "#v", 2))
	       {
		  if (is_verbatim && (line[2] == '-'))
		    {
		       is_verbatim = 0;
		       continue;
		    }
		  
		  if ((is_verbatim == 0) && (line[2] == '+'))
		    {
		       is_verbatim = 1;
		       continue;
		    }
	       }
	     
	     if (is_verbatim)
	       continue;
#endif
	     if (0 == strcmp (line, "-- \n"))
	       {
		  sig_lines = 0;
		  continue;
	       }
	     
	     colon = slrn_skip_whitespace (line);
	     if (*colon == 0) continue;
	     
	     err = NULL;
	     
	     if ((Slrn_Netiquette_Warnings == 0) &&
		 (Slrn_Reject_Long_Lines == 0))
	       break;
	     
	     if ((the_line == NULL) && (strlen (line) > 81)) /* allow \n to slip */
	       {
		  the_line = buf; /* magic: slrn will know that we got here */
		  strcpy (buf, line); /* safe */
	       }
	  }
	if (sig_lines > 4)
	  warnings[warn++ % MAX_WARNINGS] =
	  _("Please keep your signature short. 4 lines is a commonly accepted limit.");
     }
   
   fclose (fp);

   if (Slrn_Batch && (err != NULL))
     {
	slrn_error (_("Message is not acceptable."));
	slrn_error (_("Reason: %s"), err);
	return -1;
     }
   
   if ((err == NULL) && (!Slrn_Netiquette_Warnings || (*warnings == NULL)) &&
       (!Slrn_Reject_Long_Lines || (the_line == NULL))) return 0;
   
   Slrn_Full_Screen_Update = 1;

   slrn_set_color (0);
   
   SLsmg_cls ();
   SLsmg_gotorc (2,0);
   slrn_set_color (SUBJECT_COLOR);
   if (err != NULL)
     {
	SLsmg_write_string (_("Your message is not acceptable for the following reason:"));
	slrn_set_color (ERROR_COLOR);
	SLsmg_gotorc (4,4); SLsmg_write_string (err);
	if (num && (the_line != NULL))
	  {
	     SLsmg_gotorc (6,0);
	     slrn_set_color (SUBJECT_COLOR);
	     SLsmg_printf (_("This message was generated while looking at line %d:"), num);
	     SLsmg_gotorc (8,0);
	     slrn_set_color (QUOTE_COLOR);
	     SLsmg_write_string (the_line);
	     SLsmg_gotorc (12, 0);
	     slrn_set_color (ARTICLE_COLOR);
	     SLsmg_write_string (_("Perhaps this error was generated because you did not separate the header"));
	     SLsmg_gotorc (13, 0);
	     SLsmg_write_string (_("section from the body by a BLANK line."));
	  }
	return 1;
     }
   else
     {
	int row = 4;
	SLsmg_write_string (_("Your message breaks the following netiquette guidelines:"));
	slrn_set_color (ERROR_COLOR);
	
	if (Slrn_Netiquette_Warnings)
	  {
	     warn = 0;
	     while ((warn < MAX_WARNINGS) && warnings[warn] != NULL)
	       {
		  SLsmg_gotorc (row++, 4);
		  SLsmg_write_string (warnings[warn++]);
	       }
	  }
	
	if ((the_line != NULL) && Slrn_Reject_Long_Lines)
	  {
	     SLsmg_gotorc (++row, 4);
	     SLsmg_write_string (_("Lines with more than 80 characters generally need to be wrapped."));
	     slrn_set_color (SUBJECT_COLOR);
	     row += 2; SLsmg_gotorc (row, 0);
	     SLsmg_printf (_("This affects at least line %d:"), num);
	     row += 2; SLsmg_gotorc (row, 0);
	     slrn_set_color (QUOTE_COLOR);
	     SLsmg_write_string (the_line);
	     if (Slrn_Reject_Long_Lines == 1)
	       return 1;
	  }
	return 2;
     }
   return 0; /* never reached */
}


int slrn_save_file_to_mail_file (char *file, char *save_file, char *msgid)
{
   FILE *infp, *outfp;
   time_t now;
   char save_post_file[SLRN_MAX_PATH_LEN];
   char line [MAX_LINE_BUFLEN];
   char *system_os_name;
   int has_from = 0, has_messageid = 0, header = 1;

   if ((save_file == NULL) || (*save_file == 0))
     return 0;
	
   if (NULL == (infp = fopen (file, "r")))
     {
	slrn_error (_("File not found: %s--- message not posted."), file);
	return -1;
     }

#if SLRN_HAS_MIME
   if (Slrn_Use_Mime & MIME_ARCHIVE)
     slrn_mime_scan_file (infp);
#endif
   
   if (NULL == (outfp = slrn_open_home_file (save_file, "a", save_post_file,
					     sizeof (save_post_file), 1)))
     {
	slrn_message_now (_("***Warning: error saving to %s"), save_post_file);
	SLang_input_pending (30);
	SLang_flush_input ();
	slrn_fclose (infp);
	return 0;
     }
   
   time (&now);
   if ((*Slrn_User_Info.username == 0) || (*Slrn_User_Info.hostname == 0))
     fprintf (outfp, "From nobody@nowhere %s", ctime(&now));
   else
     fprintf (outfp, "From %s@%s %s", Slrn_User_Info.username, Slrn_User_Info.hostname, ctime(&now));
   
   while (NULL != fgets (line, sizeof(line) - 1, infp))
     {
	if (header)
	  {
	     if ((has_from == 0) &&
		 !slrn_case_strncmp ((unsigned char*) "from:",
				     (unsigned char*) line, 5))
	       has_from = 1;
	     if ((has_messageid == 0) &&
		 !slrn_case_strncmp ((unsigned char*) "message-id:",
				     (unsigned char*) line, 11))
	       has_messageid = 1;
	     
	     if ((unsigned char)*line == '\n')
	       {
		  if (has_from == 0)
		    {
		       char *from = slrn_make_from_string ();
		       if (from == NULL) from = "";
		       fprintf (outfp, "From: %s\n", from);
		    }
		  slrn_add_date_header (outfp);
		  if ((has_messageid == 0) && (msgid != NULL))
		    fprintf (outfp, "Message-Id: <slrn%s@%s>\n", msgid,
			     Slrn_User_Info.posting_host);
#if SLRN_HAS_MIME
		  if (Slrn_Use_Mime & MIME_ARCHIVE)
		    slrn_mime_add_headers (outfp);
#endif
		  system_os_name = slrn_get_os_name ();
		  fprintf (outfp, "User-Agent: slrn/%s (%s)\n", Slrn_Version,
			   system_os_name);
		  header = 0;
	       }
	     else if (is_empty_header (line))
	       continue;
#if SLRN_HAS_MIME
	     else if (Slrn_Use_Mime & MIME_ARCHIVE)
	       slrn_mime_header_encode (line, sizeof(line));
#endif
	  }
	
	if ((*line == 'F')
	    && !strncmp ("From", line, 4)
	    && ((unsigned char)line[4] <= ' '))
	  putc ('>', outfp);
	
	fputs (line, outfp);
     }
   fputs ("\n\n", outfp);	       /* separator */
   slrn_fclose (infp);
   return slrn_fclose (outfp);
}

static int post_references_header (char *line)
{
#define GNKSA_LENGTH 986 /* 998 - strlen ("References: ") */
   char buf[GNKSA_LENGTH + 2];
   char *p, *l, *r, *nextid, *tmp;
   unsigned int len = 0, extra_whitespaces=0;

   /* Make sure line does not end in whitespace */
   (void) slrn_trim_string (line);
   
   if ((NULL == (l = slrn_strchr (line, '<'))) ||
       (NULL == (r = slrn_strchr (l+1, '>'))))
     return -1;
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
	Slrn_Post_Obj->po_puts ("References: ");
	Slrn_Post_Obj->po_puts (l);
	Slrn_Post_Obj->po_puts ("\n");
	return 0;
     }
   strncpy (buf, l, len);
   p = buf + len;
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
   
   strcpy (p, "\n");
   
   Slrn_Post_Obj->po_puts ("References: ");
   Slrn_Post_Obj->po_puts (buf);
   return 0;
}

static int saved_failed_post (char *file)
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

   slrn_message (_("Failed post saved in %s"), filebuf);

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


/* This function returns 1 if postponed, 0 upon sucess, -1 upon error */
int slrn_post_file (char *file, char *to, int is_postponed)
{
   char line[MAX_LINE_BUFLEN]; /* also used for MIME encoding of the realname */
   char *linep;
   int len, header;
   FILE *fp;
   int rsp;
   int perform_cc;
   int status;
   char *msgid = NULL;
#if SLRN_HAS_GEN_MSGID
   int has_messageid = 0;
#endif
#if SLRN_HAS_SLANG
   int filter_hook;
#endif
   int once_more;
   char *system_os_name;
   char *responses;

#if SLRN_HAS_SLANG
   filter_hook = slrn_is_hook_defined (HOOK_POST_FILTER);
#endif

   system_os_name = slrn_get_os_name ();

   try_again:

   perform_cc = 0;
   once_more = 1;

   if (Slrn_Batch == 0) while (once_more)
     {
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
	     if (0 == (rsp = check_file_for_posting (file)))
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
	     if (slrn_edit_file (Slrn_Editor_Post, file, 1, 0) < 0)
	       return -1;
	     break;

#if SLRN_HAS_SLANG
	   case 'f':
	     if (filter_hook != 0)
	       {
                  slrn_run_hooks (HOOK_POST_FILTER, 1, file);
		  if (SLang_Error) 
		    filter_hook = 0;
	       }
	}
#endif
     }
   
   slrn_message_now (_("Posting ..."));
   
#if SLRN_HAS_GEN_MSGID
   msgid = slrn_create_message_id ();
#endif

   if ((Slrn_Use_Mime & MIME_ARCHIVE) && /* else: do it later */
       !Slrn_Editor_Uses_Mime_Charset)
     slrn_chmap_fix_file (file, 0);
   
   if (-1 == slrn_save_file_to_mail_file (file, Slrn_Save_Posts_File, msgid))
     return -1;
   
   if (!(Slrn_Use_Mime & MIME_ARCHIVE) &&
       !Slrn_Editor_Uses_Mime_Charset)
     slrn_chmap_fix_file (file, 0);

   if ((fp = fopen (file, "r")) == NULL)
     {
	slrn_error (_("File not found: %s--- message not posted."), file);
	return -1;
     }
   
#if SLRN_HAS_MIME
   if (Slrn_Use_Mime & MIME_DISPLAY)
     slrn_mime_scan_file (fp);
#endif
#if SLRN_HAS_SLANG
   if (1 == slrn_is_hook_defined (HOOK_POST_FILE))
     {
	fclose (fp);
	(void) slrn_run_hooks (HOOK_POST_FILE, 1, file);
	if (SLang_Error)
	  {
	     slrn_error (_("post_file_hook returned error.  %s not posted."), file);
	     if (!Slrn_Editor_Uses_Mime_Charset)
	       slrn_chmap_fix_file (file, 1);
	     (void) saved_failed_post (file);
	     return -1;
	  }

	if ((fp = fopen (file, "r")) == NULL)
	  {
	     slrn_error (_("File not found: %s--- message not posted."), file);
	     return -1;
	  }
	slrn_message_now (_("Posting ..."));
     }
#endif

   /* slrn_set_suspension (1); */
   status = Slrn_Post_Obj->po_start ();
   if (status != CONT_POST)
     {
	fclose (fp);
	if (!Slrn_Editor_Uses_Mime_Charset)
	  slrn_chmap_fix_file (file, 1);
	(void) saved_failed_post (file);
	if (status != -1)
	  slrn_error (_("Posting not allowed."));
	return -1;
     }

#if 0 /* We shouldn't have to set this one */
   if (Slrn_User_Info.username != NULL)
     Slrn_Post_Obj->po_printf ("Path: %s\n", Slrn_User_Info.username);
#endif

#if SLRN_HAS_STRICT_FROM
   /* line is not used until now, so we can use it as a buffer */
   if (NULL == (linep = slrn_make_from_string ())) return -1;
   slrn_strncpy(line, linep, sizeof(line));

# if SLRN_HAS_MIME
   if (Slrn_Use_Mime & MIME_DISPLAY)
     slrn_mime_header_encode(line, sizeof(line));
# endif

   Slrn_Post_Obj->po_printf ("From: %s\n", line);
#endif /*SLRN_HAS_STRICT_FROM*/
   /* if (Slrn_User_Info.posting_host != NULL)
     * Slrn_Post_Obj->po_printf ("X-Posting-Host: %s\n", Slrn_User_Info.posting_host); */

   linep = line + 1;
   header = 1;
   while (fgets (linep, sizeof(line) - 1, fp) != NULL)
     {
	len = strlen (linep);
	if (len == 0) continue;

	if (header)
	  {
	     unsigned char *b;
	     char *white = linep;

	     /* Check the references header.  Rumor has it that many
	      * servers choke on long references lines.  In fact, to be
	      * GNKSA compliant, references headers cannot be longer than
	      * 998 characters.  Sigh.
	      */

	     if (((*white == 'R') || (*white == 'r'))
		 && (0 == slrn_case_strncmp ((unsigned char *)"References: ",
					     (unsigned char *)white,
					     12)))
	       {
		  (void) post_references_header (white);
		  continue;
	       }

	     while ((*white == ' ') || (*white == '\t')) white++;

	     if (*white == '\n')
	       {
		  slrn_add_date_header (NULL);
#if SLRN_HAS_GEN_MSGID
		  if (has_messageid == 0)
		    {
		       Slrn_Last_Message_Id = msgid;
		       if (msgid != NULL)
			 {
			    Slrn_Post_Obj->po_printf ("Message-Id: <slrn%s@%s>\n", msgid, Slrn_User_Info.posting_host);
			 }
		    }
#endif
#if SLRN_HAS_MIME
		  if (Slrn_Use_Mime & MIME_DISPLAY)
		    slrn_mime_add_headers (0);   /* 0 --> Slrn_Post_Obj->po_puts */
#endif
		  Slrn_Post_Obj->po_printf ("User-Agent: slrn/%s (%s)\n\n", 
					    Slrn_Version, system_os_name);
		  header = 0;
#if SLRN_HAS_MIME
		  if (Slrn_Use_Mime & MIME_DISPLAY) fp = slrn_mime_encode (fp);
#endif
		  
		  continue;
	       }
	     
	     if (!slrn_case_strncmp ((unsigned char *)"Cc: ", 
				     (unsigned char *)linep, 4))
	       {
		  b = (unsigned char *) linep + 4;
		  b = (unsigned char *) slrn_skip_whitespace ((char *) b);
		  if (*b && (*b != ',')) perform_cc = 1;
		  continue;
	       }

	     if (is_empty_header (linep)) continue;
#if SLRN_HAS_GEN_MSGID
	     if (!slrn_case_strncmp ((unsigned char *)"Message-Id: ",
				     (unsigned char *)linep, 12))
	       has_messageid = 1;
#endif	     
	     linep[len - 1] = 0;
#if SLRN_HAS_MIME
	     if (Slrn_Use_Mime & MIME_DISPLAY)
	       slrn_mime_header_encode (linep, sizeof (line) - 1);
#endif
	  }

	/* Since the header may not have ended in a \n, make sure the 
	 * other lines do not either.  Later, the \n will be added.
	 */
	if (linep[len - 1] == '\n')
	  {
	     len--;
	     linep[len] = 0;
	  }
	
	if (*linep == '.')
	  {
	     linep--;
	     *linep = '.';
	  }
	
	Slrn_Post_Obj->po_puts (linep);
	Slrn_Post_Obj->po_puts ("\n");
	
	linep = line + 1;
     }
   slrn_fclose (fp);

   if (0 == Slrn_Post_Obj->po_end ())
     slrn_message (_("Posting...done."));
   else
     {
	/* convert back to users' charset, so it can easily be re-edited */
	if (!Slrn_Editor_Uses_Mime_Charset)
	  slrn_chmap_fix_file (file, 1); 
	if (Slrn_Batch) 
	  {
	     saved_failed_post (file);
	     return -1;
	  }

	/* slrn_set_suspension (0); */
	slrn_smg_refresh ();
	slrn_sleep (2);
	slrn_clear_message (); SLang_Error = 0;
   /* Note to translators: Here, "rR" is "repost", "eE" is "edit" and "cC" is
    * "cancel". Don't change the length of the string; you cannot re-use any
    * of the default characters for different fields.
    */
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
	     (void) saved_failed_post (file);
	     return -1;
	  }
	goto try_again;
     }
   
   if (perform_cc)
     {
	slrn_cc_file (file, to, msgid);
     }
   return 0;
}


int slrn_post (char *newsgroup, char *followupto, char *subj)
{
   FILE *fp;
   char file[SLRN_MAX_PATH_LEN], *from;
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
   if (NULL == (from = slrn_make_from_string ())) return -1;
   fprintf (fp, "From: %s\n", from); header_lines++;
#endif
   fprintf (fp, "Subject: %s\n", subj);
   
   if (Slrn_User_Info.org != NULL)
     {
	header_lines++;
	fprintf (fp, "Organization: %s\n", Slrn_User_Info.org);
     }
   
   fprintf (fp, "Reply-To: %s\nFollowup-To: %s\n", Slrn_User_Info.replyto,
	    followupto);
   fprintf (fp, "Keywords: \nSummary: \n");
   
   header_lines += slrn_add_custom_headers (fp, Slrn_Post_Custom_Headers, NULL);

   fputs ("\n", fp);
   
   slrn_add_signature (fp);
   slrn_fclose (fp);
   
   if (Slrn_Editor_Uses_Mime_Charset)
     slrn_chmap_fix_file (file, 0);
   
   if (slrn_edit_file (Slrn_Editor_Post, file, header_lines, 1) >= 0)
     ret = slrn_post_file (file, NULL, 0);
   else ret = -1;
   
   if (Slrn_Use_Tmpdir) (void) slrn_delete_file (file);
   return ret;
}



int slrn_add_custom_headers (FILE *fp, char *headers, int (*write_fun)(char *, FILE *))
{
   int n;
   char *s, *s1, ch, last_ch;
   
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
   
   return n;
}

static int get_postpone_dir (char *dirbuf, size_t n)
{
   char *dir;
   
   if (Slrn_Postpone_Dir != NULL)
     dir = Slrn_Postpone_Dir;
   else 
     dir = Slrn_Save_Directory;
   
   if ((dir == NULL)
       || (-1 == slrn_make_home_dirname (dir, dirbuf, n))
       || (2 != slrn_file_exists (dirbuf)))
     {
	slrn_error_now (2, _("postpone_directory not specified or does not exist"));
	return -1;
     }

   return 0;
}

	     
static int postpone_file (char *file)
{
#ifdef VMS
   slrn_error (_("Not implemented yet. Sorry"));
   return -1;
#else
   char dir [SLRN_MAX_PATH_LEN];
   char dirfile [SLRN_MAX_PATH_LEN + 256];

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

void slrn_post_postponed (void)
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
