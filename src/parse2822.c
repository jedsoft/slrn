/* -*- mode: C; mode: fold -*- */
/*
 This file is part of SLRN.

 Copyright (c) 2009-2016 John E. Davis <jed@jedsoft.org>

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

#include <stdio.h>
#include <string.h>

#include "config.h"
#ifndef SLRNPULL_CODE
#include "slrnfeat.h"
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include "parse2822.h"
#include "strutil.h"

/* The grammar from rfc2822 is:
 *
 *   address        =  mailbox | group
 *   mailbox        =  name-addr | addr-spec
 *   name-addr      =  [display-name] angle-addr
 *   angle-addr     =  [CFWS] "<" addr-spec ">" CFWS  | obs-angle-addr
 *   group          =  display-name ":" [mailbox-list | CFWS ] ";"
 *   display-name   =  phrase
 *   mailbox-list   =  (mailbox *("," mailbox)) | obs-mbox-list
 *   addr-spec      =  local-part "@" domain
 *   local-part     =  dot-atom | quoted-string | obs-local-part
 *   domain         =  dot-atom | domain-literal | obs-domain
 *   domain-literal =  [CFWS] "[" *([FWS] dcontent) [FWS] "]" [CFWS]
 *   dcontent       =  dtext | quoted-pair
 *   dtext          =  non-white-space-ctrl | 33-90 | 94-126
 *   dot-atom       =  [CFWS] dot-atom-text [CFWS]
 *   dot-atom-text  =  1*atext *("." 1*atext)
 *   atom           =  [CFWS] 1*atext [CFWS]
 *   atext          =  ascii except controls, space, and specials
 *   quoted-string  =  [CFWS] DQUOTE *([FWS qcontent) [FWS] DQUOTE [CFWS]
 *   qcontent       =  qtext | quoted-pair
 *   qtext          =  NO-WS-CTRL | 33-126 except \ and "
 *   FWS            =  ([*WSP CRLF] 1*WSP) | obs-FWS
 *   WSP            =  SPACE | TAB
 *   CFWS           =  *([FWS] comment) (([FWS] comment) | FWS)
 *   comment        =  "("  *([FWS] ccontent) [FWS] ")"
 *   ccontent       =  ctext | quoted-pair | comment
 *   ctext          =  NO-WS-CTRL | 33-39 | 42-91 | 93-126
 *   quoted-pair    =  ("\" text )
 *   text           =  ASCII except CR and LF
 *
 * Obsolete:
 *
 *   word           = atom | quoted-string
 *   phrase         = 1*word | obs-phrase
 *   obs-phrase     = word *(word | "." | CFWS)
 *   obs-local-part = word *("." word)
 *
 */
#define TYPE_ADD_ONLY 1
#define TYPE_OLD_STYLE 2
#define TYPE_RFC_2822 3

#define RFC_2822_SPECIAL_CHARS "()<>[]:;@\\,.\""
#define RFC_2822_NOT_ATOM_CHARS RFC_2822_SPECIAL_CHARS
#define RFC_2822_NOT_DOTATOM_CHARS "(),:;<>@[\\]\""
#define RFC_2822_NOT_QUOTED_CHARS "\t\\\""
#define RFC_2822_NOT_DOMLIT_CHARS "[\\]"
#define RFC_2822_NOT_COMMENT_CHARS "(\\)"

#define IS_RFC2822_SPECIAL(ch) \
   (NULL != slrn_strbyte(RFC_2822_SPECIAL_CHARS, ch))
#define IS_RFC2822_ATEXT(ch) \
   (((unsigned char)(ch) > 32) && !IS_RFC2822_SPECIAL(ch))
#define IS_RFC2822_PHRASE_CHAR(ch) \
   (((ch) == ' ') || ((ch) == '\t') || ((ch) == '.') || IS_RFC2822_ATEXT(ch))

static int check_quoted_pair (char *p, char *pmax, char **errmsg)
{
   char ch;

   if (p == pmax)
     {
	*errmsg = _("Expecting a quoted-pair in the header.");
	return -1;
     }

   ch = *p;
   if ((ch == '\r') || (ch == '\n'))
     {
	*errmsg = _("Illegal quoted-pair character in header.");
	return -1;
     }

   return 0;
}

static char *skip_quoted_string (char *p, char *pmax, char **errmsg)
{
   while (p < pmax)
     {
	char ch = *p++;

	if (ch == '"')
	  return p;

	if (ch == '\\')
	  {
	     if (-1 == check_quoted_pair (p, pmax, errmsg))
	       return NULL;

	     p++;
	     continue;
	  }

	if (NULL != slrn_strbyte(RFC_2822_NOT_QUOTED_CHARS, ch))
	  {
	     *errmsg = _("Illegal char in displayname of address header.");
	     return NULL;
	  }
     }

   *errmsg = _("Quoted string opened but never closed in address header.");
   return NULL;
}

/* This function gets called with *startp positioned to the character past the
 * opening '('.  Find the matching ')' and encode everything in between.
 */
static int parse_rfc2822_comment (char *header, char *parsemap, unsigned int *startp, unsigned int stop, char **errmsg) /*{{{*/
{
   unsigned int start;

   start = *startp;
   while (start < stop)
     {
	unsigned char ch = (unsigned char) header[start];

	if (ch == '(')
	  {
	     start++;
	     if (-1 == parse_rfc2822_comment (header, parsemap, &start, stop, errmsg))
	       {
		  *startp = start;
		  return -1;
	       }
	     continue;
	  }

	if (ch == ')')
	  {
	     start++;
	     *startp = start;
	     return 0;
	  }

	if (ch == '\\')
	  {
	     parsemap[start] = 'C';
	     start++;
	     if (-1 == check_quoted_pair (header+start, header+stop, errmsg))
	       {
		  *startp = start;
		  return -1;
	       }
	     parsemap[start] = 'C';
	     start++;
	     continue;
	  }

	if (NULL != slrn_strbyte(RFC_2822_NOT_COMMENT_CHARS, ch))
	  {
	     *errmsg = _("Illegal char in displayname of address header.");
	     *startp = start;
	     return -1;
	  }
	parsemap[start] = 'C';
	start++;
     }

   *errmsg = _("Comment opened but never closed in address header.");
   return -1;
}

/*}}}*/

static int parse_rfc2822_cfws (char *header, char *parsemap, unsigned int *startp, unsigned int stop, char **errmsg)
{
   while (1)
     {
	char *p0, *p, *pmax;

	p = p0 = header + *startp;
	pmax = header + stop;

	while ((p < pmax) && ((*p == ' ') || (*p == '\t')))
	  p++;

	*startp = (unsigned int) (p - header);
	if ((p == pmax) || (*p != '('))
	  return 0;

	*startp += 1;		       /* skip ( */
	if (-1 == parse_rfc2822_comment (header, parsemap, startp, stop, errmsg))
	  return -1;
     }
}

static int parse_rfc2822_atext (char *header, char *parsemap, unsigned int *startp, unsigned int stop, char **errmsg)
{
   unsigned int start;
   char ch;

   (void) parsemap;
#define IS_NOT_ATEXT_CHAR(ch) \
   (((ch) & 0x80) || ((unsigned char)(ch) <= ' ') || ((ch) == '.') \
       || (NULL != slrn_strbyte(RFC_2822_NOT_DOTATOM_CHARS, (ch))))

   start = *startp;
   if (start >= stop)
     {
	*errmsg = _("premature end of parse seen in atext portion of email address");
	return -1;
     }

   ch = header[start];
   if (0 == IS_RFC2822_ATEXT(ch))
     {
	*errmsg = _("Expecting an atext character");
	return -1;
     }
   start++;
   while (start < stop)
     {
	ch = header[start];
	if (0 == IS_RFC2822_ATEXT(ch))
	  break;
	start++;
     }
   *startp = start;
   return 0;
}

/* The assumption here is that the *startp is at the char following the '"' */
static int parse_rfc2822_quoted_string (char *header, char *parsemap, unsigned int *startp, unsigned int stop, char **errmsg)
{
   char *p, *pmax;

   p = header + *startp;
   pmax = header + stop;

   p = skip_quoted_string (p, pmax, errmsg);
   if (p == NULL)
     return -1;

   *startp = (p - header);

   return parse_rfc2822_cfws (header, parsemap, startp, stop, errmsg);
}

/* dotatom: [CFWS] atext [.atext ...] [CFWS]
 * Note that the obsolete forms allow CFWS on both sides of the dot.
 * Moreover, it allows quoted-strings between the dots.
 * This is also permitted here:
 *   [CFWS] atext [[CFWS] "." [CFWS] atext...] [CFWS]
 */
static int parse_rfc2822_dotatom (char *header, char *parsemap, unsigned int *startp, unsigned int stop, char **errmsg, int allow_quoted_string)
{
   if (-1 == parse_rfc2822_cfws (header, parsemap, startp, stop, errmsg))
     return -1;

   if (allow_quoted_string
       && ((*startp < stop) && (header[*startp] == '"')))
     {
	*startp += 1;
	if (-1 == parse_rfc2822_quoted_string (header, parsemap, startp, stop, errmsg))
	  return -1;
     }
   else if (-1 == parse_rfc2822_atext (header, parsemap, startp, stop, errmsg))
     return -1;

   while (*startp < stop)
     {
	if (-1 == parse_rfc2822_cfws (header, parsemap, startp, stop, errmsg))
	  return -1;

	if (header[*startp] != '.')
	  break;

	*startp += 1;

	if (-1 == parse_rfc2822_cfws (header, parsemap, startp, stop, errmsg))
	  return -1;

	if (allow_quoted_string
	    && ((*startp < stop) && (header[*startp] == '"')))
	  {
	     *startp += 1;
	     if (-1 == parse_rfc2822_quoted_string (header, parsemap, startp, stop, errmsg))
	       return -1;
	  }
	else if (-1 == parse_rfc2822_atext (header, parsemap, startp, stop, errmsg))
	  return -1;
     }

   if (-1 == parse_rfc2822_cfws (header, parsemap, startp, stop, errmsg))
     return -1;

   return 0;
}

/* This parses a string that looks like "some phrase <address>".  Stop
 * parsing at the start of <address>.
 *
 * An RFC-2822 phrase consists of "words", which are composed of
 * atoms or quoted strings, or comments, and optionally separated by dots.
 */
static int parse_rfc2822_phrase (char *header, char *parsemap, unsigned int *startp, unsigned int stop, char **errmsg) /*{{{*/
{
   unsigned int start;

   start = *startp;
   while (start < stop)
     {
	unsigned char ch = (unsigned char) header[start];

	if (ch <= 32)
	  {
	     if ((ch == '\r') || (ch == '\n'))
	       {
		  *errmsg = _("Illegal char in displayname of address header.");
		  return -1;
	       }
	     parsemap[start] = 'C';
	     start++;
	     continue;
	  }

	if (ch == '(')
	  {
	     start++;
	     if (-1 == parse_rfc2822_comment (header, parsemap, &start, stop, errmsg))
	       {
		  *startp = start;
		  return -1;
	       }
	     continue;
	  }

	if (ch == '"')
	  {
	     unsigned int start0;
	     start++;
	     start0 = start;
	     if (-1 == parse_rfc2822_quoted_string (header, parsemap, &start, stop, errmsg))
	       {
		  *startp = start;
		  return -1;
	       }

	     while (start0 < start)
	       parsemap[start0++] = 'C';

	     continue;
	  }

	if (!IS_RFC2822_PHRASE_CHAR(ch))
	  break;

	parsemap[start] = 'C';
	start++;
     }

   *startp = start;
   return 0;
}

/*}}}*/

/* The grammar implies:
 *   local-part = dot-atom | quoted-string | obs-local-part
 * Note that the obsolete local part is like the dot-atom, except it
 * permits CFWS to surround the ".".
 */
static int parse_rfc2822_localpart (char *header, char *parsemap, unsigned int *startp, unsigned int stop, char **errmsg) /*{{{*/
{
   return parse_rfc2822_dotatom (header, parsemap, startp, stop, errmsg, 1);
}

/*}}}*/

static int parse_rfc2822_domain (char *header, char *parsemap, unsigned int *startp, unsigned int stop, char **errmsg) /*{{{*/
{
   /* Here domain is a dot atom or an obsolete local part. */
   if (-1 == parse_rfc2822_dotatom (header, parsemap, startp, stop, errmsg, 0))
     return -1;

   return 0;
}

/*}}}*/

static int parse_rfc2822_domainlit (char *header, char *parsemap, unsigned int *start, unsigned int end, char **errmsg) /*{{{*/
{
   unsigned int pos = *start;

   (void) parsemap;

   while (pos < end)
     {
#ifndef HAVE_LIBIDN
	if (header[pos] & 0x80)
	  {
	     *errmsg = _("Non 7-bit char in domain of address header. libidn is not yet supported.");
	     return -1;
	  }
#endif
	if (header[pos] == ']')
	  {
	     *start=pos;
	     return 0;
	  }
	if (NULL != slrn_strbyte (RFC_2822_NOT_DOMLIT_CHARS, header[pos]))
	  {
	     *errmsg = _("Illegal char in domain-literal of address header.");
	     return -1;
	  }
	pos++;
     }
   *errmsg = _("domain-literal opened but never closed.");
   return -1;
}

/*}}}*/

/* The encodes a comma separated list of addresses.  Each item in the list
 * is assumed to be of the following forms:
 *
 *    address (Comment-text)
 *    address (Comment-text)
 *    Comment-text <address>
 *
 * Here address is local@domain, local@[domain], or local.
 *
 * Here is an example of something that is permitted:
 *
 * From: Pete(A wonderful \) chap) <pete(his account)@silly.test(his host)>
 * To:A Group(Some people)
 *    :Chris Jones <c@(Chris's host.)public.example>,
 *        joe@example.org,
 *   John <jdoe@one.test> (my dear friend); (the end of the group)
 * Cc:(Empty list)(start)Undisclosed recipients  :(nobody(that I know))  ;
 *
 * The example shows that the "local" part can contain comments, and that
 * the backquote serves as a quote character in the comments.
 */
static int rfc2822_parse (char *header, char *parsemap, int skip_colon, char **errmsg) /*{{{*/
{
   unsigned int head_start=0, head_end;
   int type=0;
   unsigned int pos=0;
   char ch;

   if (skip_colon)
     {
	while ((0 != (ch = header[head_start]))
	       && (ch != ':'))
	  head_start++;

	if (ch != ':')
	  {
	     *errmsg = _("A colon is missing from the address header");
	     return -1;
	  }
	head_start++;		       /* skip colon */
     }

   while (1)
     {
	int in_comment, in_quote;

	/* skip past leading whitespace */
	while (1)
	  {
	     ch = header[head_start];
	     if (ch == 0)
	       return 0;

	     if ((ch != ' ') && (ch != '\t'))
	       break;

	     head_start++;
	  }

	/* If multiple addresses are given, split at ',' */
	head_end=head_start;
	in_quote=0;
	in_comment = 0;
	type=TYPE_ADD_ONLY;

	/* Loop until end of string is reached, or a ',' found */
	while (1)
	  {
	     ch = header[head_end];
	     if (ch == 0)
	       break;

	     head_end++;

	     if (in_quote)
	       {
		  if (ch == '"')
		    {
		       in_quote = !in_quote;
		       continue;
		    }

		  if (ch == '\\')
		    {
		       ch = header[head_end];
		       if ((ch == 0) || (ch == '\r'))
			 {
			    *errmsg = _("Illegal quoted character in address header.");
			    return -1;
			 }
		       head_end++;
		       continue;
		    }
		  continue;
	       }

	     if (in_comment)
	       {
		  if (ch == '(')
		    {
		       in_comment++;
		       continue;
		    }
		  if (ch == ')')
		    {
		       in_comment--;
		       continue;
		    }
		  if (ch == '\\')
		    {
		       ch = header[head_end];
		       if ((ch == 0) || (ch == '\r'))
			 {
			    *errmsg = _("Illegal quoted character in address header.");
			    return -1;
			 }
		       head_end++;
		       continue;
		    }
		  continue;
	       }

	     if (ch == '"')
	       {
		  in_quote = 1;
		  continue;
	       }

	     if (ch == '(')
	       {
		  in_comment++;
		  continue;
	       }

	     if (ch == '<')
	       {
		  type = TYPE_RFC_2822;
		  continue;
	       }

	     if (ch == ',')
	       {
		  head_end--;
		  break;
	       }
	  }

	if (in_quote)
	  {
	     *errmsg = _("Quote opened but never closed in address header.");
	     return -1;
	  }

	if (in_comment)
	  {
	     *errmsg = _("Comment opened but never closed in address header.");
	     return -1;
	  }

	pos=head_start;
	if (type == TYPE_RFC_2822)     /* foo <bar> */
	  {
	     if (header[pos] != '<')
	       {
		  /* phrase <bar> */
		  if (-1 == parse_rfc2822_phrase (header, parsemap, &pos, head_end, errmsg))
		    return -1;
	       }
	     /* at this point, pos should be at '<' */
	     if (header[pos] != '<')
	       {
		  *errmsg = _("Address appears to have a misplaced '<'.");
		  return -1;
	       }
	     pos++;
	  }
	if (-1 == parse_rfc2822_localpart (header, parsemap, &pos, head_end, errmsg))
	  return -1;

	if (header[pos] == '@')
	  {
	     pos++;
	     if (header[pos] == '[')
	       {
		  pos++;
		  if (-1 == parse_rfc2822_domainlit (header, parsemap, &pos, head_end, errmsg))
		    return -1;
		  pos++;	       /* skip ']' */
	       }
	     else
	       {
		  if (-1 == parse_rfc2822_domain (header, parsemap, &pos, head_end, errmsg))
		    return -1;
	       }
	  }

	if (type == TYPE_RFC_2822)
	  {
	     if (header[pos] != '>')
	       {
		  *errmsg = _("Expected closing '>' character in the address");
		  return -1;
	       }
	     pos++;
	  }

	/* after domainpart only (folding) Whitespace and comments are allowed*/
	if (-1 == parse_rfc2822_cfws (header, parsemap, &pos, head_end, errmsg))
	  return -1;

	if (pos != head_end)
	  {
	     *errmsg = _("Junk found at the end of email-address");
	     /* fprintf (stderr, "BAD: %s\n", header); fprintf (stderr, "END: %s\n", header+pos); */
	     return -1;
	  }
	if (header[head_end] == 0)
	  return 0;

	/* head_end should be at ',', so skip over it. */
	head_start=head_end+1;
     }
}
/*}}}*/

/*}}}*/

/* This function takes a header of the form "KEY: VALUE" and parses it
 * according to rfc2822.   It returns a string that contains information where
 * the header may be encoded.  For example, if the header is:
 *
 *    "From: Thomas Paine <thomas@unknown.isp>"
 *
 * Then the following string will be returned:
 *
 *    "      CCCCCCCCCCCCC                    "
 *
 * The Cs indicate that the corresponding areas of the header may be encoded.
 * If an error occurs, NULL will be returned and *errmsg will be set to a string
 * describing the error.
 */
char *slrn_parse_rfc2822_addr (char *header, char **errmsg)
{
   char *encodemap;
   unsigned int len;

   *errmsg = NULL;

   len = strlen (header);
   if (NULL == (encodemap = slrn_malloc (len+1, 0, 1)))
     {
	*errmsg = _("Out of memory");
	return NULL;
     }

   memset (encodemap, ' ', len);
   encodemap[len] = 0;

   if (-1 == rfc2822_parse (header, encodemap, 0, errmsg))
     {
	if (*errmsg == NULL)
	  *errmsg = _("Error encountered while parsing an RFC2822 header");
	slrn_free (encodemap);
	return NULL;
     }

   return encodemap;
}
