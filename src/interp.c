/* -*- mode: C; mode: fold -*- */
/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999 John E. Davis <davis@space.mit.edu>
 Copyright (c) 2001-2003 Thomas Schultz <tststs@gmx.de>

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

/* rest of file inside this #if statement */
#if SLRN_HAS_SLANG

/*{{{ Include files */

#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#include <string.h>
#ifdef HAVE_LOCALE_H
# include <locale.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include "slrn.h"
#include "group.h"
#include "art.h"
#include "misc.h"
#include "startup.h"
#include "util.h"
#include "server.h"
#include "menu.h"
#include "interp.h"
#include "print.h"
#include "score.h"
#include "snprintf.h"
#include "mime.h"
#include "version.h"
#include "hooks.h"

/*}}}*/

/*{{{ Public Global Variables */

int Slrn_Use_Slang = 0;
char *Slrn_Macro_Dir;

/*}}}*/


/*{{{ Screen update and message functions */

static void error (char *msg) /*{{{*/
{
   slrn_error ("%s", msg);
}

/*}}}*/

static void update (void) /*{{{*/
{
   slrn_update_screen ();
}
/*}}}*/

static void free_argv_list (char **argv, unsigned int argc)
{
   unsigned int i;
   
   for (i = 0; i < argc; i++)
     slrn_free (argv[i]);
   slrn_free ((char *) argv);
}

static char **pop_argv_list (unsigned int *argcp)
{
   int n;
   char **argv;
   unsigned int i, argc;

   if (-1 == SLang_pop_integer (&n))
     return NULL;

   if (n < 0)
     {
	slrn_error (_("positive integer expected"));
	return NULL;
     }

   argc = (unsigned int) n;
      
   if (NULL == (argv = (char **) slrn_malloc (sizeof (char *) * (argc + 1), 1, 1)))
     {
	SLang_Error = SL_MALLOC_ERROR;
	return NULL;
     }
   
   argv [n] = NULL;
   i = argc;
   while (i != 0)
     {
	i--;
	if (-1 == SLpop_string (argv + i))
	  {
	     free_argv_list (argv, argc);
	     return NULL;
	  }
     }
   *argcp = argc;

   return argv;
}
   
static int interp_select_box (void) /*{{{*/
{
   unsigned int n;
   Slrn_Select_Box_Type box;
   int ret;

   if (Slrn_Batch)
     {
	slrn_error (_("select box function not available in batch mode."));
	return -1;
     }

   if (NULL == (box.lines = pop_argv_list (&n)))
     return -1;

   if (-1 == SLpop_string (&box.title))
     {
	free_argv_list (box.lines, n);
	return -1;
     }
   
   ret = slrn_select_box (&box);
   free_argv_list (box.lines, n);
   slrn_free (box.title);
   
   return ret;
}

/*}}}*/

static void select_list_box (void)
{
   char *title;
   unsigned int argc;
   char **argv;
   int active;
   int ret;

   if (-1 == SLang_pop_integer (&active))
     return;
   
   argv = pop_argv_list (&argc);
   if (argv == NULL)
     return;
   
   if (-1 == SLpop_string (&title))
     {
	free_argv_list (argv, argc);
	return;
     }
   
   ret = slrn_select_list_mode (title, argc, argv, active - 1, 1, NULL);
   if (ret == -1)
     {
	SLang_push_string ("");
	return;
     }
   
   SLang_push_string (argv[ret]);

   slrn_free (title);
   free_argv_list (argv, argc);
}

static int popup_window (void)
{
   char *title = NULL, *text = NULL;
   int retval = 0;
   
   if ((-1 == SLpop_string (&text)) ||
       (-1 == SLpop_string (&title)))
     goto free_and_return;
   
   retval = slrn_popup_win_mode (title, text);
   
   free_and_return:
   slrn_free (title);
   slrn_free (text);
   return retval;
}

static int get_yesno_cancel (char *prompt)
{
   return slrn_get_yesno_cancel ("%s", prompt);
}

static int get_response (char *choices, char *prompt)
{
   return slrn_get_response (choices, "%s", prompt);
}

   
static void tt_send (char *s)
{
   if (Slrn_Batch == 0)
     {
	SLtt_write_string (s);
	SLtt_flush_output ();
     }
}

/*}}}*/

/*{{{ File functions */

static void make_home_filename (char *name) /*{{{*/
{
   char file [SLRN_MAX_PATH_LEN];
   slrn_make_home_filename (name, file, sizeof (file));
   SLang_push_string (file);
}

/*}}}*/

static int evalfile (char *file)
{
   if (-1 == slrn_eval_slang_file (file))
     return 0;

   return 1;
}


int slrn_eval_slang_file (char *name) /*{{{*/
{
   char file [SLRN_MAX_PATH_LEN];
   
   if (Slrn_Macro_Dir != NULL)
     {
	int n = 0;
	char dir[SLRN_MAX_PATH_LEN];
	
	while (1)
	  {
	     if (-1 == SLextract_list_element (Slrn_Macro_Dir, n, ',',
						   file, sizeof (file)))
	       break;
	     
	     slrn_make_home_dirname (file, dir, sizeof (dir));
	     if ((-1 != slrn_dircat (dir, name, file, sizeof (file)))
		 && (1 == slrn_file_exists (file)))
	       {
		  slrn_message_now (_("loading %s"), file);
		  if (-1 == SLang_load_file (file))
		    return -1;
		  return 0;
	       }
	     n++;
	  }
     }
   
   slrn_make_home_filename (name, file, sizeof (file));
   slrn_message_now (_("loading %s"), file);
   if (0 == SLang_load_file (file))
     return -1;
   return 0;
}

/*}}}*/



/*}}}*/

/*{{{ Set/Get Variable Functions */

static void set_string_variable (char *s1, char *s2) /*{{{*/
{
   if (-1 == slrn_set_string_variable (s1, s2))
     slrn_error (_("%s is not a valid variable name."), s1);
}

/*}}}*/

static void set_integer_variable (char *s1, int *val) /*{{{*/
{
   if (-1 == slrn_set_integer_variable (s1, *val))
     slrn_error (_("%s is not a valid variable name."), s1);
}

/*}}}*/

static void get_variable_value (char *name) /*{{{*/
{
   char **s;
   int *ip;
   int type;
   
   if (-1 == slrn_get_variable_value (name, &type, &s, &ip))
     {
	slrn_error (_("%s is not a valid variable name."), name);
	return;
     }
   
   if (type == STRING_TYPE)
     {
	char *str;
	if ((s == NULL) || (*s == NULL)) str = "";
	else str = *s;
	SLang_push_string (str);
     }
   else if (type == INT_TYPE)
     {
	int i;
	if (ip == NULL) i = 0; else i = *ip;
	SLang_push_integer (i);
     }
}

/*}}}*/

static char *interp_setlocale (int *category, char *locale) /*{{{*/
{
#ifdef HAVE_SETLOCALE
   char *ret = setlocale (*category, locale);
   return (ret == NULL) ? "" : ret;
#else
   (void) category; (void) locale;
   slrn_error (_("setlocale is not available on this system."));
   return "";
#endif
}

/*}}}*/

static void set_utf8_conversion_table (void) /*{{{*/
{
#if SLANG_VERSION < 10400
   slrn_error (_("To use this feature, please update s-lang and recompile slrn."));
#else
# if ! SLRN_HAS_MIME
   slrn_error (_("This version of slrn lacks MIME support."));
# else
   SLang_Array_Type *at;
   int rows;
   
   if (-1 == SLang_pop_array_of_type (&at, SLANG_INT_TYPE))
     {
	slrn_error (_("Array of integer expected."));
	return;
     }
   
   if ((at->num_dims != 2) || (at->dims[0] != 2))
     {
	slrn_error (_("Two-dimensional array with two columns expected."));
	goto free_and_return;
     }
   
   slrn_free (Slrn_Utf8_Table);
   Slrn_Utf8_Table = NULL;
   
   if ((rows = at->dims[1]) == 0)
     goto free_and_return; /* no data, so set default (iso-8859-1) */
   
   if (NULL == (Slrn_Utf8_Table = slrn_malloc (65536, 1, 0)))
     {
	slrn_error (_("Could not get memory for conversion table."));
	goto free_and_return;
     }
   
   while (rows-- != 0)
     {
	int dims[2];
	int unicode, local;
	
	dims[0] = 0;
	dims[1] = rows;
	(void) SLang_get_array_element (at, dims, &unicode);
	
	if (((unicode != 0) && (unicode < 128)) || (unicode > 65535))
	  {
	     slrn_error (_("Unicode value in row %d out of range."), rows);
	     goto free_and_return;
	  }
	
	dims[0] = 1;
	(void) SLang_get_array_element (at, dims, &local);
	
	if ((local < 1) || (local > 255))
	  {
	     slrn_error (_("Local charset value in row %d out of range."),
			 rows);
	     goto free_and_return;
	  }
	
	Slrn_Utf8_Table[unicode] = (char) local;
     }
   
   free_and_return:
   SLang_free_array (at);
# endif /* SLRN_HAS_MIME */
#endif /* SLANG_VERSION */
}
/*}}}*/

static void generic_set_regexp (SLRegexp_Type **regexp_table) /*{{{*/
{
#if SLANG_VERSION < 10400
   slrn_error (_("To use this feature, please update s-lang and recompile slrn."));
#else
   SLang_Array_Type *at;
   SLcmd_Cmd_Table_Type cmd_table;
   int argc, i;
   
   if (-1 == SLang_pop_array_of_type (&at, SLANG_STRING_TYPE))
     {
	slrn_error (_("Array of string expected."));
	return;
     }
   
   if (at->num_dims != 1)
     {
	slrn_error (_("One-dimensional array expected."));
	goto free_and_return;
     }
   
   argc = at->dims[0];
   
   if ((argc<1) || (argc > SLRN_MAX_REGEXP))
     {
	slrn_error (_("Array must contain at least one, at most %d elements."),
		    SLRN_MAX_REGEXP);
	goto free_and_return;
     }
   
   if (NULL == (cmd_table.string_args = (char**) slrn_malloc
		((argc+1)*sizeof(char*), 0, 0)))
     {
	slrn_error (_("Failed to allocate memory."));
	goto free_and_return;
     }
   
   for (i = 0; i < argc; i++)
     {
	(void) SLang_get_array_element (at, &i, &(cmd_table.string_args[i+1]));
     }
   
   slrn_generic_regexp_fun (argc+1, &cmd_table, regexp_table);
   
   slrn_free ((char*)cmd_table.string_args);
   
   free_and_return:
   SLang_free_array (at);
#endif   
}/*}}}*/

static void set_ignore_quotes (void) /*{{{*/
{
   generic_set_regexp (Slrn_Ignore_Quote_Regexp);
}/*}}}*/

static void set_strip_re_regexp (void) /*{{{*/
{
   generic_set_regexp (Slrn_Strip_Re_Regexp);
}/*}}}*/

static void set_strip_sig_regexp (void) /*{{{*/
{
   generic_set_regexp (Slrn_Strip_Sig_Regexp);
}/*}}}*/

static void set_strip_was_regexp (void) /*{{{*/
{
   generic_set_regexp (Slrn_Strip_Was_Regexp);
}/*}}}*/
/*}}}*/

static char *get_server_name (void) /*{{{*/
{
   if ((NULL == Slrn_Server_Obj) 
       || (NULL == Slrn_Server_Obj->sv_name))
     return "";
   
   return Slrn_Server_Obj->sv_name;
}

/*}}}*/
static void quit (int *code) /*{{{*/
{
   slrn_quit (*code);
}

/*}}}*/
   
/*{{{ Keyboard related functions */

static void definekey (char *fun, char *key, char *map) /*{{{*/
{
   SLKeyMap_List_Type *kmap;
   
   if (!strcmp (map, "readline"))
     map = "ReadLine";
   
   if (NULL == (kmap = SLang_find_keymap (map)))
     {
	slrn_error (_("definekey: no such keymap."));
	return;
     }
   
   if ((kmap == Slrn_Keymap_RLI->keymap) &&
       (NULL == SLang_find_key_function(fun, kmap)))
     {
	SLKeymap_Function_Type *tmp = kmap->functions;
	kmap->functions = Slrn_Custom_Readline_Functions;
	SLang_define_key (key, fun, kmap);
	kmap->functions = tmp;
     }
   else
     SLang_define_key (key, fun, kmap);
}

/*}}}*/

static void undefinekey (char *key, char *map) /*{{{*/
{
   SLKeyMap_List_Type *kmap;
   
   if (!strcmp (map, "readline"))
     map = "ReadLine";
   
   if (NULL == (kmap = SLang_find_keymap (map)))
     {
	error (_("undefinekey: no such keymap."));
	return;
     }
   
   SLang_undefine_key (key, kmap);
}

/*}}}*/

#define PROMPT_NO_ECHO	1
#define PROMPT_FILENAME	2
#define PROMPT_VARIABLE 3
static void generic_read_mini (int mode, char *prompt, char *dfl, char *init) /*{{{*/
{
   char str[SLRL_DISPLAY_BUFFER_SIZE];
   int ret;

   strncpy (str, init, sizeof (str));
   str[sizeof(str) - 1] = 0;
   
   switch (mode)
     {
      case PROMPT_NO_ECHO:
	ret = slrn_read_input_no_echo (prompt, dfl, str, 0, 0);
	break;
      case PROMPT_FILENAME:
	ret = slrn_read_filename (prompt, dfl, str, 0, 0);
	break;
      case PROMPT_VARIABLE:
	ret = slrn_read_variable (prompt, dfl, str, 0, 0);
	break;
      default:
	ret = slrn_read_input (prompt, dfl, str, 0, 0);
     }
   
   if (-1 == ret)
     {
	error (_("Quit!"));
	return;
     }
   SLang_push_string (str);
}

/*}}}*/
static void read_mini (char *prompt, char *dfl, char *init) /*{{{*/
{
   generic_read_mini (0, prompt, dfl, init);
}

/*}}}*/
static void read_mini_no_echo (char *prompt, char *dfl, char *init) /*{{{*/
{
   generic_read_mini (PROMPT_NO_ECHO, prompt, dfl, init);
}

/*}}}*/

static void read_mini_filename (char *prompt, char *dfl, char *init) /*{{{*/
{
   generic_read_mini (PROMPT_FILENAME, prompt, dfl, init);
}

/*}}}*/

static void read_mini_variable (char *prompt, char *dfl, char *init) /*{{{*/
{
   generic_read_mini (PROMPT_VARIABLE, prompt, dfl, init);
}
/*}}}*/

static int read_mini_integer (char *prompt, int *dfl) /*{{{*/
{
   int status, retval;
   status = slrn_read_integer (prompt, dfl, &retval);
   if (-1 == status)
     error (_("Quit!"));
   slrn_clear_message ();
   return retval;
}
/*}}}*/

static void set_prefix_arg (int *arg) /*{{{*/
{
   slrn_set_prefix_argument (*arg);
}

/*}}}*/

static int get_prefix_arg (void) /*{{{*/
{
#define NO_PREFIX_ARGUMENT -1

   return Slrn_Prefix_Arg_Ptr ? *Slrn_Prefix_Arg_Ptr : NO_PREFIX_ARGUMENT;
} 

/*}}}*/

static void reset_prefix_arg (void) /*{{{*/
{
   Slrn_Prefix_Arg_Ptr = NULL;
}
  
/*}}}*/


static int check_tty (void)
{
   if (Slrn_TT_Initialized & SLRN_TTY_INIT)
     return 0;
   
   error (_("Terminal not initialized."));
   return -1;
}

static int input_pending (int *tsecs)
{
   if (check_tty ())
     return 0;
   
   return SLang_input_pending (*tsecs);
}

static int getkey (void)
{
   if (check_tty ())
     return 0;
   
   return SLang_getkey ();
}

static void ungetkey (int *c)
{
   if (check_tty ())
     return;

   SLang_ungetkey (*c);
}

static void set_input_string (void)
{
   char *s;
   if (-1 == SLpop_string (&s)) s = NULL;
   slrn_set_input_string (s);
}

static void set_input_chars (void)
{
   char *s;
   if (-1 == SLpop_string (&s)) s = NULL;
   slrn_set_input_chars (s);
}

/*}}}*/

/*{{{ Article Mode Functions */

static int check_article_mode (void) /*{{{*/
{
   if ((Slrn_Current_Mode == NULL)
       || (Slrn_Current_Mode->mode != SLRN_ARTICLE_MODE))
     {
	error (_("Not in article mode."));
	return -1;
     }
   return 0;
}

/*}}}*/

static int is_article_mode (void)
{
   return ((Slrn_Current_Mode != NULL)
	   && (Slrn_Current_Mode->mode == SLRN_ARTICLE_MODE));
}

static void set_article_window_size (int *nrows)
{
   slrn_set_article_window_size (*nrows);
}

static int get_article_window_size (void)
{
   return slrn_get_article_window_size ();
}

/*{{{ Article Mode Article Functions */

static void pipe_article_cmd (char *cmd) /*{{{*/
{
   if (-1 == check_article_mode ())
     return;
   
   (void) slrn_pipe_article_to_cmd (cmd);
}

/*}}}*/

static int save_current_article (char *file) /*{{{*/
{
   if (-1 == check_article_mode ())
     return -1;
   
   return slrn_save_current_article (file);
}

/*}}}*/

static int generic_search_article (char *str, int is_regexp, /*{{{*/
				   int dir)
{
   Slrn_Article_Line_Type *l;
   char *ptr;
   
   if (-1 == check_article_mode ())
     return 0;
   
   l = slrn_search_article (str, &ptr, is_regexp, 1, dir);
   if (l == NULL)
     return 0;
   
   (void) SLang_push_string (l->buf);
   return 1;
}

/*}}}*/

static int bsearch_article (char *s) /*{{{*/
{
   return generic_search_article (s, 0, 0);
}
/*}}}*/

static int search_article (char *s) /*{{{*/
{
   return generic_search_article (s, 0, 1);
}
/*}}}*/

static int search_article_first (char *s) /*{{{*/
{
   return generic_search_article (s, 0, 2);
}
/*}}}*/

static int re_bsearch_article (char *s) /*{{{*/
{
   return generic_search_article (s, 1, 0);
}
/*}}}*/

static int re_search_article (char *s) /*{{{*/
{
   return generic_search_article (s, 1, 1);
}
/*}}}*/

static int re_search_article_first (char *s) /*{{{*/
{
   return generic_search_article (s, 1, 2);
}
/*}}}*/

static char *extract_article_header (char *h) /*{{{*/
{
   unsigned int len;
   char buf[128];
   
   if (-1 == check_article_mode ())
     return NULL;

   len = strlen (h);
   if ((len + 3) <= sizeof (buf))
     {
	slrn_snprintf (buf, sizeof (buf), "%s: ", h);
	h = slrn_cur_extract_header (buf, len + 2);
     }
   else h = NULL;
   
   if (h == NULL) h = "";

   return h;
}

/*}}}*/

static char *extract_current_article_header (char *h) /*{{{*/
{
   unsigned int len;
   char buf[128];
   
   if (-1 == check_article_mode ())
     return NULL;
   if (NULL == Slrn_Current_Article)
     {
	slrn_error (_("No article selected"));
	return NULL;
     }

   len = strlen (h);
   if ((len + 3) <= sizeof (buf))
     {
	slrn_snprintf (buf, sizeof (buf), "%s: ", h);
	h = slrn_extract_header (buf, len + 2);
     }
   else h = NULL;

   if (h == NULL) h = "";

   return h;
}

/*}}}*/

static char *generic_article_as_string (int use_orig) /*{{{*/
{
   unsigned int len;
   Slrn_Article_Line_Type *l;
   char *s, *s1;
   
   if (Slrn_Current_Article == NULL)
     return "";

   l = use_orig ? Slrn_Current_Article->raw_lines : Slrn_Current_Article->lines;

   len = 0;
   while (l != NULL)
     {
	char *buf;
	Slrn_Article_Line_Type *next = l->next;
	
	buf = l->buf;
	if (l->flags & WRAPPED_LINE) buf++;   /* skip space */
	
	len += strlen (buf);
	
	if ((next == NULL) || (0 == (next->flags & WRAPPED_LINE)))
	  len++;
	l = next;
     }
   
   if (NULL == (s = (char *) SLMALLOC (len + 1)))
     {
	SLang_Error = SL_MALLOC_ERROR;
	return NULL;
     }
   
   l = use_orig ? Slrn_Current_Article->raw_lines : Slrn_Current_Article->lines;
   s1 = s;
   
   while (l != NULL)
     {
	char *buf;
	Slrn_Article_Line_Type *next = l->next;
	
	buf = l->buf;
	if (l->flags & WRAPPED_LINE) buf++;   /* skip space */
	
	while (*buf != 0) *s1++ = *buf++;
	       
	if ((next == NULL) || (0 == (next->flags & WRAPPED_LINE)))
	  *s1++ = '\n';
	l = next;
     }
   *s1 = 0;
   return s;
}
/*}}}*/

static char *article_as_string (void) /*{{{*/
{
   return generic_article_as_string (0);
}
/*}}}*/

static char *raw_article_as_string (void) /*{{{*/
{
   return generic_article_as_string (1);
}
/*}}}*/

static void sort_articles (void) /*{{{*/
{
   if (-1 == check_article_mode ())
     return;
   
   slrn_sort_headers ();
}

/*}}}*/

static char *article_cline_as_string (void) /*{{{*/
{
   if (-1 == check_article_mode () ||
       (Slrn_Current_Article == NULL))
     return "";
   return Slrn_Current_Article->cline->buf;
}
/*}}}*/

static int article_cline_number (void) /*{{{*/
{
   if (-1 == check_article_mode ()) return 0;
   return (int)slrn_art_cline_num();
}
/*}}}*/

static int article_count_lines (void) /*{{{*/
{
   if (-1 == check_article_mode ()) return 0;
   return (int)slrn_art_count_lines();
}
/*}}}*/

static int article_goto_line (int *num) /*{{{*/
{
   int offset;
   if ((-1 == check_article_mode ())
       || (*num <= 0))
     return 0;
   if (0 != (offset = *num - slrn_art_cline_num()))
     {
	if (offset > 0) slrn_art_linedn_n ((unsigned int)offset);
	else slrn_art_lineup_n ((unsigned int)-offset);
     }
   return (int)slrn_art_cline_num();
}
/*}}}*/

static int article_line_up (int *num) /*{{{*/
{
   if ((-1 == check_article_mode ())
       || (*num <= 0))
     return 0;
   
   return slrn_art_lineup_n ((unsigned int)*num);
}
/*}}}*/

static int article_line_down (int *num) /*{{{*/
{
   if ((-1 == check_article_mode ())
       || (*num <= 0))
     return 0;
   
   return slrn_art_linedn_n ((unsigned int)*num);
}
/*}}}*/

/*}}}*/

/*{{{ Article Mode Header Functions */

/*{{{ Header Flag Variables */

static int Interp_Header_Read = HEADER_READ;
static int Interp_Header_Tagged = HEADER_TAGGED;
static int Interp_Header_High_Score = HEADER_HIGH_SCORE;
static int Interp_Header_Low_Score = HEADER_LOW_SCORE;
static int Interp_Attr_Blink = SLTT_BLINK_MASK;
static int Interp_Attr_Bold = SLTT_BOLD_MASK;
static int Interp_Attr_Rev = SLTT_REV_MASK;
static int Interp_Attr_Uline = SLTT_ULINE_MASK;
#ifdef HAVE_SETLOCALE
# ifdef LC_TIME
static int Interp_Lc_Time = LC_TIME;
# endif
# ifdef LC_CTYPE
static int Interp_Lc_Ctype = LC_CTYPE;
# endif
#endif

/*}}}*/

static void uncollapse_threads (void) /*{{{*/
{
   if (0 == check_article_mode ())
     slrn_uncollapse_threads (1);
}

/*}}}*/

static void collapse_threads (void) /*{{{*/
{
   if (0 == check_article_mode ())
     slrn_collapse_threads (1);
}

/*}}}*/

static void collapse_thread (void)
{
   if (0 == check_article_mode ())
     slrn_collapse_this_thread (Slrn_Current_Header, 1);
}

static void uncollapse_thread (void)
{
   if (0 == check_article_mode ())
     slrn_uncollapse_this_thread (Slrn_Current_Header, 1);
}

static int thread_size (void)
{
   if (check_article_mode ()) return -1;
   return (int) slrn_thread_size (Slrn_Current_Header);
}

static int has_parent (void)
{
   if (check_article_mode ()) return -1;
   return (Slrn_Current_Header->parent == NULL) ? 0 : 1;
}

static int is_thread_collapsed (void)
{
   if (check_article_mode ()) return -1;
   return slrn_is_thread_collapsed (Slrn_Current_Header);
}

static int header_cursor_pos (void) /*{{{*/
{
   if (check_article_mode ()) return -1;
   return slrn_header_cursor_pos ();
}
/*}}}*/

static int header_down (int *num) /*{{{*/
{
   if ((-1 == check_article_mode ())
       || (*num <= 0))
     return 0;
   
   return slrn_header_down_n (*num, 0);
}

/*}}}*/

static int header_up (int *num) /*{{{*/
{
   if ((-1 == check_article_mode ())
       || (*num <= 0))
     return 0;
   
   return slrn_header_up_n (*num, 0);
}

/*}}}*/

static int header_next_unread (void)
{
   if (-1 == check_article_mode ())
     return -1;
   
   return slrn_next_unread_header (1);
}

static int get_header_flags (void) /*{{{*/
{
   if ((-1 == check_article_mode ())
       || (Slrn_Current_Header == NULL))
     return 0;
   return (int) (Slrn_Current_Header->flags & HEADER_HARMLESS_FLAGS_MASK);
}

/*}}}*/

static void set_header_flags (int *flagsp) /*{{{*/
{
   if ((-1 == check_article_mode ())
       || (Slrn_Current_Header == NULL))
     return;
   
   slrn_set_header_flags (Slrn_Current_Header, (unsigned int) *flagsp);
}

/*}}}*/

static int get_body_status (void) /*{{{*/
{
   if ((-1 == check_article_mode ())
       || (Slrn_Current_Header == NULL))
     return -1;
   if (Slrn_Current_Header->flags & HEADER_REQUEST_BODY)
     return 2;
   return (Slrn_Current_Header->flags & HEADER_WITHOUT_BODY) ? 1 : 0;
}
/*}}}*/

static void request_body (int *mode) /*{{{*/
{
   if ((-1 == check_article_mode ())
       || (Slrn_Current_Header == NULL))
     return;
   if (*mode)
     slrn_request_header (Slrn_Current_Header);
   else
     slrn_unrequest_header (Slrn_Current_Header);
}
/*}}}*/

static int get_header_number (void)
{
   if ((-1 == check_article_mode ())
       || (Slrn_Current_Header == NULL))
     return 0;
   return Slrn_Current_Header->number;
}

static int get_header_tag_number (void)
{
   if ((Slrn_Current_Header == NULL) 
       || (0 == (Slrn_Current_Header->flags & HEADER_NTAGGED)))
     return 0;
   
   return (int) Slrn_Current_Header->tag_number;
}

static void set_header_score (int *score)
{
   (void) slrn_set_header_score (Slrn_Current_Header, *score, 0, NULL);
}

static int get_header_score (void)
{
   if ((-1 == check_article_mode ())
       || (Slrn_Current_Header == NULL))
     return 0;

   return Slrn_Current_Header->score;
}

static int get_grouplens_score (void)
{
   if ((-1 == check_article_mode ())
       || (Slrn_Current_Header == NULL))
     return 0;

#if SLRN_HAS_GROUPLENS
   return Slrn_Current_Header->gl_pred;
#else
   return 0;
#endif
}

/*{{{ Header Searching */

static int re_header_search (char *pat, unsigned int offset, int dir) /*{{{*/
{
   SLRegexp_Type *re;
   Slrn_Header_Type *h = Slrn_Current_Header;
   
   if ((-1 == check_article_mode ())
       || (h == NULL)
       || (NULL == (re = slrn_compile_regexp_pattern (pat))))
     return 0;
   
   while (h != NULL)
     {
	if (NULL != slrn_regexp_match (re, *(char **) ((char *)h + offset)))
	  {
	     slrn_goto_header (h, 0);
	     return 1;
	  }
	
	if (dir > 0)
	  h = h->next;
	else
	  h = h->prev;
     }
   return 0;   
}

/*}}}*/

static int re_subject_search_forward (char *pat)
{
   Slrn_Header_Type h;
   return re_header_search (pat, (char *) &h.subject - (char *)&h, 1);
}


static int re_subject_search_backward (char *pat)
{
   Slrn_Header_Type h;
   return re_header_search (pat, (char *) &h.subject - (char *)&h, -1);
}

static int re_author_search_forward (char *pat)
{
   Slrn_Header_Type h;
   return re_header_search (pat, (char *) &h.from - (char *)&h, 1);
}

static int re_author_search_backward (char *pat)
{
   Slrn_Header_Type h;
   return re_header_search (pat, (char *) &h.from - (char *)&h, -1);
}
/*}}}*/

/*}}}*/

/*}}}*/

/*{{{ Group Functions */

static int Interp_Group_Unsubscribed = GROUP_UNSUBSCRIBED;
static int Interp_Group_New_Group_Flag = GROUP_NEW_GROUP_FLAG;

static int check_group_mode (void)
{
   if ((Slrn_Current_Mode == NULL)
       || (Slrn_Current_Mode->mode != SLRN_GROUP_MODE))
     {
	error (_("Not in group mode."));
	return -1;
     }
   return 0;
}

static int is_group_mode (void)
{
   return ((Slrn_Current_Mode != NULL)
	   && (Slrn_Current_Mode->mode == SLRN_GROUP_MODE));
}

static int group_down_n (int *np)
{
   int n = *np;
   
   if (-1 == check_group_mode ())
     return 0;
   
   if (n < 0) return slrn_group_up_n (-n);
   return slrn_group_down_n (n);
}

static int group_up_n (int *np)
{
   int n = *np;
   
   if (-1 == check_group_mode ())
     return 0;
   
   if (n < 0) return slrn_group_down_n (-n);
   return slrn_group_up_n (n);
}

static int group_search (char *str)
{
   if (-1 == check_group_mode ())
     return 0;
   return slrn_group_search (str, 1);
}

static char *current_group_name (void)
{
   if (Slrn_Group_Current_Group == NULL)
     return "";
   
   return Slrn_Group_Current_Group->group_name;
}

static int get_group_unread_count (void)
{
   if (is_article_mode ())
     return slrn_art_get_unread ();
   if (Slrn_Group_Current_Group == NULL) return 0;
   return Slrn_Group_Current_Group->unread;
}

static int select_group (void)
{
   if (-1 == check_group_mode ())
     return -1;
   
   return slrn_group_select_group ();
}

static int get_group_flags (void)
{	
   if (Slrn_Group_Current_Group == NULL)
     return 0;
   
   return Slrn_Group_Current_Group->flags & GROUP_HARMLESS_FLAGS_MASK;
}

static void set_group_flags (int *flagsp)
{
   unsigned int flags;
   
   if (Slrn_Group_Current_Group == NULL)
     return;
   
   Slrn_Group_Current_Group->flags &= ~GROUP_HARMLESS_FLAGS_MASK;
   flags = ((unsigned int) *flagsp) & GROUP_HARMLESS_FLAGS_MASK;
   Slrn_Group_Current_Group->flags |= flags;
}

static void hide_current_group (void)
{
   if (0 == check_group_mode ())
     slrn_hide_current_group ();
}

/*}}}*/

static void set_color (char *obj, char *fg, char *bg)
{
   slrn_set_object_color (obj, fg, bg, 0);
}

static void set_color_attr (char *obj, char *fg, char *bg, int *attr)
{
   slrn_set_object_color (obj, fg, bg, (SLtt_Char_Type) *attr);
}

static char *get_fg_color (char *name)
{
   return slrn_get_object_color (name, 0);
}

static char *get_bg_color (char *name)
{
   return slrn_get_object_color (name, 1);
}

static FILE *Log_File_Ptr;
static void close_log_file (void)
{	
   if (Log_File_Ptr != NULL)
     {
	(void) fclose (Log_File_Ptr);
	Log_File_Ptr = NULL;
     }
}

static void open_log_file (char *file)
{
   close_log_file ();

   Log_File_Ptr = fopen (file, "w");
   if (Log_File_Ptr == NULL)
     slrn_error (_("Unable to open log file %s"), file);
}

static void log_message (char *buf)
{
   FILE *fp;
   
   fp = Log_File_Ptr;
   if (fp == NULL) fp = stderr;
   
   fputs (buf, fp);
   fflush (fp);
}

static void message_now (char *s)
{
   slrn_message_now ("%s", s);
}

static void set_visible_headers (char *h)
{
   (void) slrn_set_visible_headers (h);
}

static char *get_visible_headers (void)
{
   if (Slrn_Visible_Headers_String == NULL)
     return "";
   return Slrn_Visible_Headers_String;
}

static void set_header_display_format (int *num, char *fmt)
{
   (void) slrn_set_header_format (*num, fmt);
}

static void set_group_display_format (int *num, char *fmt)
{
   (void) slrn_set_group_format (*num, fmt);
}

static void print_file (char *file)
{
   (void) slrn_print_file (file);
}

static int locate_header_by_msgid (char *msgid, int *qs)
{
   if (-1 == slrn_locate_header_by_msgid (msgid, 1, *qs))
     return 0;
   return 1;
}

static void replace_article_cmd (char *str)
{
   if ((-1 == check_article_mode ())
       || (Slrn_Current_Header == NULL))
     return;

   if (-1 == slrn_string_to_article (str))
     slrn_error (_("Could not replace article with given string."));
}

static int is_article_visible (void)
{
   int x = slrn_is_article_visible ();
   if ((x & 1) == 0)
     return 0;
   return x;
}

static void reload_scorefile (int *apply_now) /*{{{*/
{
   char file[SLRN_MAX_PATH_LEN];
   
   if (Slrn_Score_File == NULL)
     return;
   
   slrn_make_home_filename (Slrn_Score_File, file, sizeof (file));

   if (Slrn_Scorefile_Open != NULL)
     slrn_free (Slrn_Scorefile_Open);
   Slrn_Scorefile_Open = slrn_safe_strmalloc (file);
   
   if (is_article_mode() && *apply_now)
     slrn_apply_scores (*apply_now);
}

/*}}}*/

static SLang_Intrin_Fun_Type Slrn_Intrinsics [] = /*{{{*/
{
   MAKE_INTRINSIC_0("headers_hidden_mode", slrn_is_hidden_headers_mode, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("replace_article", replace_article_cmd, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_S("message_now", message_now, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_S("set_visible_headers", set_visible_headers, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("get_visible_headers", get_visible_headers, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_S("print_file", print_file, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_S("open_log_file", open_log_file, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("close_log_file", close_log_file, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_S("log_message", log_message, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_IS("set_header_display_format", set_header_display_format, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_IS("set_group_display_format", set_group_display_format, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("get_prefix_arg", get_prefix_arg, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("reset_prefix_arg", reset_prefix_arg, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_SI("locate_header_by_msgid", locate_header_by_msgid, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("article_as_string", article_as_string, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_0("article_cline_as_string", article_cline_as_string, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_0("article_cline_number", article_cline_number, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("article_count_lines", article_count_lines, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("article_goto_line", article_goto_line, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("article_line_down", article_line_down, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("article_line_up", article_line_up, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("bsearch_article", bsearch_article, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("call", slrn_call_command, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("collapse_thread", collapse_thread, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("collapse_threads", collapse_threads, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("current_newsgroup", current_group_name, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_S("datestring_to_unixtime", slrn_date_to_order_parm, SLANG_INT_TYPE),
   MAKE_INTRINSIC_SSS("definekey", definekey, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_S("extract_article_header", extract_article_header, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_S("extract_displayed_article_header", extract_current_article_header, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_S("get_bg_color", get_bg_color, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_0("get_body_status", get_body_status, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("get_fg_color", get_fg_color, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_0("get_group_flags", get_group_flags, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("get_grouplens_score", get_grouplens_score, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("get_header_flags", get_header_flags, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("get_header_number", get_header_number, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("get_header_score", get_header_score, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("get_header_tag_number", get_header_tag_number, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("get_next_art_pgdn_action", slrn_get_next_pagedn_action, SLANG_INT_TYPE),
   MAKE_INTRINSIC_SS("get_response", get_response, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("get_select_box_response", interp_select_box, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("get_variable_value", get_variable_value, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_S("get_yes_no_cancel", get_yesno_cancel, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("getkey", getkey, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("goto_num_tagged_header", slrn_goto_num_tagged_header, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("group_down_n", group_down_n, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("group_search", group_search, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("group_unread", get_group_unread_count, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("group_up_n", group_up_n, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("has_parent", has_parent, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("header_cursor_pos", header_cursor_pos, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("header_down", header_down, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("header_next_unread", header_next_unread, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("header_up", header_up, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("input_pending", input_pending, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("_is_article_visible", slrn_is_article_visible, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("is_article_visible", is_article_visible, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("is_article_window_zoomed", slrn_is_article_win_zoomed, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("is_group_mode", is_group_mode, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("is_thread_collapsed", is_thread_collapsed, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("make_home_filename", make_home_filename, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("next_tagged_header", slrn_next_tagged_header, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("pipe_article", pipe_article_cmd, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("popup_window", popup_window, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("prev_tagged_header", slrn_prev_tagged_header, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("quit", quit, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("raw_article_as_string", raw_article_as_string, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_S("re_bsearch_article", re_bsearch_article, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("re_bsearch_author", re_author_search_backward, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("re_bsearch_subject", re_subject_search_backward, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("re_fsearch_author", re_author_search_forward, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("re_fsearch_subject", re_subject_search_forward, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("re_search_article", re_search_article, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("re_search_article_first", re_search_article_first, SLANG_INT_TYPE),
   MAKE_INTRINSIC_SSS("read_mini", read_mini, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_SSS("read_mini_no_echo", read_mini_no_echo, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_SSS("read_mini_filename", read_mini_filename, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_SSS("read_mini_variable", read_mini_variable, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_SI("read_mini_integer", read_mini_integer, SLANG_INT_TYPE),
   MAKE_INTRINSIC_SS("register_hook", slrn_register_hook_by_name, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("reload_scorefile", reload_scorefile, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_I("request_body", request_body, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_S("save_current_article", save_current_article, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("search_article", search_article, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("search_article_first", search_article_first, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("select_group", select_group, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("select_list_box", select_list_box, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("server_name", get_server_name, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_0("get_article_window_size", get_article_window_size, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("set_article_window_size", set_article_window_size, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_SSS("set_color", set_color, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_4("set_color_attr", set_color_attr, SLANG_VOID_TYPE,
		    SLANG_STRING_TYPE, SLANG_STRING_TYPE, SLANG_STRING_TYPE, SLANG_INT_TYPE),
   MAKE_INTRINSIC_I("set_group_flags", set_group_flags, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("hide_current_group", hide_current_group, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_I("set_header_flags", set_header_flags, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_I("set_header_score", set_header_score, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("set_ignore_quotes", set_ignore_quotes, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("set_input_chars", set_input_chars, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("set_input_string", set_input_string, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_SI("set_integer_variable", set_integer_variable, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_I("set_prefix_argument", set_prefix_arg, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_SS("set_string_variable", set_string_variable, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("set_strip_re_regexp", set_strip_re_regexp, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("set_strip_sig_regexp", set_strip_sig_regexp, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("set_strip_was_regexp", set_strip_was_regexp, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("set_utf8_conversion_table", set_utf8_conversion_table, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("get_group_order", slrn_intr_get_group_order, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("set_group_order", slrn_intr_set_group_order, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_IS("setlocale", interp_setlocale, SLANG_STRING_TYPE),
   MAKE_INTRINSIC_0("sort_by_sorting_method", sort_articles, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("thread_size", thread_size, SLANG_INT_TYPE),
   MAKE_INTRINSIC_S("tt_send", tt_send, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("uncollapse_thread", uncollapse_thread, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0("uncollapse_threads", uncollapse_threads, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_SS("undefinekey", undefinekey, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_I("ungetkey", ungetkey, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_SS("unregister_hook", slrn_unregister_hook_by_name, SLANG_INT_TYPE),
   MAKE_INTRINSIC_0("update", update, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_0(NULL, NULL, 0)
};

/*}}}*/

static SLang_Intrin_Var_Type Intrin_Vars [] = /*{{{*/
{
   MAKE_VARIABLE("ATTR_BLINK", &Interp_Attr_Blink, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("ATTR_BOLD", &Interp_Attr_Bold, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("ATTR_REV", &Interp_Attr_Rev, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("ATTR_ULINE", &Interp_Attr_Uline, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("GROUPS_DIRTY", &Slrn_Groups_Dirty, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("GROUP_NEW_GROUP_FLAG", &Interp_Group_New_Group_Flag, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("GROUP_UNSUBSCRIBED", &Interp_Group_Unsubscribed, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("HEADER_HIGH_SCORE", &Interp_Header_High_Score, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("HEADER_LOW_SCORE", &Interp_Header_Low_Score, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("HEADER_READ", &Interp_Header_Read, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("HEADER_TAGGED", &Interp_Header_Tagged, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("SCREEN_HEIGHT", &SLtt_Screen_Rows, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("SCREEN_WIDTH", &SLtt_Screen_Cols, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("_slrn_version", &Slrn_Version_Number, SLANG_INT_TYPE, 1),
   MAKE_VARIABLE("_slrn_version_string", &Slrn_Version, SLANG_STRING_TYPE, 1),
#ifdef HAVE_SETLOCALE
# ifdef LC_CTYPE
   MAKE_VARIABLE("LC_CTYPE", &Interp_Lc_Ctype, SLANG_INT_TYPE, 1),
# endif
# ifdef LC_TIME
   MAKE_VARIABLE("LC_TIME", &Interp_Lc_Time, SLANG_INT_TYPE, 1),
# endif
#endif

   MAKE_VARIABLE(NULL, NULL, 0, 0)
};

/*}}}*/

static int interp_system (char *s) /*{{{*/
{
   return slrn_posix_system (s, 1);
}

/*}}}*/

int slrn_init_slang (void) /*{{{*/
{
   Slrn_Use_Slang = 0;
   if ((-1 == SLang_init_slang ())
       || (-1 == SLang_init_slmath ())
       || (-1 == SLang_init_posix_process ())
       || (-1 == SLang_init_posix_dir ())
       || (-1 == SLang_init_stdio ())
       || (-1 == SLang_init_posix_io ())
       || (-1 == SLang_init_ospath ())
       || (-1 == SLang_init_slassoc ())
#if SLANG_VERSION >= 10400
       || (-1 == SLang_init_import ()) /* enable dynamic linking */
#endif
       
       /* Now add intrinsics for this application */
       || (-1 == SLadd_intrin_fun_table(Slrn_Intrinsics, NULL))
       || (-1 == SLadd_intrin_var_table(Intrin_Vars, NULL)))
     return -1;
   
   SLadd_intrinsic_function ("system", (FVOID_STAR) interp_system, SLANG_INT_TYPE, 1, SLANG_STRING_TYPE);
   SLadd_intrinsic_function ("evalfile", (FVOID_STAR) evalfile, SLANG_INT_TYPE, 1, SLANG_STRING_TYPE);

   SLang_Error_Hook = error;

   Slrn_Use_Slang = 1;
   SLang_User_Clear_Error = slrn_clear_message;
   SLang_Exit_Error_Hook = slrn_va_exit_error;
   SLang_Dump_Routine = log_message;
   SLang_VMessage_Hook = slrn_va_message;
   return 0;
}

/*}}}*/

int slrn_reset_slang (void)
{
   close_log_file ();
   return 0;
}

#endif
