/* -*- mode: C; mode: fold -*- */
/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999 John E. Davis <davis@space.mit.edu>
 Copyright (c) 2001-2003 Thomas Schultz <tststs@gmx.de>

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

#define SLRNPULL_CODE
#include "slrnfeat.h"

/*{{{ System Includes */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <time.h>

#include <sys/stat.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <ctype.h>

#ifndef S_ISREG
# define S_ISREG(mode)  (((mode) & (_S_IFMT)) == (_S_IFREG))
#endif
 
#include <slang.h>
#include "jdmacros.h"
/*}}}*/

/*{{{ Local Includes */

#include "ttymsg.h"
#include "util.h"
#include "snprintf.h"
#if SLRN_USE_SLTCP
# include "sltcp.c"
#else
# include "clientlib.c"
#endif
#include "nntplib.h"
#include "nntpcodes.h"
#include "slrndir.h"
#include "version.h"
#include "ranges.h"
#include "vfile.h"

#include "sortdate.c"
#include "score.c"
#if SLRN_HAS_MIME
# include "mime.c"
#endif

#include "xover.c"

#undef SLRN_HAS_MSGID_CACHE
#define SLRN_HAS_MSGID_CACHE 1

#include "hash.c"

/*}}}*/

/*{{{ slrnpull global variables and structures */

static char *Slrnpull_Version = SLRN_VERSION;

#if SLRNPULL_USE_SETGID_POSTS
# define OUTGOING_DIR_MODE	(0777 | 03000)
#else
# define OUTGOING_DIR_MODE	(0777 | 01000)
#endif

static int Exit_Code;
#define SLRN_EXIT_UNKNOWN		1
#define SLRN_EXIT_BAD_USAGE		2
#define SLRN_EXIT_CONNECTION_FAILED	3
#define SLRN_EXIT_CONNECTION_LOST	4
#define SLRN_EXIT_SIGNALED		5
#define SLRN_EXIT_MALLOC_FAILED		10

#define SLRN_EXIT_FILEIO		20

FILE *Slrn_Debug_Fp = NULL;
int Slrn_Force_Authentication = 0;
char *SlrnPull_Dir = SLRNPULL_ROOT_DIR;
char *SlrnPull_Spool_News_Dir;
char *Group_Min_Max_File;	       /* relative to group dir */
char *Server_Min_File;		/* relative to group dir */
char *Overview_File;	       /* relative to group dir */
char *Headers_File = SLRN_SPOOL_HEADERS; /* relative to group dir */
char *Requests_Dir;
char *Outgoing_Dir;
char *Outgoing_Bad_Dir;
char *New_Groups_File = "new.groups";
char *New_Groups_Time_File = "new.groups-time";
char *Data_Dir = "data";
static char *Active_File = "active";

static int Stdout_Is_TTY;
static int Kill_Score;
static char *Active_Groups_File;
static time_t Start_Time;

#define CREATE_OVERVIEW 1


static int handle_interrupts (void);

typedef struct _Active_Group_Type /*{{{*/
{
   unsigned int flags;
   
   /* Unfortunately, three different sets of article ranges are required.  
    * Ideally, only one would be required but this does not seem to be 
    * possible.  This is because the articles in the spool directory 
    * created by slrnpull do not expire when the articles on the server
    * expire.  In addition, slrnpull removes duplicate articles so that
    * the articles present on the server may not actually be present in the 
    * spool directory.  
    * 
    * The main problem is that it appears that some servers will reuse 
    * article numbers from articles that have been cancelled.  The spool 
    * directory created by slrnpull does not know about cancel messages,
    * which means that the same article number can refer to two different
    * articles.  The ultimate solution would be for slrnpull to use its own
    * numbering scheme that is independent of the server's.  This would mean
    * caching all message-ids and stripping Xref headers from the articles
    * that are received from the server.  At first sight it would appear that
    * new Xref headers would have to be generated for the .overview files but
    * since slrnpull removes duplicates, this should not be necessary.  A
    * major advantage of this approach is that one could merge multiple feeds.
    * A rough estimate of the size of the message-id cache is 100 * 500 * 80,
    * or 4,000,000 bytes assuming 100 newsgroups with 500 articles per group
    * and a message id of 80 characters.  Of course that number would be
    * smaller if Pine were eliminated.
    */
   unsigned int min, max;	       /* range of articles that slrnpull has 
					* already dealt with */
   unsigned int active_min, active_max;/* article numbers that are in spool dir */
   unsigned int server_min;	       /* artcle numbers that server reports */
   unsigned int server_max;
   
   
   unsigned int max_to_get;	       /* if non-zero, get only this many */
   unsigned int expire_days;	       /* if zero, no expiration */
   int headers_only;	/* If non-zero, fetch headers only (offline reading) */
#define MAX_GROUP_NAME_LEN 80
   char name [MAX_GROUP_NAME_LEN + 1];
   char dirname [MAX_GROUP_NAME_LEN + 1];
   Slrn_Range_Type *headers;	/* list of articles that don't have bodies */
   Slrn_Range_Type *requests;	/* list of requested article bodies */
   struct _Active_Group_Type *next;
}

/*}}}*/
Active_Group_Type;

static char *Current_Newsgroup;

static Active_Group_Type *Active_Groups;
static Active_Group_Type *Active_Groups_Tail;

/*}}}*/

static FILE *MLog_Fp;
static FILE *ELog_Fp;
static FILE *KLog_Fp;

static void write_timestamp (FILE *fp) /*{{{*/
{
   struct tm *tms;
   time_t tloc;
   
   time (&tloc);
   tms = localtime (&tloc);
   
   fprintf (fp, "%02d/%02d/%04d %02d:%02d:%02d ",
	    tms->tm_mon + 1, tms->tm_mday, 1900 + tms->tm_year,
	    tms->tm_hour, tms->tm_min, tms->tm_sec);
   
}

/*}}}*/

static void write_log (FILE *fp, char *pre, char *buf)
{
   write_timestamp (fp);
   if (pre != NULL) fputs (pre, fp);
   fputs (buf, fp);
   fputc ('\n', fp);
   fflush (fp);
}

static void va_log (FILE *fp, char *pre, char *fmt, va_list ap) /*{{{*/
{
   char buf[2048];   

   slrn_vsnprintf (buf, sizeof (buf), fmt, ap);
   
   write_log (fp, pre, buf);
   
   if ((Stdout_Is_TTY == 0) || (fp == stdout) || (fp == stderr))
     return;
   if (fp == MLog_Fp) fp = stdout; else fp = stderr;

   write_log (fp, pre, buf);
}

/*}}}*/

static void log_message (char *fmt, ...) /*{{{*/
{
   va_list ap;
   
   va_start (ap, fmt);
   va_log (MLog_Fp, NULL, fmt, ap);
   va_end (ap);
}

/*}}}*/

static void log_error (char *fmt, ...) /*{{{*/
{
   va_list ap;
   
   va_start (ap, fmt);
   va_log (ELog_Fp, "***", fmt, ap);
   va_end (ap);
}

/*}}}*/

static void va_log_error (char *fmt, va_list ap) /*{{{*/
{
   va_log (ELog_Fp, "***", fmt, ap);
}

/*}}}*/

static void va_log_message (char *fmt, va_list ap) /*{{{*/
{
   va_log (MLog_Fp, NULL, fmt, ap);
}

/*}}}*/

static Active_Group_Type *find_group_type (char *name) /*{{{*/
{
   Active_Group_Type *g;
   
   g = Active_Groups;
   while (g != NULL)
     {
	if (!strcmp (name, g->name))
	  break;
	
	g = g->next;
     }
   
   return g;
}

/*}}}*/

static Active_Group_Type *add_group_type (char *name) /*{{{*/
{
   Active_Group_Type *g;
   
   g = (Active_Group_Type *) slrn_malloc (sizeof (Active_Group_Type), 1, 1);
   
   if (g == NULL)
     return NULL;
   
   strncpy (g->name, name, MAX_GROUP_NAME_LEN);   /* null terminated 
						   * by construction */
   
   if (Active_Groups_Tail != NULL)
     Active_Groups_Tail->next = g;
   else 
     Active_Groups = g;
   
   Active_Groups_Tail = g;
   return g;
}

/*}}}*/

static int do_mkdir (char *dir, int err) /*{{{*/
{
   if (0 == slrn_mkdir (dir))
     {
	log_message (_("Created dir %s."), dir);
	return 0;
     }
   
   if (errno == EEXIST)
     return 0;
	
   if (err)
     log_error (_("Unable to create directory %s. (errno = %d)"), dir, errno);

   return -1;
}

/*}}}*/

static FILE *open_server_min_file (Active_Group_Type *g, char *mode, /*{{{*/
				   char *file, size_t n)
{
   if (-1 == slrn_dircat (SlrnPull_Spool_News_Dir, g->dirname, file, n))
     return NULL;
   if (-1 == slrn_dircat (file, Server_Min_File, file, n))
     return NULL;
   
   return fopen (file, mode);
}

/*}}}*/

static unsigned int read_server_min_file (Active_Group_Type *g) /*{{{*/
{
   char file[SLRN_MAX_PATH_LEN + 1];
   char line[256];
   unsigned int retval;
   FILE *fp;

   fp = open_server_min_file (g, "r", file, sizeof (file));
   
   if (fp == NULL)
     return 0;

   if (NULL == fgets (line, sizeof(line), fp))
     {
	fclose (fp);
	log_error (_("Error reading %s."), file);
	return 0;
     }

   fclose (fp);
   
   if (1 != sscanf (line, "%u", &retval))
     {
	log_error (_("Error parsing %s."), file);
	return 0;
     }

   return retval;
}
/*}}}*/

static void write_server_min_file (Active_Group_Type *g, unsigned int val) /*{{{*/
{
   char file[SLRN_MAX_PATH_LEN + 1];
   FILE *fp;
   
   fp = open_server_min_file (g, "w", file, sizeof (file));
   if (fp == NULL)
     {
	log_error (_("Unable to open %s for writing."), file);
	return;
     }
   if (EOF == fprintf (fp, "%u", val))
     {
	log_error (_("Write to %s failed."), file);
	(void) fclose (fp);
	return;
     }
   if (-1 == slrn_fclose (fp))
     log_error (_("Error closing %s."), file);
}

/*}}}*/

static FILE *open_group_min_max_file (Active_Group_Type *g, char *mode, /*{{{*/
				      char *file, size_t n)
{
   if (-1 == slrn_dircat (SlrnPull_Spool_News_Dir, g->dirname, file, n))
     return NULL;
   if (-1 == slrn_dircat (file, Group_Min_Max_File, file, n))
     return NULL;
   
   return fopen (file, mode);
}

/*}}}*/
   
/* This function returns 1 upon success, -1 up parse error, and 0
 * if file could not be opened.
 */
static int read_group_min_max_file (Active_Group_Type *g) /*{{{*/
{
   char file[SLRN_MAX_PATH_LEN + 1];
   char line[256];
   unsigned int min, max;
   FILE *fp;

   g->min = 1;
   g->max = 0;

   fp = open_group_min_max_file (g, "r", file, sizeof (file));
   
   if (fp == NULL)
     return 0;
   
   if (NULL == fgets (line, sizeof(line), fp))
     {
	fclose (fp);
	log_error (_("Error reading %s."), file);
	return -1;
     }

   fclose (fp);
   
   if (2 != sscanf (line, "%u %u", &min, &max))
     {
	log_error (_("Error parsing %s."), file);
	return -1;
     }
   
   g->active_min = g->min = min;
   g->server_max = g->active_max = g->max = max;
   
   return 1;
}

/*}}}*/

static int write_group_min_max_file (Active_Group_Type *g) /*{{{*/
{
   char file[SLRN_MAX_PATH_LEN + 1];
   FILE *fp;
   
   fp = open_group_min_max_file (g, "w", file, sizeof (file));
   if (fp == NULL)
     {
	log_error (_("Unable to open %s for writing."), file);
	return -1;
     }
   if (EOF == fprintf (fp, "%u %u", g->min, g->max))
     {
	log_error (_("Write to %s failed."), file);
	(void) fclose (fp);
	return -1;
     }
   if (-1 == slrn_fclose (fp))
     {
	log_error (_("Error closing %s."), file);
	return -1;
     }
   return 0;
}

/*}}}*/

static int create_group_directory (Active_Group_Type *g) /*{{{*/
{
   char dirbuf [SLRN_MAX_PATH_LEN + 1];
   char *dir, *d, ch;
   unsigned int len;
   int status;
      
   strcpy (g->dirname, g->name); /* safe */
   
   d = g->dirname;
   while (*d != 0)
     {
	if (*d == '.') *d = SLRN_PATH_SLASH_CHAR;
	d++;
     }
   
   /* If the min-max file is available, we know the directory exists.  
    * Check it now.  Also, this provides a convenient check on the length
    * of the filename.
    */
   status = read_group_min_max_file (g);

   if (status == -1)
     return -1;
   
   if (status != 0)
     return 0;
   
   /* Does not exist so we will have to create the directory. */

   len = strlen (SlrnPull_Spool_News_Dir);
   
   if (sizeof (dirbuf) < len + strlen (g->dirname) + 2)
     return -1;
   
   strcpy (dirbuf, SlrnPull_Spool_News_Dir); /* safe */
   
   dirbuf [len] = SLRN_PATH_SLASH_CHAR;
   len++;
   
   dir = dirbuf + len;
   strcpy (dir, g->dirname); /* safe */
   
   d = dir + strlen (dir);
   
   if (0 == do_mkdir (dirbuf, 0))
     return 0;
   
   /* Go back and create it piece by piece. */
   d = dir;
   do
     {
	ch = *d;
	if ((ch == SLRN_PATH_SLASH_CHAR) || (ch == 0))
	  {
	     *d = 0;
	     if (-1 == do_mkdir (dirbuf, 1))
	       return -1;

	     *d = ch;
	  }
	d++;
     }
   while (ch != 0);
   
   return 0;
}

/*}}}*/

/* The argv list is NOT NULL terminated.
 */
static int chop_string (char *str, char **argv, int *argc_p, int max_args) /*{{{*/
{
   char *s;
   int argc;
   
   argc = 0;
   while (argc < max_args)
     {
	str = slrn_skip_whitespace (str);
	if (*str == 0)
	  break;
	
	s = slrn_strbrk (str, " \t\n");
	if (s != NULL)
	  *s = 0;

	argv[argc] = str;
	argc++;
	
	if (s == NULL) break;
	
	str = s + 1;
     }
   
   *argc_p = argc;
   return argc;
}

/*}}}*/

static int read_active_groups (void) /*{{{*/
{
   FILE *fp;
   char buf[256];
   unsigned int num;
   int default_max_to_get, default_expire_days, default_headers_only;
   
   fp = fopen (Active_Groups_File, "r");
   if (fp == NULL)
     {
	if (NULL == (fp = fopen (SYSCONFDIR "/" SLRNPULL_CONF, "r")))
	  {
	     log_error (_("Unable to open active groups file %s"), Active_Groups_File);
	     return -1;
	  }
	else
	  Active_Groups_File = SYSCONFDIR "/" SLRNPULL_CONF;
     }
   
   log_message (_("Reading %s"), Active_Groups_File);

   default_max_to_get = 50;
   default_expire_days = 10;
   default_headers_only = 0;

   num = 0;
   while (NULL != fgets (buf, sizeof(buf), fp))
     {
#define MAX_ARGS 10
	char *argv[MAX_ARGS];
	int argc;
	char *name, *arg;
	Active_Group_Type *g;
	int max_to_get;
	int expire_days;
	int headers_only;
	
	num++;
	
	name = slrn_skip_whitespace (buf);
	if ((*name == '#') || (*name == 0))
	  continue;
	
	slrn_trim_string (name);
	
	chop_string (name, argv, &argc, MAX_ARGS);
	
	name = argv[0]; argc--;
	if (strlen (name) > MAX_GROUP_NAME_LEN)
	  {
	     log_error (_("%s: line %u: group name too long."),
			 Active_Groups_File, num);
	     fclose (fp);
	     return -1;
	  }
	/* Make sure name is a valid name.  Here I just check for whitespace 
	 * in name which means it is invalid.
	 */
	arg = name;
	while (*arg != 0)
	  {
	     unsigned char uch;
	     
	     uch = (unsigned char) *arg;
	     
	     if (uch <= 32)
	       {
		  log_error (_("%s: line %u: Group name contains whitespace."),
			      Active_Groups_File, num);
		  fclose (fp);
		  return -1;
	       }
	     arg++;
	  }
	
	max_to_get = default_max_to_get;
	expire_days = default_expire_days;
	headers_only = default_headers_only;
	
	arg = argv[1];
	
	if (argc)
	  {
	     if ((*arg != '*') &&
		 ((1 != sscanf (arg, "%d", &max_to_get))
		  || (max_to_get < 0)))
	       {
		  log_error (_("%s: line %u: expecting positive integer in second field."),
			      Active_Groups_File, num);
		  fclose (fp);
		  return -1;
	       }
	     argc--;
	  }
	
	arg = argv[2];
	if (argc)
	  {
	     if ((*arg != '*') &&
		 ((1 != sscanf (arg, "%d", &expire_days))
		  || (expire_days < 0)))
	       {
		  log_error (_("%s: line %u: expecting positive integer in third field."),
			      Active_Groups_File, num);
		  fclose (fp);
		  return -1;
	       }
	     argc--;
	  }
	
	arg = argv[3];
	if (argc)
	  {
	     if ((*arg != '*') &&
		 ((1 != sscanf (arg, "%d", &headers_only))
		  || ((headers_only != 0) && (headers_only != 1))))
	       {
		  log_error (_("%s: line %u: expecting 0 or 1 in fourth field."),
			     Active_Groups_File, num);
		  fclose (fp);
		  return -1;
	       }
	     argc--;
	  }
	
	if (0 == strcmp (name, "default"))
	  {
	     default_expire_days = expire_days;
	     default_max_to_get = max_to_get;
	     default_headers_only = headers_only;
	     continue;
	  }

	if (NULL != find_group_type (name))
	  {
	     log_error (_("%s: line %u: group duplicated."), 
			 Active_Groups_File, num);
	     fclose (fp);
	     return -1;
	  }
	
	if (NULL == (g = add_group_type (name)))
	  {
	     log_error (_("%s: line %u: failed to add %s."),
			 Active_Groups_File, num, name);
	     fclose (fp);
	     return -1;
	  }
	
	g->max_to_get = (unsigned int) max_to_get;
	g->expire_days = (unsigned int) expire_days;
	g->headers_only = headers_only;

	if (-1 == create_group_directory (g))
	  {
	     fclose (fp);
	     return -1;
	  }
     }
   
   fclose (fp);
   return 0;
}

/*}}}*/

static char *do_dircat (char *dir, char *name)
{
   char *f;

   if (slrn_is_absolute_path (name))
     f = slrn_strmalloc (name, 0);
   else
     f = slrn_spool_dircat (dir, name, 0);

   if (f == NULL)
     {
	Exit_Code = SLRN_EXIT_MALLOC_FAILED;
	slrn_exit_error (_("Out of memory."));
     }

#if defined(__os2__) || defined(__NT__)
   slrn_os2_convert_path (f);
#endif

   return f;
}

static char *root_dircat (char *name) /*{{{*/
{
   return do_dircat (SlrnPull_Dir, name);
}

/*}}}*/

static char *data_dircat (char *name) /*{{{*/
{
   return do_dircat (Data_Dir, name);
}

/*}}}*/

static int make_filenames (void) /*{{{*/
{
   Data_Dir = root_dircat (Data_Dir);
   Outgoing_Dir = root_dircat (SLRNPULL_OUTGOING_DIR);
   Requests_Dir = root_dircat (SLRNPULL_REQUESTS_DIR);

   Active_Groups_File = root_dircat (SLRNPULL_CONF);
   SlrnPull_Spool_News_Dir = root_dircat (SLRNPULL_NEWS_DIR);
   
   New_Groups_Time_File = data_dircat (New_Groups_Time_File);
   Active_File = data_dircat (Active_File);
   Headers_File = data_dircat (Headers_File);
   New_Groups_File = data_dircat (New_Groups_File);
   
   Outgoing_Bad_Dir = slrn_spool_dircat (Outgoing_Dir, SLRNPULL_OUTGOING_BAD_DIR, 0);
   if (Outgoing_Bad_Dir == NULL)
     {
	Exit_Code = SLRN_EXIT_MALLOC_FAILED;
	slrn_exit_error (_("Out of memory."));
     }

   if (-1 == do_mkdir (SlrnPull_Spool_News_Dir, 1))
     return -1;
   
   if (-1 == do_mkdir (Data_Dir, 1))
     return -1;
   
   Overview_File = SLRN_SPOOL_NOV_FILE;
   Headers_File = SLRN_SPOOL_HEADERS;
   Group_Min_Max_File = ".minmax";
   Server_Min_File = ".servermin";

   return 0;
}

/*}}}*/

static int *listgroup_numbers (NNTP_Type *s, Active_Group_Type *g, unsigned int *nump) /*{{{*/
{
   int *numbers;
   unsigned int max, num, maxnum, curnum;
   char buf[256];
   int status;
   
   status = nntp_listgroup (s, g->name);
   if (status != OK_GROUP)
     {
	if (status == -1) log_error (_("Read failed."));
	log_error (_("listgroup %s failed: %s"), g->name, nntp_map_code_to_string (status));
	return NULL;
     }
   
   max = 0;
   num = 0;
   maxnum = 0;
   numbers = NULL;
     
   while (1 == (status = nntp_read_line (s, buf, sizeof (buf))))
     {
	if (num == max)
	  {
	     int *newnums;
	     
	     max += 1000;
	     newnums = (int *) slrn_realloc ((char *) numbers, max * sizeof (int), 1);
	     if (newnums == NULL)
	       {
		  slrn_free ((char *)numbers);
		  nntp_discard_output (s);
		  return NULL;
	       }
	     numbers = newnums;
	  }
	curnum = atoi (buf);
	if (curnum > maxnum) maxnum = curnum;
	numbers[num] = curnum;
	num++;
     }
   
   if (status == -1)
     {
	slrn_free ((char *) numbers);
	return NULL;
     }
   
   *nump = num;
   if (g->server_max < maxnum)
     g->server_max = maxnum;
   return numbers;
}

/*}}}*/

static unsigned int Num_Duplicates;

/* This function returns a malloc'ed array that contains all article numbers
 * in group g on server s. *nump is set to the length of the array.
 * If article numbers larger than g->server_max are found, that variable
 * gets updated. */
static int *list_server_numbers (NNTP_Type *s, Active_Group_Type *g,
				 int server_min, int server_max,
				 unsigned int *nump) /*{{{*/
{
   char *name; 
   unsigned int min, max;
   int status;
   char buf [512];
   int *numbers;
   unsigned int num_numbers, max_num_numbers;
   
   *nump = 0;
   
   name = g->name;
   if (1 != nntp_has_cmd (s, "XHDR"))
     return listgroup_numbers (s, g, nump);
   
   /* Server seems to have XHDR.  Good. */
   min = g->min;
   max = g->max + 1;
   if (max < min) max = min;

   /* If this is the first time this group has been visited, then do not grab
    * all the message-ids when the user specifies a limit to grab.
    */
   if ((max <= 1) && (g->max_to_get != 0)
       && ((unsigned int) server_max-server_min+1 > g->max_to_get))
     {
	max = (unsigned int) server_max - g->max_to_get + 1;
	if (server_max >= server_min)
	  log_message (_("The server may contain as many as %d articles in this group"),
		       server_max - server_min + 1);
	log_message (_("Only examining last %u articles since this is a new group"), g->max_to_get);
     }

   status = nntp_server_vcmd (s, "XHDR Message-ID %u-", max);
   if (status == -1)
     {
	log_error (_("Server failed to return proper code for XHDR.  The connection may be lost")); 
	return NULL;			       /* server closed? */
     }
   if (status == 224) status = OK_HEAD;/* Micro$oft broken server */
   if (status == ERR_ACCESS)
     { /* Command disabled by administrator. When we first test this outside
	* the group, the server won't tell us. Fall back to LISTGROUP ... */
	return listgroup_numbers (s, g, nump);
     }
   if (status != OK_HEAD)
     {
	log_error (_("Server failed XHDR command: %s"), s->rspbuf);
	return NULL;
     }
   
   num_numbers = 0;
   max_num_numbers = 0;
   numbers = NULL;
   
   min = max;
   while (1 == (status = nntp_read_line (s, buf, sizeof (buf))))
     {
	int num;
	char *b1, *b2;
	
	num = (int) atoi (buf);
	b1 = slrn_strchr (buf, '<');
	if (b1 == NULL)
	  {
	     /* defective server?? */
	     continue;
	  }
	b2 = slrn_strchr (b1, '>');
	if (b2 == NULL) continue;
	
	b2++;
	*b2 = 0;
	
	if (num > (int) max) max = (unsigned int) num;

	if (NULL != is_msgid_cached (b1, name, num, 0))
	  {
	     if (g->min > g->max) g->min = num;
	     g->max = max;	       /* was: g->max = num */
	     
	     if (g->server_max < (unsigned int) num) g->server_max = num;
	     
	     Num_Duplicates++;
	     continue;
	  }
	
	if (num_numbers == max_num_numbers)
	  {
	     int *newnums;
	     
	     max_num_numbers += 100;
	     newnums = (int *) slrn_realloc ((char *) numbers, max_num_numbers * sizeof (int), 1);
	     if (newnums == NULL)
	       {
		  slrn_free ((char *)numbers);
		  nntp_discard_output (s);
		  return NULL;
	       }
	     numbers = newnums;
	  }
	numbers [num_numbers] = num;
	num_numbers++;
     }

   log_message (_("%s: Retrieving %s %d-%d."), g->name,
		g->headers_only ? _("headers") : _("articles"),
		min, max);
   if (num_numbers) g->server_max = max;
   /* Otherwise, there were no unique articles in that range. */
   
   if (status == -1)
     {
	slrn_free ((char *) numbers);
	return NULL;
     }

   /* If there were no articles in the requested range, update g->max now
    * so that we do not have to try this range next time.
    * 
    * Unfortunately, this does not seem possible since the articles in the
    * requested range may have been cancelled and the server may reuse the
    * article numbers (yuk).  Note also that g->max has already been updated
    * for duplicate articles.
    */
   if (numbers == NULL)
     {
#if 0
	g->max = max;
#else
	log_message (_("%s: No articles in specified range."), g->name);
#endif
     }
   
	     

   *nump = num_numbers;
   return numbers;
}

/*}}}*/

static unsigned int Num_Killed;
static unsigned int Num_Articles_Received;
static unsigned int Num_Articles_To_Receive;

static void print_time_stats (NNTP_Type *s, int do_log) /*{{{*/
{
   time_t now;
   unsigned long in;
   unsigned long elapsed_time;
   unsigned int hour, min, sec;
   
   if ((Stdout_Is_TTY == 0) && (do_log == 0))
     return;
   
   if ((s == NULL) || (s->tcp == NULL))
     return;
   
   time (&now);
   
   elapsed_time = (unsigned long) now - (unsigned long) Start_Time;
   
   if (elapsed_time == 0)
     {
	if (do_log == 0) return;
	elapsed_time = 1;
     }
   
   in = sltcp_get_num_input_bytes (s->tcp);

   hour = elapsed_time / 3600;
   min = (elapsed_time - 3600 * hour) / 60;
   sec = elapsed_time % 60;

   if (Stdout_Is_TTY)
     {
	
	fprintf (stdout, _("%u/%u (%u killed), Time: %02u:%02u:%02u, BPS: %lu      "),
		 Num_Articles_Received, Num_Articles_To_Receive, Num_Killed,
		 hour, min, sec,
		 (unsigned long) (in / elapsed_time));
	fputc ('\r', stdout);
	fflush (stdout);
     }
   
   if (do_log)
     {
	log_message (_("%s: %u/%u (%u killed), Time: %02u:%02u:%02u, BPS: %lu"),
		     Current_Newsgroup, 
		     Num_Articles_Received, Num_Articles_To_Receive, Num_Killed,
		     hour, min, sec,
		     in / elapsed_time);
     }
}

/*}}}*/

static int write_xover_line (FILE *fp, Slrn_XOver_Type *xov) /*{{{*/
{
   if (fp == NULL)
     return 0;

#if CREATE_OVERVIEW
   if ((EOF == fprintf (fp,
			"%d\t%s\t%s\t%s\t%s\t%s\t%d\t%d",
			xov->id, xov->subject_malloced, 
			xov->from, xov->date_malloced, xov->message_id,
			xov->references, xov->bytes, xov->lines))
       || ((xov->xref != NULL) && (xov->xref[0] != 0)
	   && (EOF == fprintf (fp, "\tXref: %s", xov->xref)))
       || (EOF == fputc ('\n', fp)))
     {
	log_error (_("Error writing to overview database: %s:%d."), Current_Newsgroup, xov->id);
	return -1;
     }
#else
   (void) xov;
#endif
   return 0;
}

/*}}}*/

static int append_body (Active_Group_Type *g, int n, char *body) /*{{{*/
{
   char file[SLRN_MAX_PATH_LEN + 1];
   char buf[128];
   FILE *fp;
   
   slrn_snprintf (buf, sizeof(buf), "%d", n);
   
   if ((-1 == slrn_dircat (SlrnPull_Spool_News_Dir, g->dirname,
			   file, sizeof (file)))
       || (-1 == slrn_dircat (file, buf, file, sizeof (file))))
     return -1;
   
#ifdef __OS2__
   fp = fopen (file, "ab");
#else
   fp = fopen (file, "a");
#endif
   if (fp == NULL)
     {
	log_error (_("Unable to open %s for writing."), file);
	return -1;
     }

   if ((EOF == fputc ('\n', fp)) ||
       (EOF == fputs (body, fp)))
     {
	log_error (_("Error writing to %s."), file);
	fclose (fp);
	slrn_delete_file (file);
	return -1;
     }
   
   if (-1 == slrn_fclose (fp))
     {
	log_error (_("Error writing to %s."), file);
	slrn_delete_file (file);
	return -1;
     }

   return 0;
}
/*}}}*/

static int write_head_and_body (Active_Group_Type *g, int n, /*{{{*/
				char *head, char *body, 
				Slrn_XOver_Type *xov, FILE *xov_fp)
{
   char file [SLRN_MAX_PATH_LEN + 1];
   char buf[128];
   FILE *fp;
   
   if (head == NULL)
     {
	if (g->min > g->max) g->min = n;
	g->max = n;
	
	return 0;
     }
   
   slrn_snprintf (buf, sizeof (buf), "%d", n);
   
   if ((-1 == slrn_dircat (SlrnPull_Spool_News_Dir, g->dirname,
			   file, sizeof (file)))
       || (-1 == slrn_dircat (file, buf, file, sizeof (file))))
     return -1;
   
#ifdef __OS2__
   fp = fopen (file, "wb");
#else
   fp = fopen (file, "w");
#endif
   if (fp == NULL)
     {
	log_error (_("Unable to open %s for writing."), file);
	return -1;
     }
   
   if ((EOF == fputs (head, fp)) ||
       ((body!=NULL) && ((EOF == fputc ('\n', fp)) ||
			 (EOF == fputs (body, fp)))))
     {
	log_error (_("Error writing to %s."), file);
	fclose (fp);
	slrn_delete_file (file);
	return -1;
     }
   
   if (-1 == slrn_fclose (fp))
     {
	log_error (_("Error writing to %s."), file);
	slrn_delete_file (file);
	return -1;
     }

   if (-1 == write_xover_line (xov_fp, xov))
     return -1;

   if (g->min > g->max) g->active_min = g->min = n;
   g->active_max = g->max = n;
   
   return 0;
}

/*}}}*/

static int fetch_body (NNTP_Type *s, char **body) /*{{{*/
{
   int status;
   
   *body = NULL;

   print_time_stats (s, 0);

   status = nntp_get_server_response (s);
   if (status == -1)
     return -1;
   
   if (status != OK_BODY)
     return 0;
   
   if (NULL == (*body = nntp_read_and_malloc (s)))
     return -1;
   
   return 0;
}

/*}}}*/

static int get_bodies (NNTP_Type *s, int *numbers, /*{{{*/
		       char **heads, char **bodies, unsigned int num)
{
   unsigned int i;
   char buf[256], *b;
   char *crlf;
   
   crlf = "";
   b = buf;
   
   for (i = 0; i < num; i++)
     {
	bodies [i] = NULL;
	
	if (heads[i] == NULL)
	  continue;
	
	slrn_snprintf (b, sizeof (buf) - (size_t)(b - buf),
		       "%sbody %d", crlf, numbers[i]);
	b += strlen (b);
	
	crlf = "\r\n";
     }
   
   if (b == buf)
     return 0;
   
   if (-1 == nntp_start_server_cmd (s, buf))
     return -1;
   
   for (i = 0; i < num; i++)
     {	
	if (heads [i] == NULL)
	  continue;

	if (-1 == fetch_body (s, bodies + i))
	  return -1;
     }
   
   return 0;
}

/*}}}*/

static int fetch_head (NNTP_Type *s, int n, char **headers, Slrn_XOver_Type *xov) /*{{{*/
{
   int status;
   Slrn_Header_Type h;
   int score;
   Slrn_Score_Debug_Info_Type *sdi = NULL;
   
   *headers = NULL;

   print_time_stats (s, 0);

   status = nntp_get_server_response (s);
   if (status == -1)
     return -1;
   
   if (status != OK_HEAD)
     return 0;
   
   if (NULL == (*headers = nntp_read_and_malloc (s)))
     return -1;
   
   /* Now score this header. */
   if (-1 == xover_parse_head (n, *headers, xov))
     {
	slrn_free (*headers);
	*headers = NULL;
	return 0;
     }
   
   slrn_map_xover_to_header (xov, &h);

#if SLRN_HAS_MIME
   h.subject = slrn_safe_strmalloc (xov->subject_malloced);
   h.from = slrn_safe_strmalloc (xov->from);
   if (Slrn_Use_Mime)
     {
	slrn_rfc1522_decode_string (h.subject);
	slrn_rfc1522_decode_string (h.from);
     }
#endif
   
#if 1
   (void) is_msgid_cached (h.msgid, Current_Newsgroup, (unsigned int) n, 1);
#endif

   score = slrn_score_header (&h, Current_Newsgroup, (KLog_Fp != NULL) ? &sdi : NULL);
   if (score < Kill_Score)
     {
	Num_Killed++;
	if (KLog_Fp != NULL)
	  {
	     Slrn_Score_Debug_Info_Type *hlp = sdi;
	     fprintf (KLog_Fp, _("Score %d killed article %s\n"), score, h.msgid);
	     while (hlp != NULL)
	       {
		  if (hlp->description [0] != 0)
		    fprintf (KLog_Fp, _(" Score %c%5i: %s (%s:%i)\n"),
			     (hlp->stop_here ? '=' : ' '), hlp->score,
			     hlp->description, hlp->filename, hlp->linenumber);
		  else
		    fprintf (KLog_Fp, _(" Score %c%5i: %s:%i\n"),
			     (hlp->stop_here ? '=' : ' '), hlp->score,
			     hlp->filename, hlp->linenumber);
		  hlp = hlp->next;
	       }
	     fprintf (KLog_Fp, _("  Newsgroup: %s\n  From: %s\n  Subject: %s\n\n"),
		      Current_Newsgroup, h.from, h.subject);
	  }
	slrn_free (*headers);
	*headers = NULL;
/*	return 0; */
     }
   while (sdi != NULL)
     {
	Slrn_Score_Debug_Info_Type *hlp = sdi->next;
	slrn_free ((char *)sdi);
	sdi = hlp;
     }
#if SLRN_HAS_MIME
   slrn_free (h.subject);
   h.subject = xov->subject_malloced;
   slrn_free (h.from);
   h.from = xov->from;
#endif
#if 0
   /* This next call should add the message id to the cache. */
   (void) is_msgid_cached (h.msgid, Current_Newsgroup, (unsigned int) n, 1);
#endif
   return 0;
}

/*}}}*/

static int get_heads (NNTP_Type *s, int *numbers, char **heads, /*{{{*/
		      Slrn_XOver_Type *xovs, unsigned int num)
{
   unsigned int i;
   char buf[20*SLRN_MAX_QUEUED];
   char *b;
   char *crlf;
   
   b = buf;
   crlf = "";
   
   /* Final crlf added by nntp_start_server_cmd. */
   for (i = 0; i < num; i++)
     {
	unsigned int len = sizeof (buf) - (size_t) (b - buf);
	if (len < 20)
	  slrn_exit_error (_("Internal error: Buffer in get_heads not large enough!"));
	slrn_snprintf (b, len, "%shead %d", crlf, numbers[i]);
	crlf = "\r\n";
	b += strlen (b);
	
	heads [i] = NULL;
	memset ((char *) (xovs + i), 0, sizeof (Slrn_XOver_Type));
     }

   if (-1 == nntp_start_server_cmd (s, buf))
     return -1;

   for (i = 0; i < num; i++)
     {
	if (-1 == fetch_head (s, numbers[i], heads + i, xovs + i))
	  return -1;
     }
   
   return 0;
}

/*}}}*/

static FILE *open_xover_file (Active_Group_Type *g, char *mode) /*{{{*/
{
#if CREATE_OVERVIEW
   char ov_file [SLRN_MAX_PATH_LEN + 1];
   FILE *fp;
   
   fp = NULL;
   if ((-1 != slrn_dircat (SlrnPull_Spool_News_Dir, g->dirname,
			   ov_file, sizeof (ov_file)))
       && (-1 != slrn_dircat (ov_file, Overview_File,
			      ov_file, sizeof (ov_file))))
     {
	fp = fopen (ov_file, mode);
	if (fp == NULL)
	  log_error (_("Unable to open overview file %s.\n"), ov_file);
     }
   
   return fp;
#else
   (void) g;
   (void) mode;
   return NULL;
#endif
}

/*}}}*/

static void get_marked_bodies (NNTP_Type *s, Active_Group_Type *g) /*{{{*/
{
   char *heads[SLRN_MAX_QUEUED];
   char *bodies[SLRN_MAX_QUEUED];
   int numbers[SLRN_MAX_QUEUED];
   unsigned int i, num;
   int max, bmin, bmax;
   Slrn_Range_Type *r;

   Num_Articles_Received = 0;
   Num_Killed = 0;
   Num_Articles_To_Receive = 0;
   
   if (NULL == (r = g->requests))
     return;
   
   while (r!=NULL)
     {
	Num_Articles_To_Receive += r->max - r->min + 1;
	r = r->next;
     }
   r = g->requests;
   
   log_message (_("%s: Retrieving requested article bodies."), g->name);
   
   for (i = 0; i < SLRN_MAX_QUEUED; i++)
     {
	heads[i] = ""; /* insert dummy headers so get_bodies won't skip them */
     }
   
   max = r->min;
   while (r != NULL)
     {
	for (i = 0; i < SLRN_MAX_QUEUED; i++)
	  {
	     numbers[i] = max;
	     max++;
	     if (max > r->max)
	       {
		  r = r->next;
		  if (r == NULL)
		    {
		       i++;
		       break;
		    }
		  max = r->min;
	       }
	  }
	num = i;
	
	if (-1 == get_bodies (s, numbers, heads, bodies, num))
	  break;
	
	Num_Articles_Received += num;
	
	for (i=0; i < num; i++)
	  {
	     if ((bodies[i]!=NULL) &&
		 (-1 == append_body (g, numbers[i], bodies[i])))
	       {
		  slrn_free(bodies[i]);
		  bodies[i] = NULL;
	       }
	  }

	/* Update headers list */
	i=0;
	while ((i<num) && (bodies[i]==NULL))
	  i++;
	if (i<num)
	  bmin = bmax = numbers[i];
	while (i < num)
	  {
	     i++;
	     while ((i < num) && (bodies[i]==NULL))
	       i++; /* skip bodies that were unavailable */
	     if ((i==num) || (numbers[i] > bmax+1))
	       {
		  g->headers = slrn_ranges_remove (g->headers, bmin, bmax);
		  if (i!=num)
		    bmin = bmax = numbers[i];
	       }
	     else
	       bmax++;
	  }
	
	/* Free bodies */
	for (i=0; i < num; i++)
	  slrn_free(bodies[i]);
     }
   print_time_stats (s, 1);
}
/*}}}*/

static int get_articles (NNTP_Type *s, Active_Group_Type *g, int *numbers, unsigned int num) /*{{{*/
{
   unsigned int i;
#ifndef SLRN_MAX_QUEUED
# define SLRN_MAX_QUEUED 10
#endif
   char *heads[SLRN_MAX_QUEUED];
   char *bodies[SLRN_MAX_QUEUED];
   Slrn_XOver_Type xovs [SLRN_MAX_QUEUED];
   int ret;
   FILE *fp;
   
   if (-1 == get_heads (s, numbers, heads, xovs, num))
     return -1;

   ret = 0;
   
   if ((g->headers_only) ||
       (-1 != get_bodies (s, numbers, heads, bodies, num)))
     {
	fp = open_xover_file (g, "a");

	for (i = 0; i < num; i++)
	  {
	     if (-1 == write_head_and_body (g, numbers[i], heads[i],
					    g->headers_only ? NULL : bodies[i],
					    xovs + i, fp))
	       {
		  ret = -1;
		  break;
	       }
	  }
	
	if (fp != NULL)
	  {
	     if (-1 == slrn_fclose (fp))
	       {
		  log_error (_("Error closing overview file for %s."), g->name);
		  ret = -1;
	       }
	  }
     }

   if (g->headers_only) /* "register" new articles without body */
     {
	int bmin, bmax;
	unsigned int j=0;
	
	while ((j < num) && (heads[j]==NULL))
	  j++;
	if (j < num)
	  bmin = bmax = numbers[j];
	while (j < num)
	  {
	     j++;
	     while ((j < num) && (heads[j]==NULL))
	       j++; /* skip articles that were unavailable */
	     if ((j==num) || (numbers[j] > bmax+1))
	       {
		  g->headers = slrn_ranges_add (g->headers, bmin, bmax);
		  if (j!=num)
		    bmin = bmax = numbers[j];
	       }
	     else
	       bmax++;
	  }
     }
   
   for (i = 0; i < num; i++) 
     {
	if (!g->headers_only)
	  slrn_free (bodies[i]);
	slrn_free (heads[i]);
	slrn_free (xovs[i].subject_malloced);
	slrn_free (xovs[i].date_malloced);
	slrn_free_additional_headers (xovs[i].add_hdrs);
     }
   return ret;
}

/*}}}*/

static int get_group_articles (NNTP_Type *s, Active_Group_Type *g, 
			       int server_min, int server_max, int marked_bodies) /*{{{*/
{
   unsigned int gmin, gmax;
   int *numbers;
   unsigned int num_numbers, i, imin;
   
   /* Don't request bodies that are no longer there. */
   if (server_min > 1)
     g->requests = slrn_ranges_remove (g->requests, 1, server_min-1);
   get_marked_bodies (s, g);
   
   if (marked_bodies)
     return 0;

   Num_Articles_Received = 0;
   Num_Killed = 0;
   Num_Articles_To_Receive = 0;
   
   gmin = g->min;
   gmax = g->max;
   
   if (((server_min > server_max) || (server_max < 0))
       || (((unsigned int)server_max <= gmax) && (gmin <= gmax)))
     {
	log_message (_("%s: no new articles available."), g->name);
	return 0;
     }

   Num_Duplicates = 0;
   numbers = list_server_numbers (s, g, server_min, server_max, &num_numbers);
   if (Num_Duplicates)
     log_message (_("%u duplicates removed leaving %u/%u."), 
		  Num_Duplicates, num_numbers, num_numbers + Num_Duplicates);
	
   if (numbers == NULL) return -1;
   
   i = 0;
   while ((i < num_numbers) && (numbers[i] <= (int) gmax))
     i++;
   
   if (i == num_numbers)
     {
	g->max = g->server_max;
	log_message (_("%s: No new articles available."), g->name);
	slrn_free ((char *) numbers);
	return 0;
     }
   
   log_message (_("%s: %u articles available."), g->name, num_numbers - i);
   
   /* Hmmm...  How shall g->max_to_get be defined?  Here I assume that it 
    * means to attempt to retrieve the last max_to_get articles.
    */
   if ((g->max_to_get != 0) && (g->max_to_get + i < num_numbers))
     {
	log_message (_("%s: Only retrieving last %u articles."), g->name, g->max_to_get);
	i = num_numbers - g->max_to_get;
     }
	
   imin = i;

   (void) slrn_open_score (g->name);
   
   Num_Articles_To_Receive = num_numbers - i;
   
   while (i < num_numbers)
     {
	int ns[SLRN_MAX_QUEUED];
	unsigned int j;
	
	j = 0;
	while ((i < num_numbers) && (j < SLRN_MAX_QUEUED))
	  {
	     ns[j] = numbers[i];
	     i++;
	     j++;
	  }
	
	print_time_stats (s, 0);
	(void) get_articles (s, g, ns, j);
	
	Num_Articles_Received += j;
     }
   
   g->max = g->server_max;
   
   (void) slrn_close_score ();
   slrn_clear_requested_headers ();
   slrn_free ((char *) numbers);
   return 0;
}

/*}}}*/

static int pull_news (NNTP_Type *s, int marked_bodies) /*{{{*/
{
   int status;
   Active_Group_Type *g;
   
   g = Active_Groups;
   while (g != NULL)
     {
	int min, max;

	log_message (_("Fetching articles for %s."), g->name);
	
	status = nntp_select_group (s, g->name, &min, &max);
	if (status != OK_GROUP)
	  {
	     log_error (_("Error selecting group %s.  Code = %d: %s"), g->name, 
			status, nntp_map_code_to_string (status));

	     if (status == -1)
	       break;

	     g = g->next;
	     continue;
	  }

	write_server_min_file (g, min);
	if (g->server_max > (unsigned int) max)
	  g->server_max = max;

	Current_Newsgroup = g->name;
	
	(void) get_group_articles (s, g, min, max, marked_bodies);
	
	print_time_stats (s, 1);

	g = g->next;
     }	
	     
   return 0;
}

/*}}}*/

static int post_file (NNTP_Type *s, char *file) /*{{{*/
{
   FILE *fp;
   int status;
   char buf[8 * 1024];
   
   log_message (_("Attempting to post %s..."), file);

   fp = fopen (file, "r");
   if (fp == NULL)
     {
	log_error (_("Unable to open file %s for posting."), file);
	return -1;
     }
   
   status = nntp_post_cmd (s);
   if (status != CONT_POST)
     {
	log_error (_("Server failed post cmd.  Code = %d: %s"), 
		   status, nntp_map_code_to_string (status));
	fclose (fp);
	return -1;
     }
   
   while (NULL != fgets (buf, sizeof (buf), fp))
     {
	char *b;
	
	/* Kill possible \r and \r\n.  We will add it later */

	b = buf + strlen (buf);
	if ((b != buf) && (*(b - 1) == '\n'))
	  b--;
	if ((b != buf) && (*(b - 1) == '\r'))
	  b--;
	*b = 0;
	
	if ((-1 == nntp_fputs_server (s, buf))
	    || (-1 == nntp_fputs_server (s, "\r\n")))
	  {
	     log_error (_("Write to server failed while posting %s."), file);
	     fclose (fp);
	     return -1;
	  }
     }
   
   fclose (fp);

   status = nntp_end_post (s);
   if (status == -1)
     {
	log_error (_("Write to server failed while posting %s."), file);
	return -1;
     }
   
   if (status != OK_POSTED)
     {
	char *name;
	char bad_file [SLRN_MAX_PATH_LEN + 1];
	
	log_error (_("Article %s rejected. status = %d: %s."), file, status, s->rspbuf);
	
	name = slrn_basename (file);
	if (-1 == slrn_dircat (Outgoing_Bad_Dir, name,
			       bad_file, sizeof (bad_file)))
	  return -1;
	
	log_error (_("Saving article in %s..."), Outgoing_Bad_Dir);
	if (-1 == rename (file, bad_file))
	  log_error (_("Failed to rename %s to %s."), file, bad_file);
	
	return -1;
     }
   
   if (-1 == slrn_delete_file (file))
     log_error (_("Unable to delete %s after posting."), file);
   
   return 0;
}

/*}}}*/

static int make_outgoing_dir (char *dir) /*{{{*/
{
   log_error (_("%s directory does not exist.  Creating it..."), dir);
   if (-1 == slrn_mkdir (dir))
     {
	log_error (_("Unable to create %s."), dir);
	return -1;
     }
	
   if (-1 == chmod (dir, OUTGOING_DIR_MODE))
     log_error (_("chmod 0%o failed on %s."), OUTGOING_DIR_MODE, dir);

   return 0;
}

/*}}}*/

static int post_outgoing (NNTP_Type *s) /*{{{*/
{
   Slrn_Dir_Type *dp;
   Slrn_Dirent_Type *df;
   int n;
   char file [SLRN_MAX_PATH_LEN + 1];
   
   dp = slrn_open_dir (Outgoing_Dir);
   if (dp == NULL)
     return make_outgoing_dir (Outgoing_Dir);


   if (2 != slrn_file_exists (Outgoing_Bad_Dir))
     (void) make_outgoing_dir (Outgoing_Bad_Dir);
	
   if (s->can_post == 0)
     {
	log_error (_("Server does not permit posting at this time."));
	return 0;
     }

   n = 0;
   while (NULL != (df = slrn_read_dir (dp)))
     {
	char *name;
	
	name = df->name;

	if (*name != 'X')
	  continue;
	
	if (-1 == slrn_dircat (Outgoing_Dir, name, file, sizeof (file)))
	  break;
	
	if (1 != slrn_file_exists (file))
	  continue;
	
	if (-1 == post_file (s, file))
	  log_error (_("Posting of %s failed."), file);
	else 
	  {
	     log_message (_("%s posted."), file);
	     n++;
	  }
     }
   
   slrn_close_dir (dp);
   return n;
}

/*}}}*/

static int write_active (void) /*{{{*/
{
   Active_Group_Type *g = Active_Groups;
   FILE *fp;
   char file [SLRN_MAX_PATH_LEN + 5];
   
   slrn_snprintf (file, sizeof (file), "%s.tmp", Active_File);
   
   fp = fopen (file, "w");
   if (fp == NULL)
     {
	log_error (_("Unable to create tmp active file (%s)."), file);
	return -1;
     }
   
   while (g != NULL)
     {
	if (EOF == fprintf (fp, "%s %u %u y\n", g->name, 
			    g->active_max, g->active_min))
	  {
	     fclose (fp);
	     goto write_error;
	  }
	
	(void) write_group_min_max_file (g);
	  
	g = g->next;
     }
   
   if (0 == slrn_fclose (fp))
     {
	(void) slrn_delete_file (Active_File);
	if (-1 == rename (file, Active_File))
	  {
	     log_error (_("Failed to rename %s to %s."), file, Active_File);
	     return -1;
	  }
	return 0;
     }
   
   write_error:
   log_error (_("Write failed to tmp active file (%s)."), file);
   (void) slrn_delete_file (file);
   return -1;
}

/*}}}*/

static NNTP_Type *Pull_Server;
static time_t Actual_Start_Time;

static void connection_lost_hook (NNTP_Type *s)
{
   log_error (_("Connection to %s lost. Performing shutdown."), s->host);
   (void) write_active ();
   Exit_Code = SLRN_EXIT_CONNECTION_LOST;
   slrn_exit_error (NULL);
}

   
static int open_servers (char *host) /*{{{*/
{
#if SLRN_USE_SLTCP
   if (-1 == sltcp_open_sltcp ())
     {
	fprintf (stderr, _("Error opening sltcp interface.\n"));
	return -1;
     }
#endif
   time (&Actual_Start_Time);
   Pull_Server = nntp_open_server (host, -1);
   if (Pull_Server == NULL)
     {
#if SLRN_USE_SLTCP
	sltcp_close_sltcp ();
#endif
	return -1;
     }

   /* Since multple commands are sent, reconnection is not good idea since
    * the context is fuzzy.
    */

   /* Pull_Server->flags |= NNTP_RECONNECT_OK; */

   NNTP_Connection_Lost_Hook = connection_lost_hook;

   sltcp_reset_num_io_bytes (Pull_Server->tcp);
   time (&Start_Time);
   
   /* Probe for XHDR now because DNEWS does not handle probing later properly. */
   (void) nntp_has_cmd (Pull_Server, "XHDR");
   return 0;
}

/*}}}*/

static void print_stats (unsigned long bytes_in, unsigned long bytes_out) /*{{{*/
{
   time_t done;
   
   if (Actual_Start_Time == 0)
    done = 0;
   else
     time (&done);

   log_message (_("A total of %lu bytes received, %lu bytes sent in %ld seconds."),
		bytes_in, bytes_out, (long) done - (long) Actual_Start_Time);
}

/*}}}*/

static void close_servers (void) /*{{{*/
{
   unsigned long bytes_in = 0;
   unsigned long bytes_out = 0;

#if SLRN_USE_SLTCP
   (void) sltcp_close_sltcp ();
#endif

   if (Pull_Server != NULL)
     {
	if (Pull_Server->tcp != NULL) 
	  {
	     bytes_in = sltcp_get_num_input_bytes (Pull_Server->tcp);
	     bytes_out = sltcp_get_num_output_bytes (Pull_Server->tcp);
	  }
	nntp_close_server (Pull_Server);
     }
   Pull_Server = NULL;
   print_stats (bytes_in, bytes_out);
}

/*}}}*/

static void init_signals (void);

static void open_log_files (char *logfile, char *kill_logfile) /*{{{*/
{
   char file [SLRN_MAX_PATH_LEN + 1];
   
   ELog_Fp = stderr;
   MLog_Fp = stdout;
   
   if (-1 == slrn_dircat (SlrnPull_Dir, logfile, file, sizeof (file)))
     return;

   MLog_Fp = fopen (file, "a");
   if (MLog_Fp == NULL)
     {
	MLog_Fp = stdout;
	log_error (_("Unable to open %s for logging."), file);
	return;
     }
   
   ELog_Fp = MLog_Fp;
   
   if ((kill_logfile == NULL) || 
       (-1 == slrn_dircat (SlrnPull_Dir, kill_logfile, file, sizeof (file))))
     return;
   
   KLog_Fp = fopen (file, "a");
   if (KLog_Fp == NULL)
     log_error (_("Unable to open %s for logging."), file);
}

/*}}}*/

static void close_log_files (void) /*{{{*/
{
   if ((MLog_Fp != NULL) && (MLog_Fp != stdout))
     fclose (MLog_Fp);

   if ((ELog_Fp != NULL) && (ELog_Fp != stderr) && (ELog_Fp != MLog_Fp))
     fclose (ELog_Fp);
   
   if (KLog_Fp != NULL)
     fclose (KLog_Fp);
   
   ELog_Fp = stderr;
   MLog_Fp = stdout;
}

/*}}}*/

static void usage (char *pgm) /*{{{*/
{
   log_error (_("%s usage: %s [options]\n\
 Options:\n\
  -d SPOOLDIR          Spool directory to use.\n\
  -h HOSTNAME          Hostname of NNTP server to connect to.\n\
  --debug FILE         Write dialogue with server to FILE.\n\
  --expire             Perform expiration, but do not pull news.\n\
  --help               Print this usage information.\n\
  --kill-log FILE      Keep a log of all killed articles in FILE.\n\
  --kill-score SCORE   Kill articles with a score below SCORE.\n\
  --logfile FILE       Use FILE as the log file.\n\
  --marked-bodies      Only fetch bodies that were marked for download.\n\
  --new-groups         Get a list of new groups.\n\
  --no-post            Do not post news.\n\
  --post-only          Post news, but do not pull new news.\n\
  --rebuild            Like --expire; additionally rebuilds overview files.\n\
  --version            Show the version number.\n\
"),
	      pgm, pgm);
   close_log_files ();
   exit (SLRN_EXIT_BAD_USAGE);
}

/*}}}*/

static void show_version (char *pgm)
{
   log_error (_("%s version: %s\n"), pgm, Slrnpull_Version);
   close_log_files ();
   exit (0);
}


static int read_score_file (void) /*{{{*/
{
   char file [SLRN_MAX_PATH_LEN + 1];
   
   if (-1 == slrn_dircat (SlrnPull_Dir, SLRNPULL_SCORE_FILE,
			  file, sizeof (file)))
     return -1;
   
   if (1 != slrn_file_exists (file))
     return 0;

   return slrn_read_score_file (file);
}

/*}}}*/

static int do_expire (int rebuild);
static int get_new_groups (NNTP_Type *s);
static int read_authinfo (void);
static int read_headers_files (void);
static int write_headers_files (void);
static void read_requests_files (void);

int main (int argc, char **argv) /*{{{*/
{
   char *host = NULL;
   char *pgm;
   int expire_mode;
   int post_mode; /* 0==don't post; 1==post only */
   int marked_bodies=0; /* 1==get marked bodies only*/
   int check_new_groups = 0;
   char *dir;
   char *logfile;
   char *kill_logfile = NULL;

#if defined(HAVE_SETLOCALE) && defined(LC_ALL)
   (void) setlocale(LC_ALL, "");
#endif
#ifdef ENABLE_NLS
   bindtextdomain(PACKAGE,LOCALEDIR);
   textdomain(PACKAGE);
#endif
   
   pgm = argv[0];
   argv++; argc--;
   
   Stdout_Is_TTY = isatty (fileno(stdout));
   expire_mode = 0;
   post_mode = -1;

   MLog_Fp = stdout;
   ELog_Fp = stderr;
   logfile = SLRNPULL_LOGFILE;

   dir = getenv ("SLRNPULL_ROOT");
   
   while (argc > 0)
     {
	char *arg;
	
	arg = *argv++; argc--;
	
	if (!strcmp (arg, "--help")) usage (pgm);

	if (!strcmp (arg, "-h") && (argc > 0))
	  {
	     host = *argv;
	     argv++; argc--;
	  }
	else if (!strcmp (arg, "-d") && (argc > 0))
	  {
	     dir = *argv;
	     argv++; argc--;
	  }
	else if (!strcmp (arg, "--expire"))
	  expire_mode = 1;
	else if (!strcmp (arg, "--rebuild"))
	  expire_mode = 2;
	else if ((!strcmp (arg, "--post-only"))
		 || (!strcmp (arg, "--post")))
	  post_mode = 1;
	else if (!strcmp (arg, "--no-post"))
	  post_mode = 0;
	else if (!strcmp (arg, "--new-groups"))
	  check_new_groups = 1;
	else if (!strcmp (arg, "--marked-bodies"))
	  marked_bodies = 1;
	else if (!strcmp (arg, "--version"))
	  show_version (pgm);
	else if (!strcmp (arg, "--logfile") && (argc > 0))
	  {
	     logfile = *argv;
	     argv++; argc--;
	  }
	else if (!strcmp (arg, "--kill-log") && (argc > 0))
	  {
	     kill_logfile = *argv;
	     argv++; argc--;
	  }
	else if (!strcmp (arg, "--kill-score") && (argc > 0))
	  {
	     Kill_Score = atoi (*argv);
	     argv++; argc--;
	  }
	else if (!strcmp (arg, "--debug") && (argc > 0))
	  {
	     if (NULL == (Slrn_Debug_Fp = fopen (*argv, "w")))
	       {
		  fprintf (stderr, _("Unable to create debug file %s\n"), *argv);
		  exit (1);
	       }
             else
	       setbuf (Slrn_Debug_Fp, (char*) NULL);
	     
	     argv++;
	     argc--;
	  }

	else usage (pgm);
     }

   if (dir != NULL)
     SlrnPull_Dir = dir;
   
   if (SlrnPull_Dir == NULL)
     {
	fprintf (stderr, _("The slrnpull spool directory has not been defined."));
	return 1;
     }
   
   open_log_files (logfile, kill_logfile);

   if (expire_mode)
     log_message (_("slrnpull started in expire mode."));
   else
     log_message (_("slrnpull started."));
   
   SLang_init_case_tables ();
   
   Exit_Code = SLRN_EXIT_FILEIO;

   if (-1 == make_filenames ())
     slrn_exit_error (NULL);

   if (2 != slrn_file_exists (Requests_Dir))
     make_outgoing_dir (Requests_Dir);
   
   if (-1 == read_active_groups ())
     slrn_exit_error (NULL);
   
   if (-1 == read_authinfo ())
     slrn_exit_error (NULL);
   
   if (-1 == read_headers_files ())
     slrn_exit_error (NULL);
   
   if (post_mode != 1)
     read_requests_files ();
   
   if (expire_mode)
     {
	if (-1 == do_expire (expire_mode == 2))
	  slrn_exit_error (NULL);
	if (-1 == write_headers_files ())
	  {
	     Exit_Code = SLRN_EXIT_FILEIO;
	     slrn_exit_error (NULL);
	  }
	
	close_log_files ();
	return 0;
     }

   if (-1 == read_score_file ())
     slrn_exit_error (NULL);
     
   Exit_Code = SLRN_EXIT_UNKNOWN;

   if (-1 == open_servers (host))
     {
	Exit_Code = SLRN_EXIT_CONNECTION_FAILED;
	slrn_exit_error (_("Unable to initialize server."));
     }

   SLTCP_Interrupt_Hook = handle_interrupts;
   
   if (post_mode) post_outgoing (Pull_Server);
   
   if (check_new_groups)
     (void) get_new_groups (Pull_Server);
       
   if (post_mode != 1)
     {
	init_signals ();
	pull_news (Pull_Server, marked_bodies);
	if ((-1 == write_headers_files ()) | /* avoid short-circuit */
	    (-1 == write_active ()))
	  {
	     Exit_Code = SLRN_EXIT_FILEIO;
	     slrn_exit_error (NULL);
	  }
     }

   close_servers ();

   close_log_files ();
   
   return 0;
}

/*}}}*/


/* Map 1998 --> 98, 2003 --> 3, etc.... */
static int rfc977_patchup_year (int year)
{
   return year - 100 * (year / 100);
}


static int get_new_groups (NNTP_Type *s)
{
   FILE *fp_time;
   FILE *fp_ng;
   time_t tloc;
   struct tm *tm_struct;
   char line [1024];
   int num;
   char *p;
   
   log_message (_("Checking for new groups."));
   
   if (NULL == (fp_ng = fopen (New_Groups_File, "a")))
     {
	log_message (_("Unable to open new groups file %s.\n"), New_Groups_File);
	return -1;
     }
   
   time (&tloc);

   if (NULL != (fp_time = fopen (New_Groups_Time_File, "r")))
     {
	char ch;
	int i;
	int parse_error;
	
	*line = 0;
	(void) fgets (line, sizeof (line), fp_time);
	(void) fclose (fp_time);
	
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

	switch (nntp_server_cmd (s, line))
	  {
	   case OK_NEWGROUPS:
	     break;
	     
	   case ERR_FAULT:
	     return 0;
	     
	   case ERR_COMMAND:
	     log_message (_("Server does not implement NEWGROUPS command."));
	     return 0;
	     
	   default:
	     slrn_message (_("Server failed to return proper response to NEWGROUPS:\n%s\n"),
			   s->rspbuf);
	     
	     goto parse_error_label;
	  }
	
	num = 0;
	while (1 == nntp_read_line (s, line, sizeof (line)))
	  {
	     if ((EOF == fputs (line, fp_ng))
		 || (EOF == fputc ('\n', fp_ng)))
	       {
		  log_error (_("Write to %s failed."), New_Groups_File);
		  (void) fclose (fp_ng);
		  return -1;
	       }
	     num++;
	  }
	
	if (-1 == slrn_fclose (fp_ng))
	  return -1;
	
	log_message (_("%d new groups found."), num);
	
	
	parse_error_label:

	if (parse_error)
	  {
	     log_message (_("%s appears corrupt, expected to see: NEWGROUPS yymmdd hhmmss GMT, I will patch the file up for you."),
			  New_Groups_File);
	  }
     }
      
   if (NULL == (fp_time = fopen (New_Groups_Time_File, "w")))
     return -1;
   
#if defined(VMS) || defined(__BEOS__)
   /* gmtime is broken on BEOS */
   tm_struct = localtime (&tloc);
   tm_struct->tm_year = rfc977_patchup_year (1900 + tm_struct->tm_year);
   fprintf (fp_time, "NEWGROUPS %02d%02d%02d %02d%02d%02d",
            tm_struct->tm_year, 1 + tm_struct->tm_mon,
            tm_struct->tm_mday, tm_struct->tm_hour,
            tm_struct->tm_min, tm_struct->tm_sec);
#else
   tm_struct = gmtime (&tloc);
   tm_struct->tm_year = rfc977_patchup_year (1900 + tm_struct->tm_year);
   fprintf (fp_time, "NEWGROUPS %02d%02d%02d %02d%02d%02d GMT",
	    tm_struct->tm_year, 1 + tm_struct->tm_mon,
	    tm_struct->tm_mday, tm_struct->tm_hour,
	    tm_struct->tm_min, tm_struct->tm_sec);
#endif

   return slrn_fclose (fp_time);
}
   
/*{{{ Compatibility functions (slrn_error, etc... ) */
void slrn_exit_error (char *fmt, ...) /*{{{*/
{
   va_list ap;

   if (fmt != NULL)
     {
	va_start (ap, fmt);
	va_log_error (fmt, ap);
	va_end (ap);
     }

   close_servers ();
   close_log_files ();
   
   exit (Exit_Code);
}

/*}}}*/


static int Terminate_Slrn_Pull_Requested;
static void slrnpull_sigint_handler (int sig) /*{{{*/
{
   (void) sig;

   Exit_Code = SLRN_EXIT_SIGNALED;
   SLKeyBoard_Quit = 1;
   Terminate_Slrn_Pull_Requested = 1;
}

/*}}}*/

static void init_signals (void) /*{{{*/
{
   SLsignal_intr (SIGINT, slrnpull_sigint_handler);
#ifdef SIGHUP
   SLsignal_intr (SIGHUP, slrnpull_sigint_handler);
#endif
   SLsignal_intr (SIGTERM, slrnpull_sigint_handler);
#ifdef SIGPIPE
   SLsignal_intr (SIGPIPE, SIG_IGN);
#endif
}

/*}}}*/

static int handle_interrupts (void) /*{{{*/
{
   if (Terminate_Slrn_Pull_Requested)
     {
	log_error (_("Performing shutdown."));
	write_active ();
	Exit_Code = SLRN_EXIT_SIGNALED;
	slrn_exit_error (_("Slrnpull exiting on signal."));
     }
   
   return 0;
}

/*}}}*/


void slrn_error_now (unsigned int unused, char *fmt, ...) /*{{{*/
{
   va_list ap;
   (void) unused;
   va_start(ap, fmt);
   va_log_error (fmt, ap);
   va_end (ap);
}

/*}}}*/

void slrn_error (char *fmt, ...) /*{{{*/
{
   va_list ap;
   
   va_start(ap, fmt);
   va_log_error (fmt, ap);
   va_end (ap);
}

/*}}}*/

int slrn_message_now (char *fmt, ...) /*{{{*/
{
   va_list ap;
   va_start(ap, fmt);
   va_log_message (fmt, ap);
   va_end (ap);
   
   return 0;
}

/*}}}*/

int slrn_message (char *fmt, ...) /*{{{*/
{
   va_list ap;
   va_start(ap, fmt);
   va_log_message (fmt, ap);
   va_end (ap);
   
   return 0;
}

/*}}}*/


/*}}}*/

static char *read_header_from_file (char *file, int *has_body)
{
   FILE *fp;
   char line [NNTP_BUFFER_SIZE];
   char *mbuf;
   unsigned int buffer_len, buffer_len_max;
   
   if (NULL == (fp = fopen (file, "r")))
     return NULL;

   mbuf = NULL;
   buffer_len_max = buffer_len = 0;
   
   while (NULL != fgets (line, sizeof(line), fp))
     {
	unsigned int len;
	
	if (*line == '\n')
	  break;

	len = strlen (line);
	
	if (len + buffer_len + 4 > buffer_len_max)
	  {
	     char *new_mbuf;
	     
	     buffer_len_max += 4096 + len;
	     new_mbuf = slrn_realloc (mbuf, buffer_len_max, 0);
	     
	     if (new_mbuf == NULL)
	       {
		  slrn_free (mbuf);
		  mbuf = NULL;
		  break;
	       }
	     mbuf = new_mbuf;
	  }
   
	strcpy (mbuf + buffer_len, line); /* safe */
	buffer_len += len;
     }
   
   *has_body = (line != NULL) && (*line == '\n');
   
   fclose (fp);
   return mbuf;
}

static int sort_int_cmp (unsigned int *a, unsigned int *b)
{
   if (*a > *b) return 1;
   if (*a == *b) return 0;
   return -1;
}

/*
 * write an overview-file entry for a single article
 * returns -1 on error, 0 if header is missing, 1 otherwise
 */
static int write_overview_entry(FILE *xov_fp, int id, char *dir)
{
   char file [SLRN_MAX_PATH_LEN + 1];
   char *header;
   struct stat st;
   Slrn_XOver_Type xov;
   int has_body;
   char buf[32];
   
   sprintf (buf, "%d", id); /* safe */
   
   if (-1 == slrn_dircat (dir, buf, file, sizeof (file)))
     {
        log_error(_("Error creating filename in %s."), dir);
        return -1;
     } 
   
   if (-1 == stat (file, &st))
     {
        log_error (_("Unable to stat %s."), file);
        return -1;
     }
   
   if (0 == S_ISREG(st.st_mode))
     return 1;
   
   header = read_header_from_file (file, &has_body);
   if (header == NULL)
     {
        log_error(_("Unable to read header %d in %s."), id, file);
        return -1;
     }
   
   if (-1 == xover_parse_head (id, header, &xov))
     {
        log_error(_("Header %d not parseable for overview."), id);
        slrn_free (header);
        return -1;
     }
   
   if (-1 == write_xover_line (xov_fp, &xov))
     {
        slrn_free (header);
        slrn_free (xov.subject_malloced);
        slrn_free (xov.date_malloced);
        return -1;
     }
   slrn_free (header);
   slrn_free (xov.subject_malloced);
   slrn_free (xov.date_malloced);
   return has_body;
}

/* 
 * this functions shows the progress during the updating of the overview file
 */
static void progress_update_overview(int i, int n_nums, Active_Group_Type *g, int creation)
{
   fprintf (stdout, _("%s overview: article %u of %u in %s."),
	    (creation ? _("Creating") : _("Updating")), i+1, n_nums, g->name);
   fputc ('\r', stdout);
   if (i+1 == n_nums)
     fputc ('\n', stdout);
   fflush (stdout);
}

/* Now also updates g->headers */
static int create_overview_for_dir (Active_Group_Type *g, unsigned int *nums, unsigned int n_nums)
{
   char dir [SLRN_MAX_PATH_LEN + 1];
   FILE *xov_fp;
   unsigned int i;
   int bmin, bmax;
   Slrn_Range_Type *headers = NULL;
   void (*qsort_fun) (char *, unsigned int, int, int (*)(unsigned int *, unsigned int *));

   log_message (_("Creating Overview file for %s..."), g->name);
   
   xov_fp = open_xover_file (g, "w");
   
   if (xov_fp == NULL)
     return -1;
   
   if (-1 == slrn_dircat (SlrnPull_Spool_News_Dir, g->dirname,
			  dir, sizeof (dir)))
     return -1;
   
   if ((nums != NULL) && (n_nums != 0))
     {
	qsort_fun = (void (*)(char *, unsigned int, int, 
			      int (*)(unsigned int *, unsigned int *))) 
	  qsort;
	
	(*qsort_fun) ((char *) nums, n_nums, sizeof (unsigned int), sort_int_cmp);
	
	g->active_min = g->min = nums[0];
	g->active_max = nums [n_nums - 1];
     }
   else 
     {
	g->active_min = g->min = g->max + 1;
	g->active_max = g->active_min - 1;
     }
   
   bmin = bmax = -1;
   for (i = 0; i < n_nums; i++)
     {
	int ret;
        if (-1 == (ret = write_overview_entry(xov_fp, (int) nums [i], dir)))
          {
             fclose(xov_fp);
	     slrn_ranges_free (headers);
             return -1;
          }
	if ((ret == 0) && (bmin == -1))
	  {
	     bmin = bmax = nums[i];
	  }
	else if ((bmin != -1) && (ret || ((int)nums[i] != bmax+1)))
	  {
	     headers = slrn_ranges_add (headers, bmin, bmax);
	     if (ret)
	       bmin = bmax = -1;
	     else
	       bmin = bmax = nums[i];
	  }
	else
	  bmax++;
	       
	if ((Stdout_Is_TTY) && (((i % 100) == 0) || (i+1 == n_nums)))
	  progress_update_overview(i, n_nums, g, 1);
     }
   if (bmin != -1)
     headers = slrn_ranges_add (headers, bmin, bmax);

   slrn_ranges_free (g->headers);
   g->headers = headers;
   return slrn_fclose (xov_fp);
}
	
/*
 * this function tries to update the current overview file for a group,
 * which is a lot faster than recreating the entire file. If the overview
 * file cannot be opened for reading, a new one is created.
 */
static int update_overview_for_dir (Active_Group_Type *g, unsigned int *nums, unsigned int n_nums)
{
   char file [SLRN_MAX_PATH_LEN + 1];
   char newfile [SLRN_MAX_PATH_LEN + 1];
   char dir [SLRN_MAX_PATH_LEN + 1];
   char buf [4096];
   unsigned int xov_nr = 0;
   FILE *xov_fp, *new_xov_fp;
   unsigned int i;
   void (*qsort_fun) (char *, unsigned int, int, int (*)(unsigned int *, unsigned int *));
   
   xov_fp = open_xover_file (g, "r");
   if (xov_fp == NULL)
     return create_overview_for_dir (g, nums, n_nums);
   
   new_xov_fp = NULL;
   if ((-1 == slrn_dircat (SlrnPull_Spool_News_Dir, g->dirname, file, sizeof (file))) ||
       (-1 == slrn_dircat (file, Overview_File, file, sizeof (file))))
     {
        log_error (_("Unable to create filename for overview file.\n"));
        (void) slrn_fclose (xov_fp);
        return -1;
     }
   slrn_strncpy (newfile, file, SLRN_MAX_PATH_LEN - 5);
   strcat (newfile, "-new"); /* safe */
   
   new_xov_fp = fopen (newfile, "w");
   if (new_xov_fp == NULL)
     {
	log_error (_("Unable to open new overview file %s.\n"), newfile);
        (void) slrn_fclose (xov_fp);
        return -1;
     }
   
   if ((nums != NULL) && (n_nums != 0))
     {
	qsort_fun = (void (*)(char *, unsigned int, int, 
			      int (*)(unsigned int *, unsigned int *))) 
	  qsort;
	
	(*qsort_fun) ((char *) nums, n_nums, sizeof (unsigned int), sort_int_cmp);
	
	g->active_min = g->min = nums[0];
	g->active_max = nums [n_nums - 1];
     }
   else 
     {
	g->active_min = g->min = g->max + 1;
	g->active_max = g->active_min - 1;
     }
   if (-1 == slrn_dircat (SlrnPull_Spool_News_Dir, g->dirname,
			  dir, sizeof (dir)))
     return -1;
   
   /*
    * read one line from overview, and one item in nums[]
    * they should match. If not, drop the overview info,
    * or create a new one if necessary
    */
   i = 0;
   while (i < n_nums)
     {
	while (xov_nr < nums[i]) /* drop old overview entries */
	  {
	     if (NULL != fgets (buf, sizeof(buf), xov_fp))
	       xov_nr = atoi (buf);
	     else
	       /* end of overview file found, write remaining entries */
	       {
		  while (i < n_nums)
		    {
		       if (-1 == write_overview_entry (new_xov_fp, (int) nums [i], dir))
			 break;
		       
		       if ((Stdout_Is_TTY) && (((i % 100) == 0) || (i+1 == n_nums)))
			 progress_update_overview(i, n_nums, g, 0);
		       i++;
		    }
		  goto end_of_loop; /* break 2 */
	       }
	  }
	
	if (nums [i] == xov_nr)
	  {
	     /* we are in sync */
	     if (EOF == fputs (buf, new_xov_fp))
	       break;
	     if ((Stdout_Is_TTY) && (((i % 100) == 0) || (i+1 == n_nums)))
	       progress_update_overview(i, n_nums, g, 0);
	     i++;
	     continue;
	  }
	
	/* In case entries are missing, insert as many as needed */
	while (nums [i] < xov_nr)
	  {
	     if (-1 == write_overview_entry (new_xov_fp, (int) nums [i], dir))
	       goto end_of_loop; /* break 2 */
	     if ((Stdout_Is_TTY) && (((i % 100) == 0) || (i+1 == n_nums)))
	       progress_update_overview(i, n_nums, g, 0);
	     i++;
	     continue;
	  }
     }

   end_of_loop:
   if (-1 == slrn_fclose (xov_fp))
     log_error (_("failed to close overview file %s.\n"), file);
   if (-1 == slrn_fclose (new_xov_fp))
     log_error (_("failed to close new overview file %s.\n"), newfile);
   
   if (i < n_nums) /* an error occurred within the loop */
     {
	(void) slrn_delete_file (newfile);
	return -1;
     }
   
   if (-1 == slrn_move_file (newfile, file))
     {
	log_error (_("failed to rename %s to %s."), file, newfile);
	return -1;
     }
   
   return 0;
}

static int expire_group (Active_Group_Type *g, int rebuild) /*{{{*/
{
   Slrn_Dir_Type *dp;
   Slrn_Dirent_Type *df;
   char dir [SLRN_MAX_PATH_LEN + 1];
   char file [SLRN_MAX_PATH_LEN + 1];
   unsigned int *ok_names;
   unsigned int num_ok_names, max_num_ok_names;
   unsigned int num_expired = 0;
   unsigned int i, n;
   unsigned int left, right, cutoff;
   void (*qsort_fun) (char *, unsigned int, int, int (*)(unsigned int *, unsigned int *));
   time_t expire_time;
   int perform_expire = 1;
   
   if (g->expire_days == 0)
     perform_expire = 0;

   /* First, make a list of all articles in this group. */
   if (-1 == slrn_dircat (SlrnPull_Spool_News_Dir, g->dirname,
			  dir, sizeof (dir)))
     return -1;

   dp = slrn_open_dir (dir);
   if (dp == NULL)
     {
	log_error (_("opendir %s failed."), dir);
	return -1;
     }
   
   ok_names = NULL;
   max_num_ok_names = num_ok_names = 0;
   
   while (NULL != (df = slrn_read_dir (dp)))
     {
	char *name, *p;
	
	name = df->name;
	
	/* Look for names composed of digits.  Skip others. */
	p = name;
	while (*p && isdigit (*p)) p++;
	if (*p != 0) continue;
	
	if (1 != sscanf (name, "%u", &n))
	  continue;		       /* hmm... I'm paranoid. */

	if (num_ok_names == max_num_ok_names)
	  {
	     max_num_ok_names += 500;
	     ok_names = (unsigned int *) slrn_realloc ((char *) ok_names, max_num_ok_names * sizeof (unsigned int), 1);
	     if (ok_names == NULL)
	       {
		  log_error (_("malloc error. Unable to expire group %s."), g->name);
		  slrn_close_dir (dp);
		  return -1;
	       }
	  }
	
	ok_names [num_ok_names] = n;
	num_ok_names++;
     }
   
   slrn_close_dir (dp);

   if (num_ok_names==0) /* nothing to do */
     return 0;
   
   /* Now, sort the array and use a binary search to find the lowest article
    * number we don't want to expire.
    */
   qsort_fun = (void (*)(char *, unsigned int, int, 
			 int (*)(unsigned int *, unsigned int *))) qsort;
   
   (*qsort_fun) ((char *) ok_names, num_ok_names, sizeof (unsigned int), sort_int_cmp);

   time (&expire_time);
   expire_time -= g->expire_days * (24 * 60 * 60);   
   
   if (perform_expire == 0)
     cutoff = 0;
   else
     {
	left = 0;
	right = num_ok_names;
	/* When we finish, left should be the array index of the first
	 * article we want to keep. */
	while (left < right)
	  {
	     char buf[32];
	     struct stat st;
	     
	     cutoff = (left + right) / 2;
	     
	     sprintf (buf, "%d", ok_names[cutoff]); /* safe */
	     
	     if (-1 == slrn_dircat (dir, buf, file, sizeof (file)))
	       goto stat_error;
	     
	     if (-1 == stat (file, &st))
	       {
		  log_error (_("Unable to stat %s."), file);
		  goto stat_error;
	       }
	     
	     if (0 == S_ISREG(st.st_mode))
	       goto stat_error;
	     
	     if (st.st_mtime > expire_time)
	       right = cutoff;
	     else
	       left = cutoff+1;
	     
	     continue;
	     
	     stat_error:
	     /* throw out the faulty entry */
	     if (cutoff < num_ok_names-1)
	       memmove (ok_names+cutoff, ok_names+cutoff+1, num_ok_names-cutoff-1);
	     if (right==num_ok_names)
	       right--;
	     num_ok_names--;
	  }
	cutoff = left;
     }
   
   /* Expire bodyless articles which are no longer on server. */
   if (cutoff < num_ok_names)
     {
	Slrn_Range_Type *r;
	unsigned int server_min = read_server_min_file (g);

	i = ok_names[cutoff];
	if (i>1)
	  g->headers = slrn_ranges_remove (g->headers, 1, i-1);
	r = g->headers;
	
	if (r!=NULL)
	  {
	     if ((int)i < r->min)
	       i = r->min;
	     
	     n = cutoff;
	     while (i < server_min)
	       {
		  while ((n<num_ok_names) && (ok_names[n]<i))
		    n++;
		  
		  if ((n<num_ok_names) && (ok_names[n]==i))
		    {
		       ok_names[n] = ok_names[cutoff];
		       ok_names[cutoff] = i;
		       cutoff++;
		    }
		  
		  i++;
		  if ((int)i>r->max)
		    {
		       r = r->next;
		       if (r != NULL)
			 i = r->min;
		       else
			 break;
		    }
	       }
	  }
	
	if (server_min>1)
	  g->headers = slrn_ranges_remove (g->headers, 1, server_min-1);
     }
   
   /* Now, actually delete the files on disk. */
   for (i=0; i < cutoff; i++)
     {
	char buf[32];
	sprintf (buf, "%d", ok_names[i]); /* safe */
	
	if ((-1 == slrn_dircat (dir, buf, file, sizeof (file))) ||
	    (-1 == slrn_delete_file (file)))
	  log_error (_("Unable to expire %s."), file);
	else
	  num_expired++;
     }
   
   if (num_expired > 0)
     {
        log_message (_("%u articles expired in %s."), num_expired, g->name);
        if (rebuild == 0)
          (void) update_overview_for_dir (g, ok_names+cutoff, num_ok_names-cutoff);
        else
          (void) create_overview_for_dir (g, ok_names+cutoff, num_ok_names-cutoff);
     }
   else if (rebuild == 1)
     (void) create_overview_for_dir (g, ok_names+cutoff, num_ok_names-cutoff);
   
   slrn_free ((char *) ok_names);
   
   return 0;
}

/*}}}*/

static int do_expire (int rebuild) /*{{{*/
{
   Active_Group_Type *g;
   
   g = Active_Groups;
   
   init_signals ();

   while (g != NULL)
     {
	if (Terminate_Slrn_Pull_Requested)
	  {
	     log_error (_("Termination requested.  Shutting down."));
	     (void) write_active ();
	     Exit_Code = SLRN_EXIT_SIGNALED;
	     slrn_exit_error (NULL);
	  }
	
	(void) expire_group (g, rebuild);
	g = g->next;
     }
   
   return write_active ();
}

/*}}}*/


static char *Auth_User_Name;
static char *Auth_Password;

static int get_authorization (char *host, char **name, char **pass)
{
   (void) host;

   *name = Auth_User_Name;
   *pass = Auth_Password;

   return 0;
}

static int read_authinfo (void)
{
   FILE *fp;
   char file [SLRN_MAX_PATH_LEN + 1];
   static char pass[256];
   static char name[256];
   
   Auth_Password = NULL;
   Auth_User_Name = NULL;
   
   if (-1 == slrn_dircat (SlrnPull_Dir, "authinfo", file, sizeof (file)))
     return -1;
   
   if (NULL == (fp = fopen (file, "r")))
     return 0;
   
   if ((NULL == fgets (name, sizeof (name), fp))
       || (NULL == fgets (pass, sizeof (pass), fp)))
     {
	log_error (_("Error reading name and password from %s."), file);
	fclose (fp);
	
	return -1;
     }
   
   fclose (fp);

   slrn_trim_string (pass);
   slrn_trim_string (name);
   
   if (0 == strlen (name))
     {
	log_error (_("Empty user name.  Please check syntax of the authinfo file."));
	return -1;
     }
   
   Auth_User_Name = name;
   Auth_Password = pass;

   NNTP_Authorization_Hook = get_authorization;
   Slrn_Force_Authentication = 1;
   
   return 0;
}

static int read_headers_files (void)
{
   VFILE *vp;
   char *vline;
   unsigned int vlen;
   Active_Group_Type *group = Active_Groups;
   char head_file [SLRN_MAX_PATH_LEN + 1];

   while (group != NULL)
     {
	if ((-1 == slrn_dircat (SlrnPull_Spool_News_Dir, group->dirname,
				head_file, sizeof (head_file)))
	    || (-1 == slrn_dircat (head_file, Headers_File,
				   head_file, sizeof (head_file))))
	  {
	     log_error (_("Unable to read headers file for group %s.\n"), group->name);
	     return -1;
	  }
	
	if (0 == slrn_file_exists (head_file))
	  {
	     group = group->next;
	     continue;
	  }
	     
	if (NULL == (vp = vopen (head_file, 4096, 0)))
	  {
	     log_error (_("File %s exists, but could not be read."), head_file);
	     return -1;
	  }
	
	if (NULL != (vline = vgets (vp, &vlen)))
	  {
	     vline[vlen] = 0; /* make sure line is NULL terminated */
	     group->headers = slrn_ranges_from_newsrc_line (vline);
	  }
	
	vclose (vp);
	group = group->next;
     }
   return 0;
}

static int write_headers_files (void)
{
   char backup_file[SLRN_MAX_PATH_LEN + 1];
   char head_file[SLRN_MAX_PATH_LEN + 1];
   FILE *fp;
   int have_backup = 0;
   Active_Group_Type *group = Active_Groups;
   int error = 0;

   while (group != NULL)
     {
	if ((-1 == slrn_dircat (SlrnPull_Spool_News_Dir, group->dirname,
				head_file, sizeof (head_file)))
	    || (-1 == slrn_dircat (head_file, Headers_File,
				   head_file, sizeof (head_file))))
	  {
	     log_error (_("Unable to write headers file for group %s.\n"), group->name);
	     error = -1;
	     goto next_group;
	  }
	   
#ifdef VMS
	slrn_snprintf (backup_file, sizeof (backup_file), "%s-bak",
		       head_file);
#else
# ifdef SLRN_USE_OS2_FAT
	slrn_os2_make_fat (backup_file, sizeof (backup_file), head_file,
			   ".bak");
# else
	slrn_snprintf (backup_file, sizeof (backup_file), "%s~", head_file);
# endif
#endif

	/* Save backup file in case anything goes wrong */
	have_backup = 1;
	if (-1 == rename (head_file, backup_file))
	  have_backup = 0;

	if (group->headers != NULL)
	  {
	     if (NULL == (fp = fopen (head_file, "w")))
	       {
		  if (have_backup) (void) rename (backup_file, head_file);
		  log_error (_("Unable to write headers file for group %s.\n"), group->name);
		  error = -1;
		  goto next_group;
	       }
	     if ((-1 == slrn_ranges_to_newsrc_file (group->headers,0,fp)) |
		 (-1 == slrn_fclose (fp)))
	       {
		  if (have_backup) (void) rename (backup_file, head_file);
		  log_error (_("Error while writing headers file for group %s.\n"), group->name);
		  error = -1;
	       }
	  }
	  
	if (have_backup) slrn_delete_file (backup_file);
	
	next_group:
	group = group->next;
     }

   return error;
}

static void read_requests_files (void)
{
   Slrn_Dir_Type *dp;
   Slrn_Dirent_Type *df;
   Active_Group_Type *group;
   char file [SLRN_MAX_PATH_LEN + 1];
   
   dp = slrn_open_dir (Requests_Dir);
   if (dp == NULL) return;
   
   /* Read all files in the requests dir and merge them. */
   while (NULL != (df = slrn_read_dir (dp)))
     {
	VFILE *vp;
	char *vline;
	unsigned int vlen;
	char *name;
	
	name = df->name;
	
	/* skip backup copies */
#ifdef VMS
	if ((strlen(name)>4) &&
	    (!strcmp("-bak", name+strlen(name)-4)))
#else
# ifdef SLRN_USE_OS2_FAT
	if ((strlen(name)>4) &&
	    (!strcmp(".bak", name+strlen(name)-4)))
# else
	if (name[strlen(name)-1]=='~')
# endif
#endif
	  continue;
	
	if (-1 == slrn_dircat (Requests_Dir, name, file, sizeof (file)))
	  break;
	
	if ((1 != slrn_file_exists (file)) ||
	    (NULL == (vp = vopen (file, 4096, 0))))
	  continue;
	
	while (NULL != (vline = vgets (vp, &vlen)))
	  {
	     Slrn_Range_Type *r;
	     
	     char *p = vline;
	     char *pmax = p + vlen;
	     
	     while ((p < pmax) && (*p != ':'))
	       p++;
	     
	     if ((p == pmax) || (p == vline))
	       continue;
	     
	     *p = 0;
	     
	     if (NULL == (group = find_group_type (vline)))
	       continue;
	     
	     vline[vlen-1] = 0;	       /* kill \n and NULL terminate */
	     
	     r = slrn_ranges_from_newsrc_line (p+1);
	     group->requests = slrn_ranges_merge (group->requests, r);
	     slrn_ranges_free (r);
	  }
	vclose (vp);
     }
   
   slrn_close_dir (dp);
   
   /* Now, make sure we don't request bodies that are already there. */
   group = Active_Groups;

   while (group != NULL)
     {
	Slrn_Range_Type *r = group->requests;
	group->requests = slrn_ranges_intersect (r, group->headers);
	slrn_ranges_free (r);
	group = group->next;
     }
}
