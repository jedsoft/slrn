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
#ifndef SLRNPULL_CODE
# include "slrnfeat.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#ifndef SLRNPULL_CODE
# include "slrn.h"
# include "group.h"
# include "misc.h"
# include "util.h"
# include "server.h"
# include "hash.h"
#endif

#include "art.h"
#include "xover.h"
#include "score.h"

/* The score file will implement article scoring for slrn.  Basically the idea 
 * is that the user will provide a set of regular expressions that
 * will act on the header of an article and return an integer or
 * 'score'. If this score is less than zero, the article will be
 * killed.  If it is a positive number greater than some user defined
 * value, the article will be flagged as interesting.  If the score
 * is zero, the article is not killed and it is not flagged as
 * interesting either.
 */

int Slrn_Perform_Scoring = SLRN_XOVER_SCORING | SLRN_EXPENSIVE_SCORING;
int Slrn_Prefer_Head = 0;

/* These two structures are pseudo-score types containing no compiled
 * regular expressions.
 */
typedef struct PScore_Regexp_Type
{
#define MAX_KEYWORD_LEN 24
   char keyword[MAX_KEYWORD_LEN];      /* used only by generic type */
   unsigned int header_type;
#define SCORE_SUBJECT 1
#define SCORE_FROM 2
#define SCORE_XREF 3
#define SCORE_NEWSGROUP 4
#define SCORE_REFERENCES 5
#define SCORE_LINES 6
#define SCORE_MESSAGE_ID 7
#define SCORE_DATE 8
#define SCORE_AGE 9
#define SCORE_BYTES 10
#define SCORE_HAS_BODY 11
#define SCORE_SUB_AND 12
#define SCORE_SUB_OR 13
   /* generic requires extra server interaction */
#define SCORE_GENERIC 16
   unsigned int flags;
#define NOT_FLAG 1
#define USE_INTEGER 2
   union
     {
	unsigned char *regexp_str;
	int ival;
	struct PScore_Regexp_Type *psrt;
     }
   ireg;
   struct PScore_Regexp_Type *next;
}
PScore_Regexp_Type;

typedef struct PScore_Type
{
   int score;
   unsigned int flags;
#define RETURN_THIS_SCORE 1
#define SCORE_IS_OR_TYPE 2
   struct PScore_Regexp_Type *pregexp_list;
   struct PScore_Type *next;

   /* the following variables are used by the score-debugger */
   const char *filename;
   unsigned int linenumber;
   const char *description;
}
PScore_Type;

typedef struct
{
   SLRegexp_Type regexp;
   unsigned char buf[512];	       /* for compiled pattern */
}
Our_SLRegexp_Type;

/* These two structures are compiled versions of the above. */
typedef struct Score_Regexp_Type
{
   unsigned int header_type;
   char *generic_keyword;	       /* pointer to space in pscore */
   int not_flag;
   int do_osearch;
   union
     {
	int ival;		       /* used by certain headers */
	Our_SLRegexp_Type re;
	SLsearch_Type se;
	struct Score_Regexp_Type *srt;
     }
   search;
   struct Score_Regexp_Type *next;
}
Score_Regexp_Type;

typedef struct Score_Type
{
   PScore_Type *pscore;		       /* points at structure this is derived from */
   struct Score_Regexp_Type regexp_list;
   struct Score_Type *next;
}
Score_Type;

#define MAX_GROUP_REGEXP_SIZE	256
#define MAX_GROUP_NAME_LEN	80

typedef struct Group_Score_Name_Type
{
   SLRegexp_Type group_regexp;
   char name[MAX_GROUP_NAME_LEN];      /* group name or pattern */
   unsigned char buf[MAX_GROUP_REGEXP_SIZE];/* for compiled pattern */
   struct Group_Score_Name_Type *next;
}
Group_Score_Name_Type;

typedef struct Group_Score_Type
{
   unsigned int gst_not_flag;
   Group_Score_Name_Type gsnt;
   PScore_Type *pst;
   struct Group_Score_Type *next;
}
Group_Score_Type;

typedef struct Scorefile_Name_Type
{
   const char *filename;
   struct Scorefile_Name_Type *next;
}
Scorefile_Name_Type;

Scorefile_Name_Type *Scorefile_Names = NULL;

static Slrn_Score_Debug_Info_Type *append_score_debug_info
  (Slrn_Score_Debug_Info_Type *prev, PScore_Type *sc)
{
   Slrn_Score_Debug_Info_Type *newentry;
   
   if ((newentry = (Slrn_Score_Debug_Info_Type *)slrn_malloc
	(sizeof (Slrn_Score_Debug_Info_Type), 0, 0)) == NULL)
     return NULL;
   
   newentry->filename = sc->filename;
   newentry->description = sc->description;
   newentry->linenumber = sc->linenumber;
   newentry->score = sc->score;
   newentry->stop_here = sc->flags & RETURN_THIS_SCORE;
   newentry->next = NULL;
   
   if (prev != NULL)
     prev->next = newentry;
   
   return newentry;
}

static Group_Score_Type *Group_Score_Root;
static Group_Score_Type *Group_Score_Next;
static Score_Type *Score_Root, *Score_Tail;

int Slrn_Apply_Score = 1;

/* returns 1 on match 0 if failed */
static int match_srt (Slrn_Header_Type *h, Score_Regexp_Type *srt,
		      char *newsgroup, int or_type) 
{
   char *s= NULL;
   int ival = 0;
   unsigned int len;

   while (srt != NULL)
     {
        switch (srt->header_type)
          {
           case SCORE_LINES:
	     ival = h->lines;
	     goto integer_compare;
	     
	   case SCORE_BYTES:
	     ival = h->bytes;
	     goto integer_compare;
	     
	   case SCORE_HAS_BODY:
	     ival = (h->flags & HEADER_WITHOUT_BODY) ? 0 : 1;
	     goto boolean_compare;
		  
           case SCORE_SUBJECT:
             s = h->subject;
             break;

           case SCORE_FROM:
             s = h->from;
             break;

           case SCORE_DATE:
             s = h->date;
             break;
	     
	   case SCORE_AGE:
	     ival = slrn_date_to_order_parm(h->date);
	     goto integer_compare;
	     
           case SCORE_MESSAGE_ID:
             s = h->msgid;
             break;

           case SCORE_XREF:
             s = h->xref;
             break;

           case SCORE_REFERENCES:
             s = h->refs;
             break;

           case SCORE_NEWSGROUP:
             s = newsgroup;
             break;

           case SCORE_GENERIC:
	     s = slrn_extract_add_header (h, srt->generic_keyword);
             break;

           case SCORE_SUB_AND:
             if (match_srt(h, srt->search.srt, newsgroup, 0))
	       { /* sub matches */
                  if (srt->not_flag)
                    {
                       if (or_type) goto next_srt;
                       return 0;
                    }
               }
             else /* sub does not match */
	       {
		 if (srt->not_flag == 0)
                   {
                      if (or_type) goto next_srt;
                      return 0;
                   }
	       }
	     if (or_type) return 1;
	     goto next_srt;
             break;

	   case SCORE_SUB_OR:
             if (match_srt(h, srt->search.srt, newsgroup, 1))
               { /* sub matches */
                  if (srt->not_flag)
                    {
                       if (or_type) goto next_srt;
                       return 0;
                    }
               }
             else /* sub does not match */
	       {
		 if (srt->not_flag == 0)
                   {
                      if (or_type) goto next_srt;
                      return 0;
                   }
	       }
             if (or_type) return 1;
	     goto next_srt;
             break;
           default:
             s = NULL;            /* not supposed to happen */
          } /*switch (srt->header_type)*/

        if (s == NULL)
          {
             if (srt->not_flag)
               {
                  /* Match */
                  if (or_type)
                    return 1;
		  goto next_srt;
               }

             /* Match failed */
             if (or_type) goto next_srt;
             return 0;
          }

        len = strlen (s);

        if (srt->do_osearch)
          {
             SLsearch_Type *se = &srt->search.se;

	     if ((len < (unsigned int) se->key_len)
                      || (NULL == SLsearch ((unsigned char *) s,
                                            (unsigned char *) s + len,
                                            se)))
               { /* expr does not match */
                  if (srt->not_flag == 0)
                    {
                       if (or_type) goto next_srt;
                       return 0;
                    }
               }
             else if (srt->not_flag)
               {
                  if (or_type) goto next_srt;
                  return 0;
               }
          }
        else
          {
             SLRegexp_Type *re;

             re = &srt->search.re.regexp;
             if ((len < re->min_length)
                 || (NULL == SLang_regexp_match ((unsigned char *)s, len, re)))
               {
                  if (srt->not_flag == 0)
                    {
                       if (or_type) goto next_srt;
                       return 0;
                    }
               }
             else if (srt->not_flag)
               {
                  if (or_type) goto next_srt;
                  return 0;
               }
          }
        /* Get here if above matched */
        if (or_type) return 1;
	srt = srt->next;
	continue;
	
	/* This is ugly but I am worried about speed. --- we only get
	 * here for those headers that have integer values.
	 */
	integer_compare:
	if ((ival < srt->search.ival) == (srt->not_flag == 0))
	  {
	     if (or_type) goto next_srt;
	     return 0;
	  }
	
	/* If we get here, the integer comparison matched. */
	if (or_type) return 1;
	srt = srt->next;
	continue;
	
	boolean_compare:
	if ((ival != srt->search.ival) == (srt->not_flag == 0))
	  {
	     if (or_type) goto next_srt;
	     return 0;
	  }
	
	if (or_type) return 1;
	
	next_srt:
        srt = srt->next;
     } /*while (srt != NULL)*/
   if (or_type) return 0;
   else return 1;
}

int slrn_score_header (Slrn_Header_Type *h, char *newsgroup,
		       Slrn_Score_Debug_Info_Type **info)
{
   Slrn_Score_Debug_Info_Type *prev = NULL;
   Score_Type *st;
   int score = 0;
   int or_type;
   Score_Regexp_Type *srt;
#ifndef SLRNPULL_CODE
#if SLRN_HAS_MSGID_CACHE
   char *s = slrn_is_msgid_cached (h->msgid, newsgroup, 1);
   if (s != NULL)
     {
	/* Kill it if this wasn't the newsgroup where we saw it */
	if (strcmp (s, newsgroup))
	  return Slrn_Kill_Score_Max;
     }
#endif
#endif				       /* NOT SLRNPULL_CODE */
   
   st = Score_Root;
   while (st != NULL)
     {
	srt = &st->regexp_list;
	or_type = st->pscore->flags & SCORE_IS_OR_TYPE;
	
        if (match_srt(h, srt, newsgroup, or_type))
          {
	     int st_score = st->pscore->score;
	     
	     if (info != NULL)
	       {
		  if (*info == NULL)
		    *info = prev = append_score_debug_info (NULL, st->pscore);
		  else if (prev != NULL)
		    prev = append_score_debug_info (prev, st->pscore);
	       }
	     
	     if (st->pscore->flags & RETURN_THIS_SCORE)
	       return st_score;
#if 0
	     if ((st_score == 9999)
		 || (st_score == -9999))
	       return st_score;
	     if (st_score == 0)
	       return score;
#endif
	     score += st_score;
	  }
	
	st = st->next;
     } /*while (st != NULL)*/
   return score;
}

static int compile_psrt (PScore_Regexp_Type *psrt, Score_Regexp_Type *srt,
			 int *generic)
{
   SLRegexp_Type *re;

   while (psrt != NULL)
     {
        unsigned int flags = psrt->flags;

        if (SCORE_GENERIC == (srt->header_type = psrt->header_type))
          {
             *generic += 1;
	     slrn_request_additional_header (psrt->keyword,
					     Slrn_Perform_Scoring & SLRN_EXPENSIVE_SCORING);
          }
	
        srt->not_flag = (0 != (flags & NOT_FLAG));
	  
	if ((srt->header_type == SCORE_SUB_AND) || 
		(srt->header_type == SCORE_SUB_OR))
	  {
             srt->search.srt = (Score_Regexp_Type *) slrn_malloc (sizeof (Score_Regexp_Type), 1, 0);
             if (srt->search.srt == NULL) return -1;

	     if (compile_psrt(psrt->ireg.psrt, srt->search.srt, generic) != 0)
	       return -1;
          }
	else
	  {
             srt->generic_keyword = psrt->keyword;

	     if (flags & USE_INTEGER)
               {
                  srt->search.ival = psrt->ireg.ival;
               }
             else
               {
                  re = &srt->search.re.regexp;
                  re->pat = psrt->ireg.regexp_str;
                  re->buf = srt->search.re.buf;
                  re->buf_len = sizeof (srt->search.re.buf);
                  re->case_sensitive = 0;

                  if (0 != SLang_regexp_compile(re))
                    {
                       return -1;
                    }

                  /* If an ordinary search is ok, use it. */
                  if (re->osearch)
                    {
                       srt->do_osearch = 1;
                       SLsearch_init ((char *) psrt->ireg.regexp_str, 1, 0, &srt->search.se);
                    }
               }
          }
		
        psrt = psrt->next;
        if (psrt == NULL) break;

        srt->next = (Score_Regexp_Type *) slrn_malloc (sizeof (Score_Regexp_Type), 1, 0);
        if (NULL == (srt = srt->next)) return -1;
     } /* while (psrt != NULL)*/
   return 0;
}

static int chain_group_regexp (PScore_Type *pst, int *generic)
{
   PScore_Regexp_Type *psrt;
   Score_Regexp_Type *srt;
   Score_Type *st;
   
   while (pst != NULL)
     {
	st = (Score_Type *) slrn_malloc (sizeof (Score_Type), 1, 0);
	if (st == NULL)
	  return -1;
	
	st->pscore = pst;
	psrt = pst->pregexp_list;
	srt = &st->regexp_list;
	
	if (compile_psrt(psrt, srt, generic) != 0)
	  slrn_free ((char*)st);
	else
	  {
	     if (Score_Root == NULL) Score_Root = st;
	     else Score_Tail->next = st;
	     Score_Tail = st;
	  }
	
	pst = pst->next;
     } /* while (pst != NULL) */
   return 0;
}

char *Slrn_Scorefile_Open = NULL;

static void free_srt (Score_Regexp_Type *srt)
{
   while (srt != NULL)
     {
	Score_Regexp_Type *srt_next = srt->next;
	if ((srt->header_type == SCORE_SUB_AND) || 
	    (srt->header_type == SCORE_SUB_OR))
	  free_srt (srt->search.srt);
	SLFREE (srt);
	srt = srt_next;
     }
}

static void free_group_chain (void)
{
   Score_Regexp_Type *srt;
   
   while (Score_Root != NULL)
     {
	Score_Type *next = Score_Root->next;
	srt = &Score_Root->regexp_list;
	/* first not malloced; free subscores only: */
	if ((srt->header_type == SCORE_SUB_AND) ||
	    (srt->header_type == SCORE_SUB_OR))
	  free_srt (srt->search.srt);
	free_srt (srt->next);
	SLFREE (Score_Root);
	Score_Root = next;
     }
   Score_Tail = NULL;
}

int slrn_open_score (char *group_name)
{
   Group_Score_Type *gsc;
   unsigned int n;
   int generic;

   free_group_chain ();
   
   if (Slrn_Scorefile_Open != NULL)
     {
	slrn_message_now (_("Reloading score file ..."));
	if (-1 == slrn_read_score_file (Slrn_Scorefile_Open))
	  slrn_error (_("Error processing score file %s."), Slrn_Scorefile_Open);
	slrn_free (Slrn_Scorefile_Open);
	Slrn_Scorefile_Open = NULL;
     }
   
   if ((Slrn_Perform_Scoring == 0) || (Group_Score_Root == NULL))
     {
	Slrn_Apply_Score = 0;
	return 0;
     }
   
   n = strlen (group_name);
   generic = 0;
   gsc = Group_Score_Root;
   while (gsc != NULL)
     {
	Group_Score_Name_Type *gsnt;
	int match;

	gsnt = &gsc->gsnt;
	match = 0;
	while ((gsnt != NULL) && (match == 0))
	  {
	     SLRegexp_Type *re = &gsnt->group_regexp;

	     match = ((re->min_length <= n)
		      && (NULL != SLang_regexp_match ((unsigned char *) group_name, n, re)));
	     gsnt = gsnt->next;
	  }

	if (gsc->gst_not_flag)
	  match = !match;

	if (match)
	  {
	     if (-1 == chain_group_regexp (gsc->pst, &generic))
	       {
		  free_group_chain ();
		  return -1;
	       }
	  }
	gsc = gsc->next;
     }
   
   if (Score_Root == NULL) return 0;
   
   Slrn_Apply_Score = 1;

#ifndef SLRNPULL_CODE
   if (generic && (Slrn_Prefer_Head || (1 != Slrn_Server_Obj->sv_has_cmd("XHDR Path"))))
     slrn_open_suspend_xover ();
#endif

   return 1;
}

void slrn_close_score (void)
{
   free_group_chain ();
#ifndef SLRNPULL_CODE
   slrn_close_suspend_xover ();
#endif
}



static int add_group_regexp (PScore_Regexp_Type *psrt, unsigned char *str,
			     unsigned char *keyword, unsigned int type,
			     int not_flag)
{
   unsigned int len;
   *str++ = 0;			       /* null terminate keyword by zeroing
					* out the colon.  This is by agreement
					* with the calling routine.
					*/
   
   if (*str == ' ') str++;       /* space following colon not meaningful */

   psrt->header_type = type;
   if ((type != SCORE_SUB_AND) && (type != SCORE_SUB_OR))
     {
	len = (unsigned int) (slrn_trim_string ((char *) str) - (char *) str);
	if (0 == len) return -1;
	
	if ((type == SCORE_LINES) || (type == SCORE_BYTES) || (type == SCORE_HAS_BODY))
	  {
	     psrt->ireg.ival = atoi((char *)str);
	     psrt->flags |= USE_INTEGER;
	  }
	else if (type == SCORE_AGE)
	  {
	     psrt->ireg.ival = time(NULL) - atoi((char *)str) * 86400;
	     psrt->flags |= USE_INTEGER;
	  }
	else
	  psrt->ireg.regexp_str = (unsigned char *) slrn_safe_strmalloc ((char *)str);
	
	strncpy (psrt->keyword, (char *) keyword, MAX_KEYWORD_LEN);
	psrt->keyword[MAX_KEYWORD_LEN - 1] = 0;
     }
   else
     {
	psrt->ireg.psrt = NULL;
     }

   if (not_flag) psrt->flags |= NOT_FLAG;
   psrt->next = NULL;
   
   return 0;
}


static int compile_group_names (char *group, Group_Score_Type *gst)
{
   char *comma;
   Group_Score_Name_Type *gsnt;
   unsigned int num_processed = 0;
   
   gsnt = &gst->gsnt;
   
   comma = group;
   while (comma != NULL)
     {
	SLRegexp_Type *re;
	
	group = slrn_skip_whitespace (group);
	
	comma = slrn_strchr ((char *) group, ',');
	if (comma != NULL) 
	  {
	     *comma++ = 0;
	     
	     while (1)
	       {
		  comma = slrn_skip_whitespace (comma);
		  if (*comma == 0)
		    {
		       comma = NULL;
		       break;
		    }
		  if (*comma != ',')
		    break;
		  comma++;
	       }
	  }
	
	(void) slrn_trim_string (group);
	if (*group == 0) continue;
	
	strncpy (gsnt->name, group, MAX_GROUP_NAME_LEN - 1);
	/* Note: because of the memset, this string is null terminated. */
   
	re = &gsnt->group_regexp;
	re->pat = (unsigned char *) slrn_fix_regexp ((char *)group);
	re->buf = gsnt->buf;
	re->buf_len = MAX_GROUP_REGEXP_SIZE;
	re->case_sensitive = 0;
	if (0 != SLang_regexp_compile (re))
	  {
	     return -1;
	  }
	
	if (comma != NULL)
	  {
	     Group_Score_Name_Type *gsnt1;
	     
	     gsnt1 = (Group_Score_Name_Type *) slrn_safe_malloc (sizeof (Group_Score_Name_Type));
	     
	     gsnt->next = gsnt1;
	     gsnt = gsnt1;
	     group = comma;
	  }
	num_processed++;
     }
   if (num_processed) return 0;
   return -1;
}

static PScore_Type *create_new_score
  (unsigned char *group, int new_group_flag, unsigned int gst_not_flag,
   int score, unsigned int pscore_flags,
   const char *filename, unsigned int linenumber, const char *description)
{
   PScore_Type *pst;
   
   if (new_group_flag)
     {
	Group_Score_Type *gst, *tail;
	
	tail = Group_Score_Next = Group_Score_Root;
	while (Group_Score_Next != NULL)
	  {
	     if ((Group_Score_Next->gsnt.next == NULL)
		 && (Group_Score_Next->gst_not_flag == gst_not_flag)
		 && !strcmp ((char *) group, Group_Score_Next->gsnt.name))
	       break;
	     tail = Group_Score_Next;
	     Group_Score_Next = Group_Score_Next->next;
	  }
	
	if (Group_Score_Next == NULL)
	  {
	     gst = (Group_Score_Type *) slrn_safe_malloc (sizeof (Group_Score_Type));
	     
	     if (-1 == compile_group_names ((char *)group, gst))
	       return NULL;
	     
	     gst->gst_not_flag = gst_not_flag;

	     if (Group_Score_Root == NULL)
	       {
		  Group_Score_Root = gst;
	       }
	     else tail->next = gst;
	     
	     Group_Score_Next = gst;
	  }
     }
   
   /* Now create the PseudoScore type and add it. */
   pst = (PScore_Type *) slrn_safe_malloc (sizeof (PScore_Type));

   pst->score = score;
   pst->flags = pscore_flags;

   pst->filename = filename;
   pst->linenumber = linenumber;

   if (description != NULL)
     pst->description = slrn_safe_strmalloc ((char *)description);
   else
     pst->description = slrn_safe_strmalloc ((char *)"");

   if (Group_Score_Next->pst == NULL)
     {
	Group_Score_Next->pst = pst;
     }
   else
     {
	PScore_Type *last = Group_Score_Next->pst;
	while (last->next != NULL) last = last->next;
	last->next = pst;
     }
   return pst;
}


static void score_error (char *msg, char *line, unsigned int linenum, char *file)
{
   slrn_error (_("Error processing %s\nLine %u:\n%s\n%s\n"),
	       file, linenum, line, msg);
}


static void free_psrt (PScore_Regexp_Type *r)
{
   while (r != NULL)
     {
	PScore_Regexp_Type *rnext = r->next;
	
	if ((r->header_type == SCORE_SUB_AND) ||
	    (r->header_type == SCORE_SUB_OR))
	  free_psrt (r->ireg.psrt);
	else if ((r->flags & USE_INTEGER) == 0)
	  slrn_free ((char *) r->ireg.regexp_str);
	SLFREE (r);
	r = rnext;
     }
}

static void free_group_scores (void)
{
   while (Group_Score_Root != NULL)
     {
	Group_Score_Type *gnext = Group_Score_Root->next;
	PScore_Type *pst = Group_Score_Root->pst;
	Group_Score_Name_Type *gsnt;
	
	gsnt = Group_Score_Root->gsnt.next;
	while (gsnt != NULL)
	  {
	     Group_Score_Name_Type *next = gsnt->next;
	     SLFREE (gsnt);
	     gsnt = next;
	  }
	
	while (pst != NULL)
	  {
	     PScore_Type *pnext = pst->next;
	     
	     free_psrt (pst->pregexp_list);
	     slrn_free ((char *) pst->description);
	     SLFREE (pst);
	     
	     pst = pnext;
	  }
	
	SLFREE (Group_Score_Root);
	Group_Score_Root = gnext;
     }
}

static int has_score_expired (unsigned char *s, unsigned long today)
{
   unsigned long mm, dd, yyyy;
   unsigned long score_time;
   
   s = (unsigned char *) slrn_skip_whitespace ((char *) s);
   if (*s == 0) return 0;
   
   if (((3 != sscanf ((char *) s, "%lu/%lu/%lu", &mm, &dd, &yyyy))
	&& (3 != sscanf ((char *) s, "%lu-%lu-%lu", &dd, &mm, &yyyy)))
       || (dd > 31)
       || (mm > 12)
       || (yyyy < 1900))
     return -1;
   
   score_time = (yyyy - 1900) * 10000 + (mm - 1) * 100 + dd;
   if (score_time > today) return 0;
   return 1;
}

static unsigned long get_today (void)
{
   unsigned long mm, yy, dd;
   time_t tloc;
   struct tm *tm_struct;
   
   time (&tloc);
   tm_struct = localtime (&tloc);
   yy = tm_struct->tm_year;
   mm = tm_struct->tm_mon;
   dd = tm_struct->tm_mday;
   
   return yy * 10000 + mm * 100 + dd;
}

typedef struct 
{
   unsigned char group[256];
   unsigned long today;
   int score;
   int start_new_group;
   int gnt_not_flag;
   int start_new_score;
   int score_has_expired;
   unsigned int pscore_flags;
   PScore_Type *pst;
   SLPreprocess_Type pt;
}
Score_Context_Type;

static int read_score_file_internal (char *, Score_Context_Type *);

static int handle_include_line (char *file, char *line, Score_Context_Type *c)
{
   char *dir, *f;
   char buf [SLRN_MAX_PATH_LEN];
#ifdef __CYGWIN__
   char convline [SLRN_MAX_PATH_LEN];
#endif
   int status;

   line = slrn_skip_whitespace (line);
   
   if (*line == 0)
     return -1;
   
   (void) slrn_trim_string (line);

#ifdef __CYGWIN__
   if (0 == slrn_cygwin_convert_path (line, convline, sizeof (convline)))
     line = convline;
#else
# if defined(IBMPC_SYSTEM)
   slrn_os2_convert_path (line);
# endif
#endif

   /* Make a filename which is relative to dir of current file.
    * Note that slrn_dircat will properly handle absolute filenames.
    */
   
   f = slrn_basename (file);
   if (NULL == (dir = slrn_strnmalloc (file, (unsigned int) (f - file), 1)))
     return -1;

   if (-1 == slrn_dircat (dir, line, buf, sizeof (buf)))
     {
	slrn_free (dir);
	return -1;
     }
   
   status = read_score_file_internal (buf, c);
   slrn_free (dir);
   return status;
}

static int phrase_score_file (char *file, FILE *fp, Score_Context_Type *c,
			      unsigned int *linenum,
			      PScore_Regexp_Type *sub_psrt)
{
    char line[1024];
    PScore_Regexp_Type *psrt;

    char *description = NULL;
    
    while (fgets (line, sizeof (line) - 1, fp))
     {
	unsigned char *lp;
	int ret, not_flag;
	
	(*linenum)++;

	psrt = NULL;
	
	if (0 == SLprep_line_ok (line, &c->pt))
	  continue;

	lp = (unsigned char *) slrn_skip_whitespace (line);
	if ((*lp == '#') || (*lp == '%') || (*lp <= ' ')) continue;

	if (sub_psrt == NULL)
	  {
	     if ((0 == slrn_case_strncmp (lp, (unsigned char *) "include", 7))
		 && ((lp[7] == ' ') || (lp[7] == '\t')))
	       {
		  if (0 == handle_include_line (file, (char *)lp + 7, c))
		    continue;
		  
		  score_error (_("Error handling INCLUDE line"), line, *linenum,
			       file);
		  slrn_free (description);
		  return -1;
	       }
	     
	     if (*lp == '[')
	       {
		  unsigned char *g, *gmax, ch;
		  g = c->group;
		  gmax = g + sizeof (c->group);
		  
		  lp++;
		  lp = (unsigned char *) slrn_skip_whitespace ((char *)lp);
		  
		  c->gnt_not_flag = 0;
		  if (*lp == '~')
		    {
		       c->gnt_not_flag = 1;
		       lp = (unsigned char *) slrn_skip_whitespace ((char *)lp + 1);
		    }
		  
		  while (((ch = *lp++) != 0)
			 && (ch != ']') && (g < gmax))
		    *g++ = ch;
		  
		  if ((ch != ']') || (g == gmax))
		    {
		       score_error (_("Syntax Error."), line, *linenum, file);
		       slrn_free (description);
		       return -1;
		    }
		  *g = 0;
		  c->start_new_group = 1;
		  c->score_has_expired = 0;
		  c->start_new_score = 0;
		  continue;
	       }
        
	     if (!slrn_case_strncmp (lp, (unsigned char *)"Score:", 6))
	       {
		  unsigned char *lpp = lp + 6, *eol;
		  c->pscore_flags = 0;
		  if (*lpp == ':')
		    {
		       lpp++;
		       c->pscore_flags |= SCORE_IS_OR_TYPE;
		    }
		  lpp = (unsigned char *) slrn_skip_whitespace ((char *) lpp);
		  if (*lpp == '=')
		    {
		       c->pscore_flags |= RETURN_THIS_SCORE;
		       lpp++;
		    }
		  c->score = atoi ((char *)lpp);
		  c->start_new_score = 1;

                  if (description != NULL)
		    {
		       slrn_free (description);
		       description = NULL;
		    }
                  eol = (unsigned char *) slrn_strchr ((char *)lpp, '#');
		  if (eol == NULL)
		    lpp = (unsigned char *) slrn_strchr ((char *)lpp, '%');
		  else lpp = eol;
                  if (lpp != NULL)
		    {
		       lpp = (unsigned char *) slrn_skip_whitespace ((char *)lpp + 1);
		       eol = lpp + strlen ((char*)lpp) - 1;
		       while ((eol >= lpp) && (*eol <= 32))
			 *eol-- = '\0';
		       description = slrn_safe_strmalloc ((char *)lpp);
		    }

		  continue;
	       }
	     
	     if (c->start_new_score)
	       {
		  if (!slrn_case_strncmp (lp, (unsigned char *) "Expires:", 8))
		    {
		       ret = has_score_expired (lp + 8, c->today);
		       if (ret == -1)
			 {
			    score_error (_("Expecting 'Expires: MM/DD/YYYY' or 'Expires: DD-MM-YYYY'"),
					 line, *linenum, file);
			    slrn_free (description);
			    return -1;
			 }
		       if (ret)
			 {
			    slrn_message (_("%s has expired score on line %d"),
					  file, *linenum);
#ifndef SLRNPULL_CODE
			    Slrn_Saw_Warning = 1;
#endif
			    c->start_new_score = 0;
			    c->score_has_expired = 1;
			 }
		       else c->score_has_expired = 0;
		       continue;
		    }
		  
		  if (NULL == (c->pst = create_new_score (c->group, c->start_new_group, c->gnt_not_flag,
							  c->score, c->pscore_flags,
                                                          file, *linenum, description)))
		    {
		       score_error (_("Bad group regular expression."), line,
				    *linenum, file);
		       slrn_free (description);
		       return -1;
		    }
		  c->start_new_group = 0;
		  c->start_new_score = 0;
		  c->score_has_expired = 0;

                  if (description != NULL)
		    {
		       slrn_free (description);
		       description = NULL;
		    }
	       }
	     
	     if (c->score_has_expired) continue;
	     
	     if (c->pst == NULL)
	       {
		  score_error (_("Expecting Score keyword."), line, *linenum, file);
		  return -1;
	       }
	     
	  } /*if (sub_psrt==null) */
	
	slrn_free (description);
	
	if (*lp == '~')
	  {
	     not_flag = 1;
	     lp++;
	  }
	else not_flag = 0;

	ret = -1;
	psrt = (PScore_Regexp_Type *) slrn_safe_malloc (sizeof (PScore_Regexp_Type));
	/* Otherwise the line is a kill one */
	if (!slrn_case_strncmp (lp, (unsigned char *)"Subject:", 8))
	  ret = add_group_regexp (psrt, lp + 7, lp, SCORE_SUBJECT, not_flag);
	else if (!slrn_case_strncmp (lp, (unsigned char *)"From:", 5))
	  ret = add_group_regexp (psrt, lp + 4, lp, SCORE_FROM, not_flag);
	else if (!slrn_case_strncmp (lp, (unsigned char *)"Xref:", 5))
	  ret = add_group_regexp (psrt, lp + 4, lp, SCORE_XREF, not_flag);
	else if (!slrn_case_strncmp (lp, (unsigned char *)"Newsgroup:", 10))
	  ret = add_group_regexp (psrt, lp + 9, lp, SCORE_NEWSGROUP, not_flag);
	else if (!slrn_case_strncmp (lp, (unsigned char *)"References:", 11))
	  ret = add_group_regexp (psrt, lp + 10, lp, SCORE_REFERENCES, not_flag);
	else if (!slrn_case_strncmp (lp, (unsigned char *)"Lines:", 6))
	  ret = add_group_regexp (psrt, lp + 5, lp, SCORE_LINES, not_flag);
	else if (!slrn_case_strncmp (lp, (unsigned char *)"Date:", 5))
	  ret = add_group_regexp (psrt, lp + 4, lp, SCORE_DATE, not_flag);
	else if (!slrn_case_strncmp (lp, (unsigned char *)"Age:", 4))
	  ret = add_group_regexp (psrt, lp + 3, lp, SCORE_AGE, not_flag);
	else if (!slrn_case_strncmp (lp, (unsigned char *)"Bytes:", 6))
	  ret = add_group_regexp (psrt, lp + 5, lp, SCORE_BYTES, not_flag);
	else if (!slrn_case_strncmp (lp, (unsigned char *)"Has-Body:", 9))
	  ret = add_group_regexp (psrt, lp + 8, lp, SCORE_HAS_BODY, not_flag);
	else if (!slrn_case_strncmp (lp, (unsigned char *)"Message-Id:", 11))
	  ret = add_group_regexp (psrt, lp + 10, lp, SCORE_MESSAGE_ID, not_flag);
        else if (!slrn_case_strncmp (lp, (unsigned char *)"{:", 2))
	  {
            if (lp[2] ==':')
	      {
		ret = add_group_regexp (psrt, lp + 1, lp, SCORE_SUB_OR, not_flag);
	      }
	    else
	      {
		ret = add_group_regexp (psrt, lp + 1, lp, SCORE_SUB_AND, not_flag);
	      }
	    if (phrase_score_file(file, fp, c, linenum, psrt))  return -1;
          }
        else if (!slrn_case_strncmp (lp, (unsigned char *)"}", 1))
	  {
	     SLFREE (psrt);
	     if (sub_psrt != NULL)
	       return 0;
	     else
	       {
		  score_error (_("Missing COLON."), line, *linenum, file);
		  return -1;
	       }
	  }
	else
	  {
	     unsigned char *lpp = lp;
	     while (*lpp && (*lpp != ':')) lpp++;
	     if (*lpp != ':')
	       {
		  SLFREE (psrt);
		  score_error (_("Missing COLON."), line, *linenum, file);
		  return -1;
	       }
	     
	     ret = add_group_regexp (psrt, lpp, lp, SCORE_GENERIC, not_flag);
	  }
	
	if (ret == -1)
	  {
	     score_error (_("No regular expression given."), line, *linenum, file);
	     return -1;
	  }
	
	if (sub_psrt == NULL)
	  {
	     if (c->pst->pregexp_list == NULL)
	       {
		  c->pst->pregexp_list = psrt;
	       }
	     else
	       {
		  PScore_Regexp_Type *last = c->pst->pregexp_list;
		  while (last->next != NULL) last = last->next;
		  last->next = psrt;
	       }
	  }
	else
	  {
	     if (sub_psrt->ireg.psrt == NULL)
	       {
		  sub_psrt->ireg.psrt = psrt;
	       }
	     else
	       {
		  PScore_Regexp_Type *last = sub_psrt->ireg.psrt;
		  while (last->next != NULL) last = last->next;
		  last->next = psrt;
	       }
	  }
     } /*while (fgets (line, sizeof (line) - 1, fp))*/

   if (sub_psrt == NULL) return 0;
   else
     {
	score_error (_("Missing '}' for  subscore"), line, *linenum, file);
	return -1;
     }
}

static int read_score_file_internal (char *file, Score_Context_Type *c)
{
   FILE *fp;
   Scorefile_Name_Type *files = Scorefile_Names, *thisfile;
   unsigned int linenum = 0;
   int ret;
   
   while (files != NULL)
     {
	if (!strcmp (files->filename, file))
	  {
	     slrn_error (_("*** Warning * Read file %s before - skipping ..."), file);
	     return 0;
	  }
	files = files->next;
     }
   
   thisfile = (Scorefile_Name_Type *)
     slrn_safe_malloc (sizeof (Scorefile_Name_Type));
   thisfile->filename = slrn_safe_strmalloc (file);
   thisfile->next = Scorefile_Names;
   Scorefile_Names = thisfile;

   if (NULL == (fp = fopen (file, "r")))
     {
	slrn_error (_("Unable to open score file %s"), file);
	return -1;
     }

   ret = phrase_score_file((char*)thisfile->filename, fp, c, &linenum, NULL);
   fclose (fp);
   return ret;
}

int slrn_read_score_file (char *file)
{
   Score_Context_Type sc;
   int status;

   if (Group_Score_Root != NULL)
     {
	free_group_scores ();
     }
   free_group_chain ();
   while (Scorefile_Names != NULL)
     {
	Scorefile_Name_Type *next = Scorefile_Names->next;
	slrn_free ((char *) Scorefile_Names->filename);
	slrn_free ((char *) Scorefile_Names);
	Scorefile_Names = next;
     }

   sc.today = get_today ();
   sc.score = 0;
   sc.start_new_group = 1;
   sc.gnt_not_flag = 0;
   sc.start_new_score = 0;
   sc.score_has_expired = 0;
   sc.pscore_flags = 0;
   sc.pst = NULL;
   sc.group[0] = '*';
   sc.group[1] = 0;

   (void) SLprep_open_prep (&sc.pt);
   status = read_score_file_internal (file, &sc);
   SLprep_close_prep (&sc.pt);
   
   if (status == -1)
     Slrn_Apply_Score = 0;
   else
     Slrn_Apply_Score = 1;

   return status;
}
