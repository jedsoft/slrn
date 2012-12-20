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

#include <string.h>
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
   bindtextdomain(NLS_PACKAGE_NAME, NLS_LOCALEDIR);
   textdomain (NLS_PACKAGE_NAME);
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

	fprintf (stderr, "slang.h version: %d\nlibslang version %d\n",
		 SLANG_VERSION, SLang_Version);

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
	fprintf (stderr, _("This library is available from <http://www.jedsoft.org/slang/>.\n"));
     }

#ifdef SIZEOF_SHORT
   if (sizeof(short) != SIZEOF_SHORT)
     {
	fprintf (stderr, "SIZEOF_SHORT[%lu] is not equal to sizeof(short)[%lu]\n",
		 (unsigned long) SIZEOF_SHORT, (unsigned long) sizeof(short));
	ret = FAILURE;
     }
#endif
#ifdef SIZEOF_INT
   if (sizeof(int) != SIZEOF_INT)
     {
	fprintf (stderr, "SIZEOF_INT[%lu] is not equal to sizeof(int)[%lu]\n",
		 (unsigned long) SIZEOF_INT, (unsigned long) sizeof(int));
	ret = FAILURE;
     }
#endif
#ifdef SIZEOF_LONG
   if (sizeof(long) != SIZEOF_LONG)
     {
	fprintf (stderr, "SIZEOF_LONG[%lu] is not equal to sizeof(long)[%lu]\n",
		 (unsigned long) SIZEOF_LONG, (unsigned long) sizeof(long));
	ret = FAILURE;
     }
#endif
#if HAVE_LONG_LONG && defined(SIZEOF_LONG_LONG)
   if (sizeof(long long) != SIZEOF_LONG_LONG)
     {
	fprintf (stderr, "SIZEOF_LONG_LONG[%lu] is not equal to sizeof(long long)[%lu]\n",
		 (unsigned long) SIZEOF_LONG_LONG, (unsigned long) sizeof(long long));
	ret = FAILURE;
     }
# if (SIZEOF_LONG_LONG >= 8)
   else
     {
	long long llx = 9223372036854775807LL;
	NNTP_Artnum_Type y = 0, z = 0;

	if ((2 != sscanf ("9223372036854775807 9223372036854775807",
			  NNTP_FMT_ARTNUM_2, &y, &z))
	    || (y != llx) || (z != llx))
	  {
	     fprintf (stderr, "The long long format \"%s\" does not appear to work properly\n", NNTP_FMT_ARTNUM);
	     fprintf (stderr, "Please check/correct the config.h file.\n");
	     ret = FAILURE;
	  }
     }
# endif
#endif

#ifdef SIZEOF_FLOAT
   if (sizeof(float) != SIZEOF_FLOAT)
     {
	fprintf (stderr, "SIZEOF_FLOAT[%lu] is not equal to sizeof(float)[%lu]\n",
		 (unsigned long) SIZEOF_FLOAT, (unsigned long) sizeof(float));
	ret = FAILURE;
     }
#endif
#ifdef SIZEOF_DOUBLE
   if (sizeof(double) != SIZEOF_DOUBLE)
     {
	fprintf (stderr, "SIZEOF_DOUBLE[%lu] is not equal to sizeof(double)[%lu]\n",
		 (unsigned long) SIZEOF_DOUBLE, (unsigned long) sizeof(double));
	ret = FAILURE;
     }
#endif

   return ret;
}
