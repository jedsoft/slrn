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
#ifndef _SLRN_UTIL_H
#define _SLRN_UTIL_H
#include <limits.h>
#include <stdarg.h>
#ifdef PATH_MAX
# define SLRN_MAX_PATH_LEN PATH_MAX
#else
# define SLRN_MAX_PATH_LEN 1024
#endif

extern int slrn_dircat (char *, char *, char *, size_t);
extern char *slrn_spool_dircat (char *, char *, int);
extern int slrn_copy_file (char *, char *);
extern int slrn_move_file (char *, char *);
extern int slrn_fclose (FILE *);
extern int slrn_delete_file (char *);
extern int slrn_file_exists (char *);
extern int slrn_file_size (char *);
extern int slrn_mkdir (char *);
extern char *slrn_basename (char *);
extern int slrn_is_absolute_path (char *);
#if defined(IBMPC_SYSTEM)
extern void slrn_os2_convert_path (char *);
extern void slrn_os2_make_fat (char *, size_t, char *, char *);
#endif
#ifdef __CYGWIN__
extern int slrn_cygwin_convert_path (char *, char *, size_t);
#endif
extern unsigned int slrn_sleep (unsigned int);

extern char *slrn_simple_strtok (char *, char *);
extern char *slrn_strchr (char *, char);
extern char *slrn_skip_whitespace (char *s);
extern char *slrn_bskip_whitespace (char *s);
extern char *slrn_trim_string (char *s);
extern int slrn_case_strncmp (unsigned char *, unsigned char *, unsigned int);
extern int slrn_case_strcmp (unsigned char *, unsigned char *);
extern char *slrn_strbrk (char *, char *);

extern char *slrn_safe_strmalloc (char *);
extern char *slrn_safe_malloc (unsigned int);
extern char *slrn_strmalloc (char *, int);
extern char *slrn_strnmalloc (char *, unsigned int, int);
extern char *slrn_malloc (unsigned int, int, int);
extern char *slrn_realloc (char *, unsigned int, int);
extern void slrn_free (char *);
extern void slrn_free_argc_argv_list (unsigned int, char **);

extern char *slrn_fix_regexp (char *);

extern void slrn_va_stderr_strcat (const char *, va_list);
extern void slrn_stderr_strcat (const char *, ...);

/* These declarations are here although the functions are not really defined 
 * in util.c.
 */
extern void slrn_exit_error (char *, ...);
char *slrn_make_from_string (void);

typedef struct Slrn_Group_Range_Type
{
   char *name;
   int min, max;
} Slrn_Group_Range_Type;

#endif				       /* _SLRN_UTIL_H */
