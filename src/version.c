/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999 John E. Davis <davis@space.mit.edu>
 Copyright (c) 2001, 2002 Thomas Schultz <tststs@gmx.de>

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
#include "version.h"
#include "util.h"
#include "server.h"
#include "group.h"
#include "art.h"
#include "chmap.h"
#include "snprintf.h"

char *Slrn_Version = SLRN_VERSION;
int Slrn_Version_Number = SLRN_VERSION_NUMBER;
char *Slrn_Date = SLRN_RELEASE_DATE;

typedef struct 
{
   char *name;
   int value;
}
Compile_Option_Type;

static Compile_Option_Type Compile_Options [] =
{
     {"nntp",			SLRN_HAS_NNTP_SUPPORT},
     {"slrnpull",		SLRN_HAS_PULL_SUPPORT},
     {"spool",			SLRN_HAS_SPOOL_SUPPORT},
#define EXTERNAL_PROG_OFFSET 3
     {"canlock",		SLRN_HAS_CANLOCK},
     {"inews",			SLRN_HAS_INEWS_SUPPORT},
     {"ssl",			SLTCP_HAS_SSL_SUPPORT},
     {"uudeview",    		SLRN_HAS_UUDEVIEW},
#define FEATURES_OFFSET 7
     {"charset_mapping",	SLRN_HAS_CHARACTER_MAP},
     {"decoding",		SLRN_HAS_DECODE},
     {"emphasized_text",	SLRN_HAS_EMPHASIZED_TEXT},
     {"end_of_thread",		SLRN_HAS_END_OF_THREAD},
     {"fake_refs",   		SLRN_HAS_FAKE_REFS},
     {"gen_msgid",   		SLRN_HAS_GEN_MSGID},
     {"grouplens",   		SLRN_HAS_GROUPLENS},
     {"mime",		     	SLRN_HAS_MIME},
     {"msgid_cache", 		SLRN_HAS_MSGID_CACHE},
     {"piping",	     		SLRN_HAS_PIPING},
     {"rnlock",	     		SLRN_HAS_RNLOCK},
     {"slang",		     	SLRN_HAS_SLANG},
     {"spoilers",    		SLRN_HAS_SPOILERS},
     {"strict_from",		SLRN_HAS_STRICT_FROM},
     {NULL, 0}
};

static void show_compile_time_options (void)
{
   Compile_Option_Type *opt;
   unsigned int len, n = 0;

   fputs (_("\n COMPILE TIME OPTIONS:"), stdout);
   
   opt = Compile_Options;
   len = 1;
   while (opt->name != NULL)
     {
	unsigned int dlen = strlen (opt->name) + 3;
	
	switch (n)
	  {
	   case 0:
	     fputs (_("\n  Backends:"), stdout);
	     len = 11;
	     break;
	   case EXTERNAL_PROG_OFFSET:
	     fputs (_("\n  External programs / libs:"), stdout);
	     len = 27;
	     break;
	   case FEATURES_OFFSET:
	     fputs (_("\n  Features:"), stdout);
	     len = 11;
	     break;
	  }

	len += dlen;
	if (len >= 80)
	  {
	     fputs ("\n   ", stdout);
	     len = dlen + 3;
	  }
	fprintf (stdout, " %c%s", (opt->value ? '+' : '-'), opt->name);
	opt++; n++;
     }
   fputc ('\n', stdout);
}



static char *make_slang_version (int v)
{
#if SLANG_VERSION >= 10307
   if (v == SLang_Version)
     return SLang_Version_String;
   else
     return SLANG_VERSION_STRING;
#else
   int a, b, c;
   static char buf[32];
   
   a = v/10000;
   b = (v - a * 10000) / 100;
   c = v - (a * 10000) - (b * 100);

   slrn_snprintf (buf, sizeof (buf), "%d.%d.%d", a, b, c);
   return buf;
#endif
}

void slrn_show_version (void) /*{{{*/
{
   char *os;
   os = slrn_get_os_name ();

   fprintf (stdout, "Slrn %s [%s]\n", Slrn_Version, Slrn_Date);
#if defined(PATCH_LEVEL)
   fprintf (stdout, _("\t* Note: This version is a developer preview.\n"));
#endif
   fprintf (stdout, _("S-Lang Library Version: %s\n"), make_slang_version (SLang_Version));
   if (SLANG_VERSION != SLang_Version)
     {
	fprintf (stdout, _("\t* Note: This program was compiled against version %s.\n"),
		 make_slang_version (SLANG_VERSION));
     }
#if defined(__DATE__) && defined(__TIME__)
   fprintf (stdout, _("Compiled at: %s %s\n"), __DATE__, __TIME__);
#endif
   fprintf (stdout, _("Operating System: %s\n"), os);
   
   show_compile_time_options ();

   fprintf (stdout, _(" DEFAULTS:\n  Default server object:     %s\n"),
	    slrn_map_object_id_to_name (0, SLRN_DEFAULT_SERVER_OBJ));
   
   fprintf (stdout, _("  Default posting mechanism: %s\n"),
	    slrn_map_object_id_to_name (1, SLRN_DEFAULT_POST_OBJ));

   
#if SLRN_HAS_CHARACTER_MAP
   slrn_chmap_show_supported ();
#endif
   exit (0);
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
