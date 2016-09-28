/* -*- mode: C; mode: fold -*- */
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
/*{{{ Include files */

#include <stdio.h>
#include <signal.h>
#include <string.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#ifdef VMS
# include "vms.h"
#else
# if !defined(sun) && !defined(IBMPC_SYSTEM)
#  include <sys/ioctl.h>
# endif
# ifdef HAVE_TERMIOS_H
#  include <termios.h>
# endif
# ifdef SYSV
#  include <sys/termio.h>
#  include <sys/stream.h>
#  include <sys/ptem.h>
#  include <sys/tty.h>
# endif
# ifndef __os2__
#  include <sys/types.h>
#  include <sys/stat.h>
# endif
# ifdef __MINGW32__
#  include <process.h>
# endif
#endif /* !VMS */

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include <errno.h>

#include "common.h"
#include "util.h"
#include "server.h"
#include "slrn.h"
#include "group.h"
#include "misc.h"
#include "startup.h"
#include "art.h"
#include "score.h"
#include "snprintf.h"
#include "charset.h"
#include "post.h"
#include "xover.h"
#include "mime.h"
#include "hooks.h"
#include "strutil.h"

#if SLRN_USE_SLTCP
# include "sltcp.h"
#endif

#if SLRN_HAS_GROUPLENS
# include "grplens.h"
#endif
#include "interp.h"
#ifdef __os2__
# define INCL_VIO
# include <os2.h>
#endif

#if defined(__NT__) || defined(__WIN32__)
# include <windows.h>
#endif

/*}}}*/

/*{{{ Global Variables */

#if SLANG_VERSION >= 20000
int Slrn_UTF8_Mode = 0;
#endif

int Slrn_TT_Initialized = 0;

/* If -1, force mouse.  If 1 the mouse will be used on in XTerm.  If 0,
 * do not use it.
 */
int Slrn_Use_Mouse;

/* Find out whether there was a warning at startup. Used for option -w0 */
int Slrn_Saw_Warning = 0;

/* We do not (yet) offer a batch mode; however, I consider it an interesting
 * idea and might implement it after version 1.0.*/
int Slrn_Batch;
int Slrn_Suspension_Ok;
int Slrn_Simulate_Graphic_Chars = 0;

char *Slrn_Newsrc_File = NULL;
Slrn_Mode_Type *Slrn_Current_Mode;

int Slrn_Default_Server_Obj = SLRN_DEFAULT_SERVER_OBJ;
int Slrn_Default_Post_Obj = SLRN_DEFAULT_POST_OBJ;

FILE *Slrn_Debug_Fp = NULL;

/* You need to call slrn_init_graphic_chars before using these */
SLwchar_Type Graphic_LTee_Char;
SLwchar_Type Graphic_UTee_Char;
SLwchar_Type Graphic_LLCorn_Char;
SLwchar_Type Graphic_HLine_Char;
SLwchar_Type Graphic_VLine_Char;
SLwchar_Type Graphic_ULCorn_Char;

int Graphic_Chars_Mode = ALT_CHAR_SET_MODE;

/*}}}*/
/*{{{ Static Variables */

static int Can_Suspend;
static volatile int Want_Suspension;
static volatile int Want_Window_Size_Change;
static void perform_suspend (int);
static int Current_Mouse_Mode;
static int Ran_Startup_Hook;

/*}}}*/
/*{{{ Static Function Declarations */

#if defined(SIGSTOP) && defined(REAL_UNIX_SYSTEM)
static int suspend_display_mode (int);
static int resume_display_mode (int, int, int);
#endif
static void init_suspend_signals (int);
static void slrn_hangup (int);
static void run_winch_functions (int, int);
/*}}}*/

/*{{{ Newsrc Locking Routines */

static void test_lock( char *file ) /*{{{*/
{
   int pid;
   FILE *fp;

   if ((fp = fopen (file, "r")) != NULL)
     {
	if (1 == fscanf (fp, "%d", &pid) )
	  {
	     if ((pid > 0)
#if !defined(IBMPC_SYSTEM)
		 && (0 == kill (pid, 0))
#endif
		 )
	       {
#if defined(IBMPC_SYSTEM)
		  slrn_exit_error (_("\
slrn: pid %d is locking the newsrc file. If you're not running another\n\
      copy of slrn, delete the file %s"),
				   pid, file);
#else
		  slrn_exit_error (_("slrn: pid %d is locking the newsrc file."), pid);
#endif
	       }
	  }
	slrn_fclose (fp);
     }
}

/*}}}*/

static int make_lock( char *file ) /*{{{*/
{
   int pid;
   FILE *fp;

#ifdef VMS
   fp = fopen (file, "w", "fop=cif");
#else
   fp = fopen (file, "w");
#endif

   if (fp == NULL) return -1;

   pid = getpid ();
   if (EOF == fprintf (fp, "%d", pid))
     {
	slrn_fclose (fp);
	return -1;
     }
   return slrn_fclose (fp);
}

/*}}}*/

static void lock_file (int how) /*{{{*/
{
   char name[SLRN_MAX_PATH_LEN];
   static int not_ok_to_unlock;
#if SLRN_HAS_RNLOCK
   int rnlock = 0;
   char file_rn[SLRN_MAX_PATH_LEN];
#endif

   if (Slrn_Newsrc_File == NULL) return;
   if (not_ok_to_unlock) return;

   not_ok_to_unlock = 1;

#ifdef SLRN_USE_OS2_FAT
   slrn_os2_make_fat (name, sizeof (name), Slrn_Newsrc_File, ".lck");
#else
   slrn_snprintf (name, sizeof (name), "%s-lock", Slrn_Newsrc_File);
#endif

#if SLRN_HAS_RNLOCK
   slrn_make_home_filename (".newsrc", file_rn, sizeof (file_rn));
   if (0 == strcmp (file_rn, Slrn_Newsrc_File))
     {
	rnlock = 1;
	slrn_make_home_filename (".rnlock", file_rn, sizeof (file_rn));
     }
#endif

   if (how == 1)
     {
	test_lock (name);
#if SLRN_HAS_RNLOCK
	if (rnlock) test_lock (file_rn);
#endif
	if (-1 == make_lock (name))
	  {
	     slrn_exit_error (_("Unable to create lock file %s."), name);
	  }

#if SLRN_HAS_RNLOCK
	if (rnlock && (-1 == make_lock (file_rn)))
	  {
	     slrn_delete_file (name); /* delete the "normal" lock file */
	     slrn_exit_error (_("Unable to create lock file %s."), file_rn);
	  }
#endif
     }
   else
     {
	if (-1 == slrn_delete_file (name))
	  {
	     /* slrn_exit_error ("Unable to remove lockfile %s.", file); */
	  }
#if SLRN_HAS_RNLOCK
        if (rnlock && -1 == slrn_delete_file (file_rn))
	  {
	     /* slrn_exit_error ("Unable to remove lockfile %s.", file_rn); */
	  }
#endif
     }
   not_ok_to_unlock = 0;
}

/*}}}*/

/*}}}*/

/*{{{ Signal Related Functions */

/*{{{ Low-Level signal-related utility functions */

static void init_like_signals (int argc, int *argv, /*{{{*/
			       void (*f0)(int),
			       void (*f1)(int),
			       int state)
{
#ifdef HAVE_SIGACTION
   struct sigaction sa;
#endif
   int i;

   if (state == 0)
     {
	for (i = 0; i < argc; i++)
	  SLsignal_intr (argv[i], f0);
	return;
     }

   for (i = 0; i < argc; i++)
     {
	int sig = argv[i];
	SLsignal_intr (sig, f1);

#if defined(SLRN_POSIX_SIGNALS)
	if (-1 != sigaction (sig, NULL, &sa))
	  {
	     int j;
	     for (j = 0; j < argc; j++)
	       {
		  if (j != i) sigaddset (&sa.sa_mask, argv[j]);
	       }

	     (void) sigaction (sig, &sa, NULL);
	  }
#endif
     }
}

/*}}}*/

/*}}}*/

/*{{{ Suspension signals */

#ifdef REAL_UNIX_SYSTEM
#define SUSPEND_STACK_SIZE 512
static char Suspend_Stack [SUSPEND_STACK_SIZE];
static unsigned int Suspension_Stack_Depth = 0;
static int Ok_To_Suspend = 0;
#endif

static int Suspend_Sigtstp_Suspension = 0;

void slrn_push_suspension (int ok) /*{{{*/
{
#ifdef REAL_UNIX_SYSTEM
   if (Suspension_Stack_Depth < SUSPEND_STACK_SIZE)
     {
	Suspend_Stack [Suspension_Stack_Depth] = Ok_To_Suspend;
     }
   else ok = 0;

   Suspension_Stack_Depth++;

   (void) slrn_handle_interrupts ();

   Ok_To_Suspend = ok;
#else
   (void) ok;
#endif
}
/*}}}*/
void slrn_pop_suspension (void) /*{{{*/
{
#ifdef REAL_UNIX_SYSTEM

   if (Suspension_Stack_Depth == 0)
     {
	slrn_error (_("pop_suspension: underflow!"));
	return;
     }

   Suspension_Stack_Depth--;

   if (Suspension_Stack_Depth < SUSPEND_STACK_SIZE)
     {
	Ok_To_Suspend = Suspend_Stack [Suspension_Stack_Depth];
     }
   else Ok_To_Suspend = 0;

   (void) slrn_handle_interrupts ();
#endif
}
/*}}}*/

/* This function is called by the SIGTSTP handler.  Since it operates
 * in an asynchronous fashion, care must be exercised to control when that
 * can happen.  This is accomplished via the push/pop_suspension functions.
 */
static void sig_suspend (int sig)
{
#ifdef REAL_UNIX_SYSTEM
   sig = errno;

   if (Ok_To_Suspend
       && (0 == Suspend_Sigtstp_Suspension))
     {
	perform_suspend (1);
     }
   else Want_Suspension = 1;

   init_suspend_signals (1);
   errno = sig;
#else
   (void) sig;
#endif
}

static void init_suspend_signals (int state) /*{{{*/
{
   int argv[2];
   int argc = 0;

   if (Can_Suspend == 0)
     return;

#ifdef SIGTSTP
   argv[argc++] = SIGTSTP;
#endif

#ifdef SIGTTIN
   argv[argc++] = SIGTTIN;
#endif

   init_like_signals (argc, argv, SIG_DFL, sig_suspend, state);
}

/*}}}*/

static void perform_suspend (int smg_suspend_flag) /*{{{*/
{
#if !defined(SIGSTOP) || !defined(REAL_UNIX_SYSTEM)
   (void) smg_suspend_flag;
   slrn_error (_("Not implemented."));
   Want_Suspension = 0;
#else

   int init;
   int mouse_mode = Current_Mouse_Mode;

# ifdef SLRN_POSIX_SIGNALS
   sigset_t mask;

   Want_Suspension = 0;
   if (Can_Suspend == 0)
     {
	slrn_error (_("Suspension not allowed by shell."));
	return;
     }

   sigemptyset (&mask);
   sigaddset (&mask, SIGTSTP);

   /* This function resets SIGTSTP to default */
   init = suspend_display_mode (smg_suspend_flag);

   kill (getpid (), SIGTSTP);

   /* If SIGTSTP is pending, it will be delivered now.  That's ok. */
   sigprocmask (SIG_UNBLOCK, &mask, NULL);
# else

   Want_Suspension = 0;
   if (Can_Suspend == 0)
     {
	slrn_error (_("Suspension not allowed by shell."));
	return;
     }

   init = suspend_display_mode (smg_suspend_flag);
   kill(getpid(),SIGSTOP);
# endif

   resume_display_mode (smg_suspend_flag, init, mouse_mode);
#endif				       /* !defined(SIGSTOP) || !defined(REAL_UNIX_SYSTEM) */
}

/*}}}*/

void slrn_suspend_cmd (void)
{
   perform_suspend (0);
}

static void check_for_suspension (void)
{
#ifdef SIGTSTP
   void (*f)(int);

   f = SLsignal (SIGTSTP, SIG_DFL);
   (void) SLsignal (SIGTSTP, f);

#if 0
   Can_Suspend = (f == SIG_DFL);
#else
   Can_Suspend = (f != SIG_IGN);
#endif

#else
   Can_Suspend = 0;
#endif
}

/*}}}*/

/*{{{ Hangup Signals */

void slrn_init_hangup_signals (int state) /*{{{*/
{
   int argv[3];
   int argc = 0;

#ifdef SIGHUP
   argv[argc++] = SIGHUP;
#endif
#ifdef SIGTERM
   argv[argc++] = SIGTERM;
#endif
#ifdef SIGQUIT
   argv[argc++] = SIGQUIT;
#endif

   init_like_signals (argc, argv, SIG_IGN, slrn_hangup, state);
}

/*}}}*/

/*}}}*/

#ifdef SIGWINCH
static void sig_winch_handler (int sig)
{
   if (Slrn_Batch) return;
   sig = errno;
   Want_Window_Size_Change = 1;
   SLsignal_intr (SIGWINCH, sig_winch_handler);
   errno = sig;
}
#endif

static void slrn_set_screen_size (int sig) /*{{{*/
{
   int old_r, old_c;

   old_r = SLtt_Screen_Rows;
   old_c = SLtt_Screen_Cols;

   SLtt_get_screen_size ();

   Slrn_Full_Screen_Update = 1;
   Want_Window_Size_Change = 0;

   run_winch_functions (old_r, old_c);

   if (sig)
     {
#if SLANG_VERSION < 10306
	SLsmg_reset_smg ();
	SLsmg_init_smg ();
#else
	SLsmg_reinit_smg ();
#endif
	slrn_redraw ();
     }
}

/*}}}*/

static void init_display_signals (int mode) /*{{{*/
{
   init_suspend_signals (mode);

   if (mode)
     {
	SLang_set_abort_signal (NULL);
#ifdef SIGPIPE
	SLsignal (SIGPIPE, SIG_IGN);
#endif
#ifdef SIGTTOU
	/* Allow background writes */
	SLsignal (SIGTTOU, SIG_IGN);
#endif
#ifdef SIGWINCH
	SLsignal_intr (SIGWINCH, sig_winch_handler);
#endif
     }
   else
     {
#ifdef SIGWINCH
	/* SLsignal_intr (SIGWINCH, SIG_DFL); */
#endif
     }
}

/*}}}*/

int slrn_handle_interrupts (void)
{
   if (Want_Suspension)
     {
	slrn_suspend_cmd ();
     }

   if (Want_Window_Size_Change)
     {
	slrn_set_screen_size (1);
     }

   return 0;
}

/*}}}*/

/*{{{ Screen Management and Terminal Init/Reset Functions */

int Slrn_Use_Flow_Control = 0;
static int init_tty (void) /*{{{*/
{
   if (Slrn_TT_Initialized & SLRN_TTY_INIT)
     {
	return 0;
     }

   if (Slrn_TT_Initialized == 0)
     init_display_signals (1);

   if (-1 == SLang_init_tty (7, !Slrn_Use_Flow_Control, 0))
     slrn_exit_error (_("Unable to initialize the tty"));

#ifdef REAL_UNIX_SYSTEM
   SLang_getkey_intr_hook = slrn_handle_interrupts;
#endif

#ifdef REAL_UNIX_SYSTEM
   if (Can_Suspend)
     SLtty_set_suspend_state (1);
#endif

   Slrn_TT_Initialized |= SLRN_TTY_INIT;

   return 0;
}

/*}}}*/

static int reset_tty (void) /*{{{*/
{
   if (0 == (Slrn_TT_Initialized & SLRN_TTY_INIT))
     {
	return 0;
     }

   SLang_reset_tty ();
   Slrn_TT_Initialized &= ~SLRN_TTY_INIT;

   if (Slrn_TT_Initialized == 0)
     init_display_signals (0);

   return 0;
}

/*}}}*/

static int init_smg (int use_resume, int mouse_mode) /*{{{*/
{
   if (Slrn_TT_Initialized & SLRN_SMG_INIT)
     return 0;

   slrn_enable_mouse (mouse_mode);

   if (Slrn_TT_Initialized == 0)
     init_display_signals (1);

   if (use_resume)
     {
	SLsmg_resume_smg ();
	Slrn_TT_Initialized |= SLRN_SMG_INIT;
     }
   else
     {
	slrn_set_screen_size (0);
	SLsmg_init_smg ();
	Slrn_TT_Initialized |= SLRN_SMG_INIT;

	/* We do not want the -> overlay cursor to affect the scroll. */
#if !defined(IBMPC_SYSTEM)
	SLsmg_Scroll_Hash_Border = 5;
#endif
	slrn_redraw ();
     }

   return 0;
}

/*}}}*/

static int reset_smg (int smg_suspend_flag) /*{{{*/
{
   if (0 == (Slrn_TT_Initialized & SLRN_SMG_INIT))
     return 0;

   slrn_enable_mouse (0);

   if (smg_suspend_flag)
     SLsmg_suspend_smg ();
   else
     {
	SLsmg_gotorc (SLtt_Screen_Rows - 1, 0);
	slrn_smg_refresh ();
	SLsmg_reset_smg ();
     }

   /* SLsignal_intr (SIGWINCH, SIG_DFL); */

   Slrn_TT_Initialized &= ~SLRN_SMG_INIT;

   if (Slrn_TT_Initialized == 0)
     init_display_signals (0);

   return 0;
}

/*}}}*/

#if defined(SIGSTOP) && defined(REAL_UNIX_SYSTEM)
static int suspend_display_mode (int smg_suspend_flag) /*{{{*/
{
   int mode = Slrn_TT_Initialized;

   SLsig_block_signals ();

   reset_smg (smg_suspend_flag);
   reset_tty ();

   SLsig_unblock_signals ();

   return mode;
}

/*}}}*/

static int resume_display_mode (int smg_suspend_flag, int mode, int mouse_mode) /*{{{*/
{
   SLsig_block_signals ();

   if (mode & SLRN_TTY_INIT)
     init_tty ();

   if (mode & SLRN_SMG_INIT)
     init_smg (smg_suspend_flag, mouse_mode);

   SLsig_unblock_signals ();
   return 0;
}

/*}}}*/
#endif /* SIGSTOP && REAL_UNIX_SYSTEM*/

void slrn_set_display_state (int state) /*{{{*/
{
   if (Slrn_Batch) return;

   SLsig_block_signals ();

   if (state & SLRN_TTY_INIT)
     init_tty ();
   else
     reset_tty ();

   if (state & SLRN_SMG_INIT)
     init_smg (0, 1);
   else
     reset_smg (0);

   SLsig_unblock_signals ();
}

/*}}}*/

void slrn_enable_mouse (int mode) /*{{{*/
{
   if (Current_Mouse_Mode == mode)
     return;

   if (Slrn_Use_Mouse)
     {
	if (-1 == SLtt_set_mouse_mode (mode, (Slrn_Use_Mouse < 0)))
	  Slrn_Use_Mouse = 0;
     }
   Current_Mouse_Mode = mode;
}

/*}}}*/

void slrn_init_graphic_chars (void) /*{{{*/
{
#ifndef IBMPC_SYSTEM
   if (SLtt_Has_Alt_Charset == 0)
     Slrn_Simulate_Graphic_Chars = 1;
#endif

   if (Slrn_Simulate_Graphic_Chars)
     {
	Graphic_LTee_Char = '+';
	Graphic_UTee_Char = '+';
	Graphic_LLCorn_Char = '`';
	Graphic_HLine_Char = '-';
	Graphic_VLine_Char = '|';
	Graphic_ULCorn_Char = '/';

	Graphic_Chars_Mode = SIMULATED_CHAR_SET_MODE;
     }
   else
     {
	Graphic_Chars_Mode = ALT_CHAR_SET_MODE;
	Graphic_LTee_Char = SLSMG_LTEE_CHAR;
	Graphic_UTee_Char = SLSMG_UTEE_CHAR;
	Graphic_LLCorn_Char = SLSMG_LLCORN_CHAR;
	Graphic_HLine_Char = SLSMG_HLINE_CHAR;
	Graphic_VLine_Char = SLSMG_VLINE_CHAR;
	Graphic_ULCorn_Char = SLSMG_ULCORN_CHAR;

	Graphic_Chars_Mode = ALT_CHAR_SET_MODE;
     }
}
/*}}}*/
/*}}}*/

static void perform_cleanup (void)
{
   slrn_set_display_state (0);

#if SLRN_HAS_GROUPLENS
   slrn_close_grouplens ();
#endif

   if (Slrn_Server_Obj != NULL)
     Slrn_Server_Obj->sv_close ();

   if (Slrn_Debug_Fp != NULL)
     {
	(void) fclose (Slrn_Debug_Fp);
	Slrn_Debug_Fp = NULL;
     }

#if SLRN_HAS_GROUPLENS
   slrn_close_grouplens ();
#endif

   if (Ran_Startup_Hook)
     (void) slrn_run_hooks (HOOK_QUIT, 0);
   (void) slrn_reset_slang ();

#if SLRN_USE_SLTCP && SLRN_HAS_NNTP_SUPPORT
   (void) sltcp_close_sltcp ();
#endif
   lock_file (0);
}

void slrn_quit (int retcode) /*{{{*/
{
   perform_cleanup ();
   if (retcode) fprintf (stderr, _("slrn: quiting on signal %d.\n"), retcode);
   exit (retcode);
}

/*}}}*/

void slrn_va_exit_error (char *fmt, va_list ap)
{
   static int trying_to_exit;

   if (trying_to_exit == 0)
     {
	trying_to_exit = 1;
	slrn_set_display_state (0);

	if (fmt != NULL)
	  {
	     fputs (_("slrn fatal error:"), stderr);
#ifdef IBMPC_SYSTEM
	     fputc ('\r', stderr);
#endif
	     fputc ('\n', stderr);
	     vfprintf (stderr, fmt, ap);
	  }
	if (Slrn_Groups_Dirty) slrn_write_newsrc (0);
	perform_cleanup ();
     }

   putc ('\n', stderr);
   exit (1);
}

void slrn_exit_error (char *fmt, ...) /*{{{*/
{
   va_list ap;
   va_start (ap, fmt);
   slrn_va_exit_error (fmt, ap);
   va_end(ap);
}

/*}}}*/

void slrn_error (char *fmt, ...) /*{{{*/
{
   va_list ap;

   va_start(ap, fmt);
   slrn_verror (fmt, ap);
   va_end (ap);
}

/*}}}*/

static void usage (char *extra, int exit_status) /*{{{*/
{
   printf (_("\
Usage: slrn [--inews] [--nntp ...] [--spool] OPTIONS\n\
-a              Use active file for getting new news.\n\
-f newsrc-file  Name of the newsrc file to use.\n\
-C[-]           [Do not] use colors.\n\
-Dname          Add 'name' to list of predefined preprocessing tokens.\n\
-d              Get new text descriptions of each group from server.\n\
                 Note: This may take a LONG time to retrieve this information.\n\
                 The resulting file can be several hundred Kilobytes!\n\
-i init-file    Name of initialization file to use (default: %s)\n\
-k              Do not process score file.\n\
-k0             Process score file but inhibit expensive scores.\n\
-m              Force XTerm mouse reporting\n\
-n              Do not check for new groups.  This usually results in\n\
                 a faster startup.\n\
-w              Wait for key before switching to full screen mode\n\
-w0             Wait for key (only when warnings / errors occur)\n\
--create        Create a newsrc file by getting list of groups from server.\n\
--debug FILE    Write debugging information to FILE\n\
--help          Print this usage.\n\
--kill-log FILE Keep a log of all killed articles in FILE\n\
--show-config   Print configuration\n\
--version       Show version and supported features\n\
\n\
NNTP mode has additional options; use \"slrn --nntp --help\" to display them.\n\
"), SLRN_USER_SLRNRC_FILENAME);

   if (extra != NULL)
     {
	printf ("\n%s\n", extra);
     }
   exit (exit_status);
}

/*}}}*/

static int parse_object_args (char *obj, char **argv, int argc) /*{{{*/
{
   int num_parsed;
   int zero_ok = 1;

   if (obj == NULL)
     {
	zero_ok = 0;
	obj = slrn_map_object_id_to_name (0, SLRN_DEFAULT_SERVER_OBJ);
     }

   num_parsed = slrn_parse_object_args (obj, argv, argc);
   if (num_parsed < 0)
     {
	if (num_parsed == -1)
	  slrn_exit_error (_("%s is not a supported option."), *argv);
	else
	  slrn_exit_error (_("%s is not supported."), obj);
     }
   if ((num_parsed == 0) && (zero_ok == 0))
     usage (NULL, 1);

   return num_parsed;
}

/*}}}*/

static void read_score_file (void)
{
   char file[SLRN_MAX_PATH_LEN];

   if (Slrn_Score_File == NULL)
     return;

   slrn_make_home_filename (Slrn_Score_File, file, sizeof (file));

   if (1 != slrn_file_exists (file))
     {
	slrn_message_now (_("*** Warning: Score file %s does not exist"), file);
	Slrn_Saw_Warning = 1;
	return;
     }

   if (-1 == slrn_read_score_file (file))
     {
	slrn_exit_error (_("Error processing score file %s."), file);
     }
}

static int main_init_and_parse_args (int argc, char **argv) /*{{{*/
{
   char *hlp_file;
   unsigned int i;
   int create_flag = 0;
   int no_new_groups = 0;
   int no_score_file = 0;
   int use_color = 0;
   int use_mouse = 0;
   int dsc_flag = 0;
   int use_active = 0;
   int wait_for_key = 0;
   FILE *fp;
   char file [SLRN_MAX_PATH_LEN];
   char *init_file = NULL;
   char *kill_logfile = NULL;
   int print_config = 0;

#if defined(HAVE_SETLOCALE) && defined(LC_ALL)
   (void) setlocale(LC_ALL, "");
#endif
#ifdef ENABLE_NLS
   bindtextdomain(NLS_PACKAGE_NAME, NLS_LOCALEDIR);
   textdomain (NLS_PACKAGE_NAME);
#endif

   check_for_suspension ();
#ifdef __unix__
   (void) umask (077);
#endif

#if 0
   if (NULL != getenv ("AUTOSUBSCRIBE"))
     Slrn_Unsubscribe_New_Groups = 0;
   if (NULL != getenv ("AUTOUNSUBSCRIBE"))
     Slrn_Unsubscribe_New_Groups = 1;
#endif

   for (i = 1; i < (unsigned int) argc; i++)
     {
	char *argv_i = argv[i];

	if ((argv_i[0] == '-') && (argv_i[1] == '-'))
	  {
	     int status;

	     argv_i += 2;

	     status = slrn_parse_object_args (argv_i, NULL, 0);
	     if (status != -1)
	       i += parse_object_args (argv_i, argv + (i + 1), argc - (i + 1));
#if 0
	     else if (!strcmp ("batch", argv_i)) Slrn_Batch = 1;
#endif
	     else if (!strcmp ("create", argv_i)) create_flag = 1;
	     else if (!strcmp ("version", argv_i))
	       {
		  slrn_show_version (stdout);
		  exit (0);
	       }
	     else if (!strcmp ("show-config", argv_i))
	       print_config = 1;
	     else if ((!strcmp ("kill-log", argv_i)) &&
		      (i + 1 < (unsigned int) argc))
	       kill_logfile = argv[++i];
             else if ((!strcmp ("debug", argv_i)) &&
                      (i + 1 < (unsigned int) argc))
               {
		  if (Slrn_Debug_Fp != NULL)
		    fclose (Slrn_Debug_Fp);
                  Slrn_Debug_Fp = fopen (argv[++i], "w");
                  if (Slrn_Debug_Fp == NULL)
                    slrn_exit_error (_("Unable to open %s for debugging."), argv[i]);
                  setbuf (Slrn_Debug_Fp, (char *) NULL);
               }
	     else if (!strcmp ("help", argv_i))
	       usage (NULL, 0);
	     else usage (NULL, 1);
	  }
	else if (!strcmp ("-create", argv_i)) create_flag = 1;
	else if (!strcmp ("-C", argv_i)) use_color = 1;
	else if (!strcmp ("-C-", argv_i)) use_color = -1;
	else if (!strcmp ("-a", argv_i)) use_active = 1;
	else if (!strcmp ("-n", argv_i)) no_new_groups = 1;
	else if (!strcmp ("-d", argv_i)) dsc_flag = 1;
	else if (!strcmp ("-m", argv_i)) use_mouse = 1;
	else if (!strcmp ("-k", argv_i)) no_score_file = 1;
	else if (!strcmp ("-k0", argv_i))
	  Slrn_Perform_Scoring &= ~SLRN_EXPENSIVE_SCORING;
	else if (!strcmp ("-w", argv_i))
	  wait_for_key = 1;
	else if (!strcmp ("-w0", argv_i))
	  wait_for_key = 2;
	else if (!strncmp ("-D", argv_i, 2) && (argv_i[2] != 0))
	  {
	     if (-1 == SLdefine_for_ifdef (argv_i + 2))
	       {
		  slrn_exit_error (_("Unable to add preprocessor name %s."),
				   argv_i + 2);
	       }
	  }
	else if (i + 1 < (unsigned int) argc)
	  {
	     if (!strcmp ("-f", argv_i)) Slrn_Newsrc_File = argv[++i];
	     else if (!strcmp ("-i", argv_i)) init_file = argv[++i];
	     else
	       {
		  i += parse_object_args (NULL, argv + i, argc - i);
		  i -= 1;
	       }
	  }
	else
	  {
	     i += parse_object_args (NULL, argv + i, argc - i);
	     i -= 1;
	  }
     }

   fprintf (stdout, "slrn %s\n", Slrn_Version_String);

   if (dsc_flag && create_flag)
     {
	usage (_("The -d and --create flags must not be specified together."), 1);
     }

   if (Slrn_Batch == 0)
     {
	Slrn_UTF8_Mode = SLutf8_enable (-1);
	SLtt_get_terminfo ();
	if (use_color == 1) SLtt_Use_Ansi_Colors = 1;
	else if (use_color == -1) SLtt_Use_Ansi_Colors = 0;
     }

#if SLRN_USE_SLTCP && SLRN_HAS_NNTP_SUPPORT
   SLtcp_Verror_Hook = slrn_verror;
   /* This is necessary to ensure that gethostname works on Win32 */
   if (-1 == sltcp_open_sltcp ())
     slrn_exit_error (_("Error initializing SLtcp interface"));
#endif

   if (-1 == slrn_init_slang ())
     slrn_exit_error (_("Error initializing S-Lang interpreter.\n"));

   slrn_startup_initialize ();

   /* We need to get user info first, because the request file in true offline
    * reading mode is chosen based on login name */
   slrn_get_user_info ();

   /* The next function call will also define slang preprocessing tokens
    * for the appropriate objects.  For that reason, it is called after
    * startup initialize.
    */
   if (-1 == slrn_init_objects ())
     {
	slrn_exit_error (_("Error configuring server objects."));
     }

   slrn_init_hangup_signals (1);

#ifdef VMS
   slrn_snprintf (file, sizeof (file), "%s%s", SLRN_CONF_DIR, "slrn.rc");
#else
   slrn_snprintf (file, sizeof (file), "%s/%s", SLRN_CONF_DIR, "slrn.rc");
#endif

   /* Make sure terminal is initialized before setting colors.  The
    * SLtt_get_terminfo call above fixed that.
    */
   slrn_read_startup_file (file);      /* global file for all users */

   if (init_file == NULL)
     {
	slrn_make_home_filename (SLRN_USER_SLRNRC_FILENAME, file, sizeof (file));
	init_file = file;
     }

   if ((-1 == slrn_read_startup_file (init_file)) && (init_file != file))
     {
	slrn_exit_error (_("\
Could not read specified config file %s\n"), init_file);
     }

   if (Slrn_Saw_Obsolete)
     {
	slrn_message (_("\n! Your configuration file contains obsolete commands or function names that\n"
		      "! will not be supported by future versions of this program.\n"
		      "! If you have Perl installed, you can use the script slrnrc-conv to change\n"
		      "! your configuration accordingly.  It can be found in the source distribution\n"
		      "! or retrieved from <URL:http://slrn.sourceforge.net/data/>.\n"));
	Slrn_Saw_Warning = 1;
     }

   if (Slrn_Server_Id == 0) Slrn_Server_Id = Slrn_Default_Server_Obj;
   if (Slrn_Post_Id == 0) Slrn_Post_Id = Slrn_Default_Post_Obj;
   if (no_new_groups) Slrn_Check_New_Groups = 0;

   slrn_prepare_charset();

   if (print_config)
     {
	(void) putc ('\n', stdout);
	slrn_show_version (stdout);
	slrn_print_config (stdout);
	slrn_quit (0);
     }

#ifdef SIGINT
   if (Slrn_TT_Initialized == 0)
     SLsignal_intr (SIGINT, SIG_DFL);
#endif

   if ((-1 == slrn_select_server_object (Slrn_Server_Id))
       || (-1 == slrn_select_post_object (Slrn_Post_Id)))
     {
	slrn_exit_error (_("Unable to select server/post object."));
     }

#if !defined(IBMPC_SYSTEM)
   /* Allow blink characters if in mono */
   if (SLtt_Use_Ansi_Colors == 0)
     SLtt_Blink_Mode = 1;
#endif

   /* Now that we have read in the startup file, check to see if the user
    * has a username and a usable hostname.  Without those, we are not
    * starting up.
    */
   if ((NULL == Slrn_User_Info.hostname)
       || (0 == slrn_is_fqdn (Slrn_User_Info.hostname)))
     {
	slrn_exit_error (_("\
Unable to find a valid hostname for constructing your e-mail address.\n\
You probably want to specify a hostname in your %s file.\n\
Please see the \"slrn reference manual\" for full details.\n"),
			 SLRN_USER_SLRNRC_FILENAME);
     }
   if ((NULL == Slrn_User_Info.username)
       || (0 == *Slrn_User_Info.username)
       || (NULL != slrn_strbyte (Slrn_User_Info.username, '@')))
     {
	slrn_exit_error (_("\
Unable to find your user name.  This means that a valid 'From' header line\n\
cannot be constructed.  Try setting the USER environment variable.\n"));
     }

   if ((NULL == Slrn_User_Info.posting_host)
       && Slrn_Generate_Message_Id)
     {
	slrn_stderr_strcat (_("\
***Warning: Unable to find a unique fully-qualified host name."), "\n", _("\
            slrn will not generate any Message-IDs."), "\n", _("\
            Please note that the \"hostname\" setting does not affect this;"), "\n", _("\
            see the \"slrn reference manual\" for details."), "\n", NULL);
	Slrn_Saw_Warning = 1;
     }

   if (no_score_file == 0) read_score_file ();

   if (NULL == (hlp_file = getenv ("SLRNHELP")))
     {
	hlp_file = file;
#ifdef VMS
	slrn_snprintf (file, sizeof (file), "%s%s", SLRN_CONF_DIR, "help.txt");
#else
	slrn_snprintf (file, sizeof (file), "%s/%s", SLRN_CONF_DIR, "help.txt");
#endif
     }

   slrn_parse_helpfile (hlp_file);

   if ((Slrn_Newsrc_File == NULL)
       && ((Slrn_Newsrc_File = slrn_map_file_to_host (Slrn_Server_Obj->sv_name)) == NULL))
     {
#if defined(VMS) || defined(IBMPC_SYSTEM)
	Slrn_Newsrc_File = "jnews.rc";
#else
	Slrn_Newsrc_File = ".jnewsrc";
#endif
	slrn_make_home_filename (Slrn_Newsrc_File, file, sizeof (file));
	Slrn_Newsrc_File = slrn_safe_strmalloc (file);
     }

   slrn_message_now (_("Using newsrc file %s for server %s."),
		     Slrn_Newsrc_File, Slrn_Server_Obj->sv_name);

   if (use_active) Slrn_List_Active_File = 1;
   if (use_mouse) Slrn_Use_Mouse = -1; /* -1 forces it. */
   if (use_color == 1) SLtt_Use_Ansi_Colors = 1;
   else if (use_color == -1) SLtt_Use_Ansi_Colors = 0;

   if (dsc_flag)
     {
	if (Slrn_Server_Obj->sv_initialize () != 0)
	  {
	     slrn_exit_error (_("Unable to initialize server."));
	  }
	slrn_get_group_descriptions ();
	Slrn_Server_Obj->sv_close ();
	exit (0);
     }

   if (create_flag == 0)
     {
	/* Check to see if the .newsrc file exists -- I should use the access
	 * system call but for now, do it this way.
	 */
	if (NULL == (fp = fopen (Slrn_Newsrc_File, "r")))
	  {
#if 0 /* now disabled in group.c */
	     slrn_error (_("Unable to open %s.  I will try .newsrc."), Slrn_Newsrc_File);
	     if (NULL == (fp = slrn_open_home_file (".newsrc", "r", file,
						    sizeof (file), 0)))
	       {
#endif
		  slrn_exit_error (_("\nUnable to open %s.\n\
If you want to create %s, add command line options:\n\
   -f %s --create\n"), Slrn_Newsrc_File, Slrn_Newsrc_File, Slrn_Newsrc_File);
#if 0
	       }
#endif
	  }
	slrn_fclose (fp);
     }

   /* make sure we have an entry for the server */
   slrn_add_to_server_list (Slrn_Server_Obj->sv_name, NULL, NULL, NULL);

   if (kill_logfile != NULL)
     {
	Slrn_Kill_Log_FP = fopen(kill_logfile, "a");
	if (Slrn_Kill_Log_FP == NULL)
	  slrn_error (_("Unable to open %s for logging."), kill_logfile);
     }

   lock_file (1);

   (void) slrn_run_hooks (HOOK_STARTUP, 0);
   Ran_Startup_Hook = 1;

#if SLRN_HAS_GROUPLENS
   if (Slrn_Server_Id != SLRN_SERVER_ID_NNTP)
     Slrn_Use_Group_Lens = 0;

   if (Slrn_Use_Group_Lens && (Slrn_Batch == 0))
     {
	slrn_message (_("Initializing GroupLens"));
	if (-1 == slrn_init_grouplens ())
	  {
	     fprintf (stderr, _("GroupLens disabled.\n"));
	     Slrn_Use_Group_Lens = 0;
	     Slrn_Saw_Warning = 1;
	  }
     }
#endif

   if (Slrn_Server_Obj->sv_initialize () != 0)
     {
	slrn_exit_error (_("Failed to initialize server."));
     }

   if (Slrn_Server_Obj->sv_has_xover == 1)
     {
	if (0 == (Slrn_Server_Obj->sv_has_xover = slrn_read_overview_fmt ()))
	  slrn_message (_("OVERVIEW.FMT not RFC 2980 compliant -- XOVER support disabled."));
     }

   if (Slrn_Check_New_Groups || create_flag)
     {
	slrn_get_new_groups (create_flag);
     }

   slrn_read_newsrc (create_flag);
   slrn_read_group_descriptions ();

   if (wait_for_key && ((wait_for_key == 1) || Slrn_Saw_Warning))
     {
	slrn_message (_("* Press Ctrl-C to quit, any other key to continue.\n"));
	slrn_set_display_state (SLRN_TTY_INIT);
	if ('\003' == SLang_getkey ())
	  slrn_exit_error (_("Exit on user request."));
     }
   else
     putc ('\n', stdout);

   slrn_set_display_state (SLRN_SMG_INIT | SLRN_TTY_INIT);

#if defined(__unix__) && !defined(IBMPC_SYSTEM)
   if (Slrn_Autobaud) SLtt_Baud_Rate = SLang_TT_Baud_Rate;
#endif

   return 0;
}

/*}}}*/

/*{{{ Main Loop and Key Processing Functions */
#define MAX_MODE_STACK_LEN 15
/* Allow one extra because the top one is the current mode */
static Slrn_Mode_Type *Mode_Stack [MAX_MODE_STACK_LEN + 1];
static unsigned int Mode_Stack_Depth;

void slrn_push_mode (Slrn_Mode_Type *mode)
{
   if (Mode_Stack_Depth == MAX_MODE_STACK_LEN)
     slrn_exit_error (_("Internal Error: Mode_Stack overflow"));

   Mode_Stack[Mode_Stack_Depth] = Slrn_Current_Mode;
   Mode_Stack_Depth++;

   Mode_Stack[Mode_Stack_Depth] = Slrn_Current_Mode = mode;

   Slrn_Full_Screen_Update = 1;
   if ((mode != NULL) && (mode->enter_mode_hook != NULL))
     (*mode->enter_mode_hook) ();
}

void slrn_pop_mode (void)
{
   if (Mode_Stack_Depth == 0)
     slrn_exit_error (_("Internal Error: Mode_Stack underflow"));

   Mode_Stack [Mode_Stack_Depth] = NULL;   /* null current mode */
   Mode_Stack_Depth--;
   Slrn_Current_Mode = Mode_Stack[Mode_Stack_Depth];

   Slrn_Full_Screen_Update = 1;
   if ((Slrn_Current_Mode != NULL) && (Slrn_Current_Mode->enter_mode_hook != NULL))
     (*Slrn_Current_Mode->enter_mode_hook) ();
}

void slrn_update_screen (void)
{
   Slrn_Mode_Type *mode;
   unsigned int i, imax;

   if (Slrn_Batch) return;

   slrn_push_suspension (0);
   imax = Mode_Stack_Depth + 1;	       /* include current mode */
   for (i = 0; i < imax; i++)
     {
	Slrn_Full_Screen_Update = 1;
	mode = Mode_Stack [i];
	if ((mode != NULL)
	    && (mode->redraw_fun != NULL))
	  (*mode->redraw_fun) ();
     }
   slrn_smg_refresh ();
   slrn_pop_suspension ();
}

static void run_winch_functions (int old_r, int old_c)
{
   Slrn_Mode_Type *mode;
   unsigned int i, imax;

   imax = Mode_Stack_Depth + 1;	       /* include current */
   for (i = 0; i < imax; i++)
     {
	mode = Mode_Stack [i];
	if ((mode != NULL)
	    && (mode->sigwinch_fun != NULL))
	  (*mode->sigwinch_fun) (old_r, old_c);
     }

   if (SLang_get_error() == 0)
     slrn_run_hooks (HOOK_RESIZE_SCREEN, 0);
}

static void slrn_hangup (int sig) /*{{{*/
{
   Slrn_Mode_Type *mode;
   unsigned int i;

   slrn_init_hangup_signals (0);

   i = Mode_Stack_Depth + 1;	       /* include current */
   while (i != 0)
     {
	i--;
	mode = Mode_Stack [i];
	if ((mode != NULL)
	    && (mode->hangup_fun != NULL))
	  (*mode->hangup_fun) (sig);
     }

   slrn_write_newsrc (0);
   slrn_quit (sig);
}

/*}}}*/

void slrn_call_command (char *cmd) /*{{{*/
{
   SLKeymap_Function_Type *list;

   if ((Slrn_Current_Mode == NULL)
       || (Slrn_Current_Mode->keymap == NULL))
     list = NULL;
   else
     list = Slrn_Current_Mode->keymap->functions;

   while ((list != NULL) && (list->name != NULL))
     {
	if (0 == strcmp (cmd, list->name))
	  {
	     (void) (*list->f) ();
	     /* sync the line number to avoid surprises in subsequent calls
	      * that might change the article window (not the article itself) */
	     if (Slrn_Current_Mode->mode == SLRN_ARTICLE_MODE)
	       slrn_art_sync_article (Slrn_Current_Article);
	     return;
	  }
	list++;
     }

   slrn_error (_("call: %s not in current keymap."), cmd);
}

/*}}}*/

int slrn_getkey (void)
{
   static char buf[32];
   static unsigned int buf_len;
   static int timeout_active;

   int ch;

   if (SLang_Key_TimeOut_Flag == 0)
     {
	timeout_active = 0;
	buf_len = 0;
     }
   else	if ((timeout_active || (0 == SLang_input_pending (10)))
	    && (buf_len + 2 < sizeof (buf)))
     {
	int r, c;

	buf[buf_len] = '-';
	buf[buf_len + 1] = 0;

	slrn_push_suspension (0);
	r = SLsmg_get_row (); c = SLsmg_get_column ();
	slrn_message ("%s", buf);
	SLsmg_gotorc (r, c);
	slrn_smg_refresh ();
	slrn_pop_suspension ();
	timeout_active = 1;
     }

   while (1)
     {
#if defined(REAL_UNIX_SYSTEM) && (SLANG_VERSION < 20202)
	int ttyfd = SLang_TT_Read_FD;
#endif
	int e;
	errno = 0;

	ch = SLang_getkey ();
	if (ch != SLANG_GETKEY_ERROR)
	  break;
	e = errno;

#if defined(REAL_UNIX_SYSTEM) && (SLANG_VERSION < 20202)
	if (ttyfd != SLang_TT_Read_FD)
	  continue;
#endif

	slrn_exit_error ("%s: errno=%d [%s]", _("SLang_getkey failed"),
			 e, SLerrno_strerror (e));
     }

   if (buf_len + 4 < sizeof (buf))
     {
	if (ch == 0)
	  {
	     /* Need to handle NULL character. */
	     buf[buf_len++] = '^';
	     buf[buf_len] = '@';
	  }
	else if (ch == 27)
	  {
	     buf[buf_len++] = 'E';
	     buf[buf_len++] = 'S';
	     buf[buf_len++] = 'C';
	     buf[buf_len] = ' ';
	  }
	else buf[buf_len] = (char) ch;
     }
   buf_len++;

   return ch;
}

void slrn_do_keymap_key (SLKeyMap_List_Type *map) /*{{{*/
{
   SLang_Key_Type *key;
   static SLKeyMap_List_Type *last_map;
   static SLang_Key_Type *last_key;

   Suspend_Sigtstp_Suspension = 1;
   key = SLang_do_key (map, slrn_getkey);
   Suspend_Sigtstp_Suspension = 0;

   if (Slrn_Message_Present || SLang_get_error())
     {
	if (SLang_get_error()) SLang_restart (0);
	slrn_clear_message ();
     }
   SLKeyBoard_Quit = 0;
   SLang_set_error (0);

   if ((key == NULL) || (key->type == 0))
     {
	SLtt_beep ();
	return;
     }

   if (key->type == SLKEY_F_INTRINSIC)
     {
	if ((map == last_map) && (key->f.f == (FVOID_STAR) slrn_repeat_last_key))
	  key = last_key;

	/* set now to avoid problems with recursive call */
	last_key = key;
	last_map = map;

	if (key->type == SLKEY_F_INTRINSIC)
	  {
	     (((void (*)(void))(key->f.f)) ());
	     return;
	  }
     }

   /* Otherwise we have interpreted key. */
   last_key = key;
   last_map = map;

   Slrn_Full_Screen_Update = 1;

   if ((*key->f.s == '.')
       || !SLang_execute_function (key->f.s))
     SLang_load_string(key->f.s);
}

/*}}}*/

void slrn_set_prefix_argument (int rep) /*{{{*/
{
   static int repeat;

   repeat = rep;
   Slrn_Prefix_Arg_Ptr = &repeat;
}

/*}}}*/

void slrn_digit_arg (void) /*{{{*/
{
   char buf[20];
   unsigned char key;
   unsigned int i;

   i = 0;
   buf[i++] = (char) SLang_Last_Key_Char;

   SLang_Key_TimeOut_Flag = 1;

   while (1)
     {
	buf[i] = 0;
	key = (unsigned char) slrn_getkey ();
	if ((key < '0') || (key > '9')) break;
	if (i + 1 < sizeof (buf))
	  {
	     buf[i] = (char) key;
	     i++;
	  }
     }

   SLang_Key_TimeOut_Flag = 0;
   slrn_set_prefix_argument (atoi (buf));

   SLang_ungetkey (key);
   if ((Slrn_Current_Mode != NULL)
       && (Slrn_Current_Mode->keymap != NULL))
     slrn_do_keymap_key (Slrn_Current_Mode->keymap);

   Slrn_Prefix_Arg_Ptr = NULL;
}

/*}}}*/

void slrn_repeat_last_key (void) /*{{{*/
{
   SLtt_beep ();
}

/*}}}*/

int main (int argc, char **argv) /*{{{*/
{
   if (-1 == main_init_and_parse_args (argc, argv))
     return 1;

   if (-1 == slrn_select_group_mode ())
     return 1;

   slrn_push_suspension (1);

   (void) slrn_run_hooks (HOOK_GROUP_MODE_STARTUP, 0);

   if (Slrn_Batch) return 1;

   if (SLang_get_error ())
     {
	SLang_restart (1);	       /* prints any queued messages */
	slrn_exit_error (_("An error was encountered during initialization."));
     }

   while (Slrn_Current_Mode != NULL)
     {
	if (SLKeyBoard_Quit)
	  {
	     SLKeyBoard_Quit = 0;
	     slrn_error (_("Quit!"));
	  }

	(void) slrn_handle_interrupts ();

	if (SLang_get_error() || !SLang_input_pending(0))
	  {
	     slrn_update_screen ();
	  }

	slrn_do_keymap_key (Slrn_Current_Mode->keymap);
     }

   return 1;
}

/*}}}*/

/*}}}*/

int slrn_posix_system (char *cmd, int reset) /*{{{*/
{
   int ret;
   int init_mode = Slrn_TT_Initialized;

   if (reset) slrn_set_display_state (0);
   ret = SLsystem (cmd);
   if (reset) slrn_set_display_state (init_mode);
   Slrn_Full_Screen_Update = 1;
   return ret;
}

/*}}}*/
