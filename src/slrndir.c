/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999, 2007-2016 John E. Davis <jed@jedsoft.org>

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

#ifndef VMS
/* I have no idea whether or not this works under VMS.  Please let me know
 * if it does.
 */
#if HAVE_DIRENT_H
# include <dirent.h>
#else
# ifdef HAVE_DIRECT_H
#  include <direct.h>
# else
#  define dirent direct
#  define NEED_D_NAMLEN
#  if HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#   include <ndir.h>
#  endif
# endif
#endif

#else				       /* VMS */
#define DIR int
#endif

#include <slang.h>
#include "jdmacros.h"

#include "util.h"
#include "slrndir.h"
#include "strutil.h"
#include "common.h"

struct _Slrn_Dir_Type
{
   DIR *dp;
};

Slrn_Dir_Type *slrn_open_dir (char *dir)
{
#ifdef VMS
   slrn_error (_("slrn_open_dir has not been ported to VMS"));
   return NULL;
#else
   Slrn_Dir_Type *d;
   DIR *dp;

   d = (Slrn_Dir_Type *) slrn_malloc (sizeof (Slrn_Dir_Type), 1, 1);
   if (d == NULL)
     return NULL;

   if (NULL == (dp = opendir (dir)))
     {
	slrn_free ((char *)d);
	return NULL;
     }

   d->dp = dp;
   return d;
#endif
}

void slrn_close_dir (Slrn_Dir_Type *d)
{
#ifndef VMS
   if (d == NULL) return;
   closedir (d->dp);
   slrn_free ((char *)d);
#endif
}

Slrn_Dirent_Type *slrn_read_dir (Slrn_Dir_Type *d)
{
#ifdef VMS
   return NULL;
#else
   struct dirent *ep;
   unsigned int len;
   static Slrn_Dirent_Type dir;

   if (d == NULL) return NULL;
   ep = readdir (d->dp);
   if (ep == NULL)
     return NULL;

   memset ((char *) &dir, 0, sizeof(Slrn_Dirent_Type));

#ifdef NEED_D_NAMLEN
   len = ep->d_namlen;
#else
   len = strlen (ep->d_name);
#endif

   if (len > SLRN_MAX_PATH_LEN)
     len = SLRN_MAX_PATH_LEN;

   strncpy (dir.name, ep->d_name, len);
   dir.name [len] = 0;

   dir.name_len = len;

   return &dir;
#endif
}

/* This function is from JED */
char *slrn_getcwd (char *cwdbuf, unsigned int buflen)
{
   static char cwd[SLRN_MAX_PATH_LEN];
   char *c;

   if (cwdbuf != NULL)
     *cwdbuf = 0;

#ifdef HAVE_GETCWD
# if defined (__EMX__)
   c = _getcwd2(cwd, sizeof(cwd)-1);   /* includes drive specifier */
# else
   c = getcwd(cwd, sizeof(cwd)-1);     /* djggp includes drive specifier */
# endif
#else
   c = (char *) getwd(cwd);
#endif

   if (c == NULL)
     {
#ifndef REAL_UNIX_SYSTEM
	slrn_error (_("Unable to getcwd"));
	return NULL;
#else
	struct stat st1, st2;

	if ((NULL == (c = getenv ("PWD")))
	    || (-1 == stat (c, &st1))
	    || (-1 == stat (".", &st2))
	    || (st1.st_dev != st2.st_dev)
	    || (st1.st_ino != st2.st_ino))
	  {
	     slrn_error (_("Unable to getcwd"));
	     return NULL;
	  }

	strncpy (cwd, c, sizeof (cwd));
	cwd [sizeof (cwd) - 1] = 0;
#endif
     }

   if (cwdbuf == NULL)
     return cwd;

   strncpy (cwdbuf, cwd, buflen);
   if (buflen) cwdbuf[buflen - 1] = 0;
   return cwdbuf;
}

int slrn_chdir (char *dir)
{
   unsigned int len;
   char dirbuf[SLRN_MAX_PATH_LEN + 1];
#ifdef __CYGWIN__
   char convdir[SLRN_MAX_PATH_LEN + 1];
#endif

   if (dir == NULL)
     return -1;

   strncpy (dirbuf, dir, SLRN_MAX_PATH_LEN);
   dirbuf[SLRN_MAX_PATH_LEN] = 0;
   len = strlen (dirbuf);

   /* I may have to add code to handle something like C:/ */
   if ((len > 1) && (dirbuf[len - 1] == SLRN_PATH_SLASH_CHAR))
     {
	len--;
	dirbuf [len] = 0;
     }

#ifdef __CYGWIN__
   if (slrn_cygwin_convert_path (dirbuf, convdir, sizeof (convdir)))
     {
	slrn_error ("Cygwin conversion of %s failed.", dirbuf);
	return -1;
     }
#else
# if defined(IBMPC_SYSTEM)
   slrn_os2_convert_path (dirbuf);
# endif
#endif

   if (-1 == chdir (dirbuf))
     {
	slrn_error (_("chdir %s failed.  Does the directory exist?"), dirbuf);
	return -1;
     }

   return 0;
}
