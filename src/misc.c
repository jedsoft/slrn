/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999, 2007-2016 John E. Davis <jed@jedsoft.org>
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

/*{{{ Include Files */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#if !defined(VMS) && !defined(__WIN32__) && !defined(__NT__)
# define HAS_PASSWORD_CODE	1
# include <pwd.h>
#endif

#ifdef VMS
# include "vms.h"
#else
# include <sys/types.h>
# include <sys/stat.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_SYS_FCNTL_H
# include <sys/fcntl.h>
#endif

#if defined(VMS) && defined(MULTINET)
# include "multinet_root:[multinet.include]netdb.h"
#else
# if defined(__NT__)
#  include <winsock.h>
# else
#  if defined(__WIN32__)
/* #   define Win32_Winsock */
#   define __USE_W32_SOCKETS
#   include <windows.h>
#   ifdef __MINGW32__
#    include <process.h>
#   endif
#  else
#   include <netdb.h>
#   if !defined(h_errno) && !defined(__CYGWIN__)
extern int h_errno;
#   endif
#  endif
# endif
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef NeXT
# undef WIFEXITED
# undef WEXITSTATUS
#endif

#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#include <slang.h>
#include "jdmacros.h"

#include "misc.h"
#include "group.h"
#include "slrn.h"
#include "util.h"
#include "server.h"
#include "ttymsg.h"
#include "art.h"
#include "post.h"
#include "snprintf.h"
#include "slrndir.h"
#include "menu.h"
#include "hooks.h"
#include "startup.h"
#include "strutil.h"
#include "common.h"

#ifdef VMS
/* valid filname chars for unix equiv of vms filename */
# define VALID_FILENAME_CHARS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789$_-/"
# include "vms.h"
#endif
/*}}}*/

/*{{{ Global Variables */
int Slrn_Full_Screen_Update = 1;
int Slrn_User_Wants_Confirmation = SLRN_CONFIRM_ALL;
int Slrn_Message_Present = 0;
int Slrn_Abort_Unmodified = 0;
int Slrn_Mail_Editor_Is_Mua = 0;

#ifndef VMS
char *Slrn_SendMail_Command;
#endif

char *Slrn_Editor;
char *Slrn_Editor_Post;
char *Slrn_Editor_Score;
char *Slrn_Editor_Mail;
int Slrn_Editor_Uses_Mime_Charset = 0;

char *Slrn_Top_Status_Line;

Slrn_User_Info_Type Slrn_User_Info;
SLKeyMap_List_Type *Slrn_RLine_Keymap;
SLang_RLine_Info_Type *Slrn_Keymap_RLI;

/*}}}*/
/*{{{ Static Variables */

static int Error_Present;
static char *Input_String;
static char *Input_String_Ptr;
static char *Input_Chars_Ptr;
static char *Input_Chars;
static int Beep_Pending;
/*}}}*/

static void redraw_message (void);
static void redraw_mini_buffer (void);

/*{{{ Screen Update Functions */

void slrn_smg_refresh (void)
{
   if (Slrn_TT_Initialized & SLRN_SMG_INIT)
     {
	slrn_push_suspension (0);
	if (Beep_Pending)
	  SLtt_beep ();
	Beep_Pending = 0;
	SLsmg_refresh ();
	slrn_pop_suspension ();
     }
}

void slrn_set_color (int color) /*{{{*/
{
   SLsmg_set_color (color);
}

/*}}}*/

void slrn_redraw (void) /*{{{*/
{
   if (Slrn_Batch) return;

   slrn_push_suspension (0);

   SLsmg_cls ();
   Slrn_Full_Screen_Update = 1;

   redraw_message ();
   slrn_update_screen ();
   redraw_mini_buffer ();

   slrn_smg_refresh ();
   slrn_pop_suspension ();
}

/*}}}*/

/* theoretically, buf needs at least space for 66 characters */
char *slrn_print_percent (char *buf, SLscroll_Window_Type *w, int lines) /*{{{*/
{
   if (lines)
     {
	sprintf (buf, "%d/%d", w->line_num, w->num_lines);
     }
   else
     {
	int bot_showing;
	unsigned int bot_number;

	bot_number = w->line_num + (w->nrows - w->window_row) - 1;
	bot_showing = ((w->bot_window_line == NULL)
		       || (w->num_lines == bot_number));

	if (w->line_num == w->window_row + 1)
	  slrn_strncpy (buf, bot_showing ? _("All") : _("Top"), 66);
	else if (bot_showing)
	  slrn_strncpy (buf, _("Bot"), 66);
	else
	  sprintf (buf, "%d%%", (100 * bot_number) / w->num_lines); /* safe */
    }
  return buf;
}
/*}}}*/

static char *top_status_line_cb (char ch, void *data, int *len, int *color) /*{{{*/
{
   static char buf[32];
   char *retval = NULL;
   time_t now;

   (void) data; (void) color; /* we currently don't use these */

   switch (ch)
     {
      case 'd':
	time(&now);
	if (0 != (*len = strftime (buf, sizeof(buf), "%x", localtime(&now))))
	  retval = buf;
	break;

      case 'n':
	if (NULL != Slrn_Group_Current_Group)
	  retval = Slrn_Group_Current_Group->group_name;
	break;

      case 's':
	retval = Slrn_Server_Obj->sv_name;
	break;

      case 't':
	time(&now);
	if (0 != (*len = strftime (buf, sizeof(buf), "%X", localtime(&now))))
	  retval = buf;
	break;

      case 'v':
	retval = Slrn_Version_String;
	break;

     }
   return retval == NULL ? "" : retval;
}
/*}}}*/
void slrn_update_top_status_line (void) /*{{{*/
{
   char *fmt = Slrn_Top_Status_Line;

   if (Slrn_Full_Screen_Update == 0) return;

   if ((fmt == NULL) || (*fmt == 0))
     fmt = _("slrn %v ** Press '?' for help, 'q' to quit. ** Server: %s");

   slrn_custom_printf (fmt, top_status_line_cb, NULL, 0, MENU_COLOR);
}
/*}}}*/

/*}}}*/
/*{{{ Message/Error Functions */

static char *skip_char (char *s, char *smax, int ignore_combining)
{
   if (s >= smax)
     return s;

   if (Slrn_UTF8_Mode == 0)
     return s+1;

   return (char *)SLutf8_skip_chars ((SLuchar_Type *)s, (SLuchar_Type *)smax, 1, NULL, ignore_combining);
}

/* The first character is the color */
static char Message_Buffer[1024];

static void redraw_message (void)
{
   int color;
   char *m, *mmax;

   if (Slrn_Batch) return;

   if (Slrn_Message_Present == 0)
     return;

   slrn_push_suspension (0);

   SLsmg_gotorc (SLtt_Screen_Rows - 1, 0);

   color = Message_Buffer [0];
   m = Message_Buffer + 1;
   mmax = m + strlen (m);

   while (1)
     {
	char *m1 = slrn_strbyte (m, 1);
	if (m1 == NULL)
	  m1 = mmax;

	slrn_set_color (color);
	SLsmg_write_nchars (m, (unsigned int) (m1 - m));
	if (m1 == mmax)
	  break;
	m1++;
	if (m1 == mmax)
	  break;

	slrn_set_color (RESPONSE_CHAR_COLOR);
	m = m1;
	m1 = skip_char (m, mmax, 1);
	SLsmg_write_nchars (m, m1-m);
	m = m1;
     }

   SLsmg_erase_eol ();
   slrn_set_color (0);

   slrn_pop_suspension ();
}

static void vmessage_1 (int color, char *fmt, va_list ap)
{
   slrn_vsnprintf (Message_Buffer + 1, sizeof(Message_Buffer)-1, fmt, ap);

   Message_Buffer[0] = (char) color;
   Slrn_Message_Present = 1;
   redraw_message ();
}

static void vmessage (FILE *fp, char *fmt, va_list ap)
{
   if (Slrn_TT_Initialized & SLRN_SMG_INIT)
     vmessage_1 (MESSAGE_COLOR, fmt, ap);
   else
     slrn_tty_vmessage (fp, fmt, ap);
}

static void log_error_message (char *fmt, va_list ap)
{
   if (Slrn_Debug_Fp != NULL)
     {
	(void) vfprintf (Slrn_Debug_Fp, fmt, ap);
	(void) fputs ("\n", Slrn_Debug_Fp);
	(void) fflush (Slrn_Debug_Fp);
     }
}

void slrn_verror (char *fmt, va_list ap)
{
   va_list ap1;

   VA_COPY(ap1, ap);

   if ((Slrn_TT_Initialized & SLRN_SMG_INIT) == 0)
     {
	slrn_tty_vmessage (stderr, fmt, ap);
     }
   else if (Error_Present == 0)
     {
	slrn_clear_message ();
	Error_Present = 1;
	Beep_Pending = 1;
	vmessage_1 (ERROR_COLOR, fmt, ap);
	SLang_flush_input ();
     }

   log_error_message (fmt, ap1);
   va_end (ap1);

   if (SLang_get_error () == 0) SLang_set_error (INTRINSIC_ERROR);
}

/*}}}*/
void slrn_clear_message (void) /*{{{*/
{
   Slrn_Message_Present = Error_Present = 0;
   /* SLang_Error = 0; */
   Beep_Pending = 0;
   SLKeyBoard_Quit = 0;

   if ((Slrn_TT_Initialized & SLRN_SMG_INIT) == 0)
     return;

   slrn_push_suspension (0);
   SLsmg_gotorc (SLtt_Screen_Rows - 1, 0);
   SLsmg_erase_eol ();
   *Message_Buffer = 0;
   slrn_pop_suspension ();
}

/*}}}*/

void slrn_clear_error (void)
{
   SLang_set_error (0);
   slrn_clear_message ();
}

void slrn_va_message (char *fmt, va_list ap)
{
   if (Error_Present == 0)
     vmessage (stdout, fmt, ap);
}

int slrn_message (char *fmt, ...) /*{{{*/
{
   va_list ap;

   if (Error_Present) return -1;
   va_start(ap, fmt);
   vmessage (stdout, fmt, ap);
   va_end (ap);
   return 0;
}

/*}}}*/

int slrn_message_now (char *fmt, ...) /*{{{*/
{
   va_list ap;

   if (Error_Present) return -1;
   va_start(ap, fmt);
   vmessage (stdout, fmt, ap);
   va_end (ap);
   slrn_smg_refresh ();
   Slrn_Message_Present = 0;
   return 0;
}

/*}}}*/

void slrn_error_now (unsigned secs, char *fmt, ...) /*{{{*/
{
   va_list ap;

   if (fmt != NULL)
     {
	va_start(ap, fmt);
	slrn_verror (fmt, ap);
	va_end (ap);
     }
   slrn_smg_refresh ();
   Slrn_Message_Present = 0;
   if (secs) slrn_sleep (secs);
}

/*}}}*/

/* Wrapper around the SLsmg routine.
 * This writes n bytes (not necessarily screen characters), replacing
 * unprintable 8bit chars with question marks.
 */

/* Wrapper around the SLsmg routine. */
void slrn_write_nbytes (char *s, unsigned int n) /*{{{*/
{
   unsigned char *s1, *smax;
   unsigned int eight_bit;

   if (Slrn_UTF8_Mode)
     {
	SLsmg_write_nchars (s, n);
	return;
     }

   s1 = (unsigned char *) s;
   smax = s1 + n;
   eight_bit = SLsmg_Display_Eight_Bit;

   while (s1 < smax)
     {
	if ((*s1 & 0x80) && (eight_bit > (unsigned int) *s1))
	  {
	     if (s != (char *) s1)
	       SLsmg_write_nchars (s, (unsigned int) ((char *)s1 - s));
	     SLsmg_write_char ('?');
	     s1++;
	     s = (char *) s1;
	  }
	else s1++;
     }

   if (s != (char *)s1)
     SLsmg_write_nchars (s, (unsigned int) ((char *)s1 - s));
}
/*}}}*/

/* generic function to display format strings;
 * cb is called when a descriptor other than '%', '?' or 'g' is encountered */
void slrn_custom_printf (char *fmt, PRINTF_CB cb, void *param, /*{{{*/
			 int row, int def_color)
{
   char ch, *cond_end = NULL, *elsepart = NULL;

   SLsmg_gotorc (row, 0);
   slrn_set_color (def_color);

   while ((ch = *fmt) != 0)
     {
	char *s, *smax, *p;
	int color = def_color;
	int len = -1, field_len = -1;
	int justify, spaces = 0;

	if (fmt == elsepart)
	  {
	     fmt = cond_end + 1; /* skip the '?' */
	     elsepart = cond_end = NULL;
	     continue;
	  }

	if (ch != '%')
	  {
	     if ((NULL == (s = slrn_strbyte (fmt, '%'))) && (elsepart == NULL))
	       {
		  SLsmg_write_string (fmt);
		  break;
	       }
	     else
	       {
		  char *end;
		  if ((elsepart != NULL) && (s > elsepart)) end = elsepart;
		  else end = s;
		  SLsmg_write_nchars (fmt, end - fmt);
		  fmt = end;
		  continue;
	       }
	  }

	fmt++;
	ch = *fmt++;

	if (ch == '?')
	  {
	     int column;
	     char *res;

	     if (((ch = *fmt++) == '\0') || (*fmt++ != '?') ||
		 (NULL == (cond_end = slrn_strbyte (fmt, '?'))))
	       break; /* syntax error; stop here */
	     if ((NULL == (elsepart = slrn_strbyte (fmt, '&'))) ||
		 (elsepart > cond_end))
	       elsepart = cond_end;

	     column = SLsmg_get_column (); /* callback could print something */
	     res = (cb)(ch, param, &len, &color);
	     SLsmg_gotorc (row, column);

	     if ((res == NULL) || (*res == '\0') ||
		 ((*res == '0' || isspace(*res)) && *(res+1) == '\0'))
	       {
		  fmt = elsepart + 1;
		  if (elsepart == cond_end) cond_end = NULL;
		  elsepart = cond_end;
	       }
	     continue;
	  }

	s = NULL;

	if (ch == '-')
	  {
	     justify = 1;
	     ch = *fmt++;
	  }
	else if (ch == '*')
	  {
	     justify = 2;
	     ch = *fmt++;
	  }
	else
	  justify = 0;

	if (isdigit (ch))
	  {
	     field_len = 0;
	     do
	       {
		  field_len = 10 * field_len + (int) (ch - '0');
		  ch = *fmt++;
	       }
	     while (isdigit (ch));
	  }

	switch (ch)
	  {
	   case 0:
	     break;

	   case '%':
	     s = "%";
	     len = 1;
	     break;

	   case 'g':
	     SLsmg_erase_eol ();
	     if (justify)
	       field_len = SLtt_Screen_Cols - field_len;
	     SLsmg_gotorc (row, field_len);
	     break;

	   default:
	     s = (cb)(ch, param, &len, &color);
	     break;
	  }

	if (s == NULL)
	  continue;

	if (color != def_color)
	  slrn_set_color (color);

	/* In case of UTF-8 characters, it is important to distinguish between
	 * the length in bytes and the length in characters. */
	smax = s + strlen (s);
	len = slrn_screen_strlen (s, smax);
	if (NULL != (p = slrn_strbyte (s, '\n')))
	  len = slrn_screen_strlen (s, p);

	if (field_len != -1)
	  {
	     if (field_len > len)
	       spaces = field_len - len;
	     else
	       len = field_len;
	  }
	if (justify)
	  {
	     int n;
	     if (justify == 1) n = 0; /* right justify */
	     else n = spaces - (spaces / 2); /* center */
	     while (spaces > n)
	       {
		  SLsmg_write_nchars (" ", 1);
		  spaces--;
	       }
	  }
	SLsmg_write_nstring (s, len);
	while (spaces)
	  {
	     SLsmg_write_nchars (" ", 1);
	     spaces--;
	  }
	if (color != def_color)
	  slrn_set_color (def_color);
     }

   SLsmg_erase_eol ();
}
/*}}}*/

int slrn_set_display_format (char **formats, unsigned int num, char *entry) /*{{{*/
{
   if (num >= SLRN_MAX_DISPLAY_FORMATS)
     return -1;

   if (formats[num] != NULL)
     SLang_free_slstring (formats[num]);

   if ((entry == NULL) || (*entry == 0))
     {
	formats[num] = NULL;
	return 0;
     }

   if (NULL == (formats[num] = SLang_create_slstring (entry)))
     return -1;

   return 0;
}
/*}}}*/

unsigned int slrn_toggle_format (char **formats, unsigned int cur) /*{{{*/
{
   unsigned int retval = cur;

   if (Slrn_Prefix_Arg_Ptr != NULL)
     {
	retval = (unsigned int) *Slrn_Prefix_Arg_Ptr % SLRN_MAX_DISPLAY_FORMATS;
	Slrn_Prefix_Arg_Ptr = NULL;
     }
   else retval = (retval + 1) % SLRN_MAX_DISPLAY_FORMATS;

   while ((retval != cur) && (formats[retval] == NULL))
     retval = (retval + 1) % SLRN_MAX_DISPLAY_FORMATS;

   Slrn_Full_Screen_Update = 1;
   return retval;
}
/*}}}*/

int slrn_check_batch (void)
{
   if (Slrn_Batch == 0) return 0;
   slrn_error (_("This function is not available in batch mode."));
   return -1;
}

/*}}}*/

/*{{{ File Related Functions */

#ifdef VMS
/*{{{ VMS Filename fixup functions */

static void vms_fix_name(char *name)
{
   int idx, pos;

   pos = strspn(name, VALID_FILENAME_CHARS);
   if (pos == strlen(name))
     return;
   for(idx=pos;idx<strlen(name);idx++)
     if (!(isdigit(name[idx]) || isalpha(name[idx]) || (name[idx] == '$') || (name[idx] == '_') || (name[idx] == '-')
	   || (name[idx] == '/')))
       name[idx] = '-';
}

static char Copystr[SLRN_MAX_PATH_LEN];
static int vms_copyname1 (char *name)
{
   slrn_strncpy(Copystr, name, sizeof (Copystr));
   return(1);
}

static int vms_copyname2 (char *name, int type)
{
   slrn_strncpy(Copystr, name, sizeof (Copystr));
   return(1);
}

/*}}}*/
#endif

void slrn_make_home_filename (char *name, char *file, size_t n) /*{{{*/
{
   char *home;
#ifndef VMS
   if (slrn_is_absolute_path (name)
       || ((name[0] == '.') &&
	   ((name[1] == SLRN_PATH_SLASH_CHAR) ||
	    ((name[1] == '.') && (name[2] == SLRN_PATH_SLASH_CHAR))))
#if defined(IBMPC_SYSTEM)
       || ((name[0] == '.') &&
	   ((name[1] == '/') ||
	    ((name[1] == '.') && name[2] == '/')))
#endif
       )
     {
#ifdef __CYGWIN__
	if (slrn_cygwin_convert_path (name, file, n))
#endif
	  slrn_strncpy (file, name, n);
#if defined(IBMPC_SYSTEM) && !defined(__CYGWIN__)
	slrn_os2_convert_path (file);
#endif
	return;
     }

   if (NULL == (home = getenv ("SLRNHOME")))
     home = getenv ("HOME");

   *file = 0;
   slrn_dircat (home, name, file, n);
#else /* VMS */
   char *cp, *cp1;
   static char fname[SLRN_MAX_PATH_LEN];
   char fn[SLRN_MAX_PATH_LEN], fn1[SLRN_MAX_PATH_LEN];
   int rc, idx;

   slrn_strncpy (fn1, name, sizeof (fn1));
   if (NULL != slrn_strbyte (name, ':'))
     {
	slrn_strncpy (file, name, n);
	return;
     }

   if (NULL == (home = getenv ("SLRNHOME")))
     home = getenv ("HOME");

   *file = 0;
   if (NULL != (cp = slrn_strbyte (fn1, '/')))
     {
# ifdef __DECC
	*cp = '\0'; cp++;
	cp1 = decc$translate_vms(home);
	if (cp1 == 0 || (int)cp1 == -1)
	  { /* error translating */ }
	else
	  {
	     slrn_strncpy(fname, cp1, sizeof (fname));
	     strcat(cp1, "/"); /* safe ? */
 	  }
	strcat (cp1, fn1); /* safe ? */

	vms_fix_name (cp1);

	rc = decc$to_vms(cp1, vms_copyname2, 0, 2);
 	if (rc > 0)
 	  {
 	     slrn_strncpy(fname, Copystr, sizeof (fname));
 	     rc = mkdir(fname, 0755);
 	  }
	if (strlen (fname) + strlen (cp) < sizeof (fname))
	  strcat(fname, cp); /* safe */
# else
	*cp = '\0'; cp++;
	cp1 = shell$translate_vms(home);
	if (cp1 == 0 || (int)cp1 == -1)
	  { /* error translating */ }
	else
 	  {
	     slrn_strncpy(fname, cp1, sizeof (fname));
	     strcat(cp1, "/"); /* safe ? */
	  }
	strcat (cp1, fn1); /* safe ? */

	vms_fix_name (cp1);

	rc = shell$to_vms(cp1, vms_copyname2, 0, 2);
	if (rc > 0)
	  {
	     slrn_strncpy(fname, Copystr, sizeof (fname));
	     rc = mkdir(fname, 0755);
	  }
	if (strlen (fname) + strlen (cp) < sizeof (fname))
	  strcat(fname, cp); /* safe */
# endif
	slrn_strncpy (file, fname, n);
     }
   else
     {
	if (home != NULL) slrn_strncpy(file, home, n);
	if (strlen (file) + strlen (name) < n)
	  strcat (file, name); /* safe */
     }
#endif /* VMS */
}

/*}}}*/

void slrn_make_home_dirname (char *name, char *dir, size_t n) /*{{{*/
{
   /* This needs modified to deal with VMS directory syntax */
#ifndef VMS
   slrn_make_home_filename (name, dir, n);
#else
   char *home, *cp;
   char fn[SLRN_MAX_PATH_LEN];
   static char fname[SLRN_MAX_PATH_LEN];
   int rc, idx, len;

   if (NULL != slrn_strbyte (name, ':'))
     {
	slrn_strncpy (dir, name, n);
	return;
     }
   home = getenv ("HOME");
   *dir = 0;
   if (cp = strbyte(name,'/'))
     {
#ifdef __DECC
	cp = decc$translate_vms(home);
	if (cp == 0 || (int)cp == -1)
	  { /* error translating */ }
	else
	  {
	     slrn_strncpy(fname, cp, sizeof (fname));
	     strcat(cp, "/"); /* safe ? */
	  }
	strcat (cp, name); /* safe ? */
	vms_fix_name (cp);

	rc = decc$to_vms(cp, vms_copyname2, 0, 2);
	if (rc > 0)
	  {
	     slrn_strncpy(fname, Copystr, sizeof (fname));
	     rc = mkdir(fname, 0755);
	  }
#else
	if (shell$from_vms(home, vms_copyname1, 0))
	  {
	     if (Copystr != NULL)
	       slrn_strncpy (fn, Copystr, sizeof (fn));
	     if (strlen (fn) + 1 < sizeof (fn))
	       strcat(fn, "/"); /* safe */
	  }
	if (strlen (fn) + strlen (name) < sizeof (fn))
	  strcat (fn, name); /* safe */
	vms_fix_name(fn);
	if (shell$to_vms(fn, vms_copyname1, 0))
	  slrn_strncpy (fname, Copystr, sizeof (fname));
#endif
	slrn_strncpy (dir, fname, n);
     }
   else
     {
	if (home != NULL)
	  {
	     slrn_strncpy(dir, home, n);
	     len = strlen(dir) - 1;
	     if (dir[len] == ']')
	       {
		  if (len + 3 + strlen (name) < n)
		    {
		       dir[len] = '.';
		       strcat(dir, name); /* safe */
		       strcat(dir, "]"); /* safe */
		    }
	       }
	     else if (len + 1 + strlen (name) < n)
	       strcat(dir, name); /* safe */
	 }
	else if (strlen (dir) + strlen (name) < n)
	  strcat(dir, name); /* safe */
     }
#endif /* VMS */

   return;
}

/*}}}*/

static unsigned int make_random (void)
{
   static unsigned long s;
   static int init;

   if (init == 0)
     {
	s = (unsigned long) time (NULL) + (unsigned long) getpid ();
	init = 1;
     }

   s = s * 69069UL + 1013904243UL;

   /* The time has been added to make this number somewhat unpredictable
    * based on the previous number.  The whole point of this is to foil
    * any attempt of a hacker to determine the name of the _next_ temp
    * file that slrn will create.
    */
   return (unsigned int) (s + (unsigned long) time (NULL));
}

/* Note: This function should not create a file that is deleted when it
 * is closed.
 */

FILE *slrn_open_tmpfile_in_dir (char *dir, char *file, size_t n)
{
   FILE *fp;
   unsigned int len;
   unsigned int i;
   char buf[80];

#ifndef VMS
   if (2 != slrn_file_exists (dir))
     return NULL;
#endif

#if defined(IBMPC_SYSTEM)
   slrn_snprintf (buf, sizeof (buf), "SLRN%04u", make_random ());
#else
   slrn_snprintf (buf, sizeof (buf), "SLRN%X", make_random ());
#endif

   if (-1 == slrn_dircat (dir, buf, file, n))
     return NULL;

#if defined(IBMPC_SYSTEM)
# define MAX_TMP_FILE_NUMBER 999
#else
# define MAX_TMP_FILE_NUMBER 1024
#endif

   len = strlen (file);
   for (i = 0; i < MAX_TMP_FILE_NUMBER; i++)
     {
	int fd;

	if (len + 5 < n)
	  sprintf (file + len, ".%u", i); /* safe */
	else if (i)
	  break;

	fd = open (file, O_WRONLY | O_CREAT | O_EXCL, S_IREAD | S_IWRITE);
	if (fd != -1)
	  {
	     if (NULL == (fp = fdopen (fd, "w")))
	       close (fd);
	     else
	       return fp;
	  }
     }
   return NULL;
}

FILE *slrn_open_tmpfile (char *file, size_t n) /*{{{*/
{
   char *dir;

   dir = getenv ("TMP");
   if ((dir == NULL) || (2 != slrn_file_exists (dir)))
     dir = getenv ("TMPDIR");

   if ((dir == NULL) || (2 != slrn_file_exists (dir)))
     {
#ifdef VMS
	dir = "SYS$LOGIN:";
#else
# if defined(IBMPC_SYSTEM)
	dir = ".";
# else
	dir = "/tmp";
# endif
#endif
     }

   return slrn_open_tmpfile_in_dir (dir, file, n);
}

/*}}}*/

FILE *slrn_open_home_file (char *name, char *mode, char *file, /*{{{*/
			   size_t n, int create_flag)
{
   char filebuf[SLRN_MAX_PATH_LEN];

   if (file == NULL)
     {
	file = filebuf;
	n = sizeof (filebuf);
     }

   slrn_make_home_filename (name, file, n);

#ifdef VMS
   if (create_flag)
     {
	FILE *fp = fopen (file, mode, "fop=cif");
	if (fp == NULL) perror ("fopen");
	return fp;
     }
#else
   (void) create_flag;
#endif

   if (2 == slrn_file_exists (file)) /* don't open directories */
     return NULL;
   return fopen (file, mode);
}

/*}}}*/

VFILE *slrn_open_home_vfile (char *name, char *file, size_t n)
{
   char filebuf[SLRN_MAX_PATH_LEN];

   if (file == NULL)
     {
	file = filebuf;
	n = sizeof (filebuf);
     }

   slrn_make_home_filename (name, file, n);

   return vopen (file, 4096, 0);
}

/*}}}*/

int slrn_mail_file (char *file, int edit, unsigned int editline, char *to, char *subject) /*{{{*/
{
   FILE *pp;
   Slrn_Article_Type *a;
#if defined(IBMPC_SYSTEM)
   char *buf;
   char outfile [SLRN_MAX_PATH_LEN];
#endif

   if (edit && (Slrn_Batch == 0))
     {
	if (slrn_edit_file (Slrn_Editor_Mail, file, editline, 1) < 0) return -1;
	if (Slrn_Mail_Editor_Is_Mua) return 0;

	while (1)
	  {
	     char rsp;
/* Note to translators: "yY" is for "yes", "nN" for "no", "eE" for "edit".
 * Do not change the length of this string. You cannot use any of the
 * default characters for different fields. */
	     char *responses=_("yYnNeE");

	     if (strlen (responses) != 6)
	       responses = "";
	     rsp = slrn_get_response ("yYnNeE", responses, _("Mail the message? \001Yes, \001No, \001Edit"));
	     rsp = slrn_map_translated_char ("yYnNeE", responses, rsp) | 0x20;
	     if (rsp == 'n') return -1;
	     if (rsp == 'y') break;
	     if (slrn_edit_file (Slrn_Editor_Mail, file, 1, 0) < 0) return -1;
	  }
     }
#ifdef VMS
   buf = slrn_strdup_printf ("%s\"%s\"", MAIL_PROTOCOL, to);
   vms_send_mail (buf, subject, file);
   slrn_free (buf);
#else

   while (1)
     {
	int rsp;

	a = (Slrn_Article_Type*) slrn_malloc (sizeof(Slrn_Article_Type), 1, 1);
	if (a == NULL)
	  return -1;

	if (0 == (rsp = slrn_prepare_file_for_posting(file, &editline, a, to, 1)))
	  break;

	if (rsp == -1)
	  {
	     slrn_art_free_article(a);
	     return -1;
	  }

	SLtt_beep ();
	if (rsp == 1)
	  {
/* Note to translators:
* In the next two strings, "yY" is "yes", "eE" is "edit", "nN" is "no",
* "cC" is "cancel" and "fF" means "force". The usual rules apply.
*/
	     char *responses = _("yYeEnNcC");
	     if (strlen (responses) != 8)
	       responses = "";
	     rsp = slrn_get_response ("yYEenNcC\007", responses, _("re-\001Edit,  or \001Cancel"));
	  }
	else
	  {
	     char *responses = _("yYeEnNcCfF");
	     if (strlen (responses) != 10)
	       responses = "";
	     rsp = slrn_get_response ("EeyYnNcC\007Ff", responses, _("re-\001Edit, \001Cancel, or \001Force the mailing (not recommended)"));
	  }

	rsp = slrn_map_translated_char ("yYeEnNcCfF", _("yYeEnNcCfF"), rsp) | 0x20;

	if ((rsp == 'c') || (rsp == 'n') ||(rsp == 7) )
	  {
	     slrn_art_free_article(a);
	     return -1;
	  }
	if (rsp == 'f')
	  break;

	if (slrn_edit_file (Slrn_Editor_Mail, file, editline, 1) < 0)
	  {
	     slrn_art_free_article(a);
	     return -1;
	  }
	/* try again */
	slrn_art_free_article (a);
     } /* while (1)*/

   slrn_message_now (_("Sending ..."));

# if defined(IBMPC_SYSTEM)
   pp = slrn_open_tmpfile (outfile, sizeof (outfile));
# else
   pp = slrn_popen (Slrn_SendMail_Command, "w");
# endif

   a->cline=a->lines;
   while (a->cline != NULL)
     {
	fputs (a->cline->buf, pp);
	putc('\n', pp);
	a->cline=a->cline->next;
     }
# if defined(IBMPC_SYSTEM)
   slrn_fclose (pp);
   buf = slrn_strdup_strcat (Slrn_SendMail_Command, " ", outfile, NULL);
   /* FIXME */
   if (buf != NULL)
     {
	slrn_posix_system (buf, 0);
	slrn_free (buf);
     }
# else
   slrn_pclose (pp);
# endif

#endif /* NOT VMS */
   slrn_message (_("Sending...done"));

   if (-1 == slrn_save_article_to_mail_file (a, Slrn_Save_Replies_File))
     {
	slrn_art_free_article(a);
	return -1;
     }

   slrn_art_free_article(a);
   return 0;
}

/*}}}*/

#if SLRN_HAS_PIPING
int _slrn_pclose (FILE *fp)
{
   int ret;

   ret = pclose (fp);
   if (ret == 0) return ret;

#if defined(WIFEXITED) && defined(WEXITSTATUS)
   if ((ret != -1) && WIFEXITED(ret))
     {
	ret = WEXITSTATUS(ret);
     }
#endif
   return ret;
}
#endif				       /* SLRN_HAS_PIPING */

#if SLRN_HAS_PIPING
/* There appears to be no simply way of getting the command assocated with
 * a pipe file descriptor.  Therefore, I will simply store them in a table.
 */
typedef struct _Pipe_Cmd_Table_Type
{
   char *cmd;
   FILE *fp;
   struct _Pipe_Cmd_Table_Type *next;
}
Pipe_Cmd_Table_Type;

Pipe_Cmd_Table_Type *Pipe_Cmd_Table;

static int store_pipe_cmd (char *cmd, FILE *fp)
{
   Pipe_Cmd_Table_Type *p;

   if (NULL == (cmd = SLang_create_slstring (cmd)))
     return -1;

   p = (Pipe_Cmd_Table_Type *)slrn_malloc (sizeof (Pipe_Cmd_Table_Type), 1, 0);
   if (p == NULL)
     {
	SLang_free_slstring (cmd);
	return -1;
     }
   p->cmd = cmd;
   p->fp = fp;
   p->next = Pipe_Cmd_Table;
   Pipe_Cmd_Table = p;
   return 0;
}

static void delete_pipe_cmd (FILE *fp)
{
   Pipe_Cmd_Table_Type *last, *next;

   last = NULL;
   next = Pipe_Cmd_Table;
   while (next != NULL)
     {
	if (next->fp == fp)
	  {
	     if (last != NULL)
	       last->next = next->next;
	     else
	       Pipe_Cmd_Table = next->next;

	     SLang_free_slstring (next->cmd);
	     slrn_free ((char *) next);
	     return;
	  }

	last = next;
	next = next->next;
     }

   /* should not be reached */
}

static char *get_pipe_cmd (FILE *fp)
{
   Pipe_Cmd_Table_Type *p;

   p = Pipe_Cmd_Table;
   while (p != NULL)
     {
	if (p->fp == fp)
	  return p->cmd;

	p = p->next;
     }

   return _("**UNKNOWN**");
}
#endif 				       /* SLRN_HAS_PIPING */
int slrn_pclose (FILE *fp) /*{{{*/
{
#if SLRN_HAS_PIPING
   int ret;
   if (fp == NULL) return -1;

   ret = _slrn_pclose (fp);
   if (ret)
     {
	char buf[SLRN_MAX_PATH_LEN];
	fprintf (stderr, _("Command %s returned exit status %d.  Press RETURN.\n"),
		 get_pipe_cmd (fp), ret);
	fgets (buf, sizeof(buf), stdin);
     }

   delete_pipe_cmd (fp);

   slrn_set_display_state (SLRN_TTY_INIT | SLRN_SMG_INIT);
   return 0;
#else
   return -1;
#endif
}

/*}}}*/

FILE *slrn_popen (char *cmd, char *mode) /*{{{*/
{
#if SLRN_HAS_PIPING
   FILE *fp;

   slrn_set_display_state (0);
   fp = popen (cmd, mode);

   if (fp == NULL)
     {
	char buf[256];
	fprintf (stderr, _("Command %s failed to run.  Press RETURN.\n"), cmd);
	fgets (buf, sizeof(buf), stdin);
	slrn_set_display_state (SLRN_TTY_INIT | SLRN_SMG_INIT);
     }
   else (void) store_pipe_cmd (cmd, fp);

   return fp;
#else
   return NULL;
#endif
}

/*}}}*/

/*}}}*/

#if defined(__GNUC__)
# pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif
/* returns a malloced string */
static char *create_edit_command (char *edit, char *file, unsigned int line) /*{{{*/
{
   int d, s;
   char ch, *p = edit;
   /* Look for %d and %s */

   d = s = 0;

   while (0 != (ch = *p++))
     {
	if (ch != '%') continue;
	ch = *p;
	if (!d && (ch == 'd'))
	  {
	     *p = 'u';		       /* map %d to %u (unsigned) */
	     if (s == 0) d = 1; else d = 2;
	  }
	else if (!s && (ch == 's'))
	  {
	     if (d == 0) s = 1; else s = 2;
	  }
	else
	  {
	     slrn_error (_("Invalid Editor definition."));
	     return NULL;
	  }
	p++;
     }

#if defined(IBMPC_SYSTEM)
   /* Convert editor pathnames from / form to \\ form.  I wonder what
    * happens when the pathname contains a space.  Hmmm...
    */
   p = edit;
   while ((*p != ' ') && (*p != '\t') && (*p != 0))
     {
	if (*p == '/') *p = SLRN_PATH_SLASH_CHAR;
	p++;
     }
#endif

   /* No %d, %s */

   if ((d == 0) && (s == 0))
     {
	return slrn_strdup_strcat (edit, " ", file, NULL);
     }
   else if (d == 0)
     {
	return slrn_strdup_printf (edit, file);
     }
   else if (s == 0)
     {
	char *retval, *cmd1;
	cmd1 = slrn_strdup_printf (edit, (int) line);
	retval = slrn_strdup_strcat (cmd1, " ", file, NULL);
	slrn_free (cmd1);
	return retval;
     }
   else /* d and s */
     {
	if (d == 1)
	  return slrn_strdup_printf (edit, line, file);
	else return slrn_strdup_printf (edit, file, line);
     }
   /* We should never get here */
   return NULL;
}
#if defined(__GNUC__)
# pragma GCC diagnostic warning "-Wformat-nonliteral"
#endif

/*}}}*/

/* This function returns -1 upon failure, -2 upon unmodified edit */
int slrn_edit_file (char *editor, char *file, unsigned int line,
		    int check_mtime) /*{{{*/
{
   char *cmd, *editcmd, *msg = NULL;
   int ret;
   unsigned long mtime = 0;
   struct stat st;

   if (Slrn_Abort_Unmodified == 0)
     check_mtime = 0;

   if ((editor == NULL) || (*editor == 0))
     editor = Slrn_Editor;

   if (((editor == NULL) || (*editor == 0))
       && (NULL == (editor = getenv("SLRN_EDITOR")))
       && (NULL == (editor = getenv("SLANG_EDITOR")))
       && (NULL == (editor = getenv("EDITOR")))
       && (NULL == (editor = getenv("VISUAL"))))
     {
#if defined(VMS) || defined(__NT__) || defined(__WIN32__)
	editor = "edit";
#else
# if defined(__os2__)
	editor = "e";
# else
#  ifdef __unix__
	editor = "vi";
#  else
	slrn_error (_("No editor command defined."));
	return -1;
#  endif
# endif
#endif
     }
   editcmd = slrn_safe_strmalloc (editor);

   if (NULL == (cmd = create_edit_command (editcmd, file, line))) return -1;

   if (check_mtime
       && (0 == stat (file, &st)))
     mtime = (unsigned long) st.st_mtime;

   ret = slrn_posix_system (cmd, 1);

#if defined(WIFEXITED) && defined(WEXITSTATUS)
   if ((ret != -1) && WIFEXITED(ret))
     {
	ret = WEXITSTATUS(ret);
	if (ret == 127)
	  {
	     msg = _("The editor could not be found.");
	     ret = -1;
	  }
	else if (ret == 126)
	  {
	     msg = _("The editor was found, but could not be executed.");
	     ret = -1;
	  }
	else if (ret)
	  msg = _("The editor returned a non-zero status.");
     }
#endif

   /* Am I the only one who thinks this is a good idea?? */
   if (Slrn_TT_Initialized) while (SLang_input_pending (5))
     SLang_flush_input ();

   if (check_mtime
       && (ret != -1)
       && (0 == stat (file, &st))
       && (mtime == (unsigned long) st.st_mtime))
     {
	if (msg == NULL)
	  msg = _("File was not modified.");
	ret = -2;
     }

#if defined(IBMPC_SYSTEM) && !defined(__CYGWIN__)
   if ((strlen (cmd) > 1)
       && (cmd[1] == ':')
       && (cmd[2] != '\\'))
     {
	msg = _("Please use double '\\\\' to separate directories in editor_command");
     }
#endif

   if (msg != NULL)
     {
	slrn_message ("%s", msg);
	if (ret >= 0)
	  {
	     slrn_update_screen ();
	     slrn_sleep (2);
	  }
     }
   slrn_free (cmd);
   slrn_free (editcmd);
   return ret;
}

/*}}}*/

/*{{{ Get Input From User Related Functions */

static void rline_update (SLrline_Type *rli, char *prompt,
			  char *buf, unsigned int len, unsigned int point,
			  VOID_STAR client_data)
{
   int col;
   char *ubuf, *u, *umax, *upoint;
   unsigned int prompt_len;

   (void) client_data;
   (void) rli;

   slrn_push_suspension (0);
   if (prompt == NULL)
     prompt = "";

   prompt_len = strlen (prompt);
   ubuf = slrn_safe_malloc (prompt_len + len + 1);
   strcpy (ubuf, prompt);
   strncpy (ubuf + prompt_len, buf, len);

   len += prompt_len;
   ubuf[len] = 0;

   umax = ubuf + len;
   upoint = ubuf + prompt_len + point;

   u = ubuf;
   do
     {
	SLsmg_gotorc (SLtt_Screen_Rows - 1, 0);
	SLsmg_write_nchars (u, (unsigned int) (upoint - u));
	col = SLsmg_get_column ();
	if (col < SLtt_Screen_Cols)
	  break;

	u = skip_char (u, upoint, 1);
     }
   while (u < upoint);

   SLsmg_write_nchars (upoint, (unsigned int) (umax-upoint));
   slrn_free (ubuf);

   SLsmg_erase_eol ();
   SLsmg_gotorc (SLtt_Screen_Rows - 1, col);
   slrn_smg_refresh ();
   slrn_pop_suspension ();
}

/* If 1, redraw read_line.  If 2, redraw message */
static int Reading_Input;

static void redraw_mini_buffer (void) /*{{{*/
{
   if (Reading_Input == 1)
     SLrline_redraw (Slrn_Keymap_RLI);
   else if (Reading_Input == 2)
     redraw_message ();
}

/*}}}*/

static SLang_RLine_Info_Type *init_readline (void) /*{{{*/
{
   SLrline_Type *rli;
   unsigned int flags = SL_RLINE_BLINK_MATCH;

   if (Slrn_UTF8_Mode)
     flags |= SL_RLINE_UTF8_MODE;

   if (NULL == (rli = SLrline_open (SLtt_Screen_Cols, flags)))
     return NULL;

   (void) SLrline_set_update_hook (rli, rline_update, NULL);
   return rli;
}

/*}}}*/

static int read_from_input_string (char *str)
{
   char *s;

   if (Input_String == NULL) return -1;

   s = slrn_strbyte (Input_String_Ptr, '\n');
   if (s != NULL)
     *s = 0;

   strncpy (str, Input_String_Ptr, 255);
   str[255] = 0;

   if (s == NULL)
     {
	SLFREE (Input_String);
	Input_String_Ptr = Input_String = NULL;
     }
   else Input_String_Ptr = s + 1;

   return strlen (str);
}

/* s could be NULL.  If so, input string is cleared.  The calling routine
 * should not free the input pointer.
 */
void slrn_set_input_string (char *s)
{
   slrn_free (Input_String);
   Input_String = s;
   Input_String_Ptr = s;
}

static char read_from_input_char (void)
{
   if (Input_Chars_Ptr == NULL)
     return 0;

   if (*Input_Chars_Ptr == 0)
     {
	slrn_set_input_chars (NULL);
	return 0;
     }

   return *Input_Chars_Ptr++;
}

void slrn_set_input_chars (char *s)
{
   slrn_free (Input_Chars);
   Input_Chars = s;
   Input_Chars_Ptr = s;
}

/* The char * argument is expected to be SLRN_MAX_PATH_LEN in size */
static int (*Complete_Open) (char *);
static int (*Complete_Next) (char *);

static char File_Pattern[SLRN_MAX_PATH_LEN], Dir_Name[SLRN_MAX_PATH_LEN];
static Slrn_Dir_Type *Dir;
static int In_Completion;

/* buf needs room for at least SLRN_MAX_PATH_LEN chars */
static int dir_findnext (char *buf) /*{{{*/
{
   Slrn_Dirent_Type *dirent;
   unsigned int len = strlen (File_Pattern);

   while (NULL != (dirent = slrn_read_dir (Dir)))
     {
	char *name = dirent->name;
	if ((*name == '.') && ((*(name+1) == 0) ||
			       ((*(name+1) == '.') && (*(name+2) == 0))))
	  continue;
	if (!strncmp (dirent->name, File_Pattern, len))
	  {
	     strcpy (buf, Dir_Name); /* safe */
	     len = strlen (buf);
	     slrn_strncpy (buf+len, dirent->name, SLRN_MAX_PATH_LEN - len - 1);
	     return 1;
	  }
     }
   return 0;
}
/*}}}*/

static int dir_findfirst (char *buf) /*{{{*/
{
   int pos = strlen (buf);

   if (Dir != NULL)
     {
	slrn_close_dir (Dir);
	Dir = NULL;
     }

   while ((pos >= 0) && (buf[pos] != SLRN_PATH_SLASH_CHAR))
     pos--;

   if (pos == -1)
     {
	unsigned int len;
	if (NULL == slrn_getcwd (Dir_Name, sizeof (Dir_Name) - 1))
	  return 0;
	len = strlen (Dir_Name);
	Dir_Name[len] = SLRN_PATH_SLASH_CHAR;
	Dir_Name[len+1] = '\0';
     }
   else
     {
	if (pos + 2 > SLRN_MAX_PATH_LEN)
	  pos = SLRN_MAX_PATH_LEN - 2;
	slrn_strncpy (Dir_Name, buf, pos + 2);
     }

   pos++;
   slrn_strncpy (File_Pattern, buf + pos, sizeof (File_Pattern));

   if (NULL == (Dir = slrn_open_dir (Dir_Name)))
     return 0;

   return dir_findnext (buf);
}
/*}}}*/

typedef struct
{
   char *what;
   void *dummy;
}
Generic_Var_Type;

static Generic_Var_Type *Var;
static int Var_Pos;

static int var_findnext (char *buf) /*{{{*/
{
   unsigned int len = strlen (File_Pattern);

   while (Var->what != NULL)
     {
	int retval = 0;
	if (!strncmp (Var->what, File_Pattern, len))
	  {
	     strcpy (buf, Dir_Name); /* safe */
	     len = strlen (buf);
	     slrn_strncpy (buf+len, Var->what, SLRN_MAX_PATH_LEN - len - 1);
	     retval = 1;
	  }
	Var++;
	if ((Var->what == NULL) && (Var_Pos == 0))
	  {
	     Var = (Generic_Var_Type*) Slrn_Str_Variables;
	     Var_Pos++;
	  }
	if (retval) return 1;
     }
   return 0;
}
/*}}}*/

static int var_findfirst (char *buf) /*{{{*/
{
   int pos = strlen (buf);
   while ((pos > 0) && (buf[pos-1] != ' '))
     pos--;

   slrn_strncpy (Dir_Name, buf, pos + 1);
   slrn_strncpy (File_Pattern, buf + pos, SLRN_MAX_PATH_LEN);
   Var = (Generic_Var_Type*) Slrn_Int_Variables; Var_Pos = 0;
   return var_findnext (buf);
}
/*}}}*/

static int strpcmp (char **a, char **b)
{
   return strcmp (*a, *b);
}

static void rli_self_insert (void) /*{{{*/
{
   char last_char [2];
   last_char[0] = (char) SLang_Last_Key_Char;
   last_char[1] = '\0';
   (void) SLrline_ins (Slrn_Keymap_RLI, last_char, 1);
   /* SLrline_redraw in slang-2.0.x did not use the redraw-hook.  So
    * avoid using this here.  It is not needed anyway.
    */
   /* SLrline_redraw (Slrn_Keymap_RLI); */
}
/*}}}*/

static void generic_mini_complete (int cycle) /*{{{*/
{
   char *pl, *pb;
   char last[SLRN_MAX_PATH_LEN], buf[SLRN_MAX_PATH_LEN];
   static char prev[SLRN_MAX_PATH_LEN], prevcall[SLRN_MAX_PATH_LEN];
   int repeat = 1; /* whether we are called with identical values again */
   unsigned int n;
   static int flag = 0;  /* when flag goes 0, we call open */
   static unsigned int lastpoint = 0;
   char **argv = NULL;
   unsigned int argc = 0, maxargc = 0;
   unsigned int point;
   char *rli_buf;

   if (Complete_Open == NULL)
     {
	rli_self_insert ();
	return;
     }

   n = sizeof (buf);
   (void) SLrline_get_point (Slrn_Keymap_RLI, &point);
   if (point < n)
     n = point + 1;

   rli_buf = SLrline_get_line (Slrn_Keymap_RLI);   /* malloced pointer */
   if (rli_buf == NULL)
     return;

   slrn_strncpy (buf, rli_buf, n);
   n = 0;

   if (strcmp (rli_buf, prevcall) ||
       (point != lastpoint) ||
       (0 == In_Completion))
     {
	slrn_strncpy (prev, buf, sizeof(prev));
	/* strcpy (prev, buf); */ /* safe */ /* save this search context */
	flag = 0;
	repeat = 0;
     }

   SLfree (rli_buf); rli_buf = NULL;

   if (In_Completion == 2)
     repeat = 0;

   if (cycle)
     {
	if (flag)
	  flag = (*Complete_Next)(buf);
	else if (0 == (flag = (*Complete_Open)(buf)))
	  {
	     rli_self_insert ();
	     return;
	  }
	if (flag == 0)
	  slrn_strncpy (buf, prev, sizeof (buf));
	strcpy (last, buf); /* safe */
     }
   else
     {
	flag = (*Complete_Open)(buf);
	strcpy (last, buf); /* safe */

	/* This loop tests all values from complete_next and returns the
	 * smallest length match of initial characters of buf */
	while (flag)
	  {
	     pl = last;
	     pb = buf;

	     if (repeat)
	       {
		  if (argc == maxargc)
		    {
		       char **newargv;
		       maxargc += 512;
		       newargv = (char **) slrn_realloc
			 ((char *) argv, maxargc * sizeof (char *), 0);
		       if (newargv == NULL)
			 {
			    slrn_free_argc_argv_list (argc, argv);
			    slrn_free ((char *) argv);
			    argc = 0;
			 }
		       argv = newargv;
		    }
		  if ((NULL != argv) &&
		      (NULL != (argv[argc] = slrn_safe_strmalloc (buf))))
		    argc++;
	       }

	     while (*pl && (*pl == *pb))
	       {
		  pl++;
		  pb++;
	       }

	     *pl = 0;
	     n++;
	     flag = (*Complete_Next)(buf);
	  }
     }

   if (cycle || n)
     {
	unsigned int len;

	if (repeat && (argc > 1))
	  {
	     int sel;
	     void (*qsort_fun) (char **, unsigned int,
				unsigned int, int (*)(char **, char **));
	     /* This seems to be necessary (see art_sort.c:slrn_sort_headers) */
	     qsort_fun = (void (*)(char **, unsigned int,
				   unsigned int, int (*)(char **, char **)))
	       qsort;

	     qsort_fun (argv, argc, sizeof (char *), strpcmp);

	     sel = slrn_select_list_mode (_("Possible completions"), argc,
					  argv, 0, 1, NULL);
	     slrn_update_screen ();
	     if (sel != -1)
	       {
		  slrn_strncpy (last, argv[sel], sizeof (last));
		  n = 1;
	       }
	  }

	len = strlen (last);

	if ((n < 2) && ((FVOID_STAR) Complete_Open == (FVOID_STAR) dir_findfirst) &&
	    (flag || !cycle) && (len + 1 < sizeof (last)))
	  {
	     if (2 == slrn_file_exists (last))
	       {
		  last[len] = SLRN_PATH_SLASH_CHAR;
		  last[len+1] = 0;
	       }
	  }

	if (-1 == SLrline_set_line (Slrn_Keymap_RLI, last))
	  return;

	SLrline_redraw (Slrn_Keymap_RLI);
     }
   else SLtt_beep();

   if (!cycle)
     strcpy (prev, last); /* safe; make this the new search context */

   slrn_free_argc_argv_list (argc, argv);
   slrn_free ((char *) argv);
   rli_buf = SLrline_get_line (Slrn_Keymap_RLI);
   if (rli_buf == NULL)
     {
	In_Completion = 0;
	return;
     }
   slrn_strncpy (prevcall, rli_buf, sizeof (prevcall));
   SLfree (rli_buf);
   (void) SLrline_get_point (Slrn_Keymap_RLI, &lastpoint);
   In_Completion = cycle ? 2 : 1;

   return;
}

/*}}}*/

static int mini_complete (SLrline_Type *rli)
{
   (void) rli;
   generic_mini_complete (0);
   return 0;
}

static int mini_cycle (SLrline_Type *rli)
{
   (void) rli;
   generic_mini_complete (1);
   return 0;
}

static int rli_del_bol (SLrline_Type *rli) /*{{{*/
{
   unsigned int point;

   (void) SLrline_get_point (rli, &point);
   (void) SLrline_set_point (rli, 0);
   (void) SLrline_del (rli, point);
   return 0;
}
/*}}}*/

static int rli_del_bow (SLrline_Type *rli) /*{{{*/
{
   char *buf, *b;
   unsigned int point;
   unsigned int len;

   (void) SLrline_get_point (rli, &point);
   if (point == 0)
     return 0;

   if (NULL == (buf = SLrline_get_line (rli)))
     return -1;

   b = buf + (point - 1);
   while ((b > buf) && ((*b == ' ') || (*b == '\t')))
     b--;

   while (b > buf)
     {
	if ((*b == ' ') || (*b == '\t'))
	  {
	     b++;
	     break;
	  }
	b--;
     }

   len = point;
   point = b - buf;
   (void) SLrline_set_point (rli, point);
   (void) SLrline_del (rli, len - point);

   SLfree (buf);
   return 0;
}
/*}}}*/

#define A_KEY(s, f)  {s, (int (*)(void)) f}

static SLKeymap_Function_Type Slrn_Custom_Readline_Functions [] =
{
   A_KEY("complete", mini_complete),
   A_KEY("cycle", mini_cycle),
   A_KEY("delbol", rli_del_bol),
   A_KEY("delbow", rli_del_bow),
   A_KEY(NULL, NULL)
};

int slrn_rline_setkey (char *key, char *fun, SLkeymap_Type *kmap)
{
   SLKeymap_Function_Type *tmp;
   int failure;

   if (NULL != SLang_find_key_function(fun, kmap))
     return SLang_define_key (key, fun, kmap);

   tmp = kmap->functions;
   kmap->functions = Slrn_Custom_Readline_Functions;
   failure = SLang_define_key (key, fun, kmap);
   kmap->functions = tmp;

   return failure;
}

/* str needs to have enough space for SLRL_DISPLAY_BUFFER_SIZE characters */
static int generic_read_input (char *prompt, char *dfl, char *str, int trim_flag,
			       int no_echo, int point) /*{{{*/
{
   int i;
   int tt_init_state;
   char prompt_buf[SLRL_DISPLAY_BUFFER_SIZE];
   char *buf;
   unsigned int len;
   int save_slang_error;

   if (prompt == NULL) prompt = "";

   Slrn_Full_Screen_Update = 1;

   slrn_strncpy (prompt_buf, prompt, sizeof (prompt_buf));
   len = strlen (prompt);

   if ((dfl != NULL) && *dfl)
     {
	if (slrn_snprintf (prompt_buf + len, sizeof (prompt_buf) - len,
	   _("(default: %s) "), dfl) == (int) (sizeof (prompt_buf) - len))
	  {
	     *(prompt_buf + len) = '\0';
	     dfl = NULL;
	  }
     }
   prompt = prompt_buf;

   if ((str == NULL) && (dfl == NULL)) return -1;

   SLrline_set_display_width (Slrn_Keymap_RLI, SLtt_Screen_Cols);

   /* slrn_set_suspension (1); */

   if ((str != NULL) && *str)
     {
	if (-1 == SLrline_set_line (Slrn_Keymap_RLI, str))
	  return -1;

	if (point < 0)
	  {
	     unsigned int slen = strlen (str);
	     if ((unsigned int)-point > slen) point = 0;
	     else point = slen + (1 + point);
	  }
	SLrline_set_point (Slrn_Keymap_RLI, point);
	*str = 0;
     }
   if (str == NULL) str = dfl;

   i = read_from_input_string (str);
   if (i >= 0) return i;

   tt_init_state = Slrn_TT_Initialized;

   slrn_set_display_state (Slrn_TT_Initialized | SLRN_TTY_INIT);

   if (no_echo)
     SLrline_set_echo (Slrn_Keymap_RLI, 0);
   else
     SLrline_set_echo (Slrn_Keymap_RLI, 1);

   if (tt_init_state & SLRN_SMG_INIT)
     (void) SLrline_set_update_hook (Slrn_Keymap_RLI, rline_update, NULL);
   else
     (void) SLrline_set_update_hook (Slrn_Keymap_RLI, NULL, NULL);

   slrn_enable_mouse (0);

   save_slang_error = SLang_get_error ();
   SLang_set_error (0);

   Reading_Input = 1;
   slrn_set_color (MESSAGE_COLOR);

   buf = SLrline_read_line (Slrn_Keymap_RLI, prompt, &len);
   slrn_set_color (0);
   Reading_Input = 0;

   slrn_enable_mouse (1);

   if ((buf != NULL) && (0 == SLang_get_error ()) && !SLKeyBoard_Quit)
     {
	char *b = buf;

	if (*b)
	  {
	     if (no_echo == 0) SLrline_save_line (Slrn_Keymap_RLI);
	     if (trim_flag)
	       {
		  slrn_trim_string (b);
		  b = slrn_skip_whitespace (b);
	       }
	  }
	else if (dfl != NULL) b = dfl;

	/* b could be equal to dfl and dfl could be equal to str.  If this is
	 * the case, there is no need to perform the strcpy */
	if (b != str)
	  slrn_strncpy (str, b, SLRL_DISPLAY_BUFFER_SIZE); /* safe */

	i = strlen (str);
     }
   else i = -1;

   SLfree (buf);

   if (SLKeyBoard_Quit) i = -1;
   SLKeyBoard_Quit = 0;
   SLang_set_error (save_slang_error);

   slrn_set_display_state (tt_init_state);

   if (tt_init_state & SLRN_SMG_INIT)
     {
	/* put cursor at edge of screen to comfort user */
	SLsmg_gotorc (SLtt_Screen_Rows - 1, 0);
	slrn_smg_refresh ();
     }
   else
     {
	putc ('\n', stdout);
	fflush (stdout);
     }

   /* slrn_set_suspension (0); */
   return i;
}

/*}}}*/

int slrn_read_input (char *prompt, char *dfl, char *str, int trim_flag, int point)
{
   return generic_read_input (prompt, dfl, str, trim_flag, 0, point);
}

int slrn_read_input_no_echo (char *prompt, char *dfl, char *str, int trim_flag, int point)
{
   return generic_read_input (prompt, dfl, str, trim_flag, 1, point);
}

int slrn_read_filename (char *prompt, char *dfl, char *str, int trim_flag, int point)
{
   int retval;
   Complete_Open = dir_findfirst;
   Complete_Next = dir_findnext;
   retval = generic_read_input (prompt, dfl, str, trim_flag, 0, point);
   Complete_Open = NULL;
   Complete_Next = NULL;
   In_Completion = 0;
   return retval;
}

int slrn_read_variable (char *prompt, char *dfl, char *str, int trim_flag, int point)
{
   int retval;
   Complete_Open = var_findfirst;
   Complete_Next = var_findnext;
   retval = generic_read_input (prompt, dfl, str, trim_flag, 0, point);
   Complete_Open = NULL;
   Complete_Next = NULL;
   In_Completion = 0;
   return retval;
}

int slrn_read_artnum_int (char *prompt, NNTP_Artnum_Type *dflt, NNTP_Artnum_Type *np) /*{{{*/
{
   char sdfl_buf[256];
   char *sdfl = NULL;
   char str[SLRL_DISPLAY_BUFFER_SIZE];
   NNTP_Artnum_Type n;

   if (dflt != NULL)
     {
	sprintf (sdfl_buf, NNTP_FMT_ARTNUM, *dflt); /* safe */
	sdfl = sdfl_buf;
     }

   *str = 0;
   if (-1 == slrn_read_input (prompt, sdfl, str, 1, 0))
     {
	slrn_error (_("Abort!"));
	return -1;
     }

   if (1 != sscanf(str, NNTP_FMT_ARTNUM, &n))
     {
	slrn_error (_("Integer expected."));
	return -1;
     }
   *np = n;
   return 0;
}

/*}}}*/

int slrn_init_readline (void) /*{{{*/
{
   if ((Slrn_Keymap_RLI == NULL)
       && (NULL == (Slrn_Keymap_RLI = init_readline ())))
     return -1;

   Slrn_RLine_Keymap = SLrline_get_keymap (Slrn_Keymap_RLI);
   SLkm_define_key ("\t", (FVOID_STAR) mini_complete, Slrn_RLine_Keymap);
   SLkm_define_key (" ", (FVOID_STAR) mini_cycle, Slrn_RLine_Keymap);
   SLkm_define_key ("^U", (FVOID_STAR) rli_del_bol, Slrn_RLine_Keymap);
   SLkm_define_key ("^W", (FVOID_STAR) rli_del_bow, Slrn_RLine_Keymap);

   return 0;
}

/*}}}*/

char slrn_map_translated_char (char *native_chars, /*{{{*/
			       char *translated_chars, char rsp)
{
   char *pos;
   if ((strlen (native_chars) != strlen (translated_chars)) ||
       (NULL != slrn_strbyte (native_chars, rsp)) ||
       (NULL == (pos = slrn_strbyte (translated_chars, rsp))))
     return rsp;
   return native_chars[pos - translated_chars];
}
/*}}}*/

char slrn_get_response (char *valid_chars, char *translated_chars, /*{{{*/
			char *str, ...)
{
   char ch;
   va_list ap;
   char *v;

   /* if (SLang_Error) return -1; */
   if (Error_Present)
     slrn_error_now (2, NULL);

   while (1)
     {
	Slrn_Full_Screen_Update = 1;

	ch = read_from_input_char ();

	if (ch == 0)
	  {
	     if (Slrn_TT_Initialized == 0)
	       {
		  char buf[256];

		  va_start(ap, str);
		  slrn_tty_vmessage (stdout, str, ap);
		  va_end(ap);

		  *buf = 0;
		  (void) fgets (buf, sizeof(buf), stdin);
		  ch = *buf;
	       }
	     else
	       {
		  SLang_flush_input ();
		  slrn_clear_message ();

		  va_start(ap, str);
		  vmessage_1 (MESSAGE_COLOR, str, ap);
		  va_end(ap);

		  slrn_smg_refresh ();

		  Reading_Input = 2;
		  ch = SLang_getkey ();
		  Reading_Input = 0;

		  slrn_clear_message ();
		  SLang_set_error (0);
		  SLKeyBoard_Quit = 0;
	       }
	  }

	v = valid_chars;
	while (*v)
	  {
	     if (*v == ch) return ch;
	     v++;
	  }
	v = translated_chars;
	while (*v)
	  {
	     if (*v == ch) return ch;
	     v++;
	  }

	slrn_error_now (0, _("Invalid response! Try again."));
	if (Slrn_TT_Initialized & SLRN_TTY_INIT)
	  {
	     (void) SLang_input_pending (15);
	  }
     }
}

/*}}}*/

/*{{{ Doubles percent characters. p0 is source, p is destination. */
static void escape_percent_chars (char *p, char *p0, char *pmax)
{
   while ((*p0) && (p < pmax - 2))
     {
	if (*p0 == '%')
	  *p++ = '%';
	*p++ = *p0++;
     }
   *p = '\0';
}
/*}}}*/

int slrn_get_yesno (int dflt, char *str, ...) /*{{{*/
/* For both dflt and the return value, 0 means "no", 1 is "yes". */
{
   va_list ap;
   char buf0[512], buf[512];
   char ch, rsp;
   char *prompt, *fmt;
   /* Note to translators:
    * The translated string needs to have exactly four characters.
    * The first two become valid keys for "yes", the last two for "no".
    * You *cannot* use "y" for "no" or "n" for "yes".
    */
   char *responses=_("yYnN");

   /* if (SLang_Error) return -1; */

   va_start(ap, str);
   (void) slrn_vsnprintf(buf0, sizeof (buf0), str, ap);
   va_end(ap);

   /* As the prompt will be processed through printf again (by
    * slrn_get_response), we need to escape percent characters. */
   escape_percent_chars (buf, buf0, buf + sizeof (buf));

   if (dflt)
     {
	ch = 'y';
	fmt = _("? [\001Y]es, \001No");
     }
   else
     {
	ch = 'n';
	fmt = _("? \001Yes, [\001N]o");
     }

   prompt = slrn_strdup_strcat (buf, fmt, NULL);
   if (strlen (responses) != 4) /* Translator messed it up */
     responses = "";
   rsp = slrn_get_response ("yYnN\r\n", responses, "%s", prompt);
   slrn_free (prompt);
   if ((rsp == '\r') || (rsp == '\n')) rsp = ch;
   else rsp = slrn_map_translated_char ("yYnN", responses, rsp) | 0x20;

   if (rsp == 'n')
     return 0;
   return 1;
}

/*}}}*/
int slrn_get_yesno_cancel (int dflt, char *str, ...) /*{{{*/
{
   va_list ap;
   char buf[512];
   char rsp, *prompt;
   /* Note to translators:
    * The translated string needs to have exactly six characters.
    * The first two become valid keys for "yes", the next two for "no",
    * the last ones for "cancel". You cannot use the default characters
    * for other fields than they originally stood for.
    */
   char *responses=_("yYnNcC");
   if (strlen (responses) != 6)
     responses = "";

   if (SLang_get_error ()) return -1;

   va_start(ap, str);
   (void) slrn_vsnprintf(buf, sizeof(buf), str, ap);
   va_end(ap);

   if (dflt == 1)
     prompt = slrn_strdup_strcat (buf, _("? [\001Y]es, \001No, \001Cancel"), NULL);
   else if (dflt == 0)
     prompt = slrn_strdup_strcat (buf, _("? Yes, \001[N]o, \001Cancel"), NULL);
   else
     prompt = slrn_strdup_strcat (buf, _("? Yes, \001No, \001[C]ancel"), NULL);

   if (prompt == NULL)
     return -1;

   rsp = slrn_get_response ("\007yYnNcC\r", responses, "%s", prompt);
   slrn_free (prompt);

   if (rsp == '\r') return dflt;

   if (rsp == 7) return -1;

   rsp = slrn_map_translated_char ("yYnNcC", responses, rsp) | 0x20;

   if (rsp == 'y') return 1;
   if (rsp == 'n') return 0;
   return -1;
}

/*}}}*/

void slrn_get_mouse_rc (int *rp, int *cp) /*{{{*/
{
   int r, c;

   c = (unsigned char) SLang_getkey () - 32;
   r = (unsigned char) SLang_getkey () - 32;
   if (cp != NULL) *cp = c;
   if (rp != NULL) *rp = r;
}

/*}}}*/

void slrn_evaluate_cmd (void) /*{{{*/
{
   char buf[SLRL_DISPLAY_BUFFER_SIZE];

   *buf = '\0';
   if (slrn_read_input ("S-Lang> ", NULL, buf, 0, 0) > 0)
     {
	SLang_load_string (buf);
     }

   SLang_set_error (0);
}
/*}}}*/

/*}}}*/

/*{{{ Misc Regexp Utility Functions */

SLRegexp_Type *slrn_compile_regexp_pattern (char *pat) /*{{{*/
{
   SLRegexp_Type *re;
   unsigned int flags = SLREGEXP_CASELESS;

   if (Slrn_UTF8_Mode) flags |= SLREGEXP_UTF8;
   if (NULL == (re = SLregexp_compile (pat, flags)))
     {
	slrn_error (_("Invalid regular expression or expression too long."));
	return NULL;
     }
   return re;
}

/*}}}*/

unsigned char *slrn_regexp_match (SLRegexp_Type *re, char *str) /*{{{*/
{
   return (unsigned char *)SLregexp_match (re, str, strlen (str));
}

/*}}}*/

/*}}}*/

int slrn_is_fqdn (char *h) /*{{{*/
{
   char *p;

   /* Believe it or not, I have come across one system with a '(' character
    * as part of the hostname!!!  I suppose that I should also check for
    * other strange characters as well.  This is an issue since a message
    * id will be composed from the fqdn.  For that reason, such names will
    * be rejected.  Sigh.
    */
   if (NULL != slrn_strbrk (h, "~`!@#$%^&*()=+|\\[]{}/?;"))
     return 0;

   p = slrn_strbyte (h, '.');
   if ((p == NULL) || (p == h))
     return 0;

   /* Make sure it does not end in a '.' */
   if (p [strlen(p)-1] == '.')
     return 0;

   return 1;
}
/*}}}*/

/* Try to get a fully qualified domain name. */
static char *get_hostname (void)
{
   struct hostent *host_entry;
#ifdef HAVE_GETADDRINFO
   struct addrinfo hint, *res;
   int status;
#endif
   char buf[MAX_HOST_NAME_LEN + 1];

   if ((-1 == gethostname (buf, MAX_HOST_NAME_LEN))
       || (*buf == 0))
     return NULL;

   /* gethostname may not provide the full name so use gethostbyname
    * to get more information.  Why isn't there a simplified interface to
    * get the FQDN!!!!
    */
#ifdef HAVE_GETADDRINFO
   memset(&hint, 0, sizeof (hint));
   hint.ai_flags = AI_CANONNAME;

   status = getaddrinfo(buf, NULL, &hint, &res);
   if (status == EAI_AGAIN)
     {
	slrn_sleep (2);
	status = getaddrinfo(buf, NULL, &hint, &res);
     }

   if (status == 0)
     {
        if ((res->ai_canonname != NULL)
            && slrn_is_fqdn (res->ai_canonname))
          {
             char *ret = slrn_safe_strmalloc (res->ai_canonname);
             freeaddrinfo(res);
             return ret;
          }
	freeaddrinfo(res);
     }
   /* drop */
#endif

   host_entry = gethostbyname (buf);

# if defined(TRY_AGAIN) && !defined(MULTINET)
   if ((host_entry == NULL) && (h_errno == TRY_AGAIN))
     {
	slrn_sleep (2);
	host_entry = gethostbyname (buf);
     }
# endif

   if ((host_entry != NULL)
       && (host_entry->h_name != NULL)
       && (host_entry->h_name[0] != 0))
     {
	char **aliases;

	if ((0 == slrn_is_fqdn ((char *)host_entry->h_name))
	    && (NULL !=	(aliases = host_entry->h_aliases)))
	  {
	     while (*aliases != NULL)
	       {
		  if (slrn_is_fqdn (*aliases))
		    return slrn_safe_strmalloc (*aliases);
		  aliases++;
	       }
	  }

	return slrn_safe_strmalloc ((char *)host_entry->h_name);
     }
   return slrn_safe_strmalloc (buf);
}

#ifdef OUR_HOSTNAME
/* Returns a pointer to a statically allocated area */
static char *get_host_from_filename (char *file)
{
   FILE *fp;
   char *host;
   static char line[MAX_HOST_NAME_LEN + 1];

   if (NULL == (fp = fopen (file, "r")))
     return NULL;

   host = NULL;
   if (NULL != fgets (line, sizeof (line), fp))
     {
	slrn_trim_string (line);
	if (slrn_is_fqdn (line))
	  host = line;
     }

   fclose(fp);
   return host;
}
#endif				       /* OUR_HOSTNAME */

void slrn_get_user_info (void) /*{{{*/
{
   char *name, *host, *host1, *org;
#ifdef OUR_ORGANIZATION
   char *our_org = OUR_ORGANIZATION;
#endif
#ifdef HAS_PASSWORD_CODE
   struct passwd *pw;
#endif

   /* Fill in what is assumed to be non-NULL by rest of program. */

   /* no-c-format tells gettext that the following strings do not
    * need to be checked as if they were passed to printf. */

   Slrn_User_Info.followup_string =   /* xgettext:no-c-format */
     slrn_safe_strmalloc (_("On %D, %r <%f> wrote:"));
   Slrn_User_Info.reply_string =      /* xgettext:no-c-format */
     slrn_safe_strmalloc (_("In %n, you wrote:"));
   Slrn_User_Info.followupto_string = /* xgettext:no-c-format */
     slrn_safe_strmalloc (_("[\"Followup-To:\" header set to %n.]"));

   Slrn_CC_Post_Message =         /* xgettext:no-c-format */
     slrn_safe_strmalloc (_("[This message has also been posted to %n.]"));

   /* Now get default values for rest. */
   host = get_hostname ();
   if (host != NULL)
     {
	if (slrn_is_fqdn (host))
	  Slrn_User_Info.posting_host = slrn_safe_strmalloc (host);
     }

#if ! SLRN_HAS_STRICT_FROM
   /* Allow user chance to specify another hostname.  However, it will not
    * affect the posting host.
    */
   if ((NULL != (host1 = getenv ("HOSTNAME")))
       && (0 == slrn_is_fqdn (host1)))
#endif
     host1 = NULL;

   /* If a value was compiled in, use it. */
#ifdef OUR_HOSTNAME
   if (host1 == NULL)
     {
	host1 = OUR_HOSTNAME;
	/* If the value is a file, read the file for the FQDN */
	if (slrn_is_absolute_path (host1))
	  host1 = get_host_from_filename (OUR_HOSTNAME);
     }
#endif

   if ((host1 != NULL)
       && slrn_is_fqdn (host1))
     {
	slrn_free (host);
	host = slrn_safe_strmalloc (host1);
     }

   /* Finally!! */
   Slrn_User_Info.hostname = host;

#ifdef VMS
   name = slrn_vms_getlogin();
#else
   name = NULL;
# ifdef HAS_PASSWORD_CODE
   /* I cannot use getlogin under Unix because some implementations
    * truncate the username to 8 characters.  Besides, I suspect that
    * it is equivalent to the following line.
    *
    * Also it is not clear if the valued returned by getpwuid is malloced.
    * The man page indicates that it _may_ point to a static area.  Valgrind
    * reports a leak.  Sigh.
    */
   pw = getpwuid (getuid ());
   if (pw != NULL)
     name = pw->pw_name;
# endif
#endif

   if (((name == NULL) || (*name == 0))
#if ! SLRN_HAS_STRICT_FROM
       && ((name = getenv("USER")) == NULL)
       && ((name = getenv("LOGNAME")) == NULL)
#endif
       )
     name = "";

   Slrn_User_Info.username = slrn_safe_strmalloc (name);
   Slrn_User_Info.login_name = slrn_safe_strmalloc (name);

   if ((Slrn_User_Info.replyto = getenv ("REPLYTO")) == NULL)
     Slrn_User_Info.replyto = "";
   Slrn_User_Info.replyto = slrn_safe_strmalloc (Slrn_User_Info.replyto);

#ifdef VMS
   Slrn_User_Info.realname = slrn_vms_fix_fullname(slrn_vms_get_uaf_fullname());
#else
# if ! SLRN_HAS_STRICT_FROM
   Slrn_User_Info.realname = getenv ("NAME");
# else
   Slrn_User_Info.realname = NULL;
# endif
   if ((Slrn_User_Info.realname == NULL)
# ifdef HAS_PASSWORD_CODE
       && ((pw == NULL) || ((Slrn_User_Info.realname = pw->pw_gecos) == NULL))
# endif
       )
     {
	Slrn_User_Info.realname = "";
     }
#endif

   Slrn_User_Info.realname = slrn_safe_strmalloc (Slrn_User_Info.realname);

   /* truncate at character used to delineate extra gecos fields */
   name = Slrn_User_Info.realname;
   while (*name && (*name != ',')) name++;
   *name = 0;

   org = getenv ("ORGANIZATION");
#ifdef OUR_ORGANIZATION
   if (org == NULL) org = our_org;
#endif
   if (org != NULL)
     {
	/* Check to see if this is an organization file. */
	char orgbuf[512];
	if (slrn_is_absolute_path (org))
	  {
	     FILE *fporg;
	     if (NULL != (fporg = fopen (org, "r")))
	       {
		  if (NULL != fgets (orgbuf, sizeof (orgbuf) - 1, fporg))
		    {
		       unsigned int orglen = strlen (orgbuf);
		       if (orglen && (orgbuf[orglen - 1] == '\n'))
			 orgbuf[orglen - 1] = 0;
		       org = orgbuf;
		    }
		  slrn_fclose (fporg);
	       }
#ifdef OUR_ORGANIZATION
	     else if (our_org == org)
	       org = NULL;
#endif
	  }
	if (org != NULL)
	  Slrn_User_Info.org = slrn_safe_strmalloc (org);
     }

   Slrn_User_Info.signature = slrn_safe_strmalloc (SLRN_SIGNATURE_FILE);

#if SLRN_HAS_CANLOCK
   Slrn_User_Info.cancelsecret = slrn_safe_strmalloc ("");
#endif
#ifdef SLRN_SENDMAIL_COMMAND
   Slrn_SendMail_Command = slrn_safe_strmalloc (SLRN_SENDMAIL_COMMAND);
#endif
}

/*}}}*/

#define IS_ATEXT(x) (((x) > 0x20) && ((x) < 0x7f) && \
			(NULL == slrn_strbyte ("\"(),.:;<>@[\\]", (x))))
static int is_dot_atom (char *str)
{
   char ch = *str++;
   if (!IS_ATEXT (ch)) return 0;
   while ((ch = *str++))
     {
	if (ch == '.') ch = *str++;
	if (!IS_ATEXT (ch)) return 0;
     }
   return 1;
}

static char *make_escaped_string (char *src, int is_comment)
{
   unsigned int len, dlen;
   char ch, *p, *pmax;
   char *dest;

   if ((src == NULL) || (*src == 0)) return NULL;

   len = strlen (src);
   p = src;
   pmax = p + len;
   dlen = 0;

   while (p < pmax)
     {
	switch (*p++)
	  {
	   case '\\':
	     dlen++;
	     break;

	   case '"':
	     if (is_comment == 0)
	       dlen++;
	     break;

	   case '(': case ')':
	     if (is_comment)
	       dlen++;
	     break;
	  }
     }

   dest = slrn_malloc (len + dlen + 3, 0, 1);
   if (dest == NULL)
     return NULL;

   p = dest;
   if (is_comment)
     *p++ = '(';
   else
     *p++ = '"';

   while (1)
     {
	ch = *src++;
	switch (ch)
	  {
	   case 0:
	     if (is_comment)
	       *p++ = ')';
	     else
	       *p++ = '"';
	     *p = 0;
	     return dest;

	   case '\\':
	     *p++ = '\\';
	     break;

	   case '"':
	     if (is_comment == 0)
	       *p++ = '\\';
	     break;

	   case '(': case ')':
	     if (is_comment)
	       *p++ = '\\';
	     break;
	  }

	*p++ = ch;
     }
}

/* returns NULL upon failure */
static char *make_localpart (char *username)
{
   if ((username == NULL) || (*username == 0) )
     return NULL;
   if (is_dot_atom (username))
     {
	return slrn_safe_strmalloc(username);
     }
   return make_escaped_string (username, 0);
}

static char *make_realname (char *realname)
{
   char *p, *pmax;
   int flags;
#define REALNAME_HAS_8BIT	0x1
#define REALNAME_HAS_CTRL	0x2
#define REALNAME_HAS_SPEC	0x4

   if ((realname == NULL) || (*realname == 0) )
     return NULL;
   /*
    * If realname consists only of atom-text, or 8-bit, then it may
    * be encoded as an atom.  If it contains specials, it must be encoded
    * as a quoted-string, or a comment.  It illegal for an 8 bit character
    * to be in a quoted string since the text of a quoted string cannot
    * be mime-encoded.
    */
   p = realname;
   pmax = p + strlen (realname);
   flags = 0;
   while (p < pmax)
     {
	unsigned char ch = (unsigned char) *p++;
	if (ch & 0x80)
	  {
	     flags |= REALNAME_HAS_8BIT;
	     continue;
	  }
	switch (ch)
	  {
	   case '\r':
	   case '\n':
	   case 127:
	     return NULL;

	   case '(': case ')': case '<': case '>': case '[':
	   case ']': case ':': case '@': case '\\': case ',':
	   case '.':
	   case '"':		       /* not a special, but treated like one here */
	     flags |= REALNAME_HAS_SPEC;
	     break;

	   case ' ': case '\t':
	     break;

	   default:
	     if (ch < 32)
	       {
		  flags |= REALNAME_HAS_CTRL;
		  break;
	       }
	     break;
	  }
     }

   if ((flags == 0) || (flags == REALNAME_HAS_8BIT))
     return slrn_strmalloc (realname, 1);

   if (flags & REALNAME_HAS_8BIT)
     return make_escaped_string (realname, 1);

   return make_escaped_string (realname, 0);
}

/* This function returns a malloced string of the form "From: value" */
char *slrn_make_from_header (void)
{
   static char *buf;
   char *localpart, *realname, *msg;

#if ! SLRN_HAS_STRICT_FROM
   if ((1 == slrn_run_hooks (HOOK_MAKE_FROM_STRING, 0))
       && (0 == SLang_pop_slstring (&msg)))
     {
	if (*msg != 0)
	  {
	     char *prefix = "From: ";
	     if (0 == strncmp (msg, "From: ", 6))
	       prefix = "";
	     buf = slrn_strjoin (prefix, msg, "");
	     SLang_free_slstring (msg);
	     return buf;
	  }
	SLang_free_slstring (msg);
	/* Drop through to default */
     }
#endif
   msg = NULL;

   if (( localpart = make_localpart (Slrn_User_Info.username)) == NULL)
     {
	slrn_error (_("Cannot generate \"From:\" line without a valid username."));
	return NULL;
     }
   if ((NULL == Slrn_User_Info.hostname) ||
	    (0 == *Slrn_User_Info.hostname))
     /* Note: we currently do not check whether hostname is valid */
     {
	slrn_error (_("Cannot generate \"From:\" line without a hostname."));
	return NULL;
     }

   if (( realname = make_realname (Slrn_User_Info.realname))  != NULL)
     {
	buf=slrn_safe_malloc(6 + strlen(realname) +2 + strlen(localpart) + 1
		  + strlen(Slrn_User_Info.hostname)+2);
	sprintf (buf, "From: %s <%s@%s>", realname, localpart, /* safe */
		 Slrn_User_Info.hostname);
	slrn_free(realname);
     }
   else
     {
	buf=slrn_safe_malloc(6 + strlen(localpart) + 1
		  + strlen(Slrn_User_Info.hostname)+2);
	sprintf (buf, "From: %s@%s", localpart, Slrn_User_Info.hostname); /* safe */
     }

   slrn_free(localpart);
   return buf;
}

