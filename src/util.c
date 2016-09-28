/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999, 2007-2016 John E. Davis <jed@jedsoft.org>
 Copyright (c) 2002-2006 Thomas Schultz <tststs@gmx.de>

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
#include "slrn.h"
#include "strutil.h"
#include "common.h"

size_t slrn_charset_strlen (const char *str, char *cset)
{
  if ((cset != NULL) && !slrn_case_strcmp(cset,"utf-8"))
     return SLutf8_strlen ((SLuchar_Type *)str, 1);
  else
     return strlen (str);
}

/* Find out how many characters (columns) a string would use on screen.
 * If len>=0, only the first len bytes are examined.
 */
int slrn_screen_strlen (const char *s, const char *smax) /*{{{*/
{
   if (smax == NULL)
     smax = s + strlen (s);

#if SLANG_VERSION >= 20000
   if (Slrn_UTF8_Mode)
     {
	return SLsmg_strwidth ((SLuchar_Type *) s, (SLuchar_Type *) smax);
     }
   else
#endif
     {
	int retval = 0;
	while (s < smax)
	  {
	     unsigned char ch= (unsigned char) *s++;

	     if ((ch == '\t') && (SLsmg_Tab_Width > 0))
	       {
		  retval += SLsmg_Tab_Width;
		  retval -= retval % SLsmg_Tab_Width;
	       }
	     else if (((ch >= ' ') && (ch < 127))
		      || (ch >= (unsigned char) SLsmg_Display_Eight_Bit))
	       retval++;
	     else
	       {
		  retval += 2;	       /* ^X */
		  if (ch & 0x80) retval += 2;   /* <XX> */
	       }
	  }
	return retval;
     }
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

void slrn_free_argc_argv_list (unsigned int argc, char **argv)
{
   while (argc)
     {
	argc--;
	slrn_free (argv[argc]);
     }
}

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
   f = slrn_strbyte (file, ']');
   if (f != NULL) return f + 1;
   return file;
#else

   while (NULL != (f = slrn_strbyte (file, SLRN_PATH_SLASH_CHAR)))
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

/* Some functions to handle file backups correctly ... */

/* Make and return a malloc'ed filename by adding an OS-specific suffix
 * that flags backup files. */
char *slrn_make_backup_filename (char *filename)
{
#ifdef SLRN_USE_OS2_FAT
   unsigned int len = strlen(filename)+5;
   char *retval = slrn_safe_malloc (len);
   slrn_os2_make_fat (retval, len, filename, ".bak");
   return retval;
#else
   unsigned int len;
   char *suffix;
   char *retval;
# ifdef VMS
   suffix = "-bak";
# else
   suffix = "~";
# endif
   len = 1 + strlen (suffix) + strlen (filename);
   retval = slrn_safe_malloc (len);
   (void) sprintf (retval, "%s%s", filename, suffix);
   return retval;
#endif
}

/* Creates a backup of the given file, copying it if necessary (i.e. it is
 * a softlink or there are multiple hardlinks on it) - after that, filename
 * may or may no longer denote an existing file.
 * Returns 0 on success, -1 on errors. */
int slrn_create_backup (char *filename)
{
   char *backup_file = slrn_make_backup_filename (filename);
   int retval = -1, do_copy = 0;
#ifdef __unix__
   struct stat st;

   if (0 == lstat (filename, &st))
     {
	do_copy = (st.st_nlink > 1)
# ifdef S_ISLNK
	  || (S_ISLNK(st.st_mode))
# endif
	    ;
     }
#endif /* __unix__ */

   if (slrn_file_exists(filename))
     {
	if (!do_copy)
	  retval = rename (filename, backup_file);
	if (retval == -1)
	  retval = slrn_copy_file (filename, backup_file);
     }

   SLfree (backup_file);

   return retval;
}

/* Tries to delete the backup of the given file, ignoring all errors. */
void slrn_delete_backup (char *filename)
{
   char *backup_file = slrn_make_backup_filename (filename);
   (void) slrn_delete_file (backup_file);
   SLfree (backup_file);
}

/* Tries to restore the given file from the backup, copying it if necessary
 * (same condition as above). */
int slrn_restore_backup (char *filename)
{
   char *backup_file = slrn_make_backup_filename (filename);
   int retval = -1, do_copy = 0;
#ifdef __unix__
   struct stat st;

   if (0 == lstat (filename, &st))
     {
	do_copy = (st.st_nlink > 1)
# ifdef S_ISLNK
	  || (S_ISLNK(st.st_mode))
# endif
	    ;
     }
#endif /* __unix__ */

   if (!do_copy)
     retval = rename (backup_file, filename);
   if (retval == -1)
     {
	retval = slrn_copy_file (backup_file, filename);
	if (retval == 0)
	  (void) slrn_delete_file (backup_file);
     }

   SLfree (backup_file);

   return retval;
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
