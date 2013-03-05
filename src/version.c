/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999, 2007-2012 John E. Davis <jed@jedsoft.org>
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
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_UTSNAME_H
# include <sys/utsname.h>
#endif

#include <slang.h>
#include "jdmacros.h"
#include "version.h"
#include "util.h"
#include "server.h"
#include "group.h"
#include "art.h"
#include "snprintf.h"

char *Slrn_Version_String = SLRN_VERSION_STRING;
int Slrn_Version = SLRN_VERSION;

typedef struct
{
   char *name;
   int value;
}
Compile_Option_Type;

static Compile_Option_Type Backend_Options [] =
{
   {"nntp",			SLRN_HAS_NNTP_SUPPORT},
   {"slrnpull",			SLRN_HAS_PULL_SUPPORT},
   {"spool",			SLRN_HAS_SPOOL_SUPPORT},
   {NULL, 0}
};

static Compile_Option_Type External_Lib_Options [] =
{
   {"canlock",			SLRN_HAS_CANLOCK},
   {"inews",			SLRN_HAS_INEWS_SUPPORT},
   {"ssl",			SLTCP_HAS_SSL_SUPPORT},
   {"uudeview",    		SLRN_HAS_UUDEVIEW},
#ifdef HAVE_ICONV
   {"iconv",			1},
#else
   {"iconv",			0},
#endif
   {NULL, 0}
};

static Compile_Option_Type Feature_Options [] =
{
   {"decoding",			SLRN_HAS_DECODE},
   {"emphasized_text",		SLRN_HAS_EMPHASIZED_TEXT},
   {"end_of_thread",		SLRN_HAS_END_OF_THREAD},
   {"fake_refs",   		SLRN_HAS_FAKE_REFS},
   {"gen_msgid",   		SLRN_HAS_GEN_MSGID},
   {"grouplens",   		SLRN_HAS_GROUPLENS},
   {"msgid_cache", 		SLRN_HAS_MSGID_CACHE},
   {"piping",	     		SLRN_HAS_PIPING},
   {"rnlock",	     		SLRN_HAS_RNLOCK},
   {"spoilers",    		SLRN_HAS_SPOILERS},
   {"strict_from",		SLRN_HAS_STRICT_FROM},
   {NULL, 0}
};

static void print_options (FILE *fp, Compile_Option_Type *opts, char *title)
{
   unsigned int len;

   (void) fprintf (fp, " %s:", title);
   len = strlen (title);
   while (opts->name != NULL)
     {
	unsigned int dlen = strlen (opts->name) + 2;

   	len += dlen;
	if (len >= 80)
	  {
	     (void) fputs ("\n   ", fp);
	     len = dlen + 3;
	  }
	(void) fprintf (fp, " %c%s", (opts->value ? '+' : '-'), opts->name);
	opts++;
     }
   (void) fputc ('\n', fp);
}

static void show_compile_time_options (FILE *fp)
{
   (void) fprintf (fp, "%s:\n", _("COMPILE TIME OPTIONS"));
   print_options (fp, Backend_Options, _("Backends"));
   print_options (fp, External_Lib_Options, _("External programs / libs"));
   print_options (fp, Feature_Options, _("Features"));
   (void) fprintf (fp, _(" Using %d bit integers for article numbers.\n"),
		   8*(int)sizeof(NNTP_Artnum_Type));
}

void slrn_show_version (FILE *fp) /*{{{*/
{
   char *os;

   os = slrn_get_os_name ();

   fprintf (fp, "slrn %s\n", Slrn_Version_String);
   if (*Slrn_Version_String == 'p')
     fprintf (fp, _("\t* Note: This version is a developer preview.\n"));
   fprintf (fp, _("S-Lang Library Version: %s\n"), SLang_Version_String);
   if (SLANG_VERSION != SLang_Version)
     {
	fprintf (fp, _("\t* Note: This program was compiled against version %s.\n"),
		 SLANG_VERSION_STRING);
     }
#if defined(__DATE__) && defined(__TIME__)
   fprintf (fp, _("Compiled on: %s %s\n"), __DATE__, __TIME__);
#endif
   fprintf (fp, _("Operating System: %s\n"), os);

   (void) fputs ("\n", fp);
   show_compile_time_options (fp);

   (void) fputs ("\n", fp);
   fprintf (fp, _("DEFAULTS:\n Default server object:     %s\n"),
	    slrn_map_object_id_to_name (0, SLRN_DEFAULT_SERVER_OBJ));

   fprintf (fp, _(" Default posting mechanism: %s\n"),
	    slrn_map_object_id_to_name (1, SLRN_DEFAULT_POST_OBJ));
}

/*}}}*/

#ifdef REAL_UNIX_SYSTEM
static char *get_unix_system_name (void)
{
# ifdef HAVE_UNAME
   static struct utsname u;

   if (-1 != uname (&u))
     return u.sysname;
# endif
   return "Unix";
}
#endif				       /* Unix */

#ifdef VMS
static char *get_vms_system_name (void)
{
# ifdef MULTINET
   return "VMS/Multinet";
# else
#  ifdef UXC
   return "VMS/UCX";
#  else
#   ifdef NETLIB
   return "VMS/Netlib";
#   else
   return "VMS";
#   endif
#  endif
# endif
}
#endif				       /* VMS */

#ifdef __os2__
static char *get_os2_system_name (void)
{
   return "OS/2";
}
#endif

#ifdef AMIGA
static char *get_amiga_system_name (void)
{
   return "Amiga";
}
#endif

#if defined(__WIN32__)
static char *get_win32_system_name (void)
{
   return "Win32";
}
#endif

#if defined(__BEOS__)
static char *get_beos_system_name (void)
{
   return "BeOS";
}
#endif

char *slrn_get_os_name (void)
{
#ifdef REAL_UNIX_SYSTEM
   return get_unix_system_name ();
#else
# ifdef VMS
   return get_vms_system_name ();
# else
#  ifdef __os2__
   return get_os2_system_name ();
#  else
#   ifdef AMIGA
   return get_amiga_system_name ();
#   else
#    if defined(__WIN32__)
   return get_win32_system_name ();
#    else
#     if defined(__BEOS__)
   return get_beos_system_name ();
#     else
   return "Unknown";
#     endif			       /* BEOS */
#    endif			       /* WIN32 */
#   endif			       /* AMIGA */
#  endif			       /* OS/2 */
# endif				       /* VMS */
#endif				       /* UNIX */
}
