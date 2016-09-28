/* -*- mode: C; mode: fold; -*- */
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
#include <ctype.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <slang.h>

#define MAX_ARTICLE_LINE_LEN 4096
#include "jdmacros.h"
#include "decode.h"
#include "util.h"
#include "snprintf.h"
#include "ttymsg.h"
#include "slrndir.h"
#include "strutil.h"
#include "common.h"

#ifdef STANDALONE
# include <stdarg.h>

# if SLRN_HAS_PIPING
#  define SLRN_POPEN uudecode_popen
#  define SLRN_PCLOSE uudecode_pclose
# endif
# define slrn_message_now slrn_tty_message
# define slrn_message slrn_tty_message
int Slrn_UTF8_Mode = 0;

#if 0
static FILE *uudecode_popen (char *cmd, char *mode) /*{{{*/
{
# if SLRN_HAS_PIPING
   return popen (cmd, mode);
# else
   slrn_error (_("Piping not implemented on this system."));
   return NULL;
# endif
}

/*}}}*/

static int uudecode_pclose (FILE *fp) /*{{{*/
{
# if SLRN_HAS_PIPING
   return pclose (fp);
# else
   return -1;
# endif
}

/*}}}*/
#endif /* 0 */
#else
# include <slang.h>
# include "jdmacros.h"
# include "slrn.h"
# include "misc.h"
# include "decode.h"
# define SLRN_POPEN slrn_popen
# define SLRN_PCLOSE slrn_pclose
char *Slrn_Decode_Directory;
#endif

#ifdef VMS
# include "vms.h"
#endif

static int uudecode_fclose (FILE *fp, FILE *pipe_fp) /*{{{*/
{
   if ((fp == pipe_fp) || (fp == NULL))
     return 0;

#ifdef STANDALONE
   if (0 == fclose (fp)) return 0;
   slrn_error (_("Error closing file.  File system full?"));
   return -1;
#else
   return slrn_fclose (fp);
#endif
}

/*}}}*/

/* This function can be dangerous, so we currently don't use it. */
#if 0
static int unpack_as_shell_archive (FILE *fp, char *buf, int size) /*{{{*/
{
   FILE *pp;

   size--;
   pp = SLRN_POPEN ("/bin/sh", "w");
   if (pp == NULL)
     {
	slrn_error (_("Unable to open /bin/sh\n"));
	return -1;
     }

   while (fgets (buf, size, fp) != NULL)
     {
	fputs (buf, pp);
	if (!strcmp (buf, "exit 0\n"))
	  break;
     }

   if (-1 == SLRN_PCLOSE (pp))
     {
	slrn_error (_("Error encountered while processing shell archive.\n"));
	return -1;
     }
   return 0;
}

/*}}}*/
#endif

static int file_exists (char *file)
{
   struct stat st;

   if (-1 == stat (file, &st))
     return 0;

   return 1;
}

/* Note: this function modifies name */
static FILE *open_output_file (char *name, char *type, int mode, FILE *use_this) /*{{{*/
{
   FILE *fp;
   char *p;
   char *file = NULL;

   if (use_this != NULL)
     return use_this;

   p = name;
   while (*p != 0)
     {
	if (*p == '/')
	  *p = '_';
	p++;
     }

   if (1 == file_exists (name))
     {
	unsigned int i;
	unsigned int len = strlen (name);

	len += 1 + 4 + 1;
	if (NULL == (file = slrn_malloc (len, 0, 1)))
	  return NULL;

	for (i = 1; i <= 9999; i++)
	  {
	     (void) slrn_snprintf (file, len, "%s-%d", name, i);
	     if (0 == file_exists (file))
	       {
		  name = file;
		  break;
	       }
	  }

	if (name != file)
	  {
	     slrn_free (file);
	     slrn_error(_("Unable to create %s\n"), name);
	     return NULL;
	  }
     }

   fp = fopen (name, "wb");

   if (fp == NULL)
     {
	slrn_error(_("Unable to create %s\n"), name);
	if (file != NULL) slrn_free (file);
	return NULL;
     }

   slrn_message_now (_("creating %s (%s)\n"), name, type);

   if (mode != -1)
     chmod (name, mode);

   if (file != NULL) slrn_free (file);
   return fp;
}

/*}}}*/

static unsigned char Base64_Table [256];

static void initialize_base64 (void) /*{{{*/
{
   int i;

   for (i = 0; i < 256; i++) Base64_Table[i] = 0xFF;

   for (i = 'A'; i <= 'Z'; i++) Base64_Table[i] = (unsigned char) (i - 'A');
   for (i = 'a'; i <= 'z'; i++) Base64_Table[i] = 26 + (unsigned char) (i - 'a');
   for (i = '0'; i <= '9'; i++) Base64_Table[i] = 52 + (unsigned char) (i - '0');
   Base64_Table['+'] = 62;
   Base64_Table['/'] = 63;
}

/*}}}*/

/* returns 0 if entire line was decoded, or 1if line appears to be padded, or
 * -1 if line looks bad.
 */
/* Calling routine guarantees at least 4 characters in line and multiple of 4 */
static int base64_decode_line (char *line, FILE *fp) /*{{{*/
{
   unsigned char ch;
   unsigned char ch1;
   unsigned char bytes[3];
   unsigned char *p;

   p = (unsigned char *) line;
   /* Perform simple syntax check.  The loop following this one
    * assumes this.
    */
   while ((ch = *p++) != 0)
     {
	if (Base64_Table[ch] == 0xFF)
	  return -1;

	ch = *p++;
	if (Base64_Table[ch] == 0xFF)
	  return -1;

	ch = *p++;
	if ((Base64_Table[ch] == 0xFF) && (ch != '='))
	  {
	     return -1;
	  }

	ch = *p++;
	if ((Base64_Table[ch] == 0xFF) && (ch != '='))
	  {
	     return -1;
	  }
     }

   while ((ch = (unsigned char) *line++) != 0)
     {
	ch1 = Base64_Table[ch];
	bytes[0] = ch1 << 2;

	ch = *line++;
	ch1 = Base64_Table[ch];

	bytes[0] |= ch1 >> 4;
	bytes[1] = ch1 << 4;

	ch = *line++;
	ch1 = Base64_Table[ch];
	if (ch1 == 0xFF)
	  {
	     fwrite (bytes, 1, 1, fp);
	     return 1;
	  }

	bytes[1] |= ch1 >> 2;
	bytes[2] = ch1 << 6;

	ch = *line++;
	ch1 = Base64_Table[ch];
	if (ch1 == 0xFF)
	  {
	     fwrite (bytes, 1, 2, fp);
	     return 1;
	  }
	bytes[2] |= ch1;
	fwrite (bytes, 1, 3, fp);
     }

   return 0;
}

/*}}}*/

static char *fgets_with_counter (char *s, int size, FILE* stream, /*{{{*/
				 unsigned int *c)
{
   char *retval;
   int len;

   retval = fgets (s, size, stream);
   len = strlen (s);
   if (len && (s[len - 1] == '\n'))
     {
	s[--len] = 0;
	if (len && (s[len - 1] == '\r'))
	  s[--len] = 0;
     }
   if (c != NULL)
     *c = len;

   return retval;
}

/*}}}*/

static char Base64_Filename[256];
static int Base64_Unknown_Number;

/* Returns 1 if padded line which indicates end of encoded file, 0 if
 * decoded something or -1 if nothing.
 */
static int decode_base64 (FILE *fp, FILE *fpout, char *line, unsigned int buflen) /*{{{*/
{
   int decoding = 0;
   unsigned int len;
   int ret;

   while (NULL != fgets_with_counter (line, buflen, fp, &len))
     {
	if (Base64_Table[(unsigned char) *line] == 0xFF)
	  {
	     if (decoding) return 0;
	     else return -1;
	  }

	if ((len % 4) != 0)
	  {
	     if (decoding) return 0;
	     else return -1;
	  }

	ret = base64_decode_line (line, fpout);
	if (ret)
	  {
	     if ((ret == -1) && decoding) ret = 0;
	     return ret;
	  }

	decoding = 1;
     }

   return 0;
}

/*}}}*/

static char *skip_whitespace (char *p) /*{{{*/
{
   while ((*p == ' ') || (*p == '\t') || (*p == '\n')) p++;
   return p;
}

/*}}}*/

static char *skip_header_string (char *p, char *name) /*{{{*/
{
   unsigned int len;

   p = skip_whitespace (p);
   len = strlen (name);

   if (0 != slrn_case_strncmp (p, name, len))
     return NULL;

   return p + len;
}

/*}}}*/

static char *skip_beyond_name_eqs (char *p, char *name) /*{{{*/
{
   int ch;
   unsigned int len;

   len = strlen (name);

   while (1)
     {
	p = skip_whitespace (p);
	if (*p == 0) return NULL;

	if (0 != slrn_case_strncmp ( p,  name, len))
	  {
	     while (((ch = *p) != 0) && (ch != ' ') && (ch != '\t') && (ch != '\n'))
	       p++;
	     continue;
	  }

	p = skip_whitespace (p + len);
	if (*p != '=') continue;

	p = skip_whitespace (p + 1);

	if (*p == 0)
	  return NULL;

	return p;
     }
}

/*}}}*/

static int parse_name_eqs_int (char *p, char *name, int *val) /*{{{*/
{
   int ch;
   int ival;

   p = skip_beyond_name_eqs (p, name);
   if (p == NULL) return -1;

   ival = 0;

   while (((ch = *p) != 0) && isdigit(ch))
     {
	ival = ival * 10 + (ch - '0');
	p++;
     }

   *val = ival;
   return 0;
}

/*}}}*/

static int parse_name_eqs_string (char *p, char *name, char *str, unsigned int len) /*{{{*/
{
   char ch;
   char *pmax;
   int quote = 0;

   p = skip_beyond_name_eqs (p, name);
   if (p == NULL) return -1;

   if (*p == '"')
     {
	quote = 1;
	p++;
     }

   pmax = (p + len) - 1;

   while ((p < pmax)
	  && ((ch = *p) != '"')
	  && (ch != '\n')
	  && (ch != 0))
     {
	if ((quote == 0) && ((ch == ' ') || (ch == '\t') || (ch == ';')))
	  break;

	*str++ = ch;
	p++;
     }
   *str = 0;

   return 0;
}

/*}}}*/

static void parse_content_disposition (char *p) /*{{{*/
{
   (void) parse_name_eqs_string (p, "filename", Base64_Filename, sizeof (Base64_Filename));
}

/*}}}*/

static int is_encoding_base64 (char *p) /*{{{*/
{
   p = skip_whitespace (p);
   if (slrn_case_strncmp (p, "base64", 6))
     return 0;
   return 1;
}

/*}}}*/

/* Looking for message/partial; id="bla-bla"; number=10; total=20
 *  The following is from RFC 1522.  It also illustrates what is wrong with
 *  MIME:
 *
 *  Three parameters must be specified in the Content-Type field of type
 *  message/partial: The first, "id", is a unique identifier, as close to
 *  a world-unique identifier as possible, to be used to match the parts
 *  together.  (In general, the identifier is essentially a message-id;
 *  if placed in double quotes, it can be any message-id, in accordance
 *  with the BNF for "parameter" given earlier in this specification.)
 *  The second, "number", an integer, is the part number, which indicates
 *  where this part fits into the sequence of fragments.  The third,
 *  "total", another integer, is the total number of parts. This third
 *  subfield is required on the final part, and is optional (though
 *  encouraged) on the earlier parts.  Note also that these parameters
 *  may be given in any order.
 *
 *  Thus, part 2 of a 3-part message may have either of the following
 *  header fields:
 *
 *		 Content-Type: Message/Partial;
 *		      number=2; total=3;
 *		      id="oc=jpbe0M2Yt4s@thumper.bellcore.com"
 *
 *		 Content-Type: Message/Partial;
 *		      id="oc=jpbe0M2Yt4s@thumper.bellcore.com";
 *		      number=2
 *
 *  But part 3 MUST specify the total number of parts:
 *
 *		 Content-Type: Message/Partial;
 *		      number=3; total=3;
 *		      id="oc=jpbe0M2Yt4s@thumper.bellcore.com"
 *
 *  Note that part numbering begins with 1, not 0.
 */

typedef struct /*{{{*/
{
   int number;		       /* >= 1 */
   int total;		       /* not required!! */
   char id[256];		       /* required */
}

/*}}}*/

Mime_Message_Partial_Type;

static int parse_content_type (char *p, Mime_Message_Partial_Type *mpt) /*{{{*/
{
#if 1
   (void) parse_name_eqs_string (p, "name",
				 Base64_Filename, sizeof (Base64_Filename));
#endif
   if ((NULL == (p = skip_header_string (p, "message")))
       || (NULL == (p = skip_header_string (p, "/")))
       || (NULL == (p = skip_header_string (p, "partial")))
       || (NULL == (p = skip_header_string (p, ";"))))
     return -1;

   if (-1 == parse_name_eqs_int (p, "number", &mpt->number))
     return -1;

   if (-1 == parse_name_eqs_int (p, "total", &mpt->total))
     mpt->total = 0;		       /* not reqired on all parts */

   if (-1 == parse_name_eqs_string (p, "id", mpt->id, sizeof (mpt->id)))
     {
	*mpt->id = 0;
	return -1;
     }

   return 0;
}

/*}}}*/

static void read_in_whole_header_line (char *line, FILE *fp) /*{{{*/
{
   unsigned int len;
   unsigned int space;
   int c;

   /* We have already read in one line.  Prepare to read in continuation lines */
   len = strlen (line);
   space = MAX_ARTICLE_LINE_LEN - len;

   while (space)
     {
	c = getc (fp);
	c = ungetc (c, fp);
	if (c == EOF) return;

	if ((c != ' ') && (c != '\t'))
	  {
	     return;
	  }

	line += len;
	if (NULL == fgets (line, space, fp)) return;

	len = strlen (line);
	space -= len;
     }
}

/*}}}*/

static int check_and_decode_base64 (FILE *fp, FILE *pipe_fp) /*{{{*/
{
   char *p;
   int found_base64_signature = 0;
   FILE *fpout = NULL;
   char line[MAX_ARTICLE_LINE_LEN];
   Mime_Message_Partial_Type *current_pmt, pmt_buf;
   int part = 1;

   initialize_base64 ();

   current_pmt = NULL;

   while (NULL != fgets_with_counter (line, sizeof (line), fp, NULL))
     {
	try_again:

	if (*line == 0)
	  {
	     if (found_base64_signature)
	       {
		  int ret;

		  if (*Base64_Filename == 0)
		    {
		       slrn_snprintf (Base64_Filename,
				      sizeof (Base64_Filename),
				      "unknown-64.%d", Base64_Unknown_Number);
		       Base64_Unknown_Number++;
		    }

		  if (fpout == NULL)
		    fpout = open_output_file (Base64_Filename, "base64", -1, pipe_fp);

		  if (fpout == NULL) return -1;

		  ret = decode_base64 (fp, fpout, line, sizeof (line));
		  /* decode_base64 performs fgets.  So if nothing was decoded
		   * skip fgets at top of loop
		   */
		  if (ret == -1) goto try_again;

		  if ((current_pmt == NULL)
		      || (current_pmt->number == current_pmt->total)
		      || (part != current_pmt->number)
		      || (ret == 1))
		    {
		       uudecode_fclose (fpout, pipe_fp);
		       fpout = NULL;
		       found_base64_signature = 0;
		       current_pmt = NULL;
		       part = 0;
		       *Base64_Filename = 0;
		    }
		  part++;
	       }
	     continue;
	  }

	/* Look for 'Content-' */

	if (slrn_case_strncmp ( "Content-",  line, 8))
	  continue;

	read_in_whole_header_line (line, fp);

	p = line + 8;

	if (0 == slrn_case_strncmp ( p,  "Transfer-Encoding: ", 19))
	  {
	     if (is_encoding_base64 (p + 19))
	       {
		  if (found_base64_signature)
		    {
		       if (fpout != NULL)
			 {
			    uudecode_fclose (fpout, pipe_fp);
			    fpout = NULL;
			    current_pmt = NULL;
			    *Base64_Filename = 0;
			    part = 1;
			 }
		    }
		  found_base64_signature = 1;
	       }
	     continue;
	  }

	if (0 == slrn_case_strncmp (p, "Disposition: ", 13))
	  {
	     parse_content_disposition (p + 13);
	     continue;
	  }

	if (0 == slrn_case_strncmp (p, "Type: ", 6))
	  {
	     Mime_Message_Partial_Type pmt;

	     if (-1 == parse_content_type (p + 6, &pmt))
	       {
		  /* current_pmt = NULL; */
		  continue;
	       }

	     if ((current_pmt == NULL)
		 || (pmt.number == 1)
		 || (part != pmt.number)
		 || strcmp (current_pmt->id, pmt.id))
	       {
		  if (fpout != NULL)
		    {
		       uudecode_fclose (fpout, pipe_fp);
		       fpout = NULL;
		       *Base64_Filename = 0;
		       found_base64_signature = 0;
		       part = 1;
		    }
	       }
	     current_pmt = &pmt_buf;
	     memcpy ((char *) current_pmt, (char *) &pmt, sizeof (Mime_Message_Partial_Type));
	     continue;
	  }
     }

   uudecode_fclose (fpout, pipe_fp);
   return 0;
}

/*}}}*/

static int parse_uuencode_begin_line (char *b, char **file, int *mode) /*{{{*/
{
   int m = 0;
   char *bmax;

   while (*b == ' ') b++;
   if (0 == isdigit (*b)) return -1;
   while (isdigit (*b))
     {
	m = m * 8 + (*b - '0');
	b++;
     }
   if (*b != ' ') return -1;

   while (*b == ' ') b++;

   if (*b < ' ') return -1;

   bmax = b + strlen (b);
   while ((unsigned char)*bmax <= ' ') *bmax-- = 0;

   *mode = m;
   *file = (char *) b;
   return 0;
}

/*}}}*/

static int check_and_uudecode (FILE *fp, FILE *pipe_fp,
			       int no_slop) /*{{{*/
{
   FILE *outfp = NULL;
   int is_mbox = -1;
   char buf[MAX_ARTICLE_LINE_LEN];

   while (NULL != fgets (buf, sizeof (buf), fp))
     {
	char *file;
	int mode = 0;
	unsigned int buflen;

	if (is_mbox == -1)
	  {
	     if ((!strncmp (buf, "From", 4)) &&
		 ((unsigned char)buf[4] <= ' '))
	       {
		  is_mbox = 1;
		  continue;
	       }
	     else
	       is_mbox = 0;
	  }

	/* We are looking for 'begin ddd filename' */
	if (strncmp (buf, "begin ", 6))
	  {
	     /* Don't unpack shell archives - they could do anything! */
#if 0
	     if ((buf[0] == '#') && (buf[1] == '!'))
	       {
		  char *binsh = buf + 2;
		  if (*binsh == ' ') binsh++;
		  if (!strcmp (binsh, "/bin/sh\n"))
		    {
		       unpack_as_shell_archive (fp, buf, sizeof (buf));
		    }
	       }
#endif
	     continue;
	  }

	if (-1 == parse_uuencode_begin_line (buf + 6, &file, &mode))
	  continue;

	if (NULL == (outfp = open_output_file (file, "uuencoded", mode, pipe_fp)))
	  return -1;

	/* Now read parts of the file in. */
	while (fgets_with_counter (buf, sizeof(buf) - 1, fp, &buflen) != NULL)
	  {
	     unsigned int len, write_len;
	     unsigned char out[60], *outp, *outmax, *b, *bmax;

	     if ((is_mbox) && (!strncmp (buf, "From", 4)) &&
		 ((unsigned char)buf[4] <= ' '))
	       {
		  do
		    {
		       if (NULL == fgets_with_counter (buf, sizeof (buf) - 1,
						       fp, &buflen))
			 {
			    slrn_error (_("Unexpected end of file.\n"));
			    return -1;
			 }
		    }
		  while (buflen);
		  continue;
	       }

	     if (((*buf & 0x3F) == ' ') && ((*buf + 1) == '\0'))
	       {
		  /* we may be on the last line before the end.  Lets check */
		  if (NULL == fgets_with_counter (buf, sizeof(buf) - 1, fp,
						  &buflen))
		    {
		       slrn_error (_("Unexpected end of file.\n"));
		       return -1;
		    }

		  if (!strcmp (buf, "end"))
		    {
		       uudecode_fclose (outfp, pipe_fp);
		       outfp = NULL;
		       break;
		    }
	       }

	     /* Now perform some sanity checking */
	     len = *buf;

	     no_slop = 0;
	     if ((len <= ' ')
		 || (len >= (60 + ' '))
		 || (buflen > 61) || (buflen < 1)
		 || (no_slop
		     && (((buflen - 1) % 4) != 0)))
	       {
#if 0
		  fprintf (stderr, "%s", buf);
#endif
		  if (strncmp (buf, "begin ", 6)
		      || (-1 == parse_uuencode_begin_line (buf + 6,
							   &file, &mode)))
		    continue;

		  uudecode_fclose (outfp, pipe_fp);

		  if (NULL == (outfp = open_output_file (file, "uuencoded", mode, pipe_fp)))
		    return -1;

		  continue;
	       }

	     buflen--;
	     len -= ' ';

	     /* In general, I cannot make the test:
	      * @ if ((3 * buflen) != (len * 4)) continue;
	      * The problem is that the last line may be padded.  Instead, do
	      * it just for normal length lines and handle padding for others.
	      */

	     if (*buf == 'M')
	       {
		  if ((3 * buflen) != (len * 4)) continue;
#if 0
		  if (0 == slrn_case_strncmp (buf, "Message-Id:", 11))
		    continue;
#endif
	       }
	     else
	       {
		  unsigned int buflen1 = ((len + 2) / 3) * 4;
		  if (buflen != buflen1)
		    {
		       if (no_slop) continue;
#if 0
#ifdef STANDALONE
		       slrn_message_now (_("uuencode line appears corrupt: \"%s\""),
					 buf);
#endif
#endif
		       continue;
		    }
	       }
	     b = (unsigned char *) buf + 1;
	     bmax = b + buflen;

	     while (b < bmax)
	       {
		  *b = (*b - 32) & 0x3F;
		  *(b + 1) = (*(b + 1) - 32) & 0x3F;
		  *(b + 2) = (*(b + 2) - 32) & 0x3F;
		  *(b + 3) = (*(b + 3) - 32) & 0x3F;
		  b += 4;
	       }

	     b = (unsigned char *) buf + 1;

	     outp = out;
	     outmax = outp + len;
	     while (outp < outmax)
	       {
		  register unsigned char b1, b2;
		  b1 = *(b + 1);
		  b2 = *(b + 2);

		  *outp++ = (*b << 2) | (b1 >> 4);

		  if (outp < outmax)
		    {
		       *outp++ = (b1 << 4) | (b2 >> 2);
		       if (outp < outmax)
			 *outp++ = (b2 << 6) | *(b + 3);
		    }
		  b += 4;
	       }

	     if (len != (write_len = fwrite ((char *) out, 1, len, outfp)))
	       {
		  slrn_error (_("write to file failed (%u/%u bytes)\n"),
			      write_len, len);
		  uudecode_fclose (outfp, pipe_fp);
		  return -1;
	       }
	  }
	/* end of part reading */

	/* back to looking for something else */
     }

   if (outfp != NULL)
     {
	uudecode_fclose (outfp, pipe_fp);
	outfp = NULL;
     }
   return 0;
}

/*}}}*/

static int decode_best_guess (FILE *fp, FILE *pipe_fp, int no_slop) /*{{{*/
{
   int ch;
   int nl_seen = 1;
   char *mime_ptr;

   while (EOF != (ch = getc (fp)))
     {
	if (ch == '\n')
	  {
	     nl_seen = 1;
	     continue;
	  }

	if (nl_seen == 0)
	  continue;

	if (ch == 'b')
	  {
	     ungetc (ch, fp);
	     return check_and_uudecode (fp, pipe_fp, no_slop);
	  }

	nl_seen = 0;

	mime_ptr = "mime-version:";
	while (1)
	  {
	     if ((ch | 0x20) != *mime_ptr)
	       break;

	     mime_ptr++;
	     if (*mime_ptr == ':')
	       {
		  return check_and_decode_base64 (fp, pipe_fp);
	       }

	     ch = getc (fp);

	     if (ch == EOF) break;
	     if (ch == '\n')
	       {
		  nl_seen = 1;
		  break;
	       }
	  }
     }
   return 0;
}

/*}}}*/

int slrn_uudecode_file (char *file, char *newdir, /*{{{*/
			int base64_only,
			FILE *pipe_fp)
{
   FILE *fp;
#ifndef STANDALONE
   char olddir_buf[SLRN_MAX_PATH_LEN + 1];
   char newdir_buf[SLRN_MAX_PATH_LEN + 1];
#endif
   int ret;
   int no_slop = 0;		       /* Set to 1 for up-to-spec files */

#ifndef STANDALONE
   if (newdir == NULL)
     {
	newdir = Slrn_Decode_Directory;
	if (newdir == NULL)
	  newdir = "News";

	slrn_make_home_dirname (newdir, newdir_buf, sizeof (newdir_buf));
	newdir = newdir_buf;
     }
#endif

#ifndef STANDALONE
   if (NULL == slrn_getcwd (olddir_buf, sizeof(olddir_buf)))
     return -1;
#endif

   if ((newdir != NULL)
       && (-1 == slrn_chdir(newdir)))
     return -1;

   if (file == NULL) fp = stdin;
   else fp = fopen (file, "r");

   if (fp == NULL)
     {
	slrn_error(_("Unable to open %s.\n"), file);
#ifndef STANDALONE
	(void) slrn_chdir (olddir_buf);
#endif
	return -1;
     }

   if (base64_only == -1)
     ret = decode_best_guess (fp, pipe_fp, no_slop);
   else if (base64_only == 0)
     {
	ret = check_and_uudecode (fp, pipe_fp, no_slop);
	if ((ret == 0) && (fp != stdin))
	  {
	     rewind (fp);
	     ret = check_and_decode_base64 (fp, pipe_fp);
	  }
     }
   else ret = check_and_decode_base64 (fp, pipe_fp);

   if (fp != stdin)
     fclose (fp);

#ifdef STANDALONE
   if (pipe_fp == NULL) slrn_message (_("No more files found.\n"));
#endif

#ifndef STANDALONE
   if (-1 == slrn_chdir (olddir_buf))
     {
	slrn_error (_("Unable to chdir back to %s.\n"), olddir_buf);
	ret = -1;
     }
#endif

   return ret;
}

/*}}}*/

#ifdef STANDALONE
static void usage (void) /*{{{*/
{
   fprintf (stderr, _("Usage: uudecode [-64] [--stdout] [--guess] [filename ...]\n"));
}

/*}}}*/

void slrn_error (char *fmt, ...)
{
   va_list ap;

   if (fmt != NULL)
     {
	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	va_end (ap);
     }
   fputc ('\n', stderr);
}

void slrn_exit_error (char *fmt, ...)
{
   va_list ap;

   if (fmt != NULL)
     {
	va_start (ap, fmt);
	vfprintf (stderr, fmt, ap);
	va_end (ap);
     }

   fputc ('\n', stderr);
   exit (1);
}

int main (int argc, char **argv) /*{{{*/
{
   int i;
   int base64_only = 0;
   FILE *pipe_fp = NULL;

#if defined(HAVE_SETLOCALE) && defined(LC_ALL)
   (void) setlocale(LC_ALL, "");
#endif
#ifdef ENABLE_NLS
   bindtextdomain(NLS_PACKAGE_NAME,NLS_LOCALEDIR);
   textdomain(NLS_PACKAGE_NAME);
#endif

   SLang_init_case_tables ();

   while (argc > 1)
     {
	if (!strcmp ("--help", argv[1]))
	  {
	     usage ();
	     return 1;
	  }

	if (!strcmp ("-64", argv[1]))
	  {
	     base64_only = 1;
	     argc--;
	     argv++;
	  }
	else if (!strcmp ("--stdout", argv[1]))
	  {
	     pipe_fp = stdout;
	     argc--;
	     argv++;
	  }
	else if (!strcmp ("--guess", argv[1]))
	  {
	     base64_only = -1;
	     argc--;
	     argv++;
	  }
	else break;
     }

   if (argc <= 1)
     {
	if (0 == isatty (fileno(stdin)))
	  {
	     return slrn_uudecode_file (NULL, NULL, base64_only, pipe_fp);
	  }
	usage ();
	return 1;
     }

   for (i = 1; i < argc; i++)
     {
	slrn_uudecode_file (argv[i], NULL, base64_only, pipe_fp);
     }
   return 0;
}

/*}}}*/

#endif
