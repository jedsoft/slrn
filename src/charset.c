/* -*- mode: C; mode: fold -*- */
/* Charset handling routines.
 *
 * Author: Felix Schueller
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

void slrn_init_charset (void)
{
#if defined(HAVE_LOCALE_H) && defined(HAVE_LANGINFO_H)
  if (Slrn_Display_Charset == NULL)
    {
       setlocale(LC_ALL, "");
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
	if (ret != (size_t) -1)
	  break;

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
	     
	   case 0:		       /* windows bug */
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

char *slrn_convert_substring(char *str, unsigned int offset, unsigned int len, char *to_charset, char *from_charset, int test)
{
#ifdef HAVE_ICONV
   iconv_t cd;
   char *substr;
   int status;
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

   if ((cd = iconv_open(to_charset, from_charset)) == (iconv_t)(-1))
     {
	slrn_error (_("Can't convert %s -> %s\n"), from_charset, to_charset);
	return NULL;
     }

   status = iconv_convert_string (cd, str+offset, len, test, &substr);
   iconv_close(cd);

   if (status == 0)
     return NULL;

   if (status == -1)
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
#else
   (void) str;
   (void) offset;
   (void) len;
   (void) to_charset;
   (void) from_charset;
   (void) test;
   return NULL;
#endif
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

   if(NULL == (*dest = slrn_convert_substring(str, 0, 0, to_charset, from_charset, 0)))
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
   
   if (NULL == (tmp = slrn_convert_substring(str, 0, 0, to_charset, from_charset, 0)))
     {
	slrn_free(str);
	return -1;
     }
   retval = fputs (tmp, fp);
   slrn_free(str);
   slrn_free(tmp);
   
   return retval;
}

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
   
   while ((line != NULL) && (line->flags & HEADER_LINE)) /* Headers are handled extra */
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

/* converts a->raw_lines and stores the encoded lines in a->lines*/
int slrn_test_convert_article(Slrn_Article_Type *a, char *to_charset, char *from_charset)
{
#ifdef HAVE_ICONV
   Slrn_Article_Line_Type *rline, *elines=NULL, *tmp;
   char *nline;
   int error=0;
   
   iconv_t cd;
   
   if ((cd = iconv_open(to_charset, from_charset)) == (iconv_t)(-1))
     {
	slrn_error (_("Can't convert %s -> %s\n"), from_charset, to_charset);
	return -1;
     }
   
   rline=a->raw_lines;
   while (rline != NULL)
     {
	if (slrn_string_nonascii(rline->buf))
	  {
	     error=0;
	     
	     switch (iconv_convert_string(cd, rline->buf, strlen (rline->buf), 1, &nline))
	       {
		case 1:
		  if (elines==NULL)
		    {
		       elines = (Slrn_Article_Line_Type *) slrn_safe_malloc (sizeof(Slrn_Article_Line_Type));
		    }
		  else
		    {
		       elines->next = (Slrn_Article_Line_Type *) slrn_safe_malloc (sizeof(Slrn_Article_Line_Type));
		       elines->next->prev=elines;
		       elines = elines->next;
		    }
		  elines->buf=nline;
		  break;

		case 0:
		  if (elines==NULL)
		    {
		       elines = (Slrn_Article_Line_Type *) slrn_safe_malloc (sizeof(Slrn_Article_Line_Type));
		    }
		  else
		    {
		       elines->next = (Slrn_Article_Line_Type *) slrn_safe_malloc (sizeof(Slrn_Article_Line_Type));
		       elines->next->prev=elines;
		       elines = elines->next;
		    }
		  elines->buf=slrn_safe_strmalloc (rline->buf);
		  break;
		  
		default:
		  if (elines != NULL)
		    {
		       while (elines->prev != NULL)
			 {
			    elines = elines->prev;
			 }
		       slrn_art_free_article_line(elines);
		    }
		  return -1;
		  break;
	       }
	  }
	rline=rline->next;
     }

   if (elines != NULL)
     {
	while (elines->prev != NULL)
	  {
	     elines = elines->prev;
	  }
     }
   
   rline=a->raw_lines;
   while (rline != NULL)
     {
	if (slrn_string_nonascii(rline->buf))
	  {
	     a->cline->next=elines;
	     elines=elines->next;
	     if (elines != NULL)
		  elines->prev=NULL;
	     tmp=rline;
	     rline=rline->next;
	     tmp->next=NULL;
	     tmp->prev=NULL;
	     slrn_art_free_article_line(tmp);
	  }
	else
	  {
	     a->cline->next=rline;
	     rline=rline->next;
	  }
	a->cline->next->prev=a->cline;
	a->cline=a->cline->next;
	a->cline->next=NULL;
     }
   a->raw_lines=NULL;
   return 0;
#else
   (void) a;
   (void) to_charset;
   (void) from_charset;
   return 0;
#endif
}

