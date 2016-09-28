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

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include "group.h"
#include "art.h"
#include "startup.h"
#include "misc.h"
#include "score.h"
#include "util.h"
#include "slrn.h"
#include "common.h"
#include "strutil.h"

/* In pathological situations, "Subject" or "From" header fields may include
 * (MIME-encoded) linebreaks that would lead to invalid scorefile entries, so
 * simply replace them with blanks. */
static void remove_linebreaks (char *line)
{
   char c;
   if (line == NULL) return;
   while ((c = *line) != 0)
     {
	if ((c == '\r') || (c == '\n'))
	  *line = ' ';
	line++;
     }
}

/* Returns 0 if score file not modified, -1 if error, or 1 if scores modified. */
int slrn_edit_score (Slrn_Header_Type *h, char *newsgroup)
{
   char ch = 'e';
   int ich;
   char file[256];
   char qregexp[2*SLRL_DISPLAY_BUFFER_SIZE];
   unsigned int mm = 0, dd = 0, yy = 0;
   int days = 0;
   int use_expired = 0, force_score = 0;
   unsigned int linenum = 0;
   char *q, *ng = newsgroup;
   int score;
   FILE *fp;
   time_t myclock;
   int file_modified = 0, re_error = 0;
#if SLANG_VERSION < 20000
   SLRegexp_Type re;
   char buf[2*SLRL_DISPLAY_BUFFER_SIZE];
#else
   SLRegexp_Type *re;
#endif
   /* Note to translators: The translated string needs to have 10 characters.
    * Each pair becomes a valid response for "Subject", "From", "References",
    * "Edit" and "Cancel" (in that order); you cannot use any of the default
    * characters for any other field than they originally stood for!
    */
   char *typeresp = _("SsFfRrEeCc");
   /* Note to translators: The translated string needs to have 4 characters.
    * The two pairs become valid responses for "This group" and "All groups",
    * respectively. You cannot use any of the default characters for any other
    * field than they originally stood for!
    */
   char *scoperesp = _("TtAa");

   if (Slrn_Score_File == NULL)
     {
	slrn_error (_("A Score file has not been specified."));
	return -1;
     }

   if (Slrn_Prefix_Arg_Ptr == NULL)
     {
	char rsp;

	if (strlen(typeresp) != 10) /* Translator messed it up */
	  typeresp = "";
	ch = slrn_get_response ("SsFfRrEeCc\007", typeresp, _("Pick Score type: \001Subject, \001From, \001References, \001Edit, \001Cancel"));
	if (ch == 7) return -1;
	ch = slrn_map_translated_char ("SsFfRrEeCc", typeresp, ch) | 0x20;
	if (ch == 'c') return -1;

	while (1)
	  {
	     *qregexp = 0;
	     if (-1 == slrn_read_input ("Score: ", "=-9999", qregexp, 1, 0))
	       return -1;
	     force_score = (*qregexp == '=');
	     if (1 == sscanf (qregexp + force_score, "%d", &score))
	       break;
	  }

	if (strlen (scoperesp) != 4) /* Translator messed it up */
	  scoperesp = "";
	rsp = slrn_get_response ("TtaA\007", scoperesp, _("Which newsgroups: \001This group, \001All groups"));
	if (rsp == 7) return -1;
	if ((slrn_map_translated_char ("TtAa", scoperesp, rsp) | 0x20) == 'a')
	  ng = "*";

	while (1)
	  {
	     *qregexp = 0;
	     if (-1 == slrn_read_input (_("Expires (MM/DD/YYYY, DD-MM-YYYY, +NN (days) or leave blank): "), NULL, qregexp, 1, 0))
	       return -1;
	     if (*qregexp)
	       {
		  if (1 == sscanf(qregexp, "+%d",&days))
		    {
		       if (days < 1)
			 continue;
		       else
			 {
			    time_t now;
			    struct tm *newtime;
			    time(&now);
			    now += days * 24 * 3600;
			    newtime = localtime(&now);
			    dd = newtime->tm_mday;
			    mm = newtime->tm_mon + 1;
			    yy = newtime->tm_year + 1900;
			 }
		    }
		  else if (((3 != sscanf (qregexp, "%u/%u/%u", &mm, &dd, &yy))
			    && (3 != sscanf (qregexp, "%u-%u-%u", &dd, &mm, &yy)))
			    || (dd > 31)
			    || (mm > 12)
			    || (yy < 1900))
		    continue;
		  use_expired = 1;
		  break;
	       }
	     else
	       {
		  use_expired = 0;
		  break;
	       }
	  }
     }

   if ((NULL == (fp = slrn_open_home_file (Slrn_Score_File, "r+", file,
					   sizeof (file), 1)))
       && (NULL == (fp = slrn_open_home_file (Slrn_Score_File, "w+", file,
					      sizeof (file), 1))))
     {
	slrn_error (_("Unable to open %s"), file);
	return -1;
     }
#if SLANG_VERSION < 20000
   re.pat = (unsigned char*) qregexp;
   re.buf = (unsigned char*) buf;
   re.buf_len = sizeof (buf);
   re.case_sensitive = 0;
#else
   re = NULL;
#endif

   if (Slrn_Prefix_Arg_Ptr == NULL)
     {
	char *line;
	int comment;
	linenum = 1;
	while (EOF != (ich = getc (fp)))
	  {
	     if (ich == '\n')	linenum++;
	  }

	myclock = time((time_t *) 0);
	fprintf(fp, "\n%%BOS\n%%Score created by slrn on %s\n[%s]\nScore: %s%d\n",
		(char *) ctime(&myclock), ng, force_score ? "=" : "", score);

	if (use_expired)
	  fprintf (fp, "Expires: %u/%u/%u\n", mm, dd, yy);
	else fprintf (fp, "%%Expires: \n");

	line = slrn_safe_strmalloc (h->subject);
	slrn_subject_strip_was (line);
	remove_linebreaks (line);

	if ((NULL == (q = SLregexp_quote_string (line, qregexp, sizeof (qregexp)))) ||
	    (ch != 's') ||
#if SLANG_VERSION < 20000
	    (0 != SLang_regexp_compile (&re))
#else
	    (NULL == (re = SLregexp_compile (qregexp, SLREGEXP_CASELESS)))
#endif
	    )
	  {
	     re_error |= (ch == 's');
	     comment = 1;
	  }
	else
	  comment = 0;
	fprintf (fp, "%c\tSubject: %s\n",
		 (comment ? '%' : ' '), (q ? q : line));
	slrn_free (line);
#if SLANG_VERSION >= 20000
	if (re != NULL)
	  {
	     SLregexp_free (re);
	     re = NULL;
	  }
#endif

	line = slrn_safe_strmalloc (h->from);
	remove_linebreaks (line);
	if ((NULL == (q = SLregexp_quote_string (line, qregexp, sizeof (qregexp)))) ||
	    (ch != 'f') ||
#if SLANG_VERSION < 20000
	    (0 != SLang_regexp_compile (&re))
#else
	    (NULL == (re = SLregexp_compile (qregexp, SLREGEXP_CASELESS)))
#endif
	    )
	  {
	     re_error |= (ch == 'f');
	     comment = 1;
	  }
	else
	  comment = 0;
	fprintf (fp, "%c\tFrom: %s\n",
		 (comment ? '%' : ' '), (q ? q : line));
	slrn_free (line);
#if SLANG_VERSION >= 20000
	if (re != NULL)
	  {
	     SLregexp_free (re);
	     re = NULL;
	  }
#endif

	if ((NULL == (q = SLregexp_quote_string (h->msgid, qregexp, sizeof (qregexp)))) ||
	    (ch != 'r') ||
#if SLANG_VERSION < 20000
	    (0 != SLang_regexp_compile (&re))
#else
	    (NULL == (re = SLregexp_compile (qregexp, SLREGEXP_CASELESS)))
#endif
	    )
	  {
	     re_error |= (ch == 'r');
	     comment = 1;
	  }
	else
	  comment = 0;
	fprintf (fp, "%c\tReferences: %s\n",
		 (comment ? '%' : ' '), (q ? q : h->msgid));
#if SLANG_VERSION >= 20000
	if (re != NULL)
	  {
	     SLregexp_free (re);
	     re = NULL;
	  }
#endif

	if (NULL != (q = SLregexp_quote_string (h->xref, qregexp, sizeof (qregexp))))
	  {
	     fprintf (fp, "%%\tXref: %s\n", q);
	  }

	if (NULL != (q = SLregexp_quote_string (newsgroup, qregexp, sizeof (qregexp))))
	  {
	     fprintf (fp, "%%\tNewsgroup: %s\n", q);
	  }

	fprintf (fp, "%%EOS\n");
	file_modified = 1;
     }

   Slrn_Prefix_Arg_Ptr = NULL;

   if (-1 == slrn_fclose (fp))
     return -1;

   if (re_error)
     {
	slrn_error ("Error in pattern - commented new scorefile entry out.");
	return -1;
     }

   if (ch == 'e')
     {
	int status;

	status = slrn_edit_file (Slrn_Editor_Score, file, linenum + 1, 1);

	if ((status == -2)	       /* unmodified */
	    && (file_modified == 0))
	  return 0;

	if (status == -1)
	  return -1;

	/* drop */
     }
   if (Slrn_Scorefile_Open != NULL)
     slrn_free (Slrn_Scorefile_Open);
   Slrn_Scorefile_Open = slrn_safe_strmalloc (file);
   return 1;
}
