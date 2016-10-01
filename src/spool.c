/* -*- mode: C; mode: fold; -*- */
/* Local spool support for slrn added by Olly Betts <olly@mantis.co.uk> */
/* Modified by Thomas Schultz:
 * Copyright (c) 2001-2006 Thomas Schultz <tststs@gmx.de>
 */
#include "config.h"
#include "slrnfeat.h"

/* define this if you want to trace command-execution in spool.c.
 * Normally, you only get errors in the debug-file. If you set this,
 * please keep an eye on your disk-space.
 */
#define DEBUG_SPOOL_TRACE 1

#include <stdio.h>
#ifndef SEEK_SET
# define SEEK_SET	0
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <string.h>
#include <errno.h>
#include <ctype.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include <time.h>

#include "misc.h"
#include "util.h"
#include "slrn.h"
#include "slrndir.h"
#include "snprintf.h"
#include "vfile.h"
#include "strutil.h"

#ifndef SLRN_SPOOL_ROOT
# define SLRN_SPOOL_ROOT "/var/spool/news" /* a common place for the newsspool */
#endif

#ifndef SLRN_SPOOL_NOV_ROOT
# define SLRN_SPOOL_NOV_ROOT SLRN_SPOOL_ROOT
#endif

#ifndef SLRN_SPOOL_NOV_FILE
# define SLRN_SPOOL_NOV_FILE ".overview"
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <stdarg.h>

#include <limits.h>

extern int Slrn_Prefer_Head;
char *Slrn_Overviewfmt_File;
char *Slrn_Requests_File;

static int spool_put_server_cmd (char *, char *, unsigned int );
static int spool_select_group (char *, NNTP_Artnum_Type *, NNTP_Artnum_Type *);
static int spool_refresh_groups (Slrn_Group_Range_Type *, int);
static void spool_close_server (void);
static int spool_has_cmd (char *);
static int spool_initialize_server (void);
static int spool_read_line (char *, unsigned int );
static int spool_xpat_cmd (char *, NNTP_Artnum_Type, NNTP_Artnum_Type, char *);
static int spool_select_article (NNTP_Artnum_Type, char *);
static int spool_get_article_size (NNTP_Artnum_Type);
static int spool_one_xhdr_command (char *, NNTP_Artnum_Type, char *, unsigned int);
static int spool_xhdr_command (char *, NNTP_Artnum_Type, NNTP_Artnum_Type);
static int spool_list (char *);
static int spool_list_newsgroups (void);
static int spool_list_active (char *);
static int spool_send_authinfo (void);
static int spool_read_xover (char *, unsigned int);
static int spool_read_xpat (char *, unsigned int);
static int spool_read_xhdr (char *, unsigned int);
static int spool_article_num_exists (NNTP_Artnum_Type);
static unsigned int spool_get_bytes (int clear);

static Slrn_Server_Obj_Type Spool_Server_Obj;

/* some state that the NNTP server would take care of if we were using one */
static FILE *Spool_fh_local=NULL;
static int Spool_Ignore_Comments=0;	/* ignore comments while reading */
static SLRegexp_Type *Spool_Output_Regexp=NULL;
static char *Spool_Group=NULL;
static char *Spool_Group_Name;

static FILE *Spool_fh_nov=NULL; /* we use the overview file lots, so keep it open */
static NNTP_Artnum_Type Spool_cur_artnum = 0;

/* These are set when the group is selected. */
static NNTP_Artnum_Type Spool_Max_Artnum = 0;
static NNTP_Artnum_Type Spool_Min_Artnum = 0;

static int Spool_Doing_XOver;	       /* if non-zero, reading from ,overview */
static int Spool_Doing_XPat;	       /* reading xpat */
static char *Spool_XHdr_Field;	       /* when reading xhdr */
static int Spool_fhead=0; /* if non-0 we're emulating "HEAD" so stop on blank line */
static int Spool_fFakingActive=0; /* if non-0 we're doing funky stuff with MH folders */

static int spool_fake_active( char *);
static int spool_fakeactive_read_line(char *, int);
static int Spool_fakeactive_newsgroups=0;

static unsigned int bytes_read=0;

/* Note: Please leave debugging output untranslated.
 *       It cannot be interpreted by the end user anyway. */
static void debug_output (char *file, int line, char *fmt, ...) ATTRIBUTE_PRINTF(3,4);

static void debug_output (char *file, int line, char *fmt, ...)
{
   va_list ap;
   if (Slrn_Debug_Fp != NULL)
     {
        if ((file != NULL) && (line != -1))
          fprintf (Slrn_Debug_Fp, "%s:%d: ", file, line);
        else
#ifdef DEBUG_SPOOL_TRACE
          fprintf (Slrn_Debug_Fp, "%lu: ", (unsigned long) clock ());
#else
	  return;
#endif

        if (fmt == NULL)
          {
             fputs ("(NULL)", Slrn_Debug_Fp);
          }
        else
          {
	     va_start(ap, fmt);
	     vfprintf(Slrn_Debug_Fp, fmt, ap);
	     va_end (ap);
          }

        putc ('\n', Slrn_Debug_Fp);
     }
}
#ifndef __FILE__
# define __FILE__ "spool.c"
#endif
#ifndef __LINE__
# define __LINE__ 0
#endif

/* close any current file (unless it's the overview file) and NULL the FILE* */
static int spool_fclose_local (void)
{
   int res = 0;

   Spool_fhead=0;
   Spool_fFakingActive=0;
#if SLANG_VERSION >= 20000
   if (Spool_Output_Regexp != NULL)
     SLregexp_free (Spool_Output_Regexp);
#endif
   Spool_Output_Regexp = NULL;
   Spool_Ignore_Comments=0;

   if (Spool_fh_local != NULL)
     {
	if (Spool_fh_local != Spool_fh_nov)
	  res = fclose(Spool_fh_local);
	Spool_fh_local=NULL;
     }
   return res;
}

static FILE *spool_open_nov_file (void)
{
   char *p, *q;
   FILE *fp;

   /* The spool_dircat function will exit if it fails to malloc. */
   p = slrn_spool_dircat (Slrn_Nov_Root, Spool_Group_Name, 1);
   q = slrn_spool_dircat (p, Slrn_Nov_File, 0);

   fp = fopen (q,"rb");
   SLFREE(q);
   SLFREE(p);

   return fp;
}

static int overview_file_seek (long fp, NNTP_Artnum_Type cur, NNTP_Artnum_Type dest)
{
   int ch;

   while (cur < dest)
     {
        if (-1 == (fp = ftell(Spool_fh_local)))
          {
             debug_output (__FILE__, __LINE__, "ftell returned -1; errno %d (%s).", errno, strerror(errno));
             return -1;
          }
	if (1 != fscanf (Spool_fh_local, NNTP_FMT_ARTNUM, &cur))
	  {
	     spool_fclose_local ();
	     return -1;
	  }

	while (((ch = getc (Spool_fh_local)) != '\n') && (ch != EOF))
	  ; /* do nothing */
     }
   if (-1 == fseek(Spool_fh_local, fp, SEEK_SET)) /* reset to start of line */
     {
        debug_output (__FILE__, __LINE__, "fseek returned -1; errno %d (%s).", errno, strerror(errno));
        return ERR_FAULT;
     }

   return 0;
}

static NNTP_Artnum_Type Spool_XOver_Next;
static NNTP_Artnum_Type Spool_XOver_Max;
static NNTP_Artnum_Type Spool_XOver_Min;

static int spool_nntp_xover (NNTP_Artnum_Type min, NNTP_Artnum_Type max)
{
   NNTP_Artnum_Type i;
   long fp;

   Spool_Doing_XOver = 0;

   spool_fclose_local ();

   if (max > Spool_Max_Artnum)
     max = Spool_Max_Artnum;

   if (min < Spool_Min_Artnum)
     min = Spool_Min_Artnum;

   if (Spool_Server_Obj.sv_has_xover)
     {
	Spool_fh_local = Spool_fh_nov;
	if (Spool_fh_local == NULL)
	  return -1;

	/* find first record in range in overview file */
	/* first look at the current position and see where we are */
	/* this is worth trying as slrn will often read a series of ranges */
	if (-1 == (fp = ftell (Spool_fh_local)))
          {
             debug_output (__FILE__, __LINE__, "ftell returned -1; errno %d (%s).", errno, strerror(errno));
             return ERR_FAULT;
          }

	if ((1 != fscanf (Spool_fh_local, NNTP_FMT_ARTNUM, &i))
	    || (i > min))
	  {
	     /* looks like we're after the start of the range */
	     /* therefore we'll have to rescan the file from the start */
	     rewind (Spool_fh_local);
	     i = -1;
	     /* this might be improved by doing some binary-chop style searching */
	  }
	else
	  {
	     int ch;

	     while (((ch = getc(Spool_fh_local)) != '\n')
		    && (ch != EOF))
	       ; /* do nothing */

	     if (ch == EOF)
	       {
		  rewind (Spool_fh_local);
		  i = -1;
	       }
	  }

	if (-1 == overview_file_seek (fp, i, min))
	  return -1;
     }

   Spool_XOver_Next = Spool_XOver_Min = min;
   Spool_XOver_Max = max;
   Spool_Doing_XOver = 1;

   return OK_XOVER;
}

static int spool_read_xover (char *the_buf, unsigned int len)
{
   long pos;

   if (Spool_Doing_XOver == 0)
     return 0;

   if (Spool_XOver_Next > Spool_XOver_Max)
     {
	Spool_Doing_XOver = 0;
	return 0;
     }

   while (1)
     {
	unsigned int buflen;

	if (-1 == (pos = ftell (Spool_fh_nov)))
          {
             debug_output (__FILE__, __LINE__, "ftell returned -1; errno %d (%s).", errno, strerror(errno));
             return -1;
          }

	if (NULL == fgets (the_buf, len, Spool_fh_nov))
	  {
	     Spool_Doing_XOver = 0;
	     return 0;
	  }

	buflen = strlen (the_buf);
	if (buflen && (the_buf[buflen - 1] == '\n'))
	  the_buf [buflen - 1] = 0;

	/* check if we've reached the end of the requested range */
	Spool_XOver_Next = NNTP_STR_TO_ARTNUM (the_buf);
	if (Spool_XOver_Next > Spool_XOver_Max)
	  {
             if (-1 == fseek(Spool_fh_nov, pos, SEEK_SET))
               {
                  debug_output (__FILE__, __LINE__, "fseek returned -1; errno %d (%s).", errno, strerror(errno));
                  return -1;
               }
	     Spool_Doing_XOver = 0;
	     return 0;
	  }

#if 0
	/* We now do this check in xover.c and get the article size (in bytes)
	 * using the same call to stat(). */
	if (Slrn_Spool_Check_Up_On_Nov == 0)
	  break;

	/* check that the article file actually exists */
	/* if not, this nov entry is defunct, so ignore it */
	if (0 == spool_article_num_exists (Spool_XOver_Next))
	  {
	     break;
	  }
	debug_output (NULL, -1, "Nov entry %ld skipped!", Spool_XOver_Next);
#else
	break;
#endif
     }

   Spool_XOver_Next++;
   return 1;
}

static int spool_find_artnum_from_msgid (char *msgid, NNTP_Artnum_Type *idp)
{
   char buf [4096];
   char *p;

   debug_output (NULL, -1, "spool_find_artnum_from_msgid('%s')", msgid);

   if (Slrn_Server_Obj->sv_has_xover == 0)
     {
	NNTP_Artnum_Type n;
	unsigned int len = strlen (msgid);

	for (n = Spool_Min_Artnum; n <= Spool_Max_Artnum; n++)
	  {
	     if (-1 == spool_one_xhdr_command ("Message-ID", n, buf, sizeof (buf)))
	       continue;

	     p = slrn_skip_whitespace (buf);
	     if (0 == strncmp (p, msgid, len))
	       {
		  *idp = n;
		  return 0;
	       }
	  }

	return -1;
     }

   if (OK_XOVER != spool_nntp_xover (1, NNTP_ARTNUM_TYPE_MAX))
     return -1;

   while (1 == spool_read_xover (buf, sizeof(buf)))
     {
	char *q;

	/* 5th field is message id. */

	if (NULL == (p = slrn_strbyte(buf, '\t'))) continue;
	if (NULL == (p = slrn_strbyte(p + 1, '\t'))) continue;
	if (NULL == (p = slrn_strbyte(p + 1, '\t'))) continue;
	if (NULL == (p = slrn_strbyte(p + 1, '\t'))) continue;

	p++; /* skip tab */
	q = slrn_strbyte(p,'\t');
	if (q != NULL) *q='\0';

	if (0 == strcmp(msgid, p))
	  {
	     debug_output (NULL, -1, ("spool_find_artnum_from_msgid() returns " NNTP_FMT_ARTNUM)
			   ,NNTP_STR_TO_ARTNUM(buf));
	     Spool_Doing_XOver = 0;
	     *idp = NNTP_STR_TO_ARTNUM(buf);
	     return 0;
	  }
     }

   debug_output (NULL, -1, "spool_find_artnum_from_msgid() found no match");

   Spool_Doing_XOver = 0;
   return -1;
}

static FILE *spool_open_article_num (NNTP_Artnum_Type num)
{
   char buf [SLRN_MAX_PATH_LEN];

   slrn_snprintf (buf, sizeof (buf), ("%s/" NNTP_FMT_ARTNUM), Spool_Group, num);

   return fopen (buf,"r");
}

static int spool_article_num_exists (NNTP_Artnum_Type num)
{
   char buf [SLRN_MAX_PATH_LEN];

   slrn_snprintf (buf, sizeof (buf), ("%s/" NNTP_FMT_ARTNUM), Spool_Group, num);

   if (1 == slrn_file_exists (buf))
     return 0;

   return -1;
}

static int spool_get_article_size (NNTP_Artnum_Type num)
{
   char buf [SLRN_MAX_PATH_LEN];

   slrn_snprintf (buf, sizeof (buf), ("%s/" NNTP_FMT_ARTNUM), Spool_Group, num);

   return slrn_file_size (buf);
}

static int spool_is_name_all_digits (char *p)
{
   char *pmax;

   pmax = p + strlen (p);
   while (p < pmax)
     {
	if (!isdigit (*p))
	  return 0;
	p++;
     }
   return 1;
}

/*{{{ The routines in this fold implement the sv_put_server_cmd */

static int spool_nntp_head (NNTP_Artnum_Type id, char *msgid, NNTP_Artnum_Type *real_id)
{
   spool_fclose_local();

   if (id == -1)
     {
	if (msgid == NULL)
	  id = Spool_cur_artnum;
	else
	  {
	     if (-1 == spool_find_artnum_from_msgid (msgid, &id))
	       id = -1;
	  }
     }

   if (real_id != NULL) *real_id = id;

   if ((id == -1)
       || (NULL == (Spool_fh_local = spool_open_article_num (id))))
     return ERR_NOARTIG; /* No such article in this group */

   Spool_cur_artnum = id;
   Spool_fhead = 1; /* set flag to stop after headers */

   return OK_HEAD; /* Head follows */
}

static int spool_nntp_next (NNTP_Artnum_Type *id)
{
   NNTP_Artnum_Type i;

   spool_fclose_local();

   /* !HACK! better to find value from overview file or active file in case the group grows while we're in it? */
   for (i = Spool_cur_artnum + 1; i <= Spool_Max_Artnum; i++)
     {
	if (-1 != spool_article_num_exists (i))
	  {
	     Spool_cur_artnum = i;
	     if (id != NULL) *id = i;

	     debug_output (NULL, -1, ("NEXT found article " NNTP_FMT_ARTNUM), Spool_cur_artnum);

	     return OK_NOTEXT; /* No text sent -- stat, next, last */
	  }
     }

   debug_output (NULL, -1, ("No NEXT -- " NNTP_FMT_ARTNUM " > " NNTP_FMT_ARTNUM),
		 Spool_cur_artnum, Spool_Max_Artnum);

   return ERR_NONEXT; /* No next article in this group */
}

static int spool_nntp_newgroups (char *line, char *buf, unsigned int len)
{
   /* expect something like "NEWGROUPS 960328 170939 GMT" GMT is optional */
   char *p;
   int Y,M,D,h,m,s;
   int c;
   int fGMT;
   int n;
   time_t threshold;
   long fpos;
   char buf1[512];
   int ch;
   int found;
   int first;

   (void) buf; (void) len;

   /* The %n format specification returns the number of charcters parsed up
    * to that point.  I do not know how portable this is.
    */
   c = sscanf(line+9,"%02d%02d%02d %02d%02d%02d%n",&Y,&M,&D,&h,&m,&s,&n);
   assert(c==6); /* group.c should have sanity checked the line */

   p = slrn_skip_whitespace(line + 9 + n);
   fGMT = (0 == strncmp (p, "GMT", 3));

   /* this !HACK! is good until 2051 -- round at 50 cos the RFC says to */
   Y += ( Y>50 ? 1900 : 2000 );
   /* for full version, use this instead:
    * if (Y<=50) Y+=100;
    * yr = this_year();
    * Y += ((yr-51)/100*100);
    */

   if (Y < 1970
       || M < 1 || M > 12
       || D < 1 || D > 31
       || h < 0 || h > 24
       || m < 0 || m > 59
       || s < 0 || s > 59)
     return ERR_FAULT;

   /* Compute the number of days since beginning of year.
    * Beware: the calculations involving leap years, etc... are naive. */
   D--; M--;
   D += 31 * M;
   if (M > 1)
     {
	D -= (M * 4 + 27)/10;
	if ((Y % 4) == 0) D++;
     }

   /* add that to number of days since beginning of time */
   Y -= 1970;
   D += Y * 365 + Y / 4;
   if ((Y % 4) == 3) D++;

   /* Now convert to secs */
   s += 60L * (m + 60L * (h + 24L * D));
   threshold = (time_t) s;

   if (0 == fGMT)
     {
#ifdef HAVE_TIMEZONE
	struct tm *t;
	(void) localtime (&threshold); /* This call sets timezone */
	threshold += timezone;
	t = localtime (&threshold);
	if (t->tm_isdst) threshold -= (time_t) 3600;
#else
# ifdef HAVE_TM_GMTOFF
	struct tm *t;
	time_t tt;

	t = localtime (&threshold);
	tt = threshold - t->tm_gmtoff;
	if (t->tm_isdst) tt += (time_t) 3600;
	t = localtime (&tt);
	threshold -= t->tm_gmtoff;
# endif /* else: we're out of luck */
#endif /* NOT HAVE_TIMEZONE */
     }

   debug_output (NULL, -1, "threshold for spool_nntp_newsgrups is %lu", (unsigned long) threshold);

   spool_fclose_local();
   Spool_fh_local = fopen (Slrn_ActiveTimes_File, "r");
   if (Spool_fh_local == NULL)
     {
	/* !HACK! at this point, it would be nice to be able to check for
	 * recently created directories with readdir() and to return those
	 * which would be useful for reading MH folders, etc.  This would
	 * be more than a little slow for a true newsspool though.
	 *
	 * Hmm, looked into this and it seems that you can't use the mtime
	 * or ctime to decide when a directory was created.  Hmmm.
	 */
	debug_output (NULL, -1, "Couldn't open active.times");
	return ERR_FAULT; /* Program fault, command not performed */
     }

   /* chunk size to step back through active.times by
    * when checking for new groups */
   /* 128 should be enough to find the last line in the probably most
    * common case of no new groups */
#define SLRN_SPOOL_ACTIVETIMES_STEP 128

   /* find start of a line */
   if (-1 == fseek (Spool_fh_local, 0, SEEK_END ))
     {
        debug_output (NULL, -1, "fseek returned -1; errno %d (%s).", errno, strerror(errno));
        return ERR_FAULT;
     }
   if (-1 == (fpos = ftell (Spool_fh_local)))
     {
        debug_output (NULL, -1, "ftell returned -1; errno %d (%s).", errno, strerror(errno));
        return ERR_FAULT;
     }

   found=0;
   first=1;

   while (!found)
     {
	int i, len1;

	len1 = SLRN_SPOOL_ACTIVETIMES_STEP;

	if (fpos < (long)len1) len1=fpos; /* don't run of the start of the file */
	fpos -= len1;
        if (-1 == fseek (Spool_fh_local, fpos, SEEK_SET ))
          {
             debug_output (__FILE__, __LINE__, "fseek returned -1; errno %d (%s).", errno, strerror(errno));
             return ERR_FAULT;
          }
	if (fpos == 0) break;

	if (first)
	  {
	     /* on the first pass, we want to ignore the last byte \n at eof */
	     --len1;
	     first=0;
	  }

	for (i = 0; i < len1; i++)
	  {
	     ch = getc(Spool_fh_local);

	     assert(ch!=EOF); /* shouldn't happen */
	     if (ch != '\n') continue;

	     while ((i < len1)
		    && (NULL != fgets (buf1, sizeof(buf1), Spool_fh_local)))
	       {
		  i -= strlen(buf1);
		  p = buf1;
		  while (*p && (0 == isspace (*p)))
		    p++;

		  if (atol(p) < threshold)/* or <= ? !HACK! */
		    {
		       found = 1;
		       break;
		    }
	       }
	     break;
	  }
     }

   if (-1 == (fpos = ftell (Spool_fh_local)))
     {
        debug_output (NULL, -1, "ftell returned -1; errno %d (%s).", errno, strerror(errno));
        return ERR_FAULT;
     }

   while (NULL != fgets( buf1, sizeof(buf1), Spool_fh_local ))
     {
	p = buf1;
	while (*p && (0 == isspace (*p))) p++;

	if (atol(p) >= threshold) /* or just > ? !HACK! */
	  {
             if (-1 == fseek (Spool_fh_local, fpos, SEEK_SET ))
               {
                  debug_output (__FILE__, __LINE__, "fseek returned -1; errno %d (%s).", errno, strerror(errno));
                  return ERR_FAULT;
               }
	     break;
	  }
	if (-1 == (fpos = ftell(Spool_fh_local)))
          {
             debug_output (__FILE__, __LINE__, "ftell returned -1; errno %d (%s).", errno, strerror(errno));
             return ERR_FAULT;
          }
     }

   return OK_NEWGROUPS; /* New newsgroups follow */
}

typedef struct
{
   char *name;
   unsigned int len;
   int (*f) (char *, char *, unsigned int);
}
Spool_NNTP_Map_Type;

static Spool_NNTP_Map_Type Spool_NNTP_Maps [] =
{
     {"NEWGROUPS", 9, spool_nntp_newgroups},
     {NULL, 0, NULL}
};

static int spool_put_server_cmd (char *line, char *buf, unsigned int len)
{
   Spool_NNTP_Map_Type *nntpmap;

   debug_output (NULL, -1, "spool_put_server_cmd('%s')", line);

   nntpmap = Spool_NNTP_Maps;
   while (nntpmap->name != NULL)
     {
	if (!slrn_case_strncmp (nntpmap->name,
				 line, nntpmap->len))
	  return (*nntpmap->f)(line, buf, len);

	nntpmap++;
     }

   debug_output (NULL, -1, "Hmmm, didn't know about that command ('%s')", line);
   return ERR_COMMAND;
}

/*}}}*/

static int spool_read_minmax_from_dp (Slrn_Dir_Type *dp, NNTP_Artnum_Type *min, NNTP_Artnum_Type *max)
{
   Slrn_Dirent_Type *ep;
   char *p;
   NNTP_Artnum_Type l;
   NNTP_Artnum_Type hi = 0;
   NNTP_Artnum_Type lo = NNTP_ARTNUM_TYPE_MAX;

   /* Scan through all the files, checking the ones with numbers for names */
   while ((ep = slrn_read_dir(dp)) != NULL)
     {
	p = ep->name;
	if (!isdigit(*p)) continue;

	if (0 == spool_is_name_all_digits (p))
	  continue;

	if (0 == (l = NNTP_STR_TO_ARTNUM(p)))
	  continue;

	if (l < lo)
	  lo = l;
	if (l > hi)
	  hi = l;
     }

   if ((lo == LONG_MAX)
       && (hi == 0))
     return -1;

   *min=lo;
   *max=hi;

   return 0;
}

static int spool_read_minmax_file (NNTP_Artnum_Type *min, NNTP_Artnum_Type *max, char *group_dir)
{
   char *file;
   FILE *fp;
   char buf[512];
   int status;

   file = slrn_spool_dircat (group_dir, ".minmax", 0);
   if (file == NULL)
     return -1;

   fp = fopen (file, "r");
   if (fp == NULL)
     {
	SLFREE (file);
	return -1;
     }

   status = 0;
   if ((NULL == fgets (buf, sizeof (buf), fp))
       || (2 != sscanf (buf, NNTP_FMT_ARTNUM_2, min, max)))
     status = -1;

   fclose (fp);
   SLFREE (file);

   return status;
}

/* Get the lowest and highest article numbers by the simple method
 * or looking at the files in the directory.
 * Returns 0 on success, -1 on failure
 */
static int spool_read_minmax_from_dir (NNTP_Artnum_Type *min, NNTP_Artnum_Type *max, char *dir)
{
   Slrn_Dir_Type *dp;

   if (dir == NULL) dir = ".";

   /* I suspect this is very unlikely to fail */
   if ((dp = slrn_open_dir (dir)) == NULL)
     {
	slrn_error (_("Unable to open directory %s"), dir);
	return -1;
     }

   if (-1 == spool_read_minmax_from_dp (dp, min, max))
     {
	if (-1 == spool_read_minmax_file (min, max, dir))
	  {
	     *min = 1;
	     *max = 0;
	  }
     }

   (void) slrn_close_dir(dp);
   return 0;
}

#if SPOOL_ACTIVE_FOR_ART_RANGE
/* Get the lowest and highest article numbers from the active file
 * Returns 0 on success, -1 on failure
 * (failure => active file didn't open, or the group wasn't in it)
 */
static int spool_read_minmax_from_active( char *name, NNTP_Artnum_Type *min, NNTP_Artnum_Type *max )
{
   char buf[512];
   unsigned int len;

   spool_fclose_local();
   Spool_fh_local = fopen(Slrn_Active_File,"r");
   if (Spool_fh_local == NULL) return -1;

   len = strlen(name);
   buf[len] = 0;		       /* init this for test below */

   while (NULL != fgets (buf, sizeof(buf), Spool_fh_local))
     {
	/* quick, crude test first to see if it could possibly be a match */
	if ((buf[len] == ' ')
	    && (0 == memcmp (buf, name, len)))
	  {
	     spool_fclose_local ();
	     if (2 != sscanf (buf + len + 1, NNTP_FMT_ARTNUM NNTP_FMT_ARTNUM, max, min))
	       return -1;

	     Spool_Max_Artnum = *max;
	     debug_output (NULL, -1, "from active:%s " NNTP_FMT_ARTNUM_2, name, *min, *max);
	     return 0;
	  }
	buf[len] = 0;
     }
   spool_fclose_local();

   return -1;
}
#endif

/* Get the lowest and highest article numbers from the overview file
 * Returns 0 on success, -1 on failure
 */
static int spool_read_minmax_from_overview (char *name, NNTP_Artnum_Type *min, NNTP_Artnum_Type *max)
{
   /* chunk size to step back through .overview files by
    * when trying to find start of last line */
#define SPOOL_NOV_STEP 1024
   /* If there's no .overview file, get min/max info from the active file */
   /* ditto if .overview file is empty */
   int ch;
   long fpos;
   int found;
   int first;

   (void) name;

   if (Slrn_Prefer_Head == 2)
     Spool_Server_Obj.sv_has_xover = 0;
   else
     /* !HACK! this assumes the overview file is rewound */
     Spool_Server_Obj.sv_has_xover = ((Spool_fh_nov != NULL)
				      && (1 == fscanf (Spool_fh_nov, NNTP_FMT_ARTNUM, min)));

   if (0 == Spool_Server_Obj.sv_has_xover)
     return -1;

   /* find start of last line */
   if (-1 == fseek (Spool_fh_nov, 0, SEEK_END))
     {
        debug_output (__FILE__, __LINE__, "fseek returned -1; errno %d (%s).", errno, strerror(errno));
        return -1;
     }
   if (-1 == (fpos = ftell (Spool_fh_nov)))
     {
        debug_output (__FILE__, __LINE__, "ftell returned -1; errno %d (%s).", errno, strerror(errno));
        return -1;
     }

   found=0;
   first=1;

   while (!found && (fpos > 0))
     {
	int i, len;

	len = SPOOL_NOV_STEP;

	/* don't run of the start of the file */
	if (fpos < (long)len) len = fpos;

	fpos -= len;
        if (-1 == fseek(Spool_fh_nov, fpos, SEEK_SET))
          {
             debug_output (__FILE__, __LINE__, "fseek returned -1; errno %d (%s).", errno, strerror(errno));
             return -1;
          }

	if (first)
	  {
	     /* on the first pass, we want to ignore the last byte \n at eof */
	     --len;
	     first = 0;
	  }

	for(i = 0; i < len; i++ )
	  {
	     ch = getc(Spool_fh_nov);

	     assert(ch!=EOF); /* shouldn't happen */
	     if (ch =='\n')
	       found = i + 1; /* and keep going in case there's another */
	  }
     }

   if (-1 == fseek(Spool_fh_nov, fpos + found, SEEK_SET))
     {
        debug_output (__FILE__, __LINE__, "fseek returned -1; errno %d (%s).", errno, strerror(errno));
        return -1;
     }

   if (1 != fscanf (Spool_fh_nov, NNTP_FMT_ARTNUM, max))
     {
        debug_output (__FILE__, __LINE__, "Hmmm, unable to understand overview file - no integer found?");
        return -1;
     }

   rewind (Spool_fh_nov);

   debug_output (NULL, -1, ("%s " NNTP_FMT_ARTNUM_2), name,*min,*max);
   return 0;
}

static int spool_select_group (char *name, NNTP_Artnum_Type *min, NNTP_Artnum_Type *max)
{
   /* close any open files */
   spool_fclose_local();

   if (Spool_fh_nov != NULL)
     {
	fclose (Spool_fh_nov);
	Spool_fh_nov = NULL;
     }

   slrn_free (Spool_Group);
   slrn_free (Spool_Group_Name);

   Spool_Group = slrn_spool_dircat (Slrn_Spool_Root, name, 1);
   Spool_Group_Name = slrn_safe_strmalloc (name);

   Spool_fh_nov = spool_open_nov_file ();

   if ((-1 == spool_read_minmax_from_overview (name, min, max))
#if SPOOL_ACTIVE_FOR_ART_RANGE
       && (-1 == spool_read_minmax_from_active (name, min, max))
#endif
       && (-1 == spool_read_minmax_from_dir (min, max, Spool_Group)))
     return -1;

   Spool_Max_Artnum = *max;
   Spool_Min_Artnum = *min;

   debug_output (NULL, -1, "Group: %s " NNTP_FMT_ARTRANGE, name, *min, *max);
   return OK_GROUP;
}

static int spool_refresh_groups (Slrn_Group_Range_Type *gr, int n)
{
   while (n)
     {
	if (-1 == spool_select_group (gr->name,
				      &(gr->min), &(gr->max)))
	  gr->min = -1;
	n--;
	gr++;
     }
   return 0;
}

static char *spool_current_group (void)
{
   return Spool_Group_Name == NULL ? "" : Spool_Group_Name;
}

static int Spool_Server_Inited = 0;

static void spool_close_server (void)
{
   slrn_free (Spool_Group);
   Spool_Group = NULL;

   spool_fclose_local();

   if (NULL != Spool_fh_nov)
     {
	fclose (Spool_fh_nov);
	Spool_fh_nov = NULL;
     }
   Spool_Server_Inited = 0;
}

static int spool_has_cmd (char *cmd)
{
   (void) cmd;
   return 0; /* deny everything */
}

static int spool_initialize_server (void)
{
   if (Spool_Server_Inited) spool_close_server ();

   if (2 != slrn_file_exists (Slrn_Spool_Root))
     {
	slrn_error (_("Local spool directory '%s' doesn't exist."), Slrn_Spool_Root);
	return -1;
     }

   /* I think it's better to think that the *server* has XOVER, but
    * some (or all) groups may not.
    * So set this to 1 here, and then to 0 or 1 in spool_select_group if we
    * find an overview file
    */
   Spool_Server_Obj.sv_has_xover = 1;
   Spool_Server_Inited = 1;
   return 0;
}

static int spool_read_fhlocal (char *line, unsigned int len)
{
   do
     {
	if ((NULL == Spool_fh_local)
	    || (NULL == fgets (line, len, Spool_fh_local))
	    || (Spool_fhead && (line[0]=='\n')))
	  {
	     spool_fclose_local();
	     return 0;
	  }
     }
   while (((Spool_Output_Regexp != NULL) &&
	   (NULL == slrn_regexp_match (Spool_Output_Regexp, line))) ||
	  (Spool_Ignore_Comments && (*line == '#')));

   len = strlen(line);

   bytes_read += len;

   if (len && (line [len - 1] == '\n'))
     line [len-1] = '\0';

   return 1;
}

static int spool_read_line (char *line, unsigned int len)
{
   if (Spool_Doing_XPat) return spool_read_xpat (line, len);

   if (Spool_Doing_XOver) return spool_read_xover (line, len);

   if (NULL != Spool_XHdr_Field)
     {
	char buf[NNTP_BUFFER_SIZE];
	int retval = -1;

	/* if (buf == NULL) return -1; */
	if ((len > 33) &&
	    (1 == (retval = spool_read_xhdr (buf, sizeof (buf)))))
	  {
	     unsigned int numlen;
	     (void) slrn_snprintf (line, len, (NNTP_FMT_ARTNUM " "), (Spool_XOver_Next - 1));
	     numlen = strlen (line);
	     if (len > numlen)
	       slrn_strncpy (line + numlen, buf, len - numlen);
	  }
	return retval;
     }

   if (Spool_fFakingActive) return spool_fakeactive_read_line (line, len);

   return spool_read_fhlocal (line, len);
}

typedef struct
{
   int xover_field;
   NNTP_Artnum_Type rmin, rmax;
   char header[80];
   char pat[256];
}
Spool_XPat_Type;

static Spool_XPat_Type Spool_XPat_Struct;

static int spool_xpat_match (char *str, char *pat)
{
   /* HACK.  This needs fixed for more general patterns. */
   if (NULL == strstr (str, pat))
     return -1;

   return 0;
}

static int spool_read_xpat (char *buf, unsigned int len)
{
   char tmpbuf [8192];

   Spool_Doing_XPat = 0;

   if (Spool_XPat_Struct.xover_field == -1)
     {
	NNTP_Artnum_Type num;

	for (num = Spool_XPat_Struct.rmin; num <= Spool_XPat_Struct.rmax; num++)
	  {
	     if (-1 == spool_one_xhdr_command (Spool_XPat_Struct.header, num,
					       tmpbuf, sizeof (tmpbuf)))
	       continue;

	     if (-1 != spool_xpat_match (tmpbuf, Spool_XPat_Struct.pat))
	       {
		  unsigned int blen;

		  Spool_Doing_XPat = 1;
		  Spool_XPat_Struct.rmin = num + 1;

		  slrn_snprintf (buf, len, (NNTP_FMT_ARTNUM " "), num);
		  blen = strlen (buf);

		  strncpy (buf + blen, tmpbuf, len - blen);
		  buf[len - 1] = 0;

		  return 1;
	       }
	  }

	Spool_XPat_Struct.rmin = Spool_XPat_Struct.rmax + 1;
     }
   else
     {
	/* read the overview file until the right field matches the pattern */
	while (NULL != (fgets (tmpbuf, sizeof (tmpbuf), Spool_fh_local)))
	  {
	     /* first field is article number; "+1" skips it */
	     int field = Spool_XPat_Struct.xover_field + 1;
	     NNTP_Artnum_Type num = NNTP_STR_TO_ARTNUM (tmpbuf);
	     char *b = tmpbuf, *end;

	     if ((num > Spool_XPat_Struct.rmax) || (num < 0))
	       {
		  spool_fclose_local ();
		  return 0;
	       }

	     while (field)
	       {
		  b = slrn_strbyte (b, '\t');
		  /* If we reach the end of the overview line before finding
		   * the correct field, the overview file must be corrupt. */
		  if (b == NULL)
		    {
		       debug_output (NULL, -1, "Overview file corrupt? Unable to find "
				     "field %d in overview line %s.",
				     Spool_XPat_Struct.xover_field, tmpbuf);
		       spool_fclose_local ();
		       return -1;
		    }
		  b++;
		  field--;
	       }
	     end = slrn_strbyte (b, '\t');
	     if (end != NULL)
	       *end = '\0';

	     if (-1 != spool_xpat_match (b, Spool_XPat_Struct.pat))
	       {
		  unsigned int blen;

		  Spool_Doing_XPat = 1;
		  slrn_snprintf (buf, len, (NNTP_FMT_ARTNUM " "), num);
		  blen = strlen (buf);

		  strncpy (buf + blen, b, len - blen);
		  buf[len - 1] = 0;
		  return 1;
	       }
	  }
	spool_fclose_local ();
     }

   return 0;
}

static int spool_xpat_cmd (char *hdr, NNTP_Artnum_Type rmin, NNTP_Artnum_Type rmax, char *pat)
{
   static char *overview_headers [] =
     {
	"Subject", "From", "Date", "Message-ID",
	"References", "Bytes", "Lines",
	NULL
     };

   spool_fclose_local ();
   Spool_Doing_XPat = 0;
   memset ((char *) &Spool_XPat_Struct, 0, sizeof (Spool_XPat_Struct));

   if (rmin < Spool_Min_Artnum)
     rmin = Spool_Min_Artnum;
   if (rmax > Spool_Max_Artnum)
     rmax = Spool_Max_Artnum;

   Spool_XPat_Struct.rmin = rmin;
   Spool_XPat_Struct.rmax = rmax;

   /* The memset will guarantee that these are NULL terminated. */
   strncpy (Spool_XPat_Struct.header, hdr, sizeof (Spool_XPat_Struct.header) - 1);
   strncpy (Spool_XPat_Struct.pat, pat, sizeof (Spool_XPat_Struct.pat) - 1);

   Spool_XPat_Struct.xover_field = -1;

   if (Slrn_Server_Obj->sv_has_xover)
     {
	int field = 0;

	while (1)
	  {
	     char *h = overview_headers [field];

	     if (h == NULL) break;
	     if (0 == slrn_case_strcmp ( h,  hdr))
	       {
		  Spool_XPat_Struct.xover_field = field;
		  break;
	       }
	     field++;
	  }
     }

   if (Spool_XPat_Struct.xover_field != -1)
     {
	Spool_fh_local = spool_open_nov_file ();
	if (Spool_fh_local == NULL)
	  return ERR_COMMAND;
	if (-1 == overview_file_seek (0, -1, Spool_XPat_Struct.rmin))
	  return -1;
     }

   Spool_Doing_XPat = 1;

   return OK_HEAD;
}

static int spool_select_article (NNTP_Artnum_Type n, char *msgid)
{
   /*    printf("spool_select_article(%d,%s)\n",n,msgid); */

   if (n == -1)
     {
	if ((msgid == NULL) || (*msgid == 0))
	  return -1;

	if (-1 == spool_find_artnum_from_msgid (msgid, &n))
	  return ERR_NOARTIG;
     }

   spool_fclose_local();

   if (NULL == (Spool_fh_local = spool_open_article_num (n)))
     return ERR_NOARTIG;

   Spool_cur_artnum = n;
   return OK_ARTICLE;
}

/* The hdr string should NOT include the ':' */
static int spool_one_xhdr_command (char *hdr, NNTP_Artnum_Type num, char *buf,
				   unsigned int buflen)
{
   char tmpbuf [1024];
   unsigned int colon;

   spool_fclose_local ();

   if (NULL == (Spool_fh_local = spool_open_article_num (num)))
     return -1;

   Spool_fhead = 1;		       /* stop after headers */

   colon = strlen (hdr);

   while (1 == spool_read_fhlocal (tmpbuf, sizeof (tmpbuf)))
     {
	char *b;
	if (slrn_case_strncmp ( tmpbuf,  hdr, colon)
	    || (tmpbuf[colon] != ':'))
	  continue;

	b = tmpbuf + (colon + 1);
	if (*b == ' ') b++;
	slrn_strncpy (buf, b, buflen);
	while ((1 == spool_read_fhlocal (tmpbuf, sizeof (tmpbuf))) &&
	       ((*tmpbuf == ' ') || (*tmpbuf == '\t')))
	  {
	     unsigned int len = strlen (buf);
	     b = tmpbuf + 1;
	     slrn_strncpy (buf + len, b, buflen - len);
	  }
	return 0;
     }

   return -1;
}

static int spool_xhdr_command (char *field, NNTP_Artnum_Type min, NNTP_Artnum_Type max)
{
   debug_output (NULL, -1, ("spool_xhdr_command(%s " NNTP_FMT_ARTNUM_2),
		 field, min, max);

   spool_fclose_local ();

   if (max > Spool_Max_Artnum)
     max = Spool_Max_Artnum;

   if (min < Spool_Min_Artnum)
     min = Spool_Min_Artnum;

   Spool_XOver_Next = Spool_XOver_Min = min;
   Spool_XOver_Max = max;
   Spool_XHdr_Field = field;

   return OK_HEAD;
}

static int spool_read_xhdr (char *the_buf, unsigned int len)
{
   int retval = -1;

   if (Spool_XHdr_Field == NULL)
     return -1;

   if (Spool_XOver_Next > Spool_XOver_Max)
     {
	Spool_XHdr_Field = NULL;
	return 0;
     }

   while ((retval == -1) && (Spool_XOver_Next <= Spool_XOver_Max))
     retval = spool_one_xhdr_command (Spool_XHdr_Field, Spool_XOver_Next++,
				      the_buf, len);

   if (Spool_XOver_Next > Spool_XOver_Max)
     Spool_XHdr_Field = NULL;

   return (retval == -1) ? -1 : 1;
}

static int spool_list (char *what)
{
   if (!slrn_case_strcmp (what,"overview.fmt"))
     {
	spool_fclose_local();
	Spool_fh_local=fopen(Slrn_Overviewfmt_File,"r");
	if (Spool_fh_local)
	  {
	     Spool_Ignore_Comments = 1;
	     return OK_GROUPS;
	  }
     }
   return ERR_FAULT;
}

static int spool_list_newsgroups (void)
{
   spool_fclose_local();
   Spool_fh_local=fopen(Slrn_Newsgroups_File,"r");
   if (!Spool_fh_local)
     {
	/* Use readdir() to return a list of newsgroups read from the
	 * "newsspool" so we can read MH folders, etc.  This would be more
	 * than a little slow for a true newsspool.
	 */
	spool_fake_active(Slrn_Spool_Root);
	Spool_fFakingActive=1;
	Spool_fakeactive_newsgroups=1;
/*	slrn_exit_error("Couldn't open newsgroups file '%s'", NEWSGROUPS); */
     }
   return OK_GROUPS;
}

/* Having an independant buffer for the spool code might spare us mysterious
 * bugs, so I'm not simply using slrn_compile_regexp_pattern */
#if SLANG_VERSION < 20000
static SLRegexp_Type *spool_compile_regexp_pattern (char *pat)
{
   static unsigned char compiled_pattern_buf [512];
   static SLRegexp_Type re;

   re.pat = (unsigned char *) pat;
   re.buf = compiled_pattern_buf;
   re.buf_len = sizeof (compiled_pattern_buf);
   re.case_sensitive = 1;

   if (0 != SLang_regexp_compile (&re))
     {
	slrn_error (_("Invalid regular expression or expression too long."));
	return NULL;
     }
   return &re;
}
#endif

static int spool_list_active (char *pat)
{
   spool_fclose_local();
   Spool_fh_local=fopen (Slrn_Active_File,"r");
   if (pat != NULL)
     {
#if SLANG_VERSION < 20000
	Spool_Output_Regexp=spool_compile_regexp_pattern (slrn_fix_regexp (pat));
#else
	if (Spool_Output_Regexp != NULL)
	  SLregexp_free (Spool_Output_Regexp);
	Spool_Output_Regexp = SLregexp_compile (slrn_fix_regexp(pat),0);
#endif
     }

   if (!Spool_fh_local)
     {
	spool_fake_active(Slrn_Spool_Root);
	Spool_fFakingActive=1;
	Spool_fakeactive_newsgroups=0;
	return OK_GROUPS;
	/* Use readdir() to return a list of newsgroups and article ranges read
	 * from the "newsspool" so we can read MH folders, etc.  This would
	 * be more than a little slow for a true newsspool.
	 */
/*	slrn_exit_error("Couldn't open active file '%s'", ACTIVE);*/
     }
   return OK_GROUPS;
}

static int spool_send_authinfo (void)
{
   return 0;
}

typedef struct _Spool_DirTree_Type
{
   struct _Spool_DirTree_Type *parent;
   Slrn_Dir_Type *dp;
   int len;
   long lo, hi;
}
Spool_DirTree_Type;

static Spool_DirTree_Type *Spool_Head;

static char Spool_Buf[256];
static char Spool_nBuf[256];
static int Spool_Is_LeafDir;

static void spool_fake_active_in (char *dir)
{
   char *p;
   Slrn_Dir_Type *dp;
   Spool_DirTree_Type *tmp;

   p = Spool_Buf + strlen(Spool_Buf);

   if ((dir != NULL) &&
       (strlen (dir) < sizeof (Spool_Buf) - (size_t) (Spool_Buf - p + 2)))
     {
	*p = SLRN_PATH_SLASH_CHAR;
	strcpy (p + 1, dir); /* safe */
     }

   if ((2 != slrn_file_exists (Spool_Buf))
       || (NULL == (dp = slrn_open_dir (Spool_Buf))))
     {
	*p = 0;
	return;
     }

   Spool_Is_LeafDir = 1;
   tmp = (Spool_DirTree_Type *) slrn_safe_malloc (sizeof(Spool_DirTree_Type));

   tmp->dp = dp;
   tmp->parent = Spool_Head;
   tmp->hi = 0;
   tmp->lo = LONG_MAX;

   if (dir == NULL)
     tmp->len = 1;
   else
     {
	tmp->len = strlen (dir);

	p = Spool_nBuf + strlen (Spool_nBuf);
	if (strlen (dir) < sizeof (Spool_nBuf) - (size_t) (Spool_nBuf - p + 2))
	  {
	     if (p != Spool_nBuf) *p++ = '.';
	     strcpy (p, dir); /* safe */
	  }
     }

   Spool_Head = tmp;
}

static void spool_fake_active_out (void)
{
   Spool_DirTree_Type *tmp;
   int i;

   (void)slrn_close_dir (Spool_Head->dp);
   Spool_Is_LeafDir = 0;

   Spool_Buf [strlen(Spool_Buf) - Spool_Head->len - 1] = '\0';

   i = strlen(Spool_nBuf) - Spool_Head->len - 1;

   if (i < 0) i = 0;
   Spool_nBuf[i]='\0';

   tmp = Spool_Head;
   Spool_Head = Spool_Head->parent;
   SLFREE(tmp);
}

static int spool_fake_active (char *path)
{
   slrn_strncpy (Spool_Buf, path, sizeof (Spool_Buf));
   *Spool_nBuf='\0';
   Spool_Head=NULL;
   spool_fake_active_in (NULL);
   return 0;
}

static int spool_fakeactive_read_line(char *line, int len)
{
   Slrn_Dirent_Type *ep;
   char *p;
   long l;

   (void) len;

   emptydir:

   if (!Spool_Head)
     {
      /* we've reached the end of the road */
	Spool_fFakingActive = 0;
	return 0;
     }

   /* Scan through all the files, checking the ones with numbers for names */
   while ((ep = slrn_read_dir(Spool_Head->dp)) != NULL)
     {
	p = ep->name;

	if ((0 == spool_is_name_all_digits (p))
	    || ((l = atol (p)) == 0))
	  {
	     if (!(p[0]=='.' && (p[1]=='\0' || (p[1]=='.' && p[2]=='\0'))))
	       {
		  spool_fake_active_in(p);
	       }
	     continue;
	  }
	if (l < Spool_Head->lo)
	  Spool_Head->lo = l;
	if (l > Spool_Head->hi)
	  Spool_Head->hi = l;
     }

   if (Spool_Head->lo == LONG_MAX && Spool_Head->hi==0)
     {
	/* assume all leaf directories are valid groups */
	/* non-leaf directories aren't groups unless they have articles in */
	if (!Spool_Is_LeafDir)
	  {
	     spool_fake_active_out();
	     goto emptydir; /* skip empty "groups" */
	  }
	Spool_Head->lo = 1;
     }

   if (Spool_fakeactive_newsgroups)
     {
      /* newsgroups: alt.foo A group about foo */
	slrn_snprintf (line, len, "%s ?\n", Spool_nBuf);
     }
   else
     {
      /* active: alt.guitar 0000055382 0000055345 y */
	slrn_snprintf (line, len, "%s %ld %ld y\n", Spool_nBuf,
		       Spool_Head->hi, Spool_Head->lo);
     }
   spool_fake_active_out();
   return 1;
}

static void spool_reset (void)
{
   spool_fclose_local ();
}

static unsigned int spool_get_bytes (int clear)
{
   unsigned int temp;

   temp = bytes_read;
   if (clear)
     bytes_read = 0;

   return temp;
}

char *Slrn_Inn_Root;
char *Slrn_Spool_Root;
char *Slrn_Nov_Root;
char *Slrn_Nov_File;
char *Slrn_Headers_File;
char *Slrn_Active_File;
char *Slrn_ActiveTimes_File;
char *Slrn_Newsgroups_File;
int Slrn_Spool_Check_Up_On_Nov;

static int spool_init_objects (void)
{
   char *login;
   Spool_Server_Obj.sv_select_group = spool_select_group;
   Spool_Server_Obj.sv_refresh_groups = spool_refresh_groups;
   Spool_Server_Obj.sv_current_group = spool_current_group;
   Spool_Server_Obj.sv_read_line = spool_read_line;
   Spool_Server_Obj.sv_close = spool_close_server;
   Spool_Server_Obj.sv_reset = spool_reset;
   Spool_Server_Obj.sv_initialize = spool_initialize_server;
   Spool_Server_Obj.sv_select_article = spool_select_article;
   Spool_Server_Obj.sv_get_article_size = spool_get_article_size;
   Spool_Server_Obj.sv_put_server_cmd = spool_put_server_cmd;
   Spool_Server_Obj.sv_xpat_cmd = spool_xpat_cmd;
   Spool_Server_Obj.sv_xhdr_command = spool_one_xhdr_command;
   Spool_Server_Obj.sv_has_cmd = spool_has_cmd;
   Spool_Server_Obj.sv_list = spool_list;
   Spool_Server_Obj.sv_list_newsgroups = spool_list_newsgroups;
   Spool_Server_Obj.sv_list_active = spool_list_active;
   Spool_Server_Obj.sv_send_authinfo = spool_send_authinfo;

   Spool_Server_Obj.sv_has_xhdr = 1;
   Spool_Server_Obj.sv_has_xover = 0;
   Spool_Server_Obj.sv_nntp_xover = spool_nntp_xover;
   Spool_Server_Obj.sv_nntp_xhdr = spool_xhdr_command;
   Spool_Server_Obj.sv_nntp_head = spool_nntp_head;
   Spool_Server_Obj.sv_nntp_next = spool_nntp_next;
   Spool_Server_Obj.sv_nntp_bytes = spool_get_bytes;
   Spool_Server_Obj.sv_id = SERVER_ID_UNKNOWN;

   Slrn_Inn_Root = slrn_safe_strmalloc (SLRN_SPOOL_INNROOT);
   Slrn_Spool_Root = slrn_safe_strmalloc (SLRN_SPOOL_ROOT);
   Slrn_Nov_Root = slrn_safe_strmalloc (SLRN_SPOOL_NOV_ROOT);
   Slrn_Nov_File = slrn_safe_strmalloc (SLRN_SPOOL_NOV_FILE);
   Slrn_Headers_File = slrn_safe_strmalloc (SLRN_SPOOL_HEADERS);
   Slrn_Active_File = slrn_safe_strmalloc (SLRN_SPOOL_ACTIVE);
   Slrn_ActiveTimes_File = slrn_safe_strmalloc (SLRN_SPOOL_ACTIVETIMES);
   Slrn_Newsgroups_File = slrn_safe_strmalloc (SLRN_SPOOL_NEWSGROUPS);
   Slrn_Overviewfmt_File = slrn_safe_strmalloc (SLRN_SPOOL_OVERVIEWFMT);
   if ((NULL == (login = Slrn_User_Info.login_name)) ||
       (*login == 0))
     login = "!unknown";
   Slrn_Requests_File = slrn_strdup_printf ("%s/%s", SLRNPULL_REQUESTS_DIR, login);

#if defined(IBMPC_SYSTEM)
   slrn_os2_convert_path (Slrn_Inn_Root);
   slrn_os2_convert_path (Slrn_Spool_Root);
   slrn_os2_convert_path (Slrn_Nov_Root);
   slrn_os2_convert_path (Slrn_Nov_File);
   slrn_os2_convert_path (Slrn_Headers_File);
   slrn_os2_convert_path (Slrn_Active_File);
   slrn_os2_convert_path (Slrn_ActiveTimes_File);
   slrn_os2_convert_path (Slrn_Newsgroups_File);
   slrn_os2_convert_path (Slrn_Overviewfmt_File);
   slrn_os2_convert_path (Slrn_Requests_File);
#endif
   return 0;
}

/* This function is used below.  It has a very specific purpose. */
static char *spool_root_dircat (char *file)
{
   char *f;

   if (slrn_is_absolute_path (file))
     return file;

   f = slrn_spool_dircat (Slrn_Inn_Root, file, 0);
   SLFREE (file);
   return f;
}

static int spool_select_server_object (void)
{
   Slrn_Server_Obj = &Spool_Server_Obj;
   Slrn_Active_File = spool_root_dircat (Slrn_Active_File);
   Slrn_ActiveTimes_File = spool_root_dircat (Slrn_ActiveTimes_File);
   Slrn_Newsgroups_File = spool_root_dircat (Slrn_Newsgroups_File);
   Slrn_Overviewfmt_File = spool_root_dircat (Slrn_Overviewfmt_File);
   Slrn_Requests_File = spool_root_dircat (Slrn_Requests_File);

   slrn_free (Spool_Server_Obj.sv_name);
   Spool_Server_Obj.sv_name = slrn_safe_strmalloc (Slrn_Spool_Root);

   return 0;
}

/* Handling of the additional newsrc-style files in true offline mode: */
Slrn_Range_Type *slrn_spool_get_no_body_ranges (char *group)
{
   VFILE *vp;
   char *vline;
   unsigned int vlen;
   Slrn_Range_Type *retval = NULL;
   char *p, *q;

   p = slrn_spool_dircat (Slrn_Spool_Root, group, 1);
   q = slrn_spool_dircat (p, Slrn_Headers_File, 0);
   SLFREE (p);

   vp = vopen (q, 4096, 0);
   SLFREE (q);
   if (NULL == vp)
     return NULL;

   if (NULL != (vline = vgets (vp, &vlen)))
     {
	if (vline[vlen-1] == '\n')
	  vline[vlen-1] = 0; /* make sure line is NULL terminated */
	else
	  vline[vlen] = 0;
	retval = slrn_ranges_from_newsrc_line (vline);
     }

   vclose (vp);

   return retval;
}

Slrn_Range_Type *slrn_spool_get_requested_ranges (char *group) /*{{{*/
{
   VFILE *vp;
   char *vline;
   unsigned int vlen;
   Slrn_Range_Type *retval = NULL;

   if (NULL == (vp = vopen (Slrn_Requests_File, 4096, 0)))
     return NULL;

   while (NULL != (vline = vgets (vp, &vlen)))
     {
	char *p = vline;
	char *pmax = p + vlen;

	while ((p < pmax) && (*p != ':'))
	  p++;

	if ((p == pmax) || (p == vline) ||
	    (strncmp(vline, group, (p-vline))))
	  continue;

	if (vline[vlen-1] == '\n')
	  vline[vlen-1] = 0;
	else
	  vline[vlen] = 0;

	retval = slrn_ranges_from_newsrc_line (p+1);
	break;
     }
   vclose (vp);
   return retval;
}
/*}}}*/

/* Updates the request file with the new ranges
 * returns 0 on success, -1 otherwise */
int slrn_spool_set_requested_ranges (char *group, Slrn_Range_Type *r) /*{{{*/
{
   char *old_file = NULL;
   FILE *fp;
   VFILE *vp;
   char *vline;
   unsigned int vlen;
   struct stat filestat;
#ifdef __unix__
   int stat_worked = 0;
#endif
   int have_old = 0;

   slrn_init_hangup_signals (0);

#ifdef __unix__
   /* Try to preserve file permissions and owner/group. */
   stat_worked = (-1 != stat (Slrn_Requests_File, &filestat));
#endif

   /* Save old file (we'll copy most of it; then, it gets deleted) */
   have_old = (0 == slrn_create_backup (Slrn_Requests_File));

   if (NULL == (fp = fopen (Slrn_Requests_File, "w")))
     {
	if (have_old) slrn_restore_backup (Slrn_Requests_File);
	slrn_init_hangup_signals (1);
	return -1;
     }

#ifdef __unix__
# if !defined(IBMPC_SYSTEM)
   /* Try to preserve file permissions and owner/group */
#  ifndef S_IRUSR
#   define S_IRUSR 0400
#   define S_IWUSR 0200
#   define S_IRGRP 0040
#  endif
   if (stat_worked)
     {
	if (-1 == chmod (Slrn_Requests_File, filestat.st_mode))
	  (void) chmod (Slrn_Requests_File, S_IWUSR | S_IRUSR | S_IRGRP);

	(void) chown (Slrn_Requests_File, filestat.st_uid, filestat.st_gid);
     }
   else
     (void) chmod (Slrn_Requests_File, S_IWUSR | S_IRUSR | S_IRGRP);
# endif
#endif

   /* Write a line for the current group */
   if (r != NULL)
     {
	if ((EOF == fputs (group, fp)) ||
	    (EOF == fputs (": ",fp)) ||
	    (-1 == slrn_ranges_to_newsrc_file (r,0,fp)) ||
	    (EOF == fputc ('\n',fp)))
	  goto write_error;
     }

   /* Now, open the old file and append the data of all other groups */
   old_file = slrn_make_backup_filename (Slrn_Requests_File);
   if (NULL != (vp = vopen (old_file, 4096, 0)))
     {
	while (NULL != (vline = vgets (vp, &vlen)))
	  {
	     char *p = vline;
	     char *pmax = p + vlen;

	     while ((p < pmax) && (*p != ':'))
	       p++;

	     if ((p != vline) && (((p-vline != (int) strlen(group)) ||
				   strncmp(vline, group, (p-vline)))))
	       {
		  /* It seems we may not write past vline[vlen-1] for vgets
		   * to work correctly, so save and reset this. */
		  char ch = vline[vlen];
		  vline[vlen] = '\0';
		  if (EOF == fputs (vline, fp))
		    {
		       vclose (vp);
		       goto write_error;
		    }
		  vline[vlen] = ch;
	       }
	  }
	vclose (vp);
     }

   if (-1 == slrn_fclose (fp))
     goto write_error;

   if (have_old) slrn_delete_backup (Slrn_Requests_File);

   slrn_init_hangup_signals (1);
   SLfree (old_file);
   return 0;

   write_error:

   slrn_fclose (fp);
   /* Put back orginal file */
   if (have_old) slrn_restore_backup (Slrn_Requests_File);

   slrn_init_hangup_signals (1);
   SLfree (old_file);
   return -1;
}
/*}}}*/
