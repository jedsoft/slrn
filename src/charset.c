/* -*- mode: C; mode: fold -*- */
/* Charset handling routines.
 *
 * Author: Felix Schueller
 * Modified by JED.
 *
 */

#include "config.h"
#include "slrnfeat.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

#if defined(HAVE_LOCALE_H) && defined(HAVE_LANGINFO_H)
# include <locale.h>
# include <langinfo.h>
#endif

#ifdef HAVE_ICONV
# include <iconv.h>
#endif

#include <slang.h>

#include "jdmacros.h"
#include "slrn.h"
#include "group.h"
#include "art.h"
#include "util.h"
#include "snprintf.h"
#include "mime.h"
#include "strutil.h"
#include "charset.h"
#include "common.h"

char *Slrn_Config_Charset  = NULL;
char *Slrn_Display_Charset  = NULL;
char *Slrn_Editor_Charset  = NULL;
char *Slrn_Outgoing_Charset  = NULL;
char *Slrn_Fallback_Input_Charset = NULL;

void slrn_init_charset (void)
{
#if defined(HAVE_LOCALE_H) && defined(HAVE_LANGINFO_H) && defined(CODESET)
  if (Slrn_Display_Charset == NULL)
    {
       /* setlocale has already been called when this function is called */
       /* setlocale(LC_ALL, ""); */
       char *charset = nl_langinfo (CODESET);
       if ((charset != NULL) && (*charset != 0))
	 Slrn_Display_Charset = slrn_safe_strmalloc (nl_langinfo (CODESET));
    }
#endif
}

void slrn_prepare_charset (void)
{
  if (Slrn_Display_Charset == NULL)
    {
       char *charset = "US-ASCII";
       if (Slrn_UTF8_Mode)
	 charset = "UTF-8";
       Slrn_Display_Charset = slrn_safe_strmalloc (charset);
    }
  if (Slrn_Outgoing_Charset == NULL)
    {
       Slrn_Outgoing_Charset = Slrn_Display_Charset;
    }
  if ((Slrn_Editor_Charset != NULL) && (0 == slrn_case_strcmp(Slrn_Display_Charset, Slrn_Editor_Charset)))
    {
       slrn_free(Slrn_Editor_Charset);
       Slrn_Editor_Charset=NULL;
    }
}

/* returns 1 if *str contains chars not in us-ascii, 0 else */
int slrn_string_nonascii(char *str)
{
  while(*str != '\0')
    {
       if (*str & 0x80)
	    return 1;
       str++;
    }
  return 0;
}

#ifdef HAVE_ICONV
/* returns the converted string, or NULL on error or if no convertion is needed*/
/* Returns 1 if iconv succeeded, 0 if it failed, or -1 upon some other error.
 * This function returns 0 only if test is 1.  Otherwise, if test is 0 and
 * illegal bytes are encountered, they will be replaced by ?s.
 */
static int iconv_convert_string (iconv_t cd, char *str, size_t len, int test, char **outstrp)
{
   char *buf, *bufp;
   unsigned int buflen;
   size_t inbytesleft;
   size_t outbytesleft;
   int fail_error;
   int need_realloc;

   if (len == 0)
     return 0;

   if (test)
     fail_error = 0;
   else
     fail_error = -1;

   *outstrp = NULL;
   inbytesleft = len;
   bufp = buf = NULL;
   buflen = 0;
   outbytesleft = 0;
   need_realloc = 1;

   while (inbytesleft)
     {
	size_t ret;

	if (need_realloc)
	  {
	     char *tmpbuf;
	     unsigned int dsize = 2*len;
	     buflen += dsize;
	     outbytesleft += dsize;
	     if (NULL == (tmpbuf = slrn_realloc (buf, buflen+1, test==0)))
	       {
		  slrn_free (buf);
		  return fail_error;
	       }
	     bufp = tmpbuf + (bufp - buf);
	     buf = tmpbuf;
	     need_realloc = 0;
	  }

	errno = 0;
	ret = iconv (cd, &str, &inbytesleft, &bufp, &outbytesleft);
#ifdef NON_GNU_ICONV
	if (ret == 0)
	  break;
#else
	if (ret != (size_t) -1)
	  break;
#endif
	switch (errno)
	  {
	   default:
	   case EINVAL:
	   case EILSEQ:	       /* invalid byte sequence */
	     if (test)
	       {
		  slrn_free (buf);
		  return 0;
	       }
	     *bufp++ = '?';
	     str++;
	     inbytesleft--;
	     outbytesleft--;
	     /* FIXME: Should the shift-state be reset? */
	     break;
#ifndef NON_GNU_ICONV
	   case 0:		       /* windows bug */
#endif
	   case E2BIG:
	     need_realloc = 1;
	     break;
	  }
     }

   len = (unsigned int) (bufp - buf);
   bufp = slrn_realloc (buf, len+1, 1);
   if (bufp == NULL)
     {
	slrn_free (buf);
	return fail_error;
     }
   bufp[len] = 0;
   *outstrp = bufp;

   return 1;
}
#endif

/* Guess a character set from the bytes in the string -- it returns a
 * malloced string.
 */
char *slrn_guess_charset (char *str, char *strmax)
{
   char *charset = "us-ascii";

   while (str < strmax)
     {
	unsigned int nconsumed;
	SLwchar_Type wch;

	if ((*str & 0x80) == 0)
	  {
	     str++;
	     continue;
	  }

	/* First see if it looks like UTF-8 */
	if (NULL != SLutf8_decode ((SLuchar_Type *)str, (SLuchar_Type *)strmax, &wch, &nconsumed))
	  {
	     charset = "UTF-8";
	     break;
	  }

	charset = Slrn_Fallback_Input_Charset;
	if (charset == NULL)
	  charset = "iso-8859-1";

	break;
     }
   return slrn_strmalloc (charset, 1);
}

char *slrn_convert_string (char *from, char *str, char *strmax, char *to, int test)
{
#ifdef HAVE_ICONV
   iconv_t cd;
   int status;
   char *substr;
   int free_from = 0;

   if ((from == NULL)
       || (0 == slrn_case_strcmp (from, "unknown-8bit"))
       || (0 == slrn_case_strcmp (from, "x-user-defined")))
     {
	from = slrn_guess_charset (str, strmax);
	if (from == NULL)
	  return NULL;
	free_from = 1;
     }

   if ((cd = iconv_open(to, from)) == (iconv_t)(-1))
     {
	if (test == 0)
	  slrn_error (_("Can't convert %s -> %s\n"), from, to);

	if (free_from)
	  slrn_free (from);

	return NULL;
     }

   status = iconv_convert_string (cd, str, strmax-str, test, &substr);
   iconv_close(cd);

   if (free_from)
     slrn_free (from);

   if (status == 0)
     return NULL;

   if (status == -1)
     return NULL;

   return substr;
#else /* no iconv */

   char *s;

   if (from != NULL)
     {
	if (0 == strcmp (to, from))
	  return slrn_strnmalloc (str, strmax-str, 1);
     }

   if (test)
     return NULL;

   /* Force it to us-ascii */
   s = slrn_strnmalloc (str, strmax-str, 1);
   if (s == NULL)
     return NULL;

   str = s;
   while (*s)
     {
	if (*s & 0x80)
	  *s = '?';
	s++;
     }
   return str;
#endif
}

char *slrn_convert_substring(char *str, unsigned int offset, unsigned int len, char *to_charset, char *from_charset, int test)
{
   char *substr;
   char *new_str;
   unsigned int new_len;
   unsigned int dlen;

   new_len = strlen (str);
   if (len == 0)
     return NULL;

   if (offset + len > new_len)
     {
	slrn_error ("Internal Error in slrn_convert_substring");
	return NULL;		       /* internal error */
     }

   substr = slrn_convert_string (from_charset, str+offset, str+offset+len,
				 to_charset, test);

   if (substr == NULL)
     return NULL;

   dlen = strlen (substr);
   new_len = (new_len - len) + dlen;
   new_str = slrn_malloc (new_len + 1, 0, 1);
   if (new_str == NULL)
     {
	slrn_free (substr);
	return NULL;
     }
   strncpy (new_str, str, offset);
   strcpy (new_str + offset, substr);
   strcpy (new_str + offset + dlen, str + offset + len);
   slrn_free (substr);
   return new_str;
}

int slrn_test_and_convert_string(char *str, char **dest, char *to_charset, char *from_charset)
{
   if (dest == NULL)
	return -1;

   *dest = NULL;

   if ((to_charset == NULL) || (from_charset == NULL))
	return 0;

   if (!slrn_string_nonascii(str))
	return 0;

   if(NULL == (*dest = slrn_convert_substring(str, 0, strlen (str), to_charset, from_charset, 0)))
     return -1;

   return 0;
}

int slrn_convert_fprintf(FILE *fp, char *to_charset, char *from_charset, const char *format, ... )
{
   va_list args;
   int retval;
   char *str,*tmp;

   va_start (args, format);

   if ((to_charset == NULL) || (from_charset == NULL) || (slrn_case_strcmp(to_charset, from_charset) == 0))
     {
	retval = vfprintf (fp, format, args);
	va_end (args);
	return retval;
     }

   str = slrn_strdup_vprintf(format, args);
   va_end (args);

   if (!slrn_string_nonascii(str))
     {
	retval = fputs (str, fp);
	slrn_free(str);
	return retval;
     }

   if (NULL == (tmp = slrn_convert_substring(str, 0, strlen (str), to_charset, from_charset, 0)))
     {
	slrn_free(str);
	return -1;
     }
   retval = fputs (tmp, fp);
   slrn_free(str);
   slrn_free(tmp);

   return retval;
}

#ifdef HAVE_ICONV
static void iconv_convert_newline (iconv_t cd)
{
   char *nl = "\n";
   char *tmp;

   if (1 == iconv_convert_string (cd, nl, 1, 1, &tmp))
     slrn_free (tmp);
}
#endif

/* converts a->lines */
int slrn_convert_article(Slrn_Article_Type *a, char *to_charset, char *from_charset)
{
#ifdef HAVE_ICONV
   iconv_t cd;
   char *tmp;
   struct Slrn_Article_Line_Type *line=a->lines;

   if ((cd = iconv_open(to_charset, from_charset)) == (iconv_t)(-1))
     {
	slrn_error (_("Can't convert %s -> %s\n"), from_charset, to_charset);
	return -1;
     }

   /* Headers are handled elsewhere */
   while ((line != NULL) && (line->flags & HEADER_LINE))
     {
	line=line->next;
     }

   while (line != NULL)
     {
	if (1 == iconv_convert_string(cd, line->buf, strlen (line->buf), 0, &tmp))
	  {
	     slrn_free((char *) line->buf);
	     line->buf=tmp;
	     a->mime.was_modified=1;
	     iconv_convert_newline (cd);
	  }
	line=line->next;
     }
   iconv_close(cd);
#else
   (void) a;
   (void) to_charset;
   (void) from_charset;
#endif
   return 0;
}

 /* It returns 0 if it did not convert, 1 if it did, -1 upon error.
  * Only those lines that have the 8bit flag set will be converted.
  */
int slrn_test_convert_lines (Slrn_Article_Line_Type *rlines, char *to_charset, char *from_charset, char **badlinep)
{
#ifdef HAVE_ICONV
   Slrn_Article_Line_Type *rline;
   Slrn_Article_Line_Type *elines, *eline;
   iconv_t cd;
   int status;

   if ((cd = iconv_open(to_charset, from_charset)) == (iconv_t)(-1))
     return 0;

   elines = eline = NULL;
   rline = rlines;

   status = 0;
   while (rline != NULL)
     {
	Slrn_Article_Line_Type *next;

	if (0 == (rline->flags & LINE_HAS_8BIT_FLAG))
	  {
	     rline = rline->next;
	     continue;
	  }

	next = (Slrn_Article_Line_Type *) slrn_malloc (sizeof(Slrn_Article_Line_Type), 1, 1);
	if (next == NULL)
	  {
	     status = -1;
	     *badlinep = rline->buf;
	     goto free_return;
	  }

	switch (iconv_convert_string (cd, rline->buf, strlen (rline->buf), 1, &next->buf))
	  {
	   case 1:		       /* line converted ok */
	     if (eline == NULL)
	       elines = next;
	     else
	       eline->next = next;
	     eline = next;
	     break;

	   case 0:		       /* failed to convert */
	     if (Slrn_Debug_Fp != NULL)
	       {
		  (void) fprintf (Slrn_Debug_Fp, "*** iconv_convert_string failed to convert:\n");
		  (void) fprintf (Slrn_Debug_Fp, "%s\n", rline->buf);
		  (void) fprintf (Slrn_Debug_Fp, "*** from charset=%s to charset=%s\n", from_charset, to_charset);
		  (void) fflush (Slrn_Debug_Fp);
	       }
	     status = 0;
	     *badlinep = rline->buf;
	     slrn_art_free_line (next);
	     goto free_return;

	   default:
	     status = -1;
	     *badlinep = rline->buf;
	     slrn_art_free_line (next);
	     goto free_return;
	  }
	rline=rline->next;
     }

   /* Converted ok if we get here */
   eline = elines;
   rline = rlines;
   while (rline != NULL)
     {
	if (0 == (rline->flags & LINE_HAS_8BIT_FLAG))
	  {
	     rline = rline->next;
	     continue;
	  }
	slrn_free (rline->buf);
	rline->buf = eline->buf;
	eline->buf = NULL;

	rline->flags &= ~LINE_HAS_8BIT_FLAG;

	rline = rline->next;
	eline = eline->next;
     }
   status = 1;
   /* drop */

free_return:
   iconv_close (cd);
   while (elines != NULL)
     {
	eline = elines;
	elines = elines->next;
	slrn_art_free_line (eline);
     }
   return status;

#else
   (void) rlines;
   (void) to_charset;
   (void) from_charset;
   (void) badlinep;
   return 1;
#endif
}

