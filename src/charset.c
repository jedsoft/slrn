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

#ifdef HAVE_ICONV_H
# include <iconv.h>
#endif


#include <slang.h>

#include "group.h"
#include "art.h"
#include "util.h"
#include "snprintf.h"
#include "mime.h"

char *Slrn_Config_Charset  = NULL;
char *Slrn_Display_Charset  = NULL;
char *Slrn_Editor_Charset  = NULL;
char *Slrn_Outgoing_Charset  = NULL;

void slrn_init_charset()
{
#if defined(HAVE_LOCALE_H) && defined(HAVE_LANGINFO_H)
  if (Slrn_Display_Charset == NULL)
    {
       setlocale(LC_ALL, "");
       Slrn_Display_Charset = slrn_safe_strmalloc (nl_langinfo (CODESET));
    }
#endif
}

void slrn_prepare_charset()
{
  if (Slrn_Display_Charset == NULL)
    {
       Slrn_Display_Charset = slrn_safe_strmalloc ("US-ASCII");
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

#ifdef HAVE_ICONV_H
/* returns the converted string, or NULL on error or if no convertion is needed*/
static char *iconv_convert_string(iconv_t cd, char *in_str, int offset, size_t len, int test, int *error)
{
  char *in_start=in_str;
  char *out_str,*out_start;
  char *ret;
  int n=2;
  size_t in_len, out_len, iconv_ret;
  
  while (n <= 42)
    {
       if (offset != 0)
	 {
	    in_str+= offset;
	 }
       in_len=strlen(in_str);
       if(len)
	 {
	    out_len=in_len + (n-1)*len + offset;
	    in_len=len;
	 }
       else
	 {
	    out_len=n*in_len + offset;
	 }
       out_start=out_str=slrn_safe_malloc(out_len+2);
       if (offset > 0)
	 {
	     strncpy(out_str, in_start, offset);
	     out_str=out_str+(offset);
	     out_len=out_len-offset;
	 }
       iconv_ret=iconv (cd, &in_str, &in_len, &out_str, &out_len);
       while ((in_len != 0) && ((iconv_ret == (size_t) (-1)) && (errno != E2BIG)) )
	 {
	     if (test)
	       {
		  if (error != NULL)
		       *error =1;
		  slrn_free(out_start);
		  return NULL;
	       }
	     *in_str='?';
	     iconv_ret=iconv (cd, &in_str, &in_len, &out_str, &out_len);
	 }
       if (len != 0)
	    in_len=strlen(in_str);
       if ( ( (iconv_ret == (size_t) (-1)) && (errno == E2BIG) )||
		 ((len != 0) && (in_len +1> out_len )) )
	 {
	    in_str=in_start;
	    n++;
	    slrn_free(out_start);
	    out_start=NULL;
	    continue;
	 }
       if (len != 0)
	 {
	    strncpy(out_str, in_str, in_len);
	    out_len -= in_len;
	    out_str += in_len;
	 }
       if(*out_str != '\0')
	 {
	     *out_str='\0';
	     out_str++;
	 }
       break;
    }
  if (out_start==NULL)
    {
       slrn_error (_("Not enough room to store converted string"));
       return NULL;
    }
       
  ret = slrn_safe_strmalloc (out_start);/* one more malloc, but less memory needed */
  slrn_free(out_start);
  return ret;
}
#endif

char *slrn_convert_substring(char *str, int offset, int len, char *to_charset, char *from_charset, int test)
{
#ifdef HAVE_ICONV_H
   iconv_t cd;
   char *tmp;

   if ((cd = iconv_open(to_charset, from_charset)) == (iconv_t)(-1))
     {
	slrn_error (_("Can't convert %s -> %s\n"), from_charset, to_charset);
	return NULL;
     }
   tmp = iconv_convert_string(cd, str, offset, len, test, NULL);
   iconv_close(cd);
   return tmp;
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

   if((*dest = slrn_convert_substring(str, 0, 0, to_charset, from_charset, 0)) == NULL)
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
   
   str = slrn_malloc_vsprintf(format, args);
   va_end (args);
   
   if (!slrn_string_nonascii(str))
     {
	retval = fputs (str, fp);
	slrn_free(str);
	return retval;
     }
   
   if ((tmp = slrn_convert_substring(str, 0, 0, to_charset, from_charset, 0)) == NULL)
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
#ifdef HAVE_ICONV_H
   iconv_t cd;
   char *tmp;
   struct Slrn_Article_Line_Type *line=a->lines;

   if ((cd = iconv_open(to_charset, from_charset)) == (iconv_t)(-1))
     {
	slrn_error (_("Can't convert %s -> %s\n"), from_charset, to_charset);
	return -1;
     }
   
   while ((line != NULL) && (line->flags & HEADER_LINE)) /* Headers are handelt extra */
     {
	line=line->next;
     }
   
   while (line != NULL)
     {
	if ((tmp = iconv_convert_string(cd, line->buf, 0, 0, 0, NULL)) != NULL)
	  {
	     slrn_free((char *) line->buf);
	     line->buf=tmp;
	     a->mime.was_modified=1;
	  }
	line=line->next;
     }
   iconv_close(cd);
   return 0;
#endif
}

/* converts a->raw_lines and stores the encoded lines in a->lines*/
int slrn_test_convert_article(Slrn_Article_Type *a, char *to_charset, char *from_charset)
{
#ifdef HAVE_ICONV_H
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
	     if ((nline = iconv_convert_string(cd, rline->buf, 0, 0, 1, &error)) != NULL)
	       {
		  if (elines==NULL)
		    {
		       elines = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
		    }
		  else
		    {
		       elines->next = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
		       elines->next->prev=elines;
		       elines = elines->next;
		    }
		  elines->buf=nline;
	       }
	     else
	       {
		  if (error==0)
		    {
		       if (elines==NULL)
			 {
			    elines = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
			 }
		       else
			 {
			    elines->next = (Slrn_Article_Line_Type *) slrn_malloc(sizeof(Slrn_Article_Line_Type),1,1);
			    elines->next->prev=elines;
			    elines = elines->next;
			 }
		       elines->buf=slrn_safe_strmalloc(rline->buf);
		    }
		  else
		    {
		       if (elines != NULL)
			 {
			    while (elines->prev != NULL)
			      {
				 elines = elines->prev;
			      }
		       
			    slrn_art_free_article_line(elines);
			 }
		       return -1;
		    }
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
#endif
}

