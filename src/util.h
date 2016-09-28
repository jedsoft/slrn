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

extern char *slrn_make_backup_filename (char *);
extern int slrn_create_backup (char *);
extern void slrn_delete_backup (char *);
extern int slrn_restore_backup (char *);

extern unsigned int slrn_sleep (unsigned int);

extern size_t slrn_charset_strlen (const char *, char *);
extern int slrn_screen_strlen (const char *, const char *);

extern void slrn_free_argc_argv_list (unsigned int, char **);

extern char *slrn_fix_regexp (char *);

extern void slrn_va_stderr_strcat (const char *, va_list);
extern void slrn_stderr_strcat (const char *, ...);

/* These declarations are here although the functions are not really defined
 * in util.c.
 */
char *slrn_make_from_header (void);

typedef struct Slrn_Group_Range_Type
{
   char *name;
   NNTP_Artnum_Type min, max;
} Slrn_Group_Range_Type;

#endif				       /* _SLRN_UTIL_H */
