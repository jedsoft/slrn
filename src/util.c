/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999 John E. Davis <davis@space.mit.edu>
 Copyright (c) 2002 Thomas Schultz <tststs@gmx.de>

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
#include "slrnfeat.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifndef VMS
# include <sys/types.h>
# include <sys/stat.h>
#else
# include "vms.h"
#endif

#ifdef __WIN32__
# include <windows.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include "util.h"
#include "ttymsg.h"
#include "snprintf.h"

/* This function allows NULL as a parameter. This fact _is_ exploited */
char *slrn_skip_whitespace (char *b) /*{{{*/
{
   if (b == NULL) return NULL;
   
   while (isspace (*b))
     b++;

   return b;
}

/*}}}*/

char *slrn_bskip_whitespace (char *smin)
{
   char *s;

   if (smin == NULL) return NULL;
   s = smin + strlen (smin);

   while (s > smin)
     {
	s--;
	if (0 == isspace(*s))
	  return s + 1;
     }
   return s;
}

   
/* returns a pointer to the end of the string */
char *slrn_trim_string (char *s) /*{{{*/
{
   s = slrn_bskip_whitespace (s);
   if (s != NULL)
     *s = 0;

   return s;
}
/*}}}*/

char *slrn_strchr (char *s, char ch) /*{{{*/
{
   register char ch1;
   
   while (((ch1 = *s) != 0) && (ch != ch1)) s++;
   if (ch1 == 0) return NULL;
   return s;
}

/*}}}*/

/* Search for characters from list in string str.  If found, return a pointer
 * to the first occurrence.  If not found, return NULL. */
char *slrn_strbrk (char *str, char *list) /*{{{*/
{
   char ch, ch1, *p;
   
   while ((ch = *str) != 0)
     {
	p = list;
	while ((ch1 = *p) != 0)
	  {
	     if (ch == ch1) return str;
	     p++;
	  }
	str++;
     }
   return NULL;
}

/*}}}*/

char *slrn_simple_strtok (char *s, char *chp) /*{{{*/
{
   static char *s1;
   char ch = *chp;
   
   if (s == NULL)
     {
	if (s1 == NULL) return NULL;
	s = s1;
     }
   else s1 = s;
   
   while (*s1 && (*s1 != ch)) s1++;
   
   if (*s1 == 0)
     {
	s1 = NULL;
     }
   else *s1++ = 0;
   return s;
}

/*}}}*/


int slrn_case_strncmp (unsigned char *a, register unsigned char *b, register unsigned int n) /*{{{*/
{
   register unsigned char cha, chb, *bmax;
   
   bmax = b + n;
   while (b < bmax)
     {
	cha = UPPER_CASE(*a);
	chb = UPPER_CASE(*b);
	if (cha != chb)
	  {
	     return (int) cha - (int) chb;
	  }
	else if (chb == 0) return 0;
	b++;
	a++;
     }
   return 0;
}

/*}}}*/

int slrn_case_strcmp (unsigned char *a, register unsigned char *b) /*{{{*/
{
   register unsigned char cha, chb;
   
   while (1)
     {
	cha = UPPER_CASE(*a);
	chb = UPPER_CASE(*b);
	if (cha != chb)
	  {
	     return (int) cha - (int) chb;
	  }
	else if (chb == 0) break;
	b++;
	a++;
     }
   return 0;
}

/*}}}*/

#if defined(IBMPC_SYSTEM)
void slrn_os2_convert_path (char *path)
{
   char ch;
   while ((ch = *path) != 0)
     {
	if (ch == '/') *path = SLRN_PATH_SLASH_CHAR;
	path++;
     }
}
#endif

#ifdef __CYGWIN__
/* return values:
 *  0  no error
 * -1  misplaced or too many colons
 * -2  outpath too short
 */
int slrn_cygwin_convert_path (char *inpath, char *outpath, size_t n)
{
   unsigned int outlen;
   char *p;
   
   /*
    **  first, a quick sanity check to look for invalid formats
    */
   p = strrchr (inpath, ':');        /* we'll re-use 'p' later on */
   if (p && (p != inpath+1))
     return -1;
   
   /*
    **  let's do some size checking
    */
   outlen = strlen (inpath) + 1;
   if (p)
     outlen += 9;        /* "/cygdrive/c" (11) replaces "c:" (2) ==> 9 */
   
   if (n < outlen)
     return -2;
   
   /*
    **  paths starting with C:, (or D:, etc) should be converted to the
    **  Cygwin native format /cygdrive/c (or /cygdrive/d, etc) while copying
    **  source (inpath) to destination (outpath)
    */
   if (p)
     {
        strcpy (outpath, "/cygdrive/");
        strncat (outpath, inpath, 1);
        strcat (outpath, p+1); /* safe */
     }
   else
     strcpy (outpath, inpath); /* safe */

   /*
    **  go through outpath character by character and change all backslashes
    **  to forward slashes
    */
   p = outpath;
   while (*p)
     {
        if (*p == '\\')
	  *p = '/';
        p++;
     }
   
   /*
    **  normal return
    */
   return 0;
}
#endif

#ifdef SLRN_USE_OS2_FAT
void slrn_os2_make_fat (char *file, size_t n, char *name, char *ext)
{
   static char drive[3] = " :";
   char fsys[5];

   slrn_strncpy (file, name, n);
   if (isalpha(file[0]) && (file[1] == ':'))
     drive[0] = file[0];
   else
     drive[0] = _getdrive();

   if ((0 == _filesys (drive, fsys, sizeof (fsys)))
       && (0 == stricmp (fsys, "FAT")))
     {
	/* FAT */
	_remext (file);                      /* Remove the extension */
     }

   if (strlen (file) + strlen (ext) < n)
     strcat (file, ext); /* safe */
}
#endif

static void fixup_path (char *path) /*{{{*/
{
#ifndef VMS
   unsigned int len;
   
   len = strlen (path);
   if (len == 0) return;
# ifdef IBMPC_SYSTEM
   slrn_os2_convert_path (path);
# endif
   if (path[len - 1] == SLRN_PATH_SLASH_CHAR) return;
   path[len] = SLRN_PATH_SLASH_CHAR;
   path[len + 1] = 0;
#endif
}

/*}}}*/

/* dir and file could be the same in which case this performs a strcat. 
 * If name looks like an absolute path, it will be returned.
 */
int slrn_dircat (char *dir, char *name, char *file, size_t n)
{
   unsigned int len = 0;
#ifdef __CYGWIN__
   char convdir [SLRN_MAX_PATH_LEN];
#endif
   
   if (name != NULL) 
     {
	if (slrn_is_absolute_path (name))
	  {
#ifdef __CYGWIN__
	     if (slrn_cygwin_convert_path (name, file, n))
#endif
	     slrn_strncpy (file, name, n);
#if defined(IBMPC_SYSTEM) && !defined(__CYGWIN__)
	     slrn_os2_convert_path (file);
#endif
	     return 0;
	  }
	
	len = strlen (name);
     }
   
   if (dir != NULL) len += strlen (dir);
   
   len += 2;			       /* for / and \0 */
   if (len > n)
     {
	slrn_error (_("File name too long."));
	return -1;
     }
   
   if (dir != NULL) 
     {
	if (dir != file) strcpy (file, dir); /* safe */
	fixup_path (file);
     }
   else *file = 0;

   if (name != NULL) strcat (file, name); /* safe */
#ifdef __CYGWIN__
   if ((0 == slrn_cygwin_convert_path (file, convdir, sizeof (convdir))) &&
       (strlen (convdir) < n))
     strcpy (file, convdir); /* safe */
   else
     slrn_error ("Cygwin conversion of %s failed.", file);
#else
# if defined(IBMPC_SYSTEM)
   slrn_os2_convert_path (file);
# endif
#endif
   return 0;
}

/*{{{ Memory Allocation Routines */

static char *do_malloc_error (int do_error)
{
   if (do_error) slrn_error (_("Memory allocation failure."));
   return NULL;
}

char *slrn_safe_strmalloc (char *s) /*{{{*/
{
   s = SLmake_string (s);
   if (s == NULL) slrn_exit_error (_("Out of memory."));
   return s;
}

/*}}}*/

char *slrn_strnmalloc (char *s, unsigned int len, int do_error)
{
   s = SLmake_nstring (s, len);
   
   if (s == NULL)
     return do_malloc_error (do_error);
   
   return s;
}

char *slrn_strmalloc (char *s, int do_error)
{
   if (s == NULL) return NULL;
   return slrn_strnmalloc (s, strlen (s), do_error);
}


char *slrn_malloc (unsigned int len, int do_memset, int do_error)
{   
   char *s;
   
   s = (char *) SLmalloc (len);
   if (s == NULL)
     return do_malloc_error (do_error);

   if (do_memset)
     memset (s, 0, len);
   
   return s;
}

char *slrn_realloc (char *s, unsigned int len, int do_error)
{
   if (s == NULL)
     return slrn_malloc (len, 0, do_error);
   
   s = SLrealloc (s, len);
   if (s == NULL)
     return do_malloc_error (do_error);
	
   return s;
}

char *slrn_safe_malloc (unsigned int len)
{
   char *s;
   
   s = slrn_malloc (len, 1, 0);

   if (s == NULL)
     slrn_exit_error (_("Out of memory"));
   
   return s;
}

void slrn_free (char *s)
{
   if (s != NULL) SLfree (s);
}

void slrn_free_argc_argv_list (unsigned int argc, char **argv)
{
   while (argc)
     {
	argc--;
	slrn_free (argv[argc]);
     }
}

/*}}}*/

char *slrn_fix_regexp (char *pat) /*{{{*/
{
   static char newpat[256];
   char *p, ch;
   unsigned int len;

   len = 1;			       /* For ^ */
   p = pat;
   while (*p != 0)
     {
	if ((*p == '.') || (*p == '*') || (*p == '+')) len++;
	len++;
	p++;
     }
   len++;			       /* for $ */
   len++;			       /* for \0 */

   if (len > sizeof(newpat))
     slrn_exit_error (_("Pattern too long for buffer"));
     
   p = newpat;

   *p++ = '^';
   while ((ch = *pat++) != 0)
     {
	if ((ch == '.') || (ch == '+'))
	  *p++ = '\\';
	else if (ch == '*')
	  *p++ = '.';

	*p++ = ch;
     }
   
   if (*(p - 1) != '$') 
     *p++ = '$';

   *p = 0;

   return newpat;
}

/*}}}*/

int slrn_is_absolute_path (char *path)
{
   if (path == NULL)
     return 0;

   if (*path == SLRN_PATH_SLASH_CHAR)
     return 1;
#if defined(IBMPC_SYSTEM)
   if (*path == '/')
     return 1;
   if (*path && (path[1] == ':'))
     return 1;
#endif
   return 0;
}


/* This is like slrn_dircat except that any dots in name can get mapped to
 * slashes.  It also mallocs space for the resulting file.
 */
char *slrn_spool_dircat (char *root, char *name, int map_dots)
{
   char *spool_group, *p, ch;
#ifdef __CYGWIN__
   char *convdir;
#endif
   unsigned int len;

   len = strlen (root);

   spool_group = SLmalloc (strlen (name) + len + 2);
   if (spool_group == NULL)
     {
	slrn_exit_error (_("Out of memory."));
     }

   strcpy (spool_group, root); /* safe */

   p = spool_group + len;
   if (len && (*(p - 1) != SLRN_PATH_SLASH_CHAR))
     *p++ = SLRN_PATH_SLASH_CHAR;

   strcpy (p, name); /* safe */

   if (map_dots) while ((ch = *p) != 0)
     {
	if (ch == '.') *p = SLRN_PATH_SLASH_CHAR;
	p++;
     }
#ifdef __CYGWIN__
   len = strlen (spool_group) + 9;
   if (NULL == (convdir = SLmalloc (len)))
     slrn_exit_error (_("Out of memory."));
   if (0 == slrn_cygwin_convert_path (spool_group, convdir, len))
     {
	slrn_free (spool_group);
	spool_group = convdir;
     }
   else
     slrn_error (_("Cygwin conversion of %s failed."), spool_group);
#endif
#if defined(IBMPC_SYSTEM)
   slrn_os2_convert_path (spool_group);
#endif
   return spool_group;
}

int slrn_delete_file (char *f) /*{{{*/
{
#ifdef VMS
   return delete(f);
#else
   return unlink(f);
#endif
}

/*}}}*/

int slrn_fclose (FILE *fp) /*{{{*/
{
   if (0 == fclose (fp)) return 0;
   slrn_error (_("Error closing file.  File system full? (errno = %d)"), errno);
   return -1;
}

/*}}}*/


int slrn_file_exists (char *file) /*{{{*/
{
   struct stat st;
   int m;
   
#ifdef _S_IFDIR
# ifndef S_IFDIR
#  define S_IFDIR _S_IFDIR
# endif
#endif
   
#ifndef S_ISDIR
# ifdef S_IFDIR
#  define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
# else
#  define S_ISDIR(m) 0
# endif
#endif
   
   if (file == NULL)
     return -1;

   if (stat(file, &st) < 0) return 0;
   m = st.st_mode;
   
   if (S_ISDIR(m)) return (2);
   return 1;
}

/*}}}*/

int slrn_file_size (char *file) /*{{{*/
{
   struct stat st;
   int m;
   
   if (file == NULL)
     return 0;
   
   if (stat(file, &st) < 0) return -1;
   m = st.st_mode;
   
   if (S_ISDIR(m)) return -1;
   return (int)st.st_size;
}
/*}}}*/

char *slrn_basename (char *file)
{
   char *f;
#ifdef VMS
   f = slrn_strchr (file, ']');
   if (f != NULL) return f + 1;
   return file;
#else

   while (NULL != (f = slrn_strchr (file, SLRN_PATH_SLASH_CHAR)))
     file = f + 1;
   
   return file;
#endif
}

int slrn_mkdir (char *dir) /*{{{*/
{
#if defined(__MINGW32__)
# define MKDIR(x,y) mkdir(x)
#else
# define MKDIR(x,y) mkdir(x,y)
#endif
   return MKDIR (dir, 0777);
}
/*}}}*/

static int file_eqs (char *a, char *b)
{
#ifdef REAL_UNIX_SYSTEM
   struct stat st_a, st_b;
#endif
   
   if (0 == strcmp (a, b))
     return 1;
   
#ifndef REAL_UNIX_SYSTEM
   return 0;
#else
   if (-1 == stat (a, &st_a))
     return 0;
   if (-1 == stat (b, &st_b))
     return 0;
   
   return ((st_a.st_ino == st_b.st_ino)
	   && (st_a.st_dev == st_b.st_dev));
#endif
}

   

int slrn_copy_file (char *infile, char *outfile)
{
   FILE *in, *out;
   int ch;
   int ret;
   
   if ((infile == NULL) || (outfile == NULL))
     return -1;

   if (file_eqs (infile, outfile))
     return 0;

   if (NULL == (in = fopen (infile, "rb")))
     {
	slrn_error (_("Error opening %s"), infile);
	return -1;
     }
   
   if (NULL == (out = fopen (outfile, "wb")))
     {
	fclose (in);
	slrn_error (_("Error opening %s"), outfile);
	return -1;
     }

   ret = 0;
   while (EOF != (ch = getc (in)))
     {
	if (EOF == putc (ch, out))
	  {
	     slrn_error (_("Write Error: %s"), outfile);
	     ret = -1;
	     break;
	  }
     }
   
   fclose (in);
   if (-1 == slrn_fclose (out))
     ret = -1;
   
   return ret;
}

int slrn_move_file (char *infile, char *outfile)
{
   if ((infile == NULL) || (outfile == NULL))
     return -1;

   if (file_eqs (infile, outfile))
     return 0;

   (void) slrn_delete_file (outfile);
   if (-1 == rename (infile, outfile))
     {
	if (-1 == slrn_copy_file (infile, outfile))
	  return -1;
	slrn_delete_file (infile);
     }
   return 0;
}


unsigned int slrn_sleep (unsigned int len)
{
#ifdef __WIN32__
   if (len)
     Sleep (len*1000);
   return 0;
#else
   return sleep (len);
#endif
}

/* Writes strings to stderr. If the _first_ character in a string is '\n',
 * it gets replaced with '\r\n' on IBMPC_SYSTEM
 * Note: We need this because gettext does not like '\r'. Sigh. */
void slrn_va_stderr_strcat (const char *str, va_list args) /*{{{*/
{
   const char *p = str;
   while (p != NULL)
     {
#ifdef IBMPC_SYSTEM
	if (*p == '\n')
	  fputc ('\r', stderr);
#endif
	fputs (p, stderr);
	p = va_arg (args, const char *);
     }
}
/*}}}*/

void slrn_stderr_strcat (const char *str, ...) /*{{{*/
{
   va_list ag;
   va_start (ag, str);
   slrn_va_stderr_strcat (str, ag);
   va_end (ag);
}
/*}}}*/
