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

#if ! SLRN_HAS_MIME
int Slrn_Use_Mime = 0;
#else /* rest of file in this ifdef */

int Slrn_Use_Mime = 5;
int Slrn_Use_Meta_Mail = 1;
int Slrn_Fold_Headers = 1;
char *Slrn_MetaMail_Cmd;
char *Slrn_Utf8_Table = NULL;

char *Slrn_Mime_Display_Charset;

/* These are all supersets of US-ASCII.  Only the first N characters are 
 * matched, where N is the length of the table entry.
 */
static char **Custom_Compatible_Charsets;
static char *Compatible_Charsets[] =
{
   "US-ASCII",			       /* This MUST be zeroth element */
   "ISO-8859-",
   "iso-latin1",		       /* knews adds his one */
   "KOI8-R",
   "utf-8",			 /* we now have a function to decode this */
   NULL
};

#ifndef SLRNPULL_CODE
static char *Char_Set;
static int Content_Type;
#define CONTENT_TYPE_TEXT		0x01
#define CONTENT_TYPE_MESSAGE		0x02
#define CONTENT_TYPE_MULTIPART		0x03
#define CONTENT_TYPE_UNSUPPORTED	0x10

static int Content_Subtype;
#define CONTENT_SUBTYPE_PLAIN		0x01
#define CONTENT_SUBTYPE_UNKNOWN		0x02
#define CONTENT_SUBTYPE_UNSUPPORTED	0x10

static int Encoding_Method;
#define ENCODED_7BIT			1
#define ENCODED_8BIT			2
#define ENCODED_QUOTED			3
#define ENCODED_BASE64			4
#define ENCODED_BINARY			5
#define ENCODED_UNSUPPORTED		6

static Slrn_Article_Line_Type *find_header_line (Slrn_Article_Type *a, char *header)
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

int slrn_set_compatible_charsets (char *charsets)
{
   static char* buf;
   char *p;
   char **pp;
   unsigned int n;
   
   slrn_free (buf);
   buf = NULL;
   slrn_free ((char *) Custom_Compatible_Charsets);
   Custom_Compatible_Charsets = NULL;
   
   if (*charsets == 0)
     return 0;

   n = 1;
   p = charsets;
   while (NULL != (p = strchr (p, ',')))
     {
	n++; p++;
     }
   
   Custom_Compatible_Charsets = (char **) SLmalloc (sizeof (char **) * (n+1));
   if (Custom_Compatible_Charsets == NULL)
     return -1;
   
   buf = slrn_strmalloc (charsets, 0);
   if (buf == NULL)
     {
	slrn_free ((char *) Custom_Compatible_Charsets);
	Custom_Compatible_Charsets = NULL;
	return -1;
     }
   
   pp = Custom_Compatible_Charsets;
   p = buf;
   while (n-- != 0)
     {
	*pp++ = p;
	if (NULL != (p = strchr (p, ',')))
	  {
	     *p = 0;
	     p++;
	  }
     }
   
   *pp = NULL;
   
   return 0;
}
#endif /* NOT SLRNPULL_CODE */

static char *_find_compatible_charset (char **compat_charset, char *cs,
				       unsigned int len)
{
   while (*compat_charset != NULL)
     {
	unsigned int len1;
	
	len1 = strlen (*compat_charset);
	if ((len1 <= len) &&
	    (0 == slrn_case_strncmp ((unsigned char *) cs,
				     (unsigned char *) *compat_charset,
				     len1)))
	  return *compat_charset;
	compat_charset++;
     }
   return NULL;
}

static char *find_compatible_charset (char *cs, unsigned int len)
{
   char *retval;
   
   if ((NULL == (retval = _find_compatible_charset (Compatible_Charsets, cs,
						   len))) &&
       (NULL != Custom_Compatible_Charsets))
     retval = _find_compatible_charset (Custom_Compatible_Charsets, cs, len);
   
   return retval;
}

#ifndef SLRNPULL_CODE
static int parse_content_type_line (Slrn_Article_Type *a)
{
   char *b;
   Slrn_Article_Line_Type *line;
   
   if (a == NULL)
     return -1;
   line = a->lines;
   /* Use default: text/plain; charset=us-ascii */
   Content_Type = CONTENT_TYPE_TEXT;
   Content_Subtype = CONTENT_SUBTYPE_PLAIN;
   Char_Set = Compatible_Charsets[0];
   
   if (NULL == (line = find_header_line (a, "Content-Type:")))
     return 0;
   
   b = slrn_skip_whitespace (line->buf + 13);
   
   if (0 == slrn_case_strncmp ((unsigned char *)b,
			       (unsigned char *) "text/",
			       5))
     {
	b += 5;
	if (0 != slrn_case_strncmp ((unsigned char *)b,
				    (unsigned char *) "plain",
				    5))
	  {
	     Content_Subtype = CONTENT_SUBTYPE_UNSUPPORTED;
	     return -1;
	  }
	b += 5;
     }
   else if (0 == slrn_case_strncmp ((unsigned char *)b,
				    (unsigned char *) "message/",
				    5))
     {
	Content_Type = CONTENT_TYPE_MESSAGE;
	Content_Subtype = CONTENT_SUBTYPE_UNKNOWN;
	b += 8;
     }
   else if (0 == slrn_case_strncmp ((unsigned char *)b,
				    (unsigned char *) "multipart/",
				    5))
     {
	Content_Type = CONTENT_TYPE_MULTIPART;
	Content_Subtype = CONTENT_SUBTYPE_UNKNOWN;
	b += 10;
     }
   else
     {
	Content_Type = CONTENT_TYPE_UNSUPPORTED;
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
	     
	     Char_Set = find_compatible_charset (charset, len);
	     return 0;
	  }
	line = line->next;
     }
   while ((line != NULL)
	  && (line->flags & HEADER_LINE)
	  && ((*(b = line->buf) == ' ') || (*b == '\t')));
   
   return 0;
}

static int parse_content_transfer_encoding_line (Slrn_Article_Type *a)
{
   Slrn_Article_Line_Type *line;
   unsigned char *buf;
   
   if (a == NULL)
     return -1;

   Encoding_Method = ENCODED_7BIT;
   line = find_header_line (a, "Content-Transfer-Encoding:");
   if (line == NULL) return 0;

   buf = (unsigned char *) slrn_skip_whitespace (line->buf + 26);
   if (*buf == '"') buf++;
   
   if (0 == slrn_case_strncmp (buf, (unsigned char *) "7bit", 4))
     Encoding_Method = ENCODED_7BIT;
   else if (0 == slrn_case_strncmp (buf, (unsigned char *) "8bit", 4))
     Encoding_Method = ENCODED_8BIT;
   else if (0 == slrn_case_strncmp (buf, (unsigned char *) "base64", 6))
     Encoding_Method = ENCODED_BASE64;
   else if (0 == slrn_case_strncmp (buf, (unsigned char *) "quoted-printable", 16))
     Encoding_Method = ENCODED_QUOTED;
   else if (0 == slrn_case_strncmp (buf, (unsigned char *) "binary", 6))
     Encoding_Method = ENCODED_BINARY;
   else
     {
	Encoding_Method = ENCODED_UNSUPPORTED;
	return -1;
     }
   return 0;
}
#endif /* NOT SLRNPULL_CODE*/

static int Index_Hex[128] =
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
#define HEX(c) (Index_Hex[(unsigned char)(c) & 0x7F])

static char *decode_quoted_printable (char *dest,
				      char *src, char *srcmax,
				      int treat_underscore_as_space,
				      int strip_8bit)
{
   char *allowed_in_qp = "0123456789ABCDEFabcdef";
   unsigned char ch, mask = 0x0;
#ifndef SLRNPULL_CODE
   if (strip_8bit && (NULL == Char_Set))
     mask = 0x80;
#else
   (void) strip_8bit;
#endif
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

static int Index_64[128] =
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

#define BASE64(c) (Index_64[(unsigned char)(c) & 0x7F])

static char *decode_base64 (char *dest, char *src, char *srcmax)
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

static char *utf_to_unicode (int *out, char *in, char *srcmax)
{
   int mask = 0;
   short len = 0, i;
   
   for (i = 1; i < 8; i++)
     {
	if ((*in & (mask |(0x01 << (8-i)))) == mask)
	  {
	     len = i;
	     break;
	  }
	mask = (mask |(0x01 << (8-i)));
     }
   
   if ((len == 0) || (len == 2) || (in + len - 1 > srcmax))
     {
	*out = -1;
	return in + 1;
     }
   if (len > 1) len--;
   
   for (i = 1; i < len; i++)
     if ((in[i] & 0xC0) != 0x80)
       {
	  *out = -1;
	  return in + 1;
       }
   
   *out = ((int) in[0]) & ~mask & 0xFF;
   for (i = 1; i < len; i++)
     *out = (*out << 6) | (((int) in[i]) & 0x7F);
   
   if (!*out)
     *out = -1;
   
   return in + len;
}

static char *decode_utf8 (char *dest, char *src, char *srcmax,
			  char *utf8_error)
{
   int ch;
   if (utf8_error != NULL)
     *utf8_error = 0;
   
   while (src < srcmax)
     {
	if (*src & 0x80)
	  {
	     src = utf_to_unicode (&ch, src, srcmax);
	     if ((ch < 0) || (ch > 65535))
	       {
		  if (utf8_error != NULL)
		    *utf8_error = 1;
		  *dest++ = '?';
	       }
	     else if (Slrn_Utf8_Table == NULL) /* convert to iso-8859-1 */
	       {
		  if (ch < 256)
		    *dest++ = (char) ch;
		  else
		    *dest++ = '?';
	       }
	     else /* use user-defined conversion table */
	       {
		  char c;
		  
		  if (ch < 128)
		    *dest++ = (char) ch;
		  else if (0 != (c = Slrn_Utf8_Table[ch]))
		    *dest++ = c;
		  else
		    *dest++ = '?';
	       }
	  }
	else
	  *dest++ = *src++;
     }
   
   return dest;
}

int slrn_rfc1522_decode_string (char *s)
{
   char *s1, *s2, ch;
   char *charset, method, *txt;
   char *after_last_encoded_word;
   char *after_whitespace;
   unsigned int count;
   unsigned int len;

   count = 0;
   after_whitespace = NULL;
   after_last_encoded_word = NULL;

/* Even if some user agents still send raw 8bit, it is safe to call
 * decode_utf8() -- if it finds 8bit chars that are not valid UTF-8, it
 * will set ch to 1 and we can leave the line untouched. */
   len = strlen (s);
   s1 = slrn_safe_malloc(len + 1);
   
   s2 = decode_utf8 (s1, s, s + len, &ch);
   *s2 = 0;
   
   if (ch == 0)
     strcpy (s, s1); /* safe */
   slrn_free (s1);
   
   while (1)
     {
	while ((NULL != (s = slrn_strchr (s, '=')))
	       && (s[1] != '?')) s++;
	if (s == NULL) break;
	
	s1 = s;
	charset = s = s1 + 2;
	while (((ch = *s) != 0)
	       && (ch != '?') && (ch != ' ') && (ch != '\t') && (ch != '\n'))
	  s++;
	
	if (ch != '?')
	  {
	     s = s1 + 2;
	     continue;
	  }
	
	charset = s1 + 2;
	len = s - charset;
	
	charset = find_compatible_charset (charset, len);
	s++;			       /* skip ? */
	method = *s++;		       /* skip B,Q */
	method = UPPER_CASE(method);
	
	if ((charset == NULL) || ((method != 'B') && (method != 'Q'))
	    || (*s != '?'))
	  {
	     s = s1 + 2;
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
	
	if (slrn_case_strncmp((unsigned char *)"utf-8",
			      (unsigned char *)charset, 5) == 0)
	  s1 = decode_utf8 (s2, s2, s1, NULL);
	
	/* Now move everything over */
	s2 = s + 2;		       /* skip final ?= */
	s = s1;			       /* start from here next loop */
	while ((ch = *s2++) != 0) *s1++ = ch;
	*s1 = 0;
	
	count++;
	
	after_last_encoded_word = s;
	s = slrn_skip_whitespace (s);
	after_whitespace = s;
     }
   return count;
}


#ifndef SLRNPULL_CODE /* rest of the file in this ifdef */

static void rfc1522_decode_headers (Slrn_Article_Type *a)
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
	    slrn_rfc1522_decode_string (line->buf))
	  {
	     a->is_modified = 1;
	     a->mime_was_modified = 1;
	  }
	line = line->next;
     }
}

static void decode_mime_base64 (Slrn_Article_Type *a)
{
   Slrn_Article_Line_Type *l;
   Slrn_Article_Line_Type *body_start, *next;
   char *buf_src, *buf_dest, *buf_pos;
   char *base;
   int len;
   
   if (a == NULL) return;
   
   l = a->lines;
   
   /* skip header and separator */
   while (((l != NULL) && (l->flags & HEADER_LINE)) || l->buf[0] == '\0')
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
   
   if (Char_Set == NULL)
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
   a->mime_was_modified = 1;
}

/* This function checks if the last character on curr_line is an = and 
 * if it is, then it merges curr_line and curr_line->next. See RFC1341,
 * section 5.1 (Quoted-Printable Content-Transfer-Encoding) rule #5.
 * [csp@ohm.york.ac.uk]
 */
static int merge_if_soft_linebreak (Slrn_Article_Line_Type *curr_line)
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

static void decode_mime_quoted_printable (Slrn_Article_Type *a)
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
	     a->mime_was_modified = 1;
	  }
	
	line = line->next;
     }
}

static void decode_mime_utf8 (Slrn_Article_Type *a)
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
	len = strlen (b);
	
	b = decode_utf8 (b, b, b + len, NULL);
	
	if (b < line->buf + len)
	  {
	     *b = 0;
	     a->is_modified = 1;
	     a->mime_was_modified = 1;
	  }
	
	line = line->next;
     }
}

void slrn_mime_article_init (Slrn_Article_Type *a)
{
   a->mime_was_modified = 0;
   a->mime_was_parsed = 0;
   a->mime_needs_metamail = 0;
}

void slrn_mime_process_article (Slrn_Article_Type *a)
{
   if ((a == NULL) || (a->mime_was_parsed))
     return;

   a->mime_was_parsed = 1;	       /* or will be */
   
   rfc1522_decode_headers (a);

/* Is there a reason to use the following line? */
/*   if (NULL == find_header_line (a, "Mime-Version:")) return;*/
   if ((-1 == parse_content_type_line (a))
       || (-1 == parse_content_transfer_encoding_line (a)))
     {
	a->mime_needs_metamail = 1;
	return;
     }
   
   switch (Encoding_Method)
     {
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
	a->mime_needs_metamail = 1;
	return;
     }
   
   if ((a->mime_needs_metamail == 0) &&
       (Char_Set != NULL) &&
       (slrn_case_strncmp((unsigned char *)"utf-8",
			  (unsigned char *)Char_Set, 5) == 0))
     decode_mime_utf8 (a);
}

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

int slrn_mime_call_metamail (void)
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


/* -------------------------------------------------------------------------
 * MIME encoding routines.
 * -------------------------------------------------------------------------*/

static char *Mime_Posting_Charset;
static int Mime_Posting_Encoding;

int slrn_mime_scan_file (FILE *fp)
{
   /* This routine scans the article to determine what CTE should be used */
   unsigned int linelen = 0;
   unsigned int maxlinelen = 0;
   int ch;
   int cr = 0;
   unsigned int hibin = 0;
   
   
   /* Skip the header.  8-bit characters in the header are taken care of
    * elsewhere since they ALWAYS need to be encoded.
    */
   while ((ch = getc(fp)) != EOF)
     {
	if (ch == '\n')
	  {
	     ch = getc(fp);
	     if (ch == '\n')
	       break;
	  }
     }
   
   if (ch == EOF)
     {
	rewind (fp);
	return -1;
     }
   
   while ((ch = getc(fp)) != EOF)
     {
	linelen++;
	if (ch & 0x80) hibin = 1; /* 8-bit character */
	
	if (ch == '\n')
	  {
	     if (linelen > maxlinelen)	maxlinelen = linelen;
	     linelen = 0;
	  }
	else if (((unsigned char)ch < 32) && (ch != '\t') && (ch != 0xC))
	  cr = 1;		       /* not tab or formfeed */
     }
   if (linelen > maxlinelen) maxlinelen = linelen;
   
   if (hibin > 0)
     {
	/* 8-bit data.  US-ASCII is NOT a valid charset, so use ISO-8859-1 */
	if (slrn_case_strcmp((unsigned char *)"us-ascii",
			     (unsigned char *)Slrn_Mime_Display_Charset) == 0)
	  Mime_Posting_Charset = "iso-8859-1";
	else
	  Mime_Posting_Charset = Slrn_Mime_Display_Charset;
     }
   else if (NULL != find_compatible_charset (Slrn_Mime_Display_Charset,
					     strlen (Slrn_Mime_Display_Charset)))
     /* 7-bit data.  Check to make sure that this display supports US-ASCII */
     Mime_Posting_Charset = "us-ascii";
   else
     Mime_Posting_Charset = Slrn_Mime_Display_Charset;

#if 0
   if ((maxlinelen > 990) || (cr > 0))
     {
	Mime_Posting_Encoding = ENCODED_QUOTED;
     }
   else
#endif
     if (hibin > 0)
     Mime_Posting_Encoding = ENCODED_8BIT;
   else
     Mime_Posting_Encoding = ENCODED_7BIT;
   
   rewind(fp);
   return 0;
}

static int get_word_len (char *s, int *is_encoded) /*{{{*/
{
   char *e = s;
   int qmarks = 0;
   
   while (*e && ((*e == ' ') || (*e == '\t')))
     e++;
   if ((e[0] == '=') && (e[1] == '?'))
     {
	e += 2;
	qmarks = 1;
     }
   while (*e && (*e != ' ') && (*e != '\t'))
     {
	if ((*e++ == '?') && qmarks) qmarks++;
     }
   if ((qmarks == 4) && (e > s+1) && (e[-1] == '=') && (e[-2] == '?'))
     *is_encoded = 1;
   return e - s;
}
/*}}}*/

static int fold_line (char *s, unsigned int bytes, unsigned int line_len) /*{{{*/
{
   int line_enc = 0, word_len, word_enc = 0, retval = -1;
   char *copy, *p;
   
   if (NULL == (copy = slrn_strmalloc (s, 0)))
     return -1;
   p = copy;
   
   /* skip the first word (folding after the keyword is not allowed) */
   word_len = get_word_len (p, &line_enc);
   strncpy (s, p, word_len);
   s += word_len; p += word_len;
   line_len += word_len; bytes -= word_len;
   
   while (*p)
     {
	word_len = get_word_len (p, &word_enc);
	if ((line_enc || word_enc) && (line_len + word_len > 76))
	  {
	     if (bytes-- <= 0) goto free_and_return;
	     *s++ = '\n';
	     line_len = 0;
	     line_enc = 0;
	  }
	if ((int)bytes <= word_len) goto free_and_return;
	strncpy (s, p, word_len);
	s += word_len; p += word_len;
	line_len += word_len; bytes -= word_len;
	line_enc |= word_enc; word_enc = 0;
     }
   
   if (bytes)
     {
	retval = 0;
	*s = '\0';
     }
   
   free_and_return:
   slrn_free (copy);
   return retval;
}
/*}}}*/

/* These functions return the number of characters written or
 * -1 if dest is too small to hold the result. */

static int copy_whitespace (unsigned char *from, unsigned char *to, /*{{{*/
			    unsigned char *dest, size_t max)
{
   int len = 0;
   while ((from <= to) && ((*from == ' ') || (*from == '\t')))
     {
	if (len == (int)max) return -1;
	*dest++ = *from++;
	len++;
     }
   return len;
}
/*}}}*/

/* Hack to keep encoded words shorter if they directly follow the keyword. */
static int Keyword_Len;

#define ENCODE_COMMENT	1
#define ENCODE_PHRASE	2

/* Do the actual encoding.
 * Note: This function does not generate encoded-words that are longer
 *       than 75 chars (or shorter, depending on Keyword_Len).  The line
 *       folding is performed by a separate function. */
static int encode_string (unsigned char *from, unsigned char *to, /*{{{*/
			  int flags, unsigned char *dest, size_t max)
{
   int len = 0, total = 0;
   char charset[64];
   
   if (0 == slrn_case_strcmp ((unsigned char *)Slrn_Mime_Display_Charset,
			      (unsigned char *)"us-ascii"))
     {
	strcpy (charset, "=?iso-8859-1?Q?"); /* safe */
     }
   else slrn_snprintf (charset, sizeof (charset), "=?%s?Q?", Slrn_Mime_Display_Charset);
   
   while (from < to)
     {
	/* Start the encoded-word */
	len = strlen (charset);
	if (len > (int) max - 1) return -1;
	strcpy ((char *)dest, charset); /* safe */
	dest += len; max -= len; total += len;
	
	/* Write the data */
	while (max && (from < to) && (len < 71 - Keyword_Len))
	  {
	     unsigned char ch;
	     if (flags && (*from == '\\') && (++from == to))
	       break;
	     if (((ch = *from) & 0x80) || (ch < 32) ||
		 (ch == '?') || (ch == '\t') || (ch == '=') || (ch == '_') ||
		 ((flags & ENCODE_COMMENT) &&
		  ((ch == '(') || (ch == ')') || (ch == '"'))) ||
		 ((flags & ENCODE_PHRASE) &&
		  (NULL != strchr ("\"(),.:;<>@[\\]", ch))))
	       {
		  if (max < 3) return -1;
		  sprintf ((char *) dest, "=%02X", (int) ch); /* safe */
		  len += 3; max -= 3; dest += 3; total += 3;
	       }
	     else
	       {
		  if (ch == ' ') *dest = '_';
		  else *dest = ch;
		  len++; max--; dest++; total++;
	       }
	     from++;
	  }
	
	/* Finish the encoded-word */
	if (max < 3) return -1;
	Keyword_Len = 0;
	*dest++ = '?'; *dest++ = '=';
	if (from < to)
	  {
	     *dest++ = ' ';
	     len += 3; max -= 3; total += 3;
	  }
	else
	  {
	     len += 2; max -= 2; total += 2;
	  }
     }
   
   if (max)
     {
	*dest = '\0';
	return total;
     }
   return -1;
}
/*}}}*/

/* Try to cause minimal overhead when encoding. */
static int min_encode (unsigned char *from, unsigned char *to, /*{{{*/
		       int flags, unsigned char *dest, size_t max)
{
   int len, total = 0;
   
   if (-1 == (len = copy_whitespace (from, to, dest, max)))
     return -1;
   from += len; dest += len; max -= len; total += len;
   
   while (from < to)
     {
	unsigned char *beg = NULL, *end = NULL;
	unsigned char *eword = from, *bword = from;
	int count;
	
	do
	  {
	     count = 0;
	     while ((eword < to) && (*eword != ' ') && (*eword != '\t'))
	       {
		  if (*eword & 0x80) count++;
		  eword++;
	       }
	     
	     if (count)
	       {
		  if (beg == NULL) beg = bword;
		  bword = end = eword;
	       }
	     
	     while ((eword < to) && ((*eword == ' ') || (*eword == '\t')))
	       eword++;
	  }
	while ((eword < to) && count);
	
	if (beg != NULL)
	  {
	     if (-1 == (len = encode_string (beg, end, flags, dest, max)))
	       return -1;
	     dest += len; max -= len; total += len;
	  }
	if (bword != eword)
	  {
	     len = eword - bword;
	     if ((int)max < len) return -1;
	     strncpy (dest, bword, len);
	     dest += len; max -= len; total += len;
	     Keyword_Len = 0;
	  }
	from = eword;
     }
   
   if (max)
     {
	*dest = '\0';
	return total; /* don't count trailing zero */
     }
   
   return -1;
}
/*}}}*/

static int encode_quoted_string (unsigned char **from, unsigned char *to, /*{{{*/
				 unsigned char *dest, size_t max)
{
   unsigned char *end = *from + 1;
   int count = 0;
   
   while ((end < to) && (*end != '"'))
     {
	if ((*end == '\\') && (++end == to))
	  break;
	if (*end & 0x80)
	  count++;
	end++;
     }
   
   if (count)
     {
	int retval = encode_string (*from + 1, end, ENCODE_PHRASE, dest, max);
	*from = end + 1;
	return retval;
     }
   else
     {
	int len = end + 1 - *from;
	if ((int)max <= len) return -1;
	strncpy (dest, *from, len);
	dest[len] = '\0';
	*from = end + 1;
	return len;
     }
}
/*}}}*/

static int encode_comment (unsigned char **from, unsigned char *to, /*{{{*/
			   unsigned char *dest, size_t max)
{
   unsigned char *end = *from + 1;
   int len, total = 0;
   
   while ((end < to) && (*end != ')'))
     {
	if (*end == '(')
	  {
	     if (**from == '(')
	       {
		  if (!max) return -1;
		  *dest++ = '('; total++; max--; (*from)++;
	       }
	     
	     if (-1 == (len = min_encode (*from, end, ENCODE_COMMENT, dest, max)))
	       return -1;
	     total += len; max -= len; dest += len;
	     
	     if (-1 == (len = encode_comment (&end, to, dest, max)))
	       return -1;
	     total += len; max -= len; dest += len;
	     *from = end;
	  }
	else
	  {
	     if ((*end == '\\') && (++end == to))
	       break;
	     end++;
	  }
     }
   
   if (**from == '(')
     {
	if (!max) return -1;
	*dest++ = '('; total++; max--; (*from)++;
     }
   if (-1 == (len = min_encode (*from, end, ENCODE_COMMENT, dest, max)))
     return -1;
   dest += len; total += len; max -= len;
   *from = end + 1;
   if (max < 2) return -1;
   *dest++ = ')'; *dest = '\0';
   
   return total + 1; /* don't count trailing zero */
}
/*}}}*/

/* Encode structured header fields ("From:", "To:", "Cc:" and such) */
static int from_encode (unsigned char *from, unsigned char *to, /*{{{*/
			unsigned char *dest, size_t max)
{
   int len, total = 0;
   
   if (-1 == (len = copy_whitespace (from, to, dest, max)))
     return -1;
   from += len; dest += len; max -= len; total += len;
   
   while (from < to)
     {
	if (*from == '(')
	  {
	     if (-1 == (len = encode_comment (&from, to, dest, max)))
	       return -1;
	     dest += len; max -= len; total += len;
	  }
	else if (*from == '"')
	  {
	     if (-1 == (len = encode_quoted_string (&from, to, dest, max)))
	       return -1;
	     dest += len; max -= len; total += len;
	  }
	else if (NULL != strchr ("),.:;<>@[\\]", *from))
	  {
	     if (!max) return -1;
	     *dest++ = *from++;
	     max--; total++;
	  }
	else
	  {
	     unsigned char *next = from + 1;
	     while ((next < to) && (NULL == strchr ("\"(),.:;<>@[\\]", *next)))
	       next++;
	     if (-1 == (len = min_encode (from, next, ENCODE_PHRASE, dest, max)))
	       return -1;
	     from = next; dest += len; max -= len; total += len;
	  }
     }
   
   if (!max) return -1;
   *dest = '\0';
   
   return total;
}
/*}}}*/

void slrn_mime_header_encode (char *s, unsigned int bytes) /*{{{*/
{
   char buf[1024], *colon;
   unsigned int len, max = bytes;
   
   /* preserve 8bit characters in those headers */
   if (!slrn_case_strncmp ((unsigned char *) s,
			   (unsigned char *) "Newsgroups: ", 12) ||
       !slrn_case_strncmp ((unsigned char *) s,
			   (unsigned char *) "Followup-To: ", 13))
     return;
   
   if ((NULL != (colon = strchr (s, ':'))) && (colon[1] == ' '))
     {
	max -= (colon + 2 - s);
	colon += 2; /* skip the keyword */
     }
   else
     colon = s;
   len = strlen (colon);
   Keyword_Len = colon - s;
   
   if (len < sizeof (buf))
     {
	int ret;
	
	strcpy (buf, colon); /* safe */
	     
	if (len && (buf[len-1] == '\n')) len--; /* save \n from being encoded */
	
	if (!slrn_case_strncmp ((unsigned char *) s,
				(unsigned char *) "From: ", 6) ||
	    !slrn_case_strncmp ((unsigned char *) s,
				(unsigned char *) "Cc: ", 4) ||
	    !slrn_case_strncmp ((unsigned char *) s,
				(unsigned char *) "To: ", 4) ||
	    !slrn_case_strncmp ((unsigned char *) s,
				(unsigned char *) "Mail-Copies-To: ", 16))
	  ret = from_encode ((unsigned char *) buf, (unsigned char *) buf + len,
			     (unsigned char *) colon, max);
	else
	  ret = min_encode ((unsigned char *) buf, (unsigned char *) buf + len,
			    0, (unsigned char *) colon, max);
	
	if ((ret != -1) && (max - ret > 1))
	  {
	     if (len && (buf[len] == '\n'))
	       {
		  colon[ret] = '\n';
		  colon[ret+1] = '\0';
	       }
	     if ((0 == Slrn_Fold_Headers) ||
		 (-1 != fold_line (colon, max, colon - s)))
	       return;
	  }
	strcpy (colon, buf); /* safe */
     }

#if 0
   /* Cannot do it so strip it to 8 bits. */
   while (*colon)
     {
	*colon = *colon & 0x7F;
	colon++;
     }
#endif
}
/*}}}*/

void slrn_mime_add_headers (FILE *fp)
{
   char *encoding;
   
   if (Mime_Posting_Charset == NULL)
     Mime_Posting_Charset = "us-ascii";

   switch (Mime_Posting_Encoding)
     {
      default:
      case ENCODED_8BIT:
	encoding = "8bit";
	break;
	
      case ENCODED_7BIT:
	if (!strcmp ("us-ascii", Mime_Posting_Charset))
	  return;
	encoding = "7bit";
	break;
	
      case ENCODED_QUOTED:
	encoding = "quoted-printable";
     }
   
   if (fp != NULL)
     {
	fprintf (fp, "\
Mime-Version: 1.0\n\
Content-Type: text/plain; charset=%s\n\
Content-Transfer-Encoding: %s\n",
		 Mime_Posting_Charset,
		 encoding);
     }
   else
     {
	Slrn_Post_Obj->po_printf ("\
Mime-Version: 1.0\n\
Content-Type: text/plain; charset=%s\n\
Content-Transfer-Encoding: %s\n",
		 Mime_Posting_Charset,
		 encoding);
     }
}

FILE *slrn_mime_encode (FILE *fp)
{
   if ((Mime_Posting_Encoding == ENCODED_7BIT)
       || (Mime_Posting_Encoding == ENCODED_8BIT))
     return fp;
   
   /* Add encoding later. */
   return fp;
}

#endif /* NOT SLRNPULL_CODE */

#endif  /* SLRN_HAS_MIME */
