/* It is too bad that this cannot be done at the preprocessor level.
 * Unfortunately, C is not completely portable yet.  Basically the #error
 * directive is the problem.
 */
#include "config.h"
#include "slrnfeat.h"

#include <stdio.h>
#ifdef VMS
# include <ssdef.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#ifdef VMS
# define SUCCESS	1
# define FAILURE	2
#else
# define SUCCESS	0
# define FAILURE	1
#endif

static char *make_version (unsigned int v)
{
   static char v_string[16];
   unsigned int a, b, c;
   
   a = v/10000;
   b = (v - a * 10000) / 100;
   c = v - (a * 10000) - (b * 100);
   sprintf (v_string, "%u.%u.%u", a, b, c); /* safe */
   return v_string;
}



int main (int argc, char **argv)
{
   unsigned int min_version, sl_version;
   unsigned int sug_version;
   int ret;

#if defined(HAVE_SETLOCALE) && defined(LC_ALL)
   (void) setlocale(LC_ALL, "");
#endif
#ifdef ENABLE_NLS
   bindtextdomain(PACKAGE,LOCALEDIR);
   textdomain(PACKAGE);
#endif

   if ((argc < 3) || (argc > 4))
     {
	fprintf (stderr, _("Usage: %s <PGM> <SLANG-VERSION> <SUGG VERSION>\n"), argv[0]);
	return FAILURE;
     }
#ifndef SLANG_VERSION
   sl_version = 0;
#else
   sl_version = SLANG_VERSION;
#ifdef REAL_UNIX_SYSTEM
   if (SLang_Version != SLANG_VERSION)
     {
	fprintf (stderr, _("\n\n******\n\
slang.h does not match slang library version.  Did you install slang as\n\
as a shared library?  Did you run ldconfig?  You have an installation problem\n\
and you will need to check the SLANG variables in the Makefile and properly\n\
set them.  Also try: make clean; make\n******\n\n"));
	return FAILURE;
     }
#endif
#endif
   
   sscanf (argv[2], "%u", &min_version);
   if (argc == 4) sscanf (argv[3], "%u", &sug_version);
   else sug_version = sl_version;
   
   
   ret = SUCCESS;
   if (sl_version < min_version)
     {
	fprintf (stderr, _("This version of %s requires slang version %s.\n"),
		 argv[1], make_version(min_version));
	ret = FAILURE;
     }
   
   if (sl_version < sug_version)
     {
	fprintf (stderr, _("Your slang version is %s.\n"), make_version(sl_version));
	fprintf (stderr, _("To fully utilize this program, you should upgrade the slang library to\n"
			   "  version %s\n"), make_version(sug_version));
	fprintf (stderr, _("This library is available via anonymous ftp from\n\
space.mit.edu in pub/davis/slang.\n"));
     }
   
   return ret;
}
