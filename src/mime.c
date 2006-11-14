/* -*- mode: C; mode: fold -*- */
/* MIME handling routines.
 *
 * Author: Michael Elkins <elkins@aero.org>
 * Modified by John E. Davis <davis@space.mit.edu>
 * Modified by Thomas Schultz <tststs@gmx.de>
 * 
 * Change Log:
 * Aug 20, 1997 patch from "Byrial Jensen" <byrial@post3.tele.dk>
 *   added.  Apparantly RFC2047 requires the whitespace separating
 *   multiple encoded words in headers to be ignored.
 *   Status: unchecked
 */

#include "config.h"
#ifndef SLRNPULL_CODE
#include "slrnfeat.h"
#endif

#include <stdio.h>
#include <string.h>


#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <ctype.h>

#if defined(__os2__) || defined(__NT__)
# include <process.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include "slrn.h"
#include "misc.h"
#include "slrn.h"
#include "group.h"
#include "art.h"
#include "util.h"
#include "server.h"
#include "snprintf.h"
#include "mime.h"
#include "charset.h"

int Slrn_Use_Meta_Mail = 1;
int Slrn_Fold_Headers = 1;
char *Slrn_MetaMail_Cmd;

#ifndef SLRNPULL_CODE
#define CONTENT_TYPE_TEXT		0x01
#define CONTENT_TYPE_MESSAGE		0x02
#define CONTENT_TYPE_MULTIPART		0x03
#define CONTENT_TYPE_UNSUPPORTED	0x10

#define CONTENT_SUBTYPE_PLAIN		0x01
#define CONTENT_SUBTYPE_UNKNOWN		0x02
#define CONTENT_SUBTYPE_UNSUPPORTED	0x10


static Slrn_Article_Line_Type *find_header_line (Slrn_Article_Type *a, char *header)/*{{{*/
{
   Slrn_Article_Line_Type *line;
   unsigned char ch = (unsigned char) UPPER_CASE(*header);
   unsigned int len = strlen (header);

   if (a == NULL)
     return NULL;
   line = a->lines;

   while ((line != NULL) && (line->flags & HEADER_LINE))
     {
	unsigned char ch1 = (unsigned char) *line->buf;
	if ((ch == UPPER_CASE(ch1))
	    && (0 == slrn_case_strncmp ((unsigned char *)header,
					(unsigned char *)line->buf,
					len)))
	  return line;
	line = line->next;
     }
   return NULL;
}
/*}}}*/
#endif /* NOT SLRNPULL_CODE */

#ifndef SLRNPULL_CODE
static int parse_content_type_line (Slrn_Article_Type *a)/*{{{*/
{
   char *b;
   Slrn_Article_Line_Type *line;
   
   if (a == NULL)
     return -1;
   line = a->lines;
   
   if (NULL == (line = find_header_line (a, "Content-Type:")))
     return 0;
   
   b = slrn_skip_whitespace (line->buf + 13);
   
   if (0 == slrn_case_strncmp ((unsigned char *)b,
			       (unsigned char *) "text/",
			       5))
     {
	a->mime.content_type = CONTENT_TYPE_TEXT;
	b += 5;
	if (0 != slrn_case_strncmp ((unsigned char *)b,
				    (unsigned char *) "plain",
				    5))
	  {
	     a->mime.content_subtype = CONTENT_SUBTYPE_UNSUPPORTED;
	     return -1;
	  }
	else
	  {
	     a->mime.content_subtype = CONTENT_SUBTYPE_PLAIN;
	  }
	b += 5;
     }
   else if (0 == slrn_case_strncmp ((unsigned char *)b,
				    (unsigned char *) "message/",
				    5))
     {
	a->mime.content_type = CONTENT_TYPE_MESSAGE;
	a->mime.content_subtype = CONTENT_SUBTYPE_UNKNOWN;
	b += 8;
     }
   else if (0 == slrn_case_strncmp ((unsigned char *)b,
				    (unsigned char *) "multipart/",
				    5))
     {
	a->mime.content_type = CONTENT_TYPE_MULTIPART;
	a->mime.content_subtype = CONTENT_SUBTYPE_UNKNOWN;
	b += 10;
     }
   else
     {
	a->mime.content_type = CONTENT_TYPE_UNSUPPORTED;
	return -1;
     }
   
   do
     {
	while (NULL != (b = slrn_strchr (b, ';')))
	  {
	     char *charset;
	     unsigned int len;
	     
	     b = slrn_skip_whitespace (b + 1);
	     
	     if (0 != slrn_case_strncmp ((unsigned char *)b,
					 (unsigned char *)"charset",
					 7))
	       continue;
	     
	     b = slrn_skip_whitespace (b + 7);
	     while (*b == 0)
	       {
		  line = line->next;
		  if ((line == NULL)
		      || ((line->flags & HEADER_LINE) == 0)
		      || ((*(b = line->buf) != ' ') && (*b == '\t')))
		    return -1;
		  b = slrn_skip_whitespace (b);
	       }
	     
	     if (*b != '=') continue;
	     b++;
	     if (*b == '"') b++;
	     charset = b;
	     while (*b && (*b != ';')
		    && (*b != ' ') && (*b != '\t') && (*b != '\n')
		    && (*b != '"'))
	       b++;
	     len = b - charset;
	     
	     a->mime.charset = slrn_safe_strnmalloc (charset, len);
	     return 0;
	  }
	line = line->next;
     }
   while ((line != NULL)
	  && (line->flags & HEADER_LINE)
	  && ((*(b = line->buf) == ' ') || (*b == '\t')));
   
   return 0;
}

/*}}}*/
#define ENCODED_RAW			0
#define ENCODED_7BIT			1
#define ENCODED_8BIT			2
#define ENCODED_QUOTED			3
#define ENCODED_BASE64			4
#define ENCODED_BINARY			5
#define ENCODED_UNSUPPORTED		6
static int parse_content_transfer_encoding_line (Slrn_Article_Type *a)/*{{{*/
{
   Slrn_Article_Line_Type *line;
   unsigned char *buf;
   
   if (a == NULL)
     return -1;

   line = find_header_line (a, "Content-Transfer-Encoding:");
   if (line == NULL) return ENCODED_RAW;

   buf = (unsigned char *) slrn_skip_whitespace (line->buf + 26);
   if (*buf == '"') buf++;
   
   if (0 == slrn_case_strncmp (buf, (unsigned char *) "7bit", 4))
	return ENCODED_7BIT;
   else if (0 == slrn_case_strncmp (buf, (unsigned char *) "8bit", 4))
	return ENCODED_8BIT;
   else if (0 == slrn_case_strncmp (buf, (unsigned char *) "base64", 6))
	return ENCODED_BASE64;
   else if (0 == slrn_case_strncmp (buf, (unsigned char *) "quoted-printable", 16))
	return ENCODED_QUOTED;
   else if (0 == slrn_case_strncmp (buf, (unsigned char *) "binary", 6))
	return ENCODED_BINARY;
   return ENCODED_UNSUPPORTED;
}

/*}}}*/
#endif /* NOT SLRNPULL_CODE*/

static int Index_Hex[128] =/*{{{*/
{
   -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,
     -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};

/*}}}*/
#define HEX(c) (Index_Hex[(unsigned char)(c) & 0x7F])

static char *decode_quoted_printable (char *dest,/*{{{*/
				      char *src, char *srcmax,
				      int treat_underscore_as_space,
				      int strip_8bit)
{
   char *allowed_in_qp = "0123456789ABCDEFabcdef";
   unsigned char ch, mask = 0x0;
/*
#ifndef SLRNPULL_CODE
   if (strip_8bit && (NULL == Char_Set))
     mask = 0x80;
#else
   (void) strip_8bit;
#endif*/
   while (src < srcmax)
     {
	ch = (unsigned char) *src++;
	if ((ch == '=') && (src + 1 < srcmax)
	    && (NULL != slrn_strchr (allowed_in_qp, src[0]))
	    && (NULL != slrn_strchr (allowed_in_qp, src[1])))
	  {
	     ch = (16 * HEX(src[0])) + HEX(src[1]);
	     if (ch & mask) ch = '?';
	     *dest++ = (char) ch;
	     src += 2;
	  }
	else if ((ch == '_') && treat_underscore_as_space)
	  {
	     *dest++ = ' ';
	  }
	else *dest++ = (char) ch;
     }
   return dest;
}

/*}}}*/

static int Index_64[128] =/*{{{*/
{
   -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
     -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,62, -1,-1,-1,63,
     52,53,54,55, 56,57,58,59, 60,61,-1,-1, -1,-1,-1,-1,
     -1, 0, 1, 2,  3, 4, 5, 6,  7, 8, 9,10, 11,12,13,14,
     15,16,17,18, 19,20,21,22, 23,24,25,-1, -1,-1,-1,-1,
     -1,26,27,28, 29,30,31,32, 33,34,35,36, 37,38,39,40,
     41,42,43,44, 45,46,47,48, 49,50,51,-1, -1,-1,-1,-1
};
/*}}}*/
#define BASE64(c) (Index_64[(unsigned char)(c) & 0x7F])

static char *decode_base64 (char *dest, char *src, char *srcmax) /*{{{*/
{
   while (src + 3 < srcmax)
     {
	*dest++ = (BASE64(src[0]) << 2) | (BASE64(src[1]) >> 4);
	
	if (src[2] == '=') break;
	*dest++ = ((BASE64(src[1]) & 0xf) << 4) | (BASE64(src[2]) >> 2);
	
	if (src[3] == '=') break;
	*dest++ = ((BASE64(src[2]) & 0x3) << 6) | BASE64(src[3]);
	src += 4;
     }
   return dest;
}

/*}}}*/

int slrn_rfc1522_decode_string (char **s_ptr)/*{{{*/
{
   char *s1, *s2, ch, *s;
   char *charset, method, *txt;
   char *after_last_encoded_word;
   char *after_whitespace;
   int offset;
   unsigned int count;
   unsigned int len;
   
   count = 0;
   after_whitespace = NULL;
   after_last_encoded_word = NULL;
   charset = NULL;
   s= *s_ptr;

   if (slrn_string_nonascii(s))
	return -1;
   
   while (1)
     {
	while ((NULL != (s = slrn_strchr (s, '=')))
	       && (s[1] != '?')) s++;
	if (s == NULL) break;
	
	s1 = s;
	offset= s - *s_ptr;

	charset = s = s1 + 2;
	while (((ch = *s) != 0)
	       && (ch != '?') && (ch != ' ') && (ch != '\t') && (ch != '\n'))
	  s++;
	
	if (ch != '?')
	  {
	     s = s1 + 2;
	     charset = NULL;
	     continue;
	  }
	
	charset = s1 + 2;
	len = s - charset;
	charset=slrn_strnmalloc (charset, len, 1);
	
	s++;			       /* skip ? */
	method = *s++;		       /* skip B,Q */
	/* works in utf8 mode and else */
	if (method == 'b') method = 'B';
	if (method == 'q') method = 'Q';

	if ((charset == NULL) || ((method != 'B') && (method != 'Q'))
	    || (*s != '?'))
	  {
	     s = s1 + 2;
	     slrn_free(charset);
	     charset = NULL;
	     continue;
	  }
	/* Now look for the final ?= after encoded test */
	s++;			       /* skip ? */
	txt = s;
       
	while ((ch = *s) != 0)
	  {
	     /* Appararantly some programs do not convert ' ' to '_' in 
	      * quoted printable.  Sigh.
	      */
	     if (((ch == ' ') && (method != 'Q'))
		 || (ch == '\t') || (ch == '\n'))
	       break;
	     if ((ch == '?') && (s[1] == '='))
	       break;
	     
	     s++;
	  }
	
	if ((ch != '?') || (s[1] != '='))
	  {
	     s = s1 + 2;
	     slrn_free(charset);
	     charset = NULL;
	     continue;
	  }

	if (s1 == after_whitespace)
	  s1 = after_last_encoded_word;

        /* Note: these functions return a pointer to the END of the decoded
	 * text.
	 */
	s2 = s1;
	
	if (method == 'B')
	  s1 = decode_base64 (s1, txt, s);
	else s1 = decode_quoted_printable (s1, txt, s, 1, 0);
	
	/* Now move everything over */
	s2 = s + 2;		       /* skip final ?= */
	s = s1;			       /* start from here next loop */
	while ((ch = *s2++) != 0) *s1++ = ch;
	*s1 = 0;
	
	count++;

	if (slrn_case_strncmp((unsigned char *)Slrn_Display_Charset,
			   (unsigned char *)charset,
			   (strlen(Slrn_Display_Charset) <= len) ? strlen(Slrn_Display_Charset) : len) != 0)
           {
	     /* We really need the position _after_ the decoded word, so we
	      * split the remainder of the string for charset conversion and
	      * put it back together afterward. */
	      char ch = *s;
	      *s = 0;
	      if ((s2 = slrn_convert_substring(*s_ptr, offset, 0, Slrn_Display_Charset, charset, 0)) != NULL)
		{
		   *s = ch;
		   s = slrn_strdup_strcat(s2,s,NULL);
		   slrn_free(*s_ptr);
		   *s_ptr = s;
		   s += strlen(s2);
		   slrn_free(s2);
		}
	      else
		{
		   *s = ch;
		   /* decoding fails */
		}
           }
	
	slrn_free(charset);
	charset=NULL;

	after_last_encoded_word = s;
	s = slrn_skip_whitespace (s);
	after_whitespace = s;
     }
   if (charset!=NULL)
	slrn_free(charset);
   return count;
}

/*}}}*/

#ifndef SLRNPULL_CODE /* rest of the file in this ifdef */

static void rfc1522_decode_headers (Slrn_Article_Type *a)/*{{{*/
{
   Slrn_Article_Line_Type *line;
   
   if (a == NULL)
     return;
   
   line = a->lines;

   while ((line != NULL) && (line->flags & HEADER_LINE))
     {
	if (slrn_case_strncmp ((unsigned char *)line->buf,
			       (unsigned char *)"Newsgroups:", 11) &&
	    slrn_case_strncmp ((unsigned char *)line->buf,
			       (unsigned char *)"Followup-To:", 12) &&
	    (slrn_rfc1522_decode_string (&line->buf) > 0))
	  {
	     a->is_modified = 1;
	     a->mime.was_modified = 1;
	  }
	line = line->next;
     }
}

/*}}}*/

static void decode_mime_base64 (Slrn_Article_Type *a)/*{{{*/
{
   Slrn_Article_Line_Type *l;
   Slrn_Article_Line_Type *body_start, *next;
   char *buf_src, *buf_dest, *buf_pos;
   char *base;
   int len;
   
   if (a == NULL) return;
   
   l = a->lines;
   
   /* skip header and separator */
   while ((l != NULL) && ((l->flags & HEADER_LINE) || l->buf[0] == '\0'))
     l = l->next;
   
   if (l == NULL) return;
   
   body_start = l;
   
   /* let's calculate how much space we need... */
   len = 0;
   while ( l )
     {
	len += strlen(l->buf);
	l = l->next;
     }
   
   /* get some memory */
   buf_src = slrn_safe_malloc (len + 1);
   buf_dest = slrn_safe_malloc (len + 1);
   
   /* collect all base64 encoded lines into buf_src */
   l = body_start;
   buf_pos = buf_src;
   while ( l )
     {
	strcat (buf_pos, l->buf); /* safe */
	buf_pos += strlen(l->buf);
	l = l->next;
     }
   
   /* put decoded article into buf_dest */
   buf_pos = decode_base64(buf_dest, buf_src, buf_src+len);
   *buf_pos = '\0';
   
   if (a->mime.charset == NULL)
     {
	buf_pos = buf_dest;
	while (*buf_pos)
	  {
	     if (*buf_pos & 0x80) *buf_pos = '?';
	     buf_pos++;
	  }
     }
   
   l = body_start;
   body_start = body_start->prev;
   
   /* free old body */
   while ( l )
     {
	slrn_free(l->buf);
	next = l->next;
	slrn_free((char *)l);
	l = next;
     }
   body_start->next = NULL;
   
   l = body_start;
   
   base = buf_dest;
   buf_pos = buf_dest;
   
   /* put decoded article back into article structure */
   while ( (buf_pos=strchr(buf_dest, '\n')) != NULL )
     {
	len = buf_pos - buf_dest;
	
	l->next = (Slrn_Article_Line_Type *)
	  slrn_malloc(sizeof(Slrn_Article_Line_Type), 1, 1);
	
	l->next->prev = l;
	l->next->next = NULL;
	l = l->next;
	l->buf = slrn_malloc(sizeof(char) * len + 1, 0, 1);
	
	strncpy(l->buf, buf_dest, len);
	/* terminate string and strip '\r' if necessary */
	if ( l->buf[len-1] == '\r' )
	  l->buf[len-1] = '\0';
	else
	  l->buf[len] = '\0';
	
	buf_dest = buf_pos + 1;
     }
   
   slrn_free(buf_src);
   slrn_free(base);
   
   a->is_modified = 1;
   a->mime.was_modified = 1;
}

/*}}}*/

/* This function checks if the last character on curr_line is an = and 
 * if it is, then it merges curr_line and curr_line->next. See RFC1341,
 * section 5.1 (Quoted-Printable Content-Transfer-Encoding) rule #5.
 * [csp@ohm.york.ac.uk]
 */
static int merge_if_soft_linebreak (Slrn_Article_Line_Type *curr_line)/*{{{*/
{
   Slrn_Article_Line_Type *next_line;
   char *b;

   while ((next_line = curr_line->next) != NULL)
     {
	unsigned int len;

	b = curr_line->buf;
	len = (unsigned int) (slrn_bskip_whitespace (b) - b);
	if (len == 0) return 0;
   
	len--;
	if (b[len] != '=') return 0;
	
	/* Remove the excess = character... */
	b[len] = '\0';
	
	if (NULL == (b = (char *) SLrealloc (b, 1 + len + strlen (next_line->buf))))
	  return -1;

	curr_line->buf = b;
	
	strcpy (b + len, next_line->buf); /* safe */
	
	/* Unlink next_line from the linked list of lines in the article... */
	curr_line->next = next_line->next;
	if (next_line->next != NULL)
	  next_line->next->prev = curr_line;
	
	SLFREE (next_line->buf);
	SLFREE (next_line);
     }

   /* In case the last line ends with a soft linebreak: */
   b = slrn_bskip_whitespace (curr_line->buf);
   if (b != curr_line->buf)
     {
	b--;
	if (*b == '=')
	  *b = 0;
     }
   
   return 0;
}

/*}}}*/

static void decode_mime_quoted_printable (Slrn_Article_Type *a)/*{{{*/
{
   Slrn_Article_Line_Type *line;
   
   if (a == NULL)
     return;
   
   line = a->lines;

   /* skip to body */
   while ((line != NULL) && (line->flags & HEADER_LINE))
     line = line->next;
   if (line == NULL) return;
   
   while (line != NULL)
     {
	char *b;
	unsigned int len;
	
	b = line->buf;
	len = (unsigned int) (slrn_bskip_whitespace (b) - b);
	if (len && (b[len - 1] == '=') 
	    && (line->next != NULL))
	  {
	     (void) merge_if_soft_linebreak (line);
	     b = line->buf;
	     len = strlen (b);
	  }

	b = decode_quoted_printable (b, b, b + len, 0, 1);
	if (b < line->buf + len)
	  {
	     *b = 0;
	     a->is_modified = 1;
	     a->mime.was_modified = 1;
	  }
	
	line = line->next;
     }
}

/*}}}*/

void slrn_mime_init (Slrn_Mime_Type *m)/*{{{*/
{
   m->was_modified = 0;
   m->was_parsed = 0;
   m->needs_metamail = 0;
   m->charset = NULL;
   m->content_type = 0;
   m->content_subtype = 0;
}

/*}}}*/

void slrn_mime_free (Slrn_Mime_Type *m)/*{{{*/
{
  if (m->charset != NULL)
    {
       slrn_free(m->charset);
    }
}

/*}}}*/

void slrn_malloc_mime_error(Slrn_Mime_Error_Obj **obj, /*{{{*/
	  char *msg, char *line, int lineno, int critical)
{
   Slrn_Mime_Error_Obj *err;
     
   if (*obj == NULL)
     {
	err = *obj = (Slrn_Mime_Error_Obj *)
	     slrn_malloc(sizeof(Slrn_Mime_Error_Obj),1,1);
     }
   else
     {
	err=(*obj)->next;
	(*obj)->next = (Slrn_Mime_Error_Obj *)
	     slrn_malloc(sizeof(Slrn_Mime_Error_Obj),1,1);
	(*obj)->next->prev=*obj;
	*obj = (*obj)->next;
	(*obj)->next=err;
	err=*obj;
     }
   err->msg = msg;
   if (line != NULL)
	err->err_str = slrn_safe_strmalloc(line);
   else
	err->err_str = NULL;
   err->lineno=lineno;
   err->critical=critical;
   err->next=NULL;
}

/*}}}*/

void slrn_free_mime_error(Slrn_Mime_Error_Obj *obj) /*{{{*/
{
   Slrn_Mime_Error_Obj *tmp;

   while (obj != NULL)
     {
	tmp = obj->next;
	if (obj->err_str != NULL)
	     slrn_free(obj->err_str);
	slrn_free((char *) obj);
	obj=tmp;
     }
}

/*}}}*/

int slrn_mime_process_article (Slrn_Article_Type *a)/*{{{*/
{
   if ((a == NULL) || (a->mime.was_parsed))
     return;

   a->mime.was_parsed = 1;	       /* or will be */
   
   rfc1522_decode_headers (a);

/* Is there a reason to use the following line? */
/*   if (NULL == find_header_line (a, "Mime-Version:")) return;*/
/*   if ((-1 == parse_content_type_line (a))
       || (-1 == parse_content_transfer_encoding_line (a)))*/
   if (-1 == parse_content_type_line (a))
     {
	a->mime.needs_metamail = 1;
	return 0;
     }
   
   switch (parse_content_transfer_encoding_line (a))
     {
      case ENCODED_RAW:
	/*return 0;*/
	/* Now falls through to the identity encoding. */

      case ENCODED_7BIT:
      case ENCODED_8BIT:
      case ENCODED_BINARY:
	/* Already done. */
	break;
	
      case ENCODED_BASE64:
	decode_mime_base64 (a);
	break;
	
      case ENCODED_QUOTED:
	decode_mime_quoted_printable (a);
	break;
	
      default:
	a->mime.needs_metamail = 1;
	return;
     }
   
   if ((a->mime.needs_metamail == 0) &&
	     (a->mime.charset == NULL))
     {
	a->mime.charset = slrn_safe_strmalloc("us-ascii");
	return 0;
     }
 
   if ((a->mime.needs_metamail == 0) &&
	(slrn_case_strncmp((unsigned char *)"us-ascii",
			   (unsigned char *)a->mime.charset,8) != 0) &&
	(slrn_case_strcmp((unsigned char *)Slrn_Display_Charset,
			   (unsigned char *)a->mime.charset) != 0))
     {
	return slrn_convert_article(a, Slrn_Display_Charset, a->mime.charset);
     }
   return 0;
}


/*}}}*/

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

int slrn_mime_call_metamail (void)/*{{{*/
{
#ifdef VMS
   return 0;
#else
   int init = Slrn_TT_Initialized;
   char tempfile [SLRN_MAX_PATH_LEN];
   Slrn_Article_Line_Type *ptr;
   FILE *fp;
   char *tmp, *mm, *cmd;
   
   if (NULL == Slrn_Current_Article)
     return -1;
   ptr = Slrn_Current_Article->lines;
     
   if ((Slrn_Use_Meta_Mail == 0)
       || Slrn_Batch
       || (slrn_get_yesno (1, _("Process this MIME article with metamail")) <= 0))
     return 0;

# if defined(__os2__) || defined(__NT__)
   if (NULL == (tmp = getenv ("TMP")))
     tmp = ".";
# else
   tmp = "/tmp";
# endif
   
   fp = slrn_open_tmpfile_in_dir (tmp, tempfile, sizeof (tempfile));

   if (fp == NULL)
     {
	slrn_error (_("Unable to open tmp file for metamail."));
	return 0;
     }
   
   while (ptr) 
     {
	fputs(ptr->buf, fp);
	putc('\n', fp);
	ptr = ptr->next;
     }
   slrn_fclose(fp);

   mm = Slrn_MetaMail_Cmd;
   
   if ((mm == NULL)
       || (*mm == 0)
       || (strlen (mm) > SLRN_MAX_PATH_LEN))
     mm = "metamail";

   cmd = slrn_strdup_strcat (mm, " ", tempfile, NULL);
   
   /* Make sure that metamail has a normal environment */
   slrn_set_display_state (0);
   
   slrn_posix_system(cmd, 0);
   slrn_delete_file (tempfile);
   slrn_free (cmd);
   
   printf(_("Press return to continue ..."));
   getchar();
   fflush(stdin); /* get rid of any pending input! */
   
   slrn_set_display_state (init);
   return 1;
#endif  /* NOT VMS */
}

/*}}}*/


/* -------------------------------------------------------------------------
 * MIME encoding routines.
 * -------------------------------------------------------------------------*/

/* expexts a->cline pointing to the last headerline, and the body in a->raw_lines */
Slrn_Mime_Error_Obj *slrn_mime_encode_article(Slrn_Article_Type *a, int *hibin, char *from_charset) /*{{{*/
{
  char *charset = Slrn_Outgoing_Charset;
  char *charset_end=NULL;
  Slrn_Mime_Error_Obj *err=NULL;
  Slrn_Article_Line_Type *endofheader, *rline=a->raw_lines;

  a->cline->next=(Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
  a->cline->next->prev=a->cline;
  a->cline=a->cline->next;
  a->cline->flags=HEADER_LINE;
  a->cline->buf=slrn_safe_strmalloc("Mime-Version: 1.0");
  endofheader = a->cline;
  
  if (*hibin == -1)
    {
       *hibin = 0;
       while(rline != NULL)
	 {
	    if (slrn_string_nonascii(rline->buf))
	      {
		 *hibin = 1;
		 break;
	      }
	    rline=rline->next;
	 }
       rline=a->raw_lines;
    }

  if (*hibin)
    {
       do
	 {
	    if ((charset_end=slrn_strchr(charset, ',')) != NULL)
	      {
		 *charset_end = '\0';
	      }
	  
	    if (slrn_case_strcmp(charset, from_charset) == 0)
	    /* No recoding needed */
	      {
		 while(rline != NULL)
		   {
		      a->cline->next=rline;
		      a->cline->next->prev=a->cline;
		      a->cline=a->cline->next;
		      rline=rline->next;
		   }
		 a->raw_lines=NULL;
		 break;
	      }

	    if ( slrn_test_convert_article(a, charset, from_charset)  == 0)
	      {
		 break;
	      }
	    if (charset_end != NULL)
	      {
		 *charset_end=',';
		 charset = charset_end + 1;
	      }
	 } while(charset_end != NULL);
       if (endofheader == a->cline)
	 {
	    /* if we get here, no encoding was possible*/
	    slrn_malloc_mime_error(&err, _("Can't determine suitable charset for body"), NULL, -1 , MIME_ERROR_CRIT);
	    return err;
	 }
    }
  else
    {
       a->cline->next=(Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
       a->cline->next->prev=a->cline;
       a->cline=a->cline->next;
       a->cline->flags=HEADER_LINE;
       a->cline->buf=slrn_safe_strmalloc("Content-Type: text/plain; charset=us-ascii");
       a->cline->next=(Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
       a->cline->next->prev=a->cline;
       a->cline=a->cline->next;
       a->cline->flags=HEADER_LINE;
       a->cline->buf=slrn_safe_strmalloc("Content-Transfer-Encoding: 7bit");

       while(rline != NULL)
	 {
	    a->cline->next=rline;
	    a->cline->next->prev=a->cline;
	    a->cline=a->cline->next;
	    rline=rline->next;
	 }
       a->raw_lines=NULL;
       return NULL;
    }
  /* if we get here, the posting contains 8bit chars */
  a->cline = endofheader;
  
  endofheader = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
  endofheader->flags=HEADER_LINE;
  endofheader->buf=slrn_strdup_printf ("Content-Type: text/plain; charset=%s", charset);
  
  endofheader->next = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
  endofheader->next->flags=HEADER_LINE;
  endofheader->next->buf=slrn_safe_strmalloc("Content-Transfer-Encoding: 8bit");
  endofheader->next->next = a->cline->next;
  a->cline->next->prev= endofheader->next;
  endofheader->prev = a->cline;
  a->cline->next = endofheader;
  
  if (charset_end != NULL) *charset_end=',';
  
  return NULL;
}

/*}}}*/

static Slrn_Mime_Error_Obj *fold_line (char **s_ptr)/*{{{*/
{
   int fold=0, pos=0, last_ws=0, linelen=0;
   char *s=*s_ptr;
   char *tmp, *ret;
   Slrn_Mime_Error_Obj *err=NULL;
   
   if (strlen(s) <= 78)
     {
	/* nothing to do */
	return NULL;
     }
	
   /* First step: counting. */
   
   /* skip the first word and all ws after it
    * (folding after the keyword is not allowed) */
   /* I'm not sure about that FS */
   while (((last_ws) || (s[pos] != ' ')) && (s[pos] != '\0'))
     {
	if (s[pos] == ' ')
	     last_ws = pos;
	pos++;
     }
   if (pos > 77)
     {
	slrn_malloc_mime_error(&err, _("First word of header is too long."), *s_ptr, 0,MIME_ERROR_WARN);
	return err;
     }

   do
     {
	if ((s[pos] == ' ') || (s[pos] == '\0'))
	  {
	     if (pos > 77)
	       {
		  if (last_ws == 0)
		    {
		       slrn_malloc_mime_error(&err, _("One word of the header is too long to get folded."), *s_ptr, 0,MIME_ERROR_WARN);
		       return err;
		    }
		  fold++;
		  s += last_ws;
		  pos=0;
	       }
	     last_ws=pos;
	  }
     }
   while (s[pos++] != '\0');
  
   /* Second step: Folding */
   s=*s_ptr;
   ret=tmp=slrn_safe_malloc(strlen(s)+1+fold);
   last_ws=pos=0;
   do
     {
	if ((s[pos] == ' ') || (s[pos] == '\0'))
	  {
	     if (pos > 77)
	       {
		  strncpy(tmp, s, last_ws);
		  tmp+=last_ws;
		  *(tmp++)='\n';
		  s+=last_ws;
		  pos=0;
	       }
	     last_ws=pos;
	  }
     }
   while (s[pos++] != '\0');

   strncpy(tmp, s, strlen(s)+1);

   slrn_free(*s_ptr);
   *s_ptr=ret;
   return NULL;
}
/*}}}*/

/* Do the actual encoding.
 * Note: This function does not generate encoded-words that are longer
 *       than max_len chars. The line folding is performed by
 *       a separate function. */
static Slrn_Mime_Error_Obj *encode_string (char **s_ptr, int offset,/*{{{*/
			  int len, char *from_charset, int max_len, int *chars_more)
{
  int extralen[2] = {0,0}, total = 0;
  char *s=*s_ptr + offset;
  char *charset= Slrn_Outgoing_Charset;
  char *charset_end=NULL;
  char *ret, *tmp=NULL;
  int i;
  Slrn_Mime_Error_Obj *err=NULL;

  do
    {
       if ((charset_end=slrn_strchr(charset, ',')) != NULL)
	 {
	    *charset_end = '\0';
	 }
     
       if (!slrn_case_strcmp(charset, from_charset))
       /* No recoding needed */
	 {
	    tmp=*s_ptr;
	    extralen[0] =0;
	    break;
	 }

       if ((tmp = slrn_convert_substring(*s_ptr, offset, len, charset, from_charset, 1)) != NULL)
	 {
	    extralen[0] = strlen(tmp) - strlen(*s_ptr);
	    slrn_free(*s_ptr);
	    *s_ptr=tmp;
	    s=*s_ptr + offset;

	    break;
	 }
       if (charset_end != NULL)
	 {
	    *charset_end=',';
	    charset = charset_end + 1;
	 }
    } while(charset_end != NULL);
  if (tmp == NULL)
    {
       /* if we get here, no encoding was possible*/
       slrn_malloc_mime_error(&err, _("Can't find suitable charset for Header"), *s_ptr, 0 ,MIME_ERROR_CRIT);
       return err;
    }
   extralen[1] = strlen(charset) + 2+3+2;
   
   for (i=0; i< len + extralen[0]; i++)
     {
	if ( *s & 0x80)
	  {
	     extralen[1] +=  2;
	  }
	s++;
     }
   if ((max_len) && (max_len < len + extralen[0] + extralen[1]))
     {
	slrn_malloc_mime_error(&err, _("One word in the header is too long after encoding."), *s_ptr, 0, MIME_ERROR_CRIT);
	return err;
     }
   ret=tmp=slrn_safe_malloc(strlen(*s_ptr) +1 +extralen[1]);
   strncpy(ret, *s_ptr, offset);
   tmp=ret + offset;
   sprintf(tmp, "=?%s?Q?", charset); /* safe */
   tmp=tmp + strlen(charset) + 5;

   if (charset_end != NULL) *charset_end=',';

   s= *s_ptr + offset;
   for (i=0; i< len + extralen[0]; i++)
     {
	unsigned char ch;
	if ((ch =*s) & 0x80)
	  {
	     sprintf (tmp, "=%02X", (int) ch); /* safe */
	     tmp+=3;
	  }
	else
	  {
	     if (ch == ' ') *tmp = '_';
	     else *tmp = ch;
	     tmp++;
	  }
	s++;
     }
   *tmp++ = '?';
   *tmp++ = '=';
   strncpy(tmp, s, strlen(s)+1); /* safe */
    
   
   slrn_free(*s_ptr);
   *s_ptr=ret;
   
   *chars_more = extralen[0] + extralen[1];
   return NULL;
}
/*}}}*/

/* Try to cause minimal overhead when encoding. */
static Slrn_Mime_Error_Obj *min_encode (char **s_ptr, char *from_charset) /*{{{*/
{
  char *s=*s_ptr;
  int pos=0, last_ws=0;
  int extralen, encode=0;
  int encode_len;
  Slrn_Mime_Error_Obj *ret;

  while (s[pos] != ':')
    {
       pos++;
    }
  while (s[++pos] == ' ')
    {
       last_ws=pos;
    }
  encode_len = 75 - pos;
   
  
  while((slrn_string_nonascii(s)) && (strlen(s) >= pos))
    {
       if ((s[pos] == ' ') || (s[pos] == '\0') || (s[pos] == '\n'))
	 {
	    if (encode)
	      {
		 if ( (ret= encode_string(s_ptr,last_ws +1,pos - (last_ws+1),from_charset,encode_len, &extralen)) != NULL)
		    {
		       return ret;
		    }
		 pos+= extralen;
		 s=*s_ptr;
		 encode = 0;
	      }
	    encode_len = 75;
	    last_ws=pos;
	 }
       if (s[pos] & 0x80)
	    encode = 1;

       pos++;
    }
  return NULL;
}/*}}}*/

/* Encode structured header fields ("From:", "To:", "Cc:" and such) {{{ */

#define TYPE_ADD_ONLY 1
#define TYPE_OLD_STYLE 2
#define TYPE_RFC_2882 3
#define RFC_2882_NOT_ATOM_CHARS "(),.:;<>@[\\]\""
#define RFC_2882_NOT_DOTATOM_CHARS "(),:;<>@[\\]\""
#define RFC_2882_NOT_QUOTED_CHARS "\t\\\""
#define RFC_2882_NOT_DOMLIT_CHARS "[\\]"
#define RFC_2882_NOT_COMMENT_CHARS "(\\)"

static Slrn_Mime_Error_Obj *encode_comment (char **s_ptr, char *from_charset, int *start, int *max) /*{{{*/
{
   char *s =*s_ptr;
   int pos = *start;
   int last_ws = *start-1;
   int encode=0;
   int extralen;
   Slrn_Mime_Error_Obj *err=NULL;

   while (pos < *max)
     {
	if ((s[pos]== ' ') || (s[pos]== '(') || (s[pos] == ')'))
	  {
	     if (encode)
	       {
		  if ( (err= encode_string(s_ptr,last_ws + 1,pos-(last_ws+1),from_charset,78, &extralen)) != NULL)
		    {
		       return err;
		    }
		  *max += extralen;
		  pos +=extralen;
		  s=*s_ptr;
		  encode=0;
	       }
	     if (s[pos] == ')')
	       {
		  *start= pos;
		  return NULL;
	       }
	     
	     if (s[pos]== '(')
	       {
		  pos++;
		  if ((err= encode_comment(s_ptr, from_charset, &pos, max)) != NULL)
		       return err;
		  s=*s_ptr;
	       }
	     last_ws=pos++;
	     continue;
	  }
	if (s[pos] & 0x80)
	  {
	     encode=1;
	     pos++;
	     continue;
	  }
	if (NULL != slrn_strchr(RFC_2882_NOT_COMMENT_CHARS, s[pos]))
	  {
	     slrn_malloc_mime_error(&err, _("Illegal char in displayname of address header."), *s_ptr, 0, MIME_ERROR_CRIT);
	     return err;
	  }
	pos++;
     }
   /* upps*/
   slrn_malloc_mime_error(&err, _("Comment opened but never closed in address header."), *s_ptr, 0, MIME_ERROR_CRIT);
   return err;
}

/*}}}*/

static Slrn_Mime_Error_Obj *encode_phrase (char **s_ptr, char *from_charset, int *start, int *max) /*{{{*/
{
   char *s =*s_ptr;
   int pos = *start;
   int last_ws = *start-1;
   int extralen;
   int in_quote=0;
   int encode=0;
   Slrn_Mime_Error_Obj *err=NULL;
     
   while (pos < *max)
     {
	if ((s[pos]== ' ') || (s[pos]== '"'))
	  {
	     if (encode)
	       {
		  if ( (err= encode_string(s_ptr,last_ws + 1,pos-(last_ws+1),from_charset,78, &extralen)) != NULL)
		    {
		       return err;
		    }
		  *max += extralen;
		  pos +=extralen;
		  s=*s_ptr;
		  encode=0;
	       }
	     if (s[pos]== '"')
	       {
		  if (in_quote)
		       in_quote=0;
		  else
		       in_quote=1;
	       }
	     last_ws=pos++;
	     continue;
	  }
	if (s[pos] & 0x80)
	  {
	     encode=1;
	     pos++;
	     continue;
	  }
	if (!in_quote)
	  {
	     if (s[pos]== '(')
	       {
		  if (encode)
		    {
		       if ( (err= encode_string(s_ptr,last_ws + 1, pos-(last_ws + 1),from_charset,78,&extralen)) != NULL)
			 {
			    return err;
			 }
		       *max += extralen;
		       pos += extralen;
		       s=*s_ptr;
		       encode=0;
		    }
		  
		  pos++;
		  if ((err= encode_comment(s_ptr, from_charset, &pos, max)) != NULL)
		       return err;
		  s=*s_ptr;
		  last_ws=pos++;
		  continue;
	       }
	     if ((s[pos-1] == ' ') && (s[pos] == '<'))
	       /* Address begins, return */
	       {
		  *start= pos;
		  return NULL;
	       }
	     if (NULL != slrn_strchr(RFC_2882_NOT_ATOM_CHARS, s[pos]))
	       {
		  slrn_malloc_mime_error(&err, _("Illegal char in displayname of address header."), *s_ptr, 0, MIME_ERROR_CRIT);
		  return err;
	       }
	  }
	else
	  {
	     if (NULL != slrn_strchr(RFC_2882_NOT_QUOTED_CHARS, s[pos]))
	       {
		  slrn_malloc_mime_error(&err, _("Illegal char in quoted displayname of address header."), *s_ptr, 0, MIME_ERROR_CRIT);
		  return err;
	       }
	  }
	pos++;
     }
   /* never reached */
   return NULL;
}

/*}}}*/

static Slrn_Mime_Error_Obj *encode_localpart (char **s_ptr, char *from_charset, int *start, int max) /*{{{*/
{
   char *s =*s_ptr;
   int pos = *start;
   int in_quote=0;
   Slrn_Mime_Error_Obj *err=NULL;

   while (pos < max)
     {
	if (s[pos] == ' ')
	  {
	     slrn_malloc_mime_error(&err, _("Space in localpart."), *s_ptr, 0, MIME_ERROR_CRIT);
	     return err;
	  }
	if (s[pos] & 0x80)
	  {
	     slrn_malloc_mime_error(&err, _("Non 7-bit char in localpart of address header."), *s_ptr, 0, MIME_ERROR_CRIT);
	     return err;
	  }
	if (in_quote)
	  {
	     if (s[pos] == '"') 
	       {
		  if (s[pos+1] == '@')
		    {
		       *start=++pos;
		       return NULL;
		    }
		  else
		    {
		       slrn_malloc_mime_error(&err, _("Wrong quote in localpart of address header."), *s_ptr, 0, MIME_ERROR_CRIT);
		       return err;
		    }
	       }
	     if (NULL != slrn_strchr(RFC_2882_NOT_QUOTED_CHARS, s[pos]))
	       {
		  slrn_malloc_mime_error(&err, _("Illegal char in quoted localpart of address header."), *s_ptr, 0, MIME_ERROR_CRIT);
		  return err;
	       }
	  }
	else
	  {
	     if (s[pos] == '@')
	       {
		  *start=pos;
		  return NULL;
	       }
	     if (NULL != slrn_strchr(RFC_2882_NOT_DOTATOM_CHARS, s[pos]))
	       {
		  slrn_malloc_mime_error(&err, _("Illegal char in localpart of address header."), *s_ptr, 0, MIME_ERROR_CRIT);
		  return err;
	       }
	  }
	pos++;
     }
   slrn_malloc_mime_error(&err, _("No domain found in address header."), *s_ptr, 0, MIME_ERROR_CRIT);
   return err;
}

/*}}}*/

static Slrn_Mime_Error_Obj *encode_domain (char **s_ptr, char *from_charset, int *start, int max, int type) /*{{{*/
{
   char *s =*s_ptr;
   int pos = *start;
   Slrn_Mime_Error_Obj *err=NULL;

   while (pos < max)
     {
#ifndef HAVE_LIBIDN
	if (s[pos] & 0x80)
	  {
	     /* TODO: encode with libidn */
	     slrn_malloc_mime_error(&err, _("Non 7-bit char in domain of address header. libidn is not yet supported."), *s_ptr, 0, MIME_ERROR_CRIT);
	     return err;
	  }
#endif
	if (type == TYPE_RFC_2882)
	  {
	     if (s[pos] == '>')
	       {
		  *start=pos;
		  return NULL;
	       }
	
	     if (s[pos] == ' ')
	       {
		  slrn_malloc_mime_error(&err, _("Space in domain."), *s_ptr, 0, MIME_ERROR_CRIT);
		  return err;
	       }
	  }
	else
	  {
	     if (s[pos] == ' ')
	       {
		  *start=pos;
		  return NULL;
	       }
	  }
	if (NULL != slrn_strchr(RFC_2882_NOT_DOTATOM_CHARS, s[pos]))
	  {
	     slrn_malloc_mime_error(&err, _("Illegal char in domain of address header."), *s_ptr, 0, MIME_ERROR_CRIT);
	     return err;
	  }
	pos++;
	
     }
   *start=pos;
   return NULL;
}

/*}}}*/

static Slrn_Mime_Error_Obj *encode_domainlit (char **s_ptr, char *from_charset, int *start, int max) /*{{{*/
{
   char *s =*s_ptr;
   int pos = *start;
   Slrn_Mime_Error_Obj *err=NULL;

   while (pos < max)
     {
#ifndef HAVE_LIBIDN
	if (s[pos] & 0x80)
	  {
	     slrn_malloc_mime_error(&err, _("Non 7-bit char in domain of address header. libidn is not yet supported."), *s_ptr, 0, MIME_ERROR_CRIT);
	     return err;
	  }
#endif
	if (s[pos] == ']')
	  {
	     *start=pos;
	     return NULL;
	  }
	if (NULL != slrn_strchr(RFC_2882_NOT_DOMLIT_CHARS, s[pos]))
	  {
	     slrn_malloc_mime_error(&err, _("Illegal char in domain-literal of address header."), *s_ptr, 0, MIME_ERROR_CRIT);
	     return err;
	  }
	pos++;
     }
   slrn_malloc_mime_error(&err, _("domain-literal opened but never closed."), *s_ptr, 0, MIME_ERROR_CRIT);
   return err;
}

/*}}}*/

static Slrn_Mime_Error_Obj *from_encode (char **s_ptr, char *from_charset) /*{{{*/
{
   int head_start=0, head_end, type=0;
   int pos=0;
   int in_quote=0;
   int i;
   char *s=*s_ptr;
   Slrn_Mime_Error_Obj *err=NULL;

   while (s[head_start++] != ':');
   
   while (head_start < strlen(s))
     {
	/* If multiple addresses are given, split at ',' */
	head_end=head_start;
	in_quote=0;
	type=TYPE_ADD_ONLY;
	while ((( in_quote) || (s[head_end] != ',')) && (s[head_end] != '\0'))
	  {
	     if ((!in_quote) && (s[head_end] == '<'))
	       {
		  type= TYPE_RFC_2882;
	       }
	     if (s[head_end++] == '"') /* quoted */
	       {
		  if (in_quote)
		       in_quote=0;
		  else
		       in_quote=1;
	       }
	  }
	if (in_quote)
	  {
	     slrn_malloc_mime_error(&err, _("Quote opened but never closed in address header."), *s_ptr, 0, MIME_ERROR_CRIT);
	     return err;
	  }

	pos=head_start;
	while ((pos < head_end) && (s[pos]== ' '))
	  {
	     pos++;
	  }
	if (type == TYPE_RFC_2882)
	  {
	     if (s[pos] != '<')
	       {
		  if ((err=encode_phrase(s_ptr, from_charset, &pos, &head_end)) != NULL)
		    {
		       return err;
		    }
	       }
	     pos++;
	  }
	
	if ((err=encode_localpart(s_ptr, from_charset, &pos, head_end)) != NULL)
	  {
	     return err;
	  }
	pos++;
	s=*s_ptr;
	if (s[pos] == '[')
	  {
	     pos++;
	     err=encode_domainlit(s_ptr, from_charset, &pos, head_end);
	  }
	else
	  {
	     err=encode_domain(s_ptr, from_charset, &pos, head_end, type);
	  }
	if (err != NULL)
	     return err;
	
	if (type == TYPE_RFC_2882)
	     pos++;
	
	s=*s_ptr;
	
	while (pos < head_end)
	  {
	     /* after domainpart only (folding) Whitespace and comments are allowed*/
	     if (s[pos] != '(')
	       {
		  pos++;
		  encode_comment(s_ptr, from_charset, &pos, &head_end);
		  s=*s_ptr;
		  pos++;
		  continue;
	       }
	     if ((s[pos] != ' ') && (s[pos] != '\n'))
	       {
		  slrn_malloc_mime_error(&err, _("Illegal char after domain of address header."), *s_ptr, 0, MIME_ERROR_CRIT);
		  return err;
	       }
	     pos++;
	  } /* while (pos < head_end) */
	
	head_start=head_end+2;
     } /*while (head_start < strlen(*s_ptr)) */
   return NULL;
}
/*}}}*/

/*}}}*/

Slrn_Mime_Error_Obj *slrn_mime_header_encode (char **s_ptr, char *from_charset) /*{{{*/
{
   char *s=*s_ptr;
   Slrn_Mime_Error_Obj *ret=NULL;

   
   /* preserve 8bit characters in those headers */
   if (!slrn_case_strncmp ((unsigned char *) s,
			   (unsigned char *) "Newsgroups: ", 12) ||
       !slrn_case_strncmp ((unsigned char *) s,
			   (unsigned char *) "Followup-To: ", 13))
     return NULL; /* folding?*/
   
   if (!slrn_case_strncmp ((unsigned char *) s,
			   (unsigned char *) "From: ", 6) ||
       !slrn_case_strncmp ((unsigned char *) s,
			   (unsigned char *) "Cc: ", 4) ||
       !slrn_case_strncmp ((unsigned char *) s,
			   (unsigned char *) "To: ", 4))
     ret = from_encode (s_ptr, from_charset);
   else if (!slrn_case_strncmp ((unsigned char *) s,
				(unsigned char *) "Mail-Copies-To: ", 16))
     {
	unsigned char *b = slrn_skip_whitespace (s + 16);
	if ((!slrn_case_strncmp(b, (unsigned char*) "nobody", 6) ||
	     !slrn_case_strncmp(b, (unsigned char*) "poster", 6)) &&
	    (0 == *slrn_skip_whitespace(b+6)))
	  return NULL; /* nothing to convert */
	ret = from_encode (s_ptr, from_charset);
     }   
   else
     {
	if (slrn_string_nonascii(s))
	  {
	     ret = min_encode (s_ptr, from_charset);
	  }
     }
   if (ret != NULL)
	return ret;

   return fold_line(s_ptr);
}

/*}}}*/

#endif /* NOT SLRNPULL_CODE */
