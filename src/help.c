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

#include <stdio.h>
#include <string.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include "help.h"
#include "slrn.h"
#include "snprintf.h"
#include "misc.h"
#include "util.h"
#include "strutil.h"

static char *Global_Help [] =
{
   "",
   "",
   N_(" More information about slrn can be found on its home page:"),
   N_("   http://slrn.sourceforge.net/"),
   N_(" Questions not covered by the documentation can be posted to"),
   N_("   news.software.readers"),
   "",
   N_(" Please email bug reports, suggestions or comments to the author of this"),
   N_(" program, John E. Davis <jed@jedsoft.org>"),
   NULL
};

static char *Art_Help[] =
{
/* begin makehelp(article) - do *NOT* modify this line */
   N_(" Note: The keys are case sensitive!  That is, 's' and 'S' are not the same."),
   N_("General movement:"),
   N_("  n                  Go to the next unread article (or next group, if at end)."),
   N_("  p                  Go to the previous unread article."),
   N_("  N, ESC RIGHT       Skip to next group."),
   N_("  ESC LEFT           Go to previous group."),
   N_("  !                  Go to the next article with a high score."),
   N_("  =                  Go to the next article with the same subject."),
   N_("  L                  Go to the last read article and display it."),
   N_("Actions:"),
   N_("  P                  Post a new article (no followup)."),
   N_("  ESC P              Post or edit a postponed article."),
   N_("  f                  Post a followup to the current article."),
   N_("      ESC 1 f        Include all headers in the followup."),
   N_("      ESC 2 f        Followup without modifying (e.g. quoting) the article."),
   N_("  r                  Reply to poster (via email)."),
   N_("  F                  Forward the current article to someone (via email)."),
   N_("      ESC 1 F        Forward the current article (including all headers)."),
   N_("  ESC Ctrl-S         Supersede article (you have to be the author)."),
   N_("  ESC Ctrl-C         Cancel article (you have to be the author)."),
   N_("  o                  Save article, tagged articles or thread to file."),
   N_("  |                  Pipe article to an external program."),
   N_("  y                  Print article (as displayed)."),
   N_("      ESC 1 y        Print article (unwrapped and including hidden lines)."),
#if SLRN_HAS_DECODE
   N_("  :                  Decode article, tagged articles or thread."),
#endif
   N_("  Ctrl-Z             Suspend slrn."),
   N_("  q                  Return to group display."),
   N_("  Q                  Quit slrn immediately."),
   N_("Moving in the article pager:"),
   N_("  ESC DOWN           Scroll article down one line."),
   N_("  ESC UP             Scroll article up one line."),
   N_("  SPACE              Scroll article down one page (or select next, if at end)."),
   N_("  DELETE, b          Scroll article up one page."),
   N_("  >                  Move to end of the article."),
   N_("  <                  Move to beginning of the article."),
   N_("  LEFT               Pan article to the left."),
   N_("  RIGHT              Pan article to the right."),
   N_("  /                  Search forward in the article."),
   N_("  TAB                Skip beyond quoted text."),
   N_("  g                  Skip to next digest."),
   N_("Moving in the header display:"),
   N_("  DOWN, Ctrl-N       Move to the next article."),
   N_("  UP, Ctrl-P         Move to the previous article."),
   N_("  Ctrl-D             Scroll down one page."),
   N_("  Ctrl-U             Scroll up one page."),
   N_("  ESC >              Go to the last article in group."),
   N_("  ESC <              Go to the first article in group."),
   N_("  j                  Jump to article (by server number)."),
   N_("  a                  Author search forward."),
   N_("  A                  Author search backward."),
   N_("  s                  Subject search forward."),
   N_("  S                  Subject search backward."),
   N_("Marking as read/unread:"),
   N_("  d                  Mark article or collapsed thread as read."),
   N_("  u                  Mark article or collapsed thread as unread."),
   N_("  ESC d              Mark entire (sub-)thread as read."),
   N_("  c                  Catchup - mark all articles as read."),
   N_("  C                  Mark all articles up to the current position as read."),
   N_("  ESC u              Un-Catchup - mark all articles as unread."),
   N_("  ESC U              Mark all articles up to the current position as unread."),
   N_("  x                  Remove all non-tagged read articles from the list."),
   N_("Article pager commands:"),
   N_("  t                  Show full headers (on/off)."),
   N_("  ESC r              Decrypt ROT-13 (on/off)."),
   N_("  T                  Display quoted lines (on/off)."),
   N_("  \\                  Show signature (on/off)."),
   N_("  W                  Wrap long lines (on/off)."),
#if SLRN_HAS_SPOILERS
   N_("  ESC ?              Reveal spoilers."),
#endif
   N_("  ]                  Show PGP signature (on/off)."),
   N_("  [                  Show verbatim marks (on/off)."),
   N_("  Ctrl-^             Enlarge the article window."),
   N_("  ^                  Shrink the article window."),
   N_("  z                  Maximize / Unmaximize the article window."),
   N_("  h                  Hide / Show the article window."),
   N_("  U                  Search for URLs and follow them."),
   N_("Header window commands:"),
   N_("  ESC t              Collapse / Uncollapse thread."),
   N_("      ESC 1 ESC t    Collapse / Uncollapse all threads."),
   N_("  ESC a              Toggle between header display formats."),
   N_("  ESC s              Select threading and sorting method."),
   N_("Miscellaneous actions:"),
   N_("  K                  Create a scorefile entry interactively."),
   N_("      ESC 1 K        Edit scorefile."),
#if SLRN_HAS_SPOOL_SUPPORT
   N_("  m                  (Un-)mark article body for download by slrnpull."),
#endif
   N_("  v                  Show which scorefile rules matched the current article."),
   N_("  * The following five commands query the server if necessary:"),
   N_("  ESC l              Locate article by its Message-ID."),
   N_("  ESC Ctrl-P         Find all children of current article."),
   N_("  ESC p              Find parent article."),
   N_("      ESC 1 ESC p    Reconstruct thread (slow when run on large threads)."),
   N_("      ESC 2 ESC p    Reconstruct thread (faster, may not find all articles)."),
   N_("  ;                  Set a mark at the current article."),
   N_("  ,                  Return to previously marked article."),
   N_("  #                  Numerically tag article (for saving / decoding)."),
   N_("  ESC #              Remove all numerical tags."),
   N_("  *                  Protect article from catchup commands."),
   N_("      ESC 1 *        Remove all protection marks."),
   N_("  .                  Repeat last key sequence."),
   N_("  Ctrl-X ESC         Read line and interpret it as S-Lang."),
#if SLRN_HAS_GROUPLENS
   N_("  0                  Rate article with GroupLens."),
#endif
   N_("  Ctrl-R, Ctrl-L     Redraw screen."),
   N_("  ?                  Display this help screen."),
/* end makehelp - do *NOT* modify this line */
   NULL
};

static char *Group_Help [] =
{
/* begin makehelp(group) - do *NOT* modify this line */
   N_(" Note: The keys are case sensitive!  That is, 's' and 'S' are not the same."),
   N_("Cursor movement:"),
   N_("  DOWN                    Go to the next group."),
   N_("  UP                      Go to the previous group."),
   N_("  Ctrl-V, Ctrl-D          Scroll to the next page."),
   N_("  ESC V, Ctrl-U           Scroll to the previous page."),
   N_("  ESC >                   Go to the bottom of the list."),
   N_("  ESC <                   Go to the top of the list."),
   N_("  /                       Group keyword search."),
   N_("Actions:"),
   N_("  SPACE, RETURN           Enter the current newsgroup."),
   N_("  * The following variations also download previously read articles:"),
   N_("      ESC 1 SPACE         Enter group with article number query."),
   N_("      ESC 2 SPACE         Enter group, but do not apply score."),
   N_("      ESC 3 SPACE         Enter group with query, but without scoring."),
   N_("      ESC 4 SPACE         Enter the current newsgroup."),
   N_("  P                       Post an article to the current newsgroup."),
   N_("  ESC P                   Post or edit a postponed article."),
   N_("  G                       Get new news from server."),
   N_("  K                       Select scoring mode."),
   N_("  .                       Repeat last key sequence."),
   N_("  Ctrl-X ESC              Read line and interpret it as S-Lang."),
   N_("  Ctrl-Z                  Suspend slrn."),
   N_("  q                       Quit slrn."),
   N_("Group management (affects newsrc file):"),
   N_("  c                       Catchup - Mark all articles as read."),
   N_("  ESC u                   Un-Catchup - Mark all articles as unread."),
   N_("  a                       Add a new newsgroup."),
   N_("  s                       Subscribe to the current newsgroup."),
   N_("      ESC 1 s             Subscribe to groups matching a pattern."),
   N_("  u                       Unsubscribe from the current newsgroup."),
   N_("      ESC 1 u             Unsubscribe from groups matching a pattern."),
   N_("  m                       Move newsgroup to a different location."),
   N_("  Ctrl-X, Ctrl-T          Transpose position of groups."),
   N_("  X                       Force a save of the newsrc file."),
   N_("Display:"),
   N_("  ESC a                   Toggle between group display formats."),
   N_("  l                       Toggle display of groups without unread articles."),
   N_("  L                       Toggle listing of unsubscribed groups."),
   N_("      ESC 1 L             Hide unsubscribed groups."),
   N_("      ESC 2 L             Show unsubscribed groups."),
   N_("  Ctrl-L, Ctrl-R          Redraw the screen."),
   N_("  ?                       Display this help screen."),
/* end makehelp - do *NOT* modify this line */
   NULL
};

static char *Copyright_Notice [] =
{
   " Copyright (c) 1994, 1999, 2007-2016 John E. Davis <jed@jedsoft.org>",
     "",
     " For parts of it:",
     " Copyright (c) 2001-2003 Thomas Schultz <tststs@gmx.de>",
     "",
     " For the parts in src/snprintf.c that are based on code from glib 1.2.8:",
     " Copyright (C) 1995-1998  Peter Mattis, Spencer Kimball and Josh MacDonald",
     " Modified by the GLib Team and others 1997-1999.  See the AUTHORS",
     " file for a list of people on the GLib Team.  See the ChangeLog",
     " files for a list of changes.  These files are distributed with",
     " GLib at ftp://ftp.gtk.org/pub/gtk/.",
     "",
     " Patches were contributed by more people than I can list here.",
     "",
     " This program is distributed under the following conditions:",
     "",
     " This program is free software; you can redistribute it and/or modify it",
     " under the terms of the GNU General Public License as published by the Free",
     " Software Foundation; either version 2 of the License, or (at your option)",
     " any later version.",
     "",
     " This program is distributed in the hope that it will be useful, but WITHOUT",
     " ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or",
     " FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for",
     " more details.",
     "",
     " You should have received a copy of the GNU General Public License along",
     " with this program; if not, write to the Free Software Foundation, Inc.,",
     " 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.",
#if ! SLRN_USE_SLTCP
     "",
     " Please note that this version of slrn also contains code published under a",
     " different license:",
     "",
     " This software is Copyright 1991 by Stan Barber.",
     "",
     " Permission is hereby granted to copy, reproduce, redistribute or otherwise",
     " use this software as long as: there is no monetary profit gained",
     " specifically from the use or reproduction or this software, it is not",
     " sold, rented, traded or otherwise marketed, and this copyright notice is",
     " included prominently in any copy made.",
     "",
     " The author make no claims as to the fitness or correctness of this software",
     " for any use whatsoever, and it is provided as is. Any use of this software",
     " is at the user's own risk.",
#endif
     NULL
};

#define MAX_HELP_LINES 256
static char *User_Article_Help[MAX_HELP_LINES];
static char *User_Group_Help[MAX_HELP_LINES];

static void do_help (char **help)
{
   int i;
   char **p, *sect = NULL;
   char quit;
   char **this_help;

   this_help = p = help;

   slrn_enable_mouse (0);

   while (1)
     {
	i = 0;
	if (*p == NULL) break;

	slrn_push_suspension (0);

	SLsmg_cls ();

	if ((sect != NULL) && (**p == ' '))
	  {
	     SLsmg_set_color (1);
	     SLsmg_gotorc (i, 0);
	     SLsmg_write_string (_(sect));
	     SLsmg_set_color (0);
	     SLsmg_write_string (_(" (continued)"));
	     i += 2;
	  }
	else if (p == Copyright_Notice)
	  {
	     char *msg = N_("\
As a translation would not be legally binding, it remains untranslated.");
	     SLsmg_set_color (1);
	     SLsmg_gotorc (i++, 0);
	     SLsmg_write_string (_("\
The following copyright notice applies to the slrn newsreader:"));
	     SLsmg_set_color (0);
	     if (strcmp (msg, _(msg)))
	       {
		  SLsmg_gotorc (i++, 0);
		  SLsmg_write_string (_(msg));
	       }
	     i++;
	  }

	while (i < SLtt_Screen_Rows - 4)
	  {
	     char pp;

	     if (*p == NULL)
	       {
		  if ((this_help == Global_Help) ||
		      (this_help == Copyright_Notice)) break;
		  this_help = p = Global_Help;
		  sect = NULL;
	       }

	     pp = **p;
	     if ((pp != ' ') && pp)
	       {
		  sect = *p;
		  if ((i + 6) > SLtt_Screen_Rows) break;
		  i++;
		  SLsmg_set_color (1);
	       }

	     SLsmg_gotorc (i, 0);
	     if (**p != '\0')
	       SLsmg_write_string (_(*p));
	     i++;
	     if (pp && (pp != ' '))
	       {
		  SLsmg_set_color (0);
		  i++;
	       }
	     p++;
	  }

	SLsmg_gotorc (i + 1, 0);
	SLsmg_set_color (1);

	if ((*p == NULL)
	    && (this_help == help))
	  {
	     this_help = p = Global_Help;
	     sect = NULL;
	  }

	if (*p == NULL)
	  {
	     if (this_help == Copyright_Notice)
	       SLsmg_write_string (_("\
Press 'c' to start over, or any other key to return to news reader."));
	     else
	       SLsmg_write_string (_("\
Press 'c' for the copyright, '?' to start over, or any other key to quit help."));
	  }
	else
	  {
	     if (this_help == Copyright_Notice)
	       SLsmg_write_string (_("\
Press 'q' to quit help, 'c' to start over, or any other key to continue."));
	     else
	       SLsmg_write_string (_("\
Press 'q' to quit help, '?' to start over, or any other key to continue."));
	  }

	slrn_smg_refresh ();

	slrn_pop_suspension ();

	SLang_flush_input ();
	quit = SLang_getkey ();
	if ((quit == '?') && (this_help != Copyright_Notice))
	  {
	     this_help = p = help;
	     sect = NULL;
	  }
	else if (((quit | 0x20) == 'c') &&
		 ((*p == NULL) || (this_help == Copyright_Notice)))
	  {
	     this_help = p = Copyright_Notice;
	     sect = NULL;
	  }
	else if ((*p == NULL) || ((quit | 0x20)== 'q'))
	    break;
     }
   Slrn_Full_Screen_Update = 1;
   slrn_set_color (0);
   /* slrn_redraw (); */
   SLang_flush_input ();
   slrn_enable_mouse (1);
}

int slrn_parse_helpfile (char *helpfile)
{
   FILE *fp;
   char buf[256];
   char ch;
   char **current_help = NULL;
   int num_lines = 0;
   unsigned char *b;
   int status;

   if (Slrn_Batch)
     return 0;

   if (NULL == (fp = fopen (helpfile, "r"))) return -1;

   status = -1;
   while (NULL != fgets (buf, sizeof (buf) - 1, fp))
     {
	ch = *buf;

	/* Skip over common comments */
	if ((ch == '#') || (ch == '%') || (ch == ';') || (ch == '!'))
	  continue;

	b = (unsigned char *) slrn_skip_whitespace (buf);
	if (*b == 0) continue;

	if (ch == '[')
	  {
	     /* end current help */
	     if (current_help != NULL)
	       {
		  slrn_free (current_help[num_lines]);
		  current_help[num_lines] = NULL;
	       }

	     num_lines = 0;
	     ch = *(buf + 1) | 0x20;
	     if (ch == 'a')
	       {
		  current_help = User_Article_Help;
	       }
	     else if (ch == 'g') current_help = User_Group_Help;
	     else current_help = NULL;

	     continue;
	  }

	if (current_help == NULL) continue;

	if (MAX_HELP_LINES == num_lines + 1)
	  {
	     current_help[num_lines] = NULL;
	     current_help = NULL;
	     continue;
	  }

	slrn_free (current_help [num_lines]);

	if (NULL == (current_help [num_lines] = (char *) slrn_strmalloc (buf, 0)))
	  goto return_error;

	num_lines++;
     }

   status = 0;
   /* drop */

return_error:
   if (current_help != NULL)
     {
	slrn_free (current_help[num_lines]);
	current_help[num_lines] = NULL;
     }
   slrn_fclose (fp);
   return status;
}

void slrn_article_help (void)
{
   char **h;

   if (Slrn_Batch) return;
   if (User_Article_Help[0] != NULL) h = User_Article_Help; else h = Art_Help;
   do_help (h);
}

void slrn_group_help (void)
{
   char **h;

   if (Slrn_Batch) return;
   if (User_Group_Help[0] != NULL) h = User_Group_Help; else h = Group_Help;
   do_help (h);
}

/* Returns the key sequence to which function f in the given keymap is
 * bound (NULL if unbound). If more than one key sequences apply, a random
 * one gets returned.
 * The returned string can be static and does not need to be freed. */
char *slrn_help_keyseq_from_function (char *f, SLKeyMap_List_Type *map) /*{{{*/
{
   int i;
   SLang_Key_Type *key, *key_root;
   FVOID_STAR fp;
   unsigned char type;
   static char buf[3];

   if (NULL == (fp = (FVOID_STAR) SLang_find_key_function(f, map)))
     type = SLKEY_F_INTERPRET;
   else type = SLKEY_F_INTRINSIC;

   i = 256;
   key_root = map->keymap;
   while (i--)
     {
	key = key_root->next;
	if ((key == NULL) && (type == key_root->type) &&
	    (((type == SLKEY_F_INTERPRET) && (!strcmp((char *) f, key_root->f.s)))
	     || ((type == SLKEY_F_INTRINSIC) && (fp == key_root->f.f))))
	  {
	     buf[0] = 2;
	     buf[1] = 256 - 1 - i;
	     buf[2] = 0;
	     return buf;
	  }

	while (key != NULL)
	  {
	     if ((key->type == type) &&
		 (((type == SLKEY_F_INTERPRET) && (!strcmp((char *) f, key->f.s)))
		  || ((type == SLKEY_F_INTRINSIC) && (fp == key->f.f))))
	       {
		  return (char*)key->str;
	       }
	     key = key->next;
	  }
	key_root++;
     }
   return NULL;
}
/*}}}*/

/* The following code handles conversion between (escape) key sequences
 * and their symbolic names, like "<Up>" */

/* In the following arrays, the same index corresponds to the same key */

#define NUMBER_OF_KEYNAMES 37

/* Symbolic names of keys as shown to the user */
static char *KeyNames[NUMBER_OF_KEYNAMES] = /*{{{*/
{
   N_( "<PageUp>" ),
   N_( "<PageDown>" ),
   N_( "<Up>" ),
   N_( "<Down>" ),
   N_( "<Right>" ),
   N_( "<Left>" ),
   N_( "<Delete>" ),
   N_( "<BackSpace>" ),
   N_( "<Insert>" ),
   N_( "<Home>" ),
   N_( "<End>" ),
   N_( "<Enter>" ),
   N_( "<Return>" ),
   N_( "<Tab>" ),
   N_( "<BackTab>" ),
   N_( "<Space>" ),
   N_( "<F1>" ),
   N_( "<F2>" ),
   N_( "<F3>" ),
   N_( "<F4>" ),
   N_( "<F5>" ),
   N_( "<F6>" ),
   N_( "<F7>" ),
   N_( "<F8>" ),
   N_( "<F9>" ),
   N_( "<F10>" ),
   N_( "<F11>" ),
   N_( "<F12>" ),
   N_( "<F13>" ),
   N_( "<F14>" ),
   N_( "<F15>" ),
   N_( "<F16>" ),
   N_( "<F17>" ),
   N_( "<F18>" ),
   N_( "<F19>" ),
   N_( "<F20>" ),
   N_( "<Esc>" )
}; /*}}}*/

/* Length information for the unlocalized versions */
static unsigned char KeyNameLengths[NUMBER_OF_KEYNAMES] =
{8,10,4,6,7,6,8,11,8,6,5,7,8,5,9,7,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,5,5,5,5};

#ifdef REAL_UNIX_SYSTEM
/* Symbolic names of keys in the termcap database */
static char *TermcapNames[NUMBER_OF_KEYNAMES] = /*{{{*/
{
   "kP",
     "kN",
     "ku",
     "kd",
     "kr",
     "kl",
     "kD",
     "kb",
     "kI",
     "kh",
     "@7",
     "@8",
     "",
     "",
     "kB",
     "",
     "k1",
     "k2",
     "k3",
     "k4",
     "k5",
     "k6",
     "k7",
     "k8",
     "k9",
     "k;",
     "F1",
     "F2",
     "F3",
     "F4",
     "F5",
     "F6",
     "F7",
     "F8",
     "F9",
     "FA",
     ""
}; /*}}}*/
#endif

/* Default key sequences as fallbacks when termcap lookup has no result */
static char *DefaultSequences[NUMBER_OF_KEYNAMES] = /*{{{*/
{
#ifdef IBMPC_SYSTEM
   "\xE0I",
     "\xE0Q",
     "\xE0H",
     "\xE0P",
     "\xE0M",
     "\xE0K",
     "\xE0S",
     "^@S",
     "\xE0R",
     "\xE0G",
     "\xE0O",
#else /* NOT IBMPC_SYSTEM */
     "\033[5~",
     "\033[6~",
     "\033[A",
     "\033[B",
     "\033[C",
     "\033[D",
     "\033[3~",
     "\010",
     "\033[2~",
     "\033[1~",
     "\033[4~",
#endif /* NOT IBMPC_SYSTEM */
     "\r",
     "\r",
     "\t",
     "\033[Z",
     " ",
#ifdef IBMPC_SYSTEM
     "^@;",
     "^@<",
     "^@=",
     "^@>",
     "^@?",
     "^@@",
     "^@A",
     "^@B",
     "^@C",
     "^@D",
     "^@\x85",
     "^@\x86",
     "^@T",
     "^@U",
     "^@V",
     "^@W",
     "^@X",
     "^@Y",
     "^@Z",
     "^@[",
#else /* NOT IBMPC_SYSTEM */
     "\033[11~",
     "\033[12~",
     "\033[13~",
     "\033[14~",
     "\033[15~",
     "\033[17~",
     "\033[18~",
     "\033[19~",
     "\033[20~",
     "\033[21~",
     "\033[23~",
     "\033[24~",
     "\033[25~",
     "\033[26~",
     "\033[28~",
     "\033[29~",
     "\033[31~",
     "\033[32~",
     "\033[33~",
     "\033[34~",
#endif /* NOT IBMPC_SYSTEM */
     "\033"
}; /*}}}*/

/* Key sequences we actually use, initialized by init_keysym_table */
/* The first character denotes the length of the sequence.
 * Additionally, the strings are nul-terminated. */
static char EscapeSequences[NUMBER_OF_KEYNAMES][SLANG_MAX_KEYMAP_KEY_SEQ+2];

/* This function has to be called once before the conversion function can be
 * used. */
void slrn_help_init_keysym_table (void) /*{{{*/
{
   int i;
   for (i = 0; i<NUMBER_OF_KEYNAMES; i++)
     {
#ifdef REAL_UNIX_SYSTEM
	if (*TermcapNames[i] != 0) /* try to find in termcap database */
	  {
	     char *s = SLtt_tgetstr (TermcapNames[i]);
	     int len;
	     if (s != NULL)
	       {
		  len = strlen(s);
		  if (len<=SLANG_MAX_KEYMAP_KEY_SEQ)
		    {
		       strcpy(EscapeSequences[i]+1,s); /* safe */
		       EscapeSequences[i][0] = len;
		       continue;
		    }
	       }
	  }
#endif
	/* Fall back to the compiled-in default */
	strcpy(EscapeSequences[i]+1,DefaultSequences[i]); /* safe */
	EscapeSequences[i][0] = strlen(DefaultSequences[i]);
     }
}
/*}}}*/

/* Returns a human-friendly string representation of the given key sequence.
 * Please note that it is stored in a local static variable.
 * May return NULL if the resulting string would be too large. */
char *slrn_help_keyseq_to_string (char *key, int keylen) /*{{{*/
{
   const int maxlen = 30;
   static char result[31]; /* maxlen+1 */
   int ind = 0;

   while (keylen && (ind < maxlen))
     {
	int i;
	for (i = 0; i < NUMBER_OF_KEYNAMES; i++)
	  {
/* On IBMPC_SYSTEM, "escape" sequences may begin with the nul character, which
 * requires special case treatment. Sigh.
 * We replaced nul with "^@" above so we can still use the str* functions */
	     if ((EscapeSequences[i][0]<=keylen) &&
		 (((*key == 0) && /* this part for IBMPC_SYSTEM */
		   (EscapeSequences[i][0]>=2) &&
		   (EscapeSequences[i][1] == '^') &&
		   (EscapeSequences[i][2] == '@') &&
		   !strncmp(EscapeSequences[i]+3,key+1, EscapeSequences[i][0]-2)) ||
		  !strncmp(EscapeSequences[i]+1, key, EscapeSequences[i][0])))
	       break;
	  }
	if (i == NUMBER_OF_KEYNAMES) /* no key name found */
	  {
	     if (*key < 32) /* non-printable */
	       {
		  char *ctrlstr = _("Ctrl-");
		  int len = strlen(ctrlstr)+1;
		  if (len > maxlen-ind)
		    break; /* not enough memory */
		  strcpy (result+ind, ctrlstr); /* safe */
		  result[ind+len-1] = *key + '@';
		  ind += len;
		  key++;
		  keylen--;
	       }
	     else
	       {
		  result[ind++] = *key++;
		  keylen--;
	       }
	  }
	else
	  {
	     int len = strlen(_(KeyNames[i]));
	     if (len > maxlen-ind)
	       break; /* not enough memory */
	     strcpy(result+ind, _(KeyNames[i])); /* safe */
	     ind += len;
	     key += EscapeSequences[i][0];
	     keylen -= EscapeSequences[i][0];
	  }
     }

   if (keylen)
     return NULL;

   result[ind] = 0;
   return result;
}
/*}}}*/

/* Returns the corresponding key sequence for a human-friendly representation.
 * Please note that the result is stored in a local static variable and may be
 * NULL if the sequence would be too large. */
char *slrn_help_string_to_keyseq (char *s) /*{{{*/
{
   /* We're using a nul-terminated representation here */
   static char result [SLANG_MAX_KEYMAP_KEY_SEQ+1];
   int ind = 0;
   int slen = strlen(s);

   while (slen && (ind <= SLANG_MAX_KEYMAP_KEY_SEQ))
     {
	char *end;
	if ((*s == '<') && (NULL != (end = slrn_strbyte (s,'>'))))
	  {
	     int i;
	     int len = end-s-1;
	     for (i = 0; i < NUMBER_OF_KEYNAMES; i++)
	       {
		  if ((KeyNameLengths[i] == len+2) &&
		      !slrn_case_strncmp (s+1,
					  KeyNames[i]+1, len))
		    break;
	       }
	     if (i < NUMBER_OF_KEYNAMES)
	       {
		  if (EscapeSequences[i][0] > SLANG_MAX_KEYMAP_KEY_SEQ - ind)
		    break; /* not enough memory */
		  strncpy(result+ind, EscapeSequences[i]+1, EscapeSequences[i][0]);
		  ind += EscapeSequences[i][0];
		  s += len+2;
		  slen -= len+2;
		  continue;
	       }
	  }
	/* by default, just copy the character */
	result[ind++] = *s++;
	slen--;
     }

   if (slen)
     return NULL;

   result[ind] = 0;
   return result;
}
/*}}}*/
