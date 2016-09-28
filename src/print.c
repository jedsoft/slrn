/* -*- mode: C; mode: fold; -*- */
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

#ifdef __WIN32__
# include <windows.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <slang.h>

#include "jdmacros.h"
#include "print.h"
#include "misc.h"
#include "util.h"
#include "snprintf.h"
#include "strutil.h"
#include "common.h"

char *Slrn_Printer_Name;

/* I am using prefixes such as popen_ for the benefit of those without a
 * folding editor.
 */
#ifdef USE_NOTEPAD_PRINT_CODE
# define np_open_printer slrn_open_printer
# define np_close_printer slrn_close_printer
# define np_write_to_printer write_to_printer
# ifdef __WIN32__
#  define NOTEPAD_PRINT_CMD	"start /m notepad /p %s"
# else
#  ifdef VMS
#   define NOTEPAD_PRINT_CMD	"PRINT/QUEUE=SYS$PRINT %s"
#  else
#   ifdef __os2__
#    define NOTEPAD_PRINT_CMD	"print %s"
#   else
#    define NOTEPAD_PRINT_CMD	"lpr %s"
#   endif /* not OS/2 */
#  endif /* not VMS */
# endif /* not WIN32 */
#else
# ifdef __WIN32__
#  define USE_WIN32_PRINT_CODE	1
#  define win32_open_printer slrn_open_printer
#  define win32_close_printer slrn_close_printer
#  define win32_write_to_printer write_to_printer
# else
#  ifdef __unix__
#   define USE_POPEN_PRINT_CODE	1
#   define popen_open_printer slrn_open_printer
#   define popen_close_printer slrn_close_printer
#   define popen_write_to_printer write_to_printer
#  else
#   define USE_DUMMY_PRINT_CODE	1
#   define dummy_open_printer slrn_open_printer
#   define dummy_close_printer slrn_close_printer
#   define dummy_write_to_printer write_to_printer
#  endif
# endif
#endif

#ifdef USE_WIN32_PRINT_CODE /*{{{*/

# ifdef HAVE_WINSPOOL_H
#  include <winspool.h>
# else
#if 0
extern WINBOOL WINAPI OpenPrinterA(LPTSTR, LPHANDLE, LPVOID);
extern WINBOOL WINAPI ClosePrinter(HANDLE);
extern WINBOOL WINAPI EndPagePrinter(HANDLE);
#endif
extern DWORD WINAPI StartDocPrinterA(HANDLE, DWORD, LPBYTE);
extern WINBOOL WINAPI EndDocPrinter(HANDLE);
extern WINBOOL WINAPI StartPagePrinter(HANDLE);
extern WINBOOL WINAPI WritePrinter(HANDLE, LPVOID, DWORD, LPDWORD);
# endif

struct _Slrn_Print_Type
{
   int error_status;
   HANDLE h;
};

int win32_close_printer (Slrn_Print_Type *p)
{
   if ((p == NULL)
       || (p->h == INVALID_HANDLE_VALUE))
     return 0;

   EndDocPrinter (p->h);
   ClosePrinter (p->h);
   slrn_free ((char *) p);
   return 0;
}

Slrn_Print_Type *win32_open_printer (void)
{
   char printer_name_buf [256];
   HANDLE h;
   static DOC_INFO_1 doc_info =
     {
	"slrn-print-file",	       /* name of the document */
	NULL,			       /* name of output file to use */
	"RAW"			       /* data type */
     };
   Slrn_Print_Type *p;
   char *printer_name;

   if (NULL == (p = (Slrn_Print_Type *) SLmalloc (sizeof (Slrn_Print_Type))))
     return NULL;

   memset ((char *) p, 0, sizeof(Slrn_Print_Type));

   if (NULL == (printer_name = Slrn_Printer_Name))
     {
	(void) GetProfileString("windows", "device", "LPT1,,", printer_name_buf, sizeof(printer_name_buf));
	printer_name = slrn_strbyte (printer_name_buf, ',');
	if (NULL != printer_name)
	  *printer_name = 0;

	printer_name = printer_name_buf;
     }

   if (FALSE == OpenPrinterA (printer_name, &h, NULL))
     {
	slrn_error (_("OpenPrinterA failed: %lu"), GetLastError ());
	slrn_free ((char *) p);
	return NULL;
     }

   if (FALSE == StartDocPrinterA (h, 1, (unsigned char *) &doc_info))
     {
	slrn_error (_("StartDocPrinterA failed: %lu"), GetLastError ());
	ClosePrinter (h);
	slrn_free ((char *) p);
	return NULL;
     }

   p->h = h;
   return p;
}

static int win32_write_to_printer (Slrn_Print_Type *p, char *buf, unsigned int len)
{
   DWORD nlen;

   if ((p == NULL) || (p->h == INVALID_HANDLE_VALUE) || (p->error_status != 0))
     return -1;

   if (FALSE == WritePrinter (p->h, buf, len, &nlen))
     {
	p->error_status = 1;
	slrn_error (_("Write to printer failed: %lu"), GetLastError ());
	return -1;
     }

   return 0;
}

/*}}}*/
#endif				       /* USE_WIN32_PRINT_CODE */

#ifdef USE_NOTEPAD_PRINT_CODE /*{{{*/

struct _Slrn_Print_Type
{
   int error_status;
   FILE *fp;
   char file [SLRN_MAX_PATH_LEN];
};

int np_close_printer (Slrn_Print_Type *p)
{
   char *cmd;

   if ((p == NULL) || (p->fp == NULL))
     return 0;

   (void) slrn_fclose (p->fp);
   cmd = slrn_strdup_printf (NOTEPAD_PRINT_CMD, p->file);
   slrn_posix_system (cmd, 1);
   slrn_free (cmd);
   (void) slrn_delete_file (p->file);

   slrn_free ((char *) p);
   return 0;
}

Slrn_Print_Type *np_open_printer (void)
{
   Slrn_Print_Type *p;

   if (NULL == (p = (Slrn_Print_Type *) SLmalloc (sizeof (Slrn_Print_Type))))
     return NULL;

   memset ((char *) p, 0, sizeof(Slrn_Print_Type));

   if (NULL == (p->fp = slrn_open_tmpfile (p->file, sizeof (p->file))))
     {
	slrn_error (_("Failed to open tmp file"));
	slrn_free ((char *) p);
	return NULL;
     }

   return p;
}

static int np_write_to_printer (Slrn_Print_Type *p, char *buf, unsigned int len)
{
   if ((p == NULL) || (p->fp == NULL) || (p->error_status))
     return -1;

   if (len != fwrite (buf, 1, len, p->fp))
     {
	p->error_status = 1;
	slrn_error (_("Write to printer failed"));
	return -1;
     }

   return 0;
}

/*}}}*/
#endif

#ifdef USE_POPEN_PRINT_CODE /*{{{*/
struct _Slrn_Print_Type
{
   int error_status;
   FILE *fp;
};

int popen_close_printer (Slrn_Print_Type *p)
{
   int code;

   if ((p == NULL) || (p->fp == NULL))
     return 0;

   code = _slrn_pclose (p->fp);

   slrn_free ((char *) p);

   if (code == 0) return 0;

   slrn_error (_("Printer process returned error code %d"), code);
   return -1;
}

Slrn_Print_Type *popen_open_printer (void)
{
   char *print_cmd;
   Slrn_Print_Type *p;
   char print_cmd_buf[1024];

   if (NULL == (print_cmd = Slrn_Printer_Name))
     {
	print_cmd = getenv ("PRINTER");
	if ((print_cmd == NULL) || (*print_cmd == 0))
	  print_cmd = "lpr";
	else
	  {
	     slrn_snprintf (print_cmd_buf, sizeof (print_cmd_buf), "lpr -P%s",
			    print_cmd);
	     print_cmd = print_cmd_buf;
	  }
     }

   if (NULL == (p = (Slrn_Print_Type *) SLmalloc (sizeof (Slrn_Print_Type))))
     return NULL;

   memset ((char *) p, 0, sizeof(Slrn_Print_Type));

   if (NULL == (p->fp = popen (print_cmd, "w")))
     {
	slrn_error (_("Failed to open process %s"), print_cmd);
	popen_close_printer (p);
	return NULL;
     }

   return p;
}

static int popen_write_to_printer (Slrn_Print_Type *p, char *buf, unsigned int len)
{
   if ((p == NULL) || (p->fp == NULL) || (p->error_status))
     return -1;

   if (len != fwrite (buf, 1, len, p->fp))
     {
	p->error_status = 1;
	slrn_error (_("Error writing to print process"));
	return -1;
     }

   return 0;
}

/*}}}*/
#endif				       /* USE_POPEN_PRINT_CODE */

#ifdef USE_DUMMY_PRINT_CODE /*{{{*/
struct _Slrn_Print_Type
{
   int error_status;
};

Slrn_Print_Type *dummy_open_printer (void)
{
   slrn_error (_("Printer not supported"));
   return NULL;
}

static int dummy_write_to_printer (Slrn_Print_Type *p, char *buf, unsigned int len)
{
   return -1;
}

int dummy_close_printer (Slrn_Print_Type *p)
{
   return -1;
}

/*}}}*/
#endif				       /* USE_DUMMY_PRINT_CODE */

int slrn_write_to_printer (Slrn_Print_Type *p, char *buf, unsigned int len)
{
   char *b, *bmax;

   /* Map \n to \r\n */
   b = buf;
   bmax = b + len;

   while (b < bmax)
     {
	if (*b != '\n')
	  {
	     b++;
	     continue;
	  }

	len = (unsigned int) (b - buf);
	if (len)
	  {
	     if (*(b - 1) == '\r')
	       {
		  b++;
		  continue;
	       }

	     if (-1 == write_to_printer (p, buf, len))
	       return -1;
	  }

	if (-1 == write_to_printer (p, "\r\n", 2))
	  return -1;

	b++;
	buf = b;
     }

   len = (unsigned int) (b - buf);
   if (len)
     return write_to_printer (p, buf, len);

   return 0;
}

int slrn_print_file (char *file)
{
   Slrn_Print_Type *p;
   FILE *fp;
   char line [1024];

   if (file == NULL) return -1;

   fp = fopen (file, "r");
   if (fp == NULL)
     {
	slrn_error (_("Unable to open %s for printing"), file);
	return -1;
     }

   p = slrn_open_printer ();
   if (p == NULL)
     {
	fclose (fp);
	return -1;
     }

   while (NULL != fgets (line, sizeof (line), fp))
     {
	if (-1 == slrn_write_to_printer (p, line, strlen(line)))
	  {
	     fclose (fp);
	     slrn_close_printer (p);
	     return -1;
	  }
     }

   fclose (fp);

   return slrn_close_printer (p);
}
