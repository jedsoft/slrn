/* -*- mode: C; mode: fold; -*- */
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
/* Read startup .slrnrc file */

#include "config.h"
#include "slrnfeat.h"

/*{{{ Include Files */

#include <stdio.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <string.h>
#include <slang.h>

#include "jdmacros.h"

#include "slrn.h"
#include "group.h"
#include "misc.h"
#include "art.h"
#include "post.h"
#include "startup.h"
#include "score.h"
#include "util.h"
#include "decode.h"
#include "mime.h"
#if SLRN_HAS_GROUPLENS
# include "grplens.h"
#endif
#if SLRN_HAS_SLANG
# include "interp.h"
#endif
#include "server.h"
#include "chmap.h"
#include "print.h"
#include "snprintf.h"

#ifdef VMS
# include "vms.h"
#endif
/*}}}*/

/*{{{ Forward Function Declarations */

static int unsetkey_fun (int, SLcmd_Cmd_Table_Type *);
static int setkey_fun (int, SLcmd_Cmd_Table_Type *);
static int server_fun (int, SLcmd_Cmd_Table_Type *);
static int color_fun (int, SLcmd_Cmd_Table_Type *);
static int mono_fun (int, SLcmd_Cmd_Table_Type *);
static int user_data_fun (int, SLcmd_Cmd_Table_Type *);
static int ignore_quote_fun (int, SLcmd_Cmd_Table_Type *);
static int strip_re_fun (int, SLcmd_Cmd_Table_Type *);
static int strip_sig_fun (int, SLcmd_Cmd_Table_Type *);
static int strip_was_fun (int, SLcmd_Cmd_Table_Type *);
static int autobaud_fun (int, SLcmd_Cmd_Table_Type *);
static int set_variable_fun (int, SLcmd_Cmd_Table_Type *);
static int nnrp_fun (int, SLcmd_Cmd_Table_Type *);
static int grouplens_fun (int, SLcmd_Cmd_Table_Type *);
static int interpret_fun (int, SLcmd_Cmd_Table_Type *);
static int include_file_fun (int, SLcmd_Cmd_Table_Type *);
static int set_header_format_fun (int, SLcmd_Cmd_Table_Type *);
static int set_group_format_fun (int, SLcmd_Cmd_Table_Type *);
static int set_visible_headers_fun (int, SLcmd_Cmd_Table_Type *);
static int set_comp_charsets_fun (int, SLcmd_Cmd_Table_Type *);
static int set_posting_host (int, SLcmd_Cmd_Table_Type *);

/*}}}*/
/*{{{ Static Global Variables */

static int This_Line_Num;	       /* current line number in startup file */
static char *This_File;
static char *This_Line;		       /* line being parsed */

char *Server_Object;
char *Post_Object;

static SLcmd_Cmd_Table_Type Slrn_Cmd_Table;
static SLcmd_Cmd_Type Slrn_Startup_File_Cmds[] = /*{{{*/
{
     {unsetkey_fun, "unsetkey", "SS"},
     {setkey_fun, "setkey", "SSS"},
     {server_fun, "server", "SS"},
     {color_fun, "color", "SSSssss"},
     {mono_fun, "mono", "SSsss"},
     {set_variable_fun, "set", "SG"},
     {nnrp_fun, "nnrpaccess", "SSS" },
     {ignore_quote_fun, "ignore_quotes", "Sssss"},
     {strip_re_fun, "strip_re_regexp", "Sssss"},
     {strip_sig_fun, "strip_sig_regexp", "Sssss"},
     {strip_was_fun, "strip_was_regexp", "Sssss"},
     {autobaud_fun, "autobaud", ""},
     {grouplens_fun, "grouplens_add", "S"},
     {interpret_fun, "interpret", "S"},
     {include_file_fun, "include", "S"},
     {set_header_format_fun, "header_display_format", "IS"},
     {set_group_format_fun, "group_display_format", "IS"},
     {set_visible_headers_fun, "visible_headers", "S"},
     {set_comp_charsets_fun, "compatible_charsets", "S"},
     {set_posting_host, "posting_host", "S"},

   /* The following are considered obsolete */
     {user_data_fun, "hostname", "S"},
     {user_data_fun, "username", "S"},
     {user_data_fun, "replyto", "S"},
     {user_data_fun, "organization", "S"},
     {user_data_fun, "scorefile", "S"},
     {user_data_fun, "signature", "S"},
     {user_data_fun, "realname", "S"},
     {user_data_fun, "followup", "S"},
     {user_data_fun, "cc_followup_string", "S"},
     {user_data_fun, "quote_string", "S"},
#if SLRN_HAS_DECODE
     {user_data_fun, "decode_directory", "S"},
#endif
     {user_data_fun, "editor_command", "S"},
     {NULL, "", ""}
};

/*}}}*/

/*}}}*/
/*{{{ Public Global Variables */

SLRegexp_Type *Slrn_Ignore_Quote_Regexp [SLRN_MAX_REGEXP + 1];
SLRegexp_Type *Slrn_Strip_Re_Regexp [SLRN_MAX_REGEXP + 1];
SLRegexp_Type *Slrn_Strip_Sig_Regexp [SLRN_MAX_REGEXP + 1];
SLRegexp_Type *Slrn_Strip_Was_Regexp [SLRN_MAX_REGEXP + 1];
int Slrn_Autobaud = 0;
char *Slrn_Score_File;
int Slrn_Scroll_By_Page;
int Slrn_Saw_Obsolete = 0;

/*}}}*/

/*{{{ Utility Functions */

static void exit_malloc_error (void) /*{{{*/
{
   if (This_File == NULL)
     slrn_exit_error (_("Memory Allocation Failure"));
   
   slrn_exit_error (_("%s: Line %d\n%sMemory Allocation Failure"),
		    This_File, This_Line_Num, This_Line);
}

/*}}}*/

static char *safe_malloc (unsigned int n) /*{{{*/
{
   char *s;
   s = (char *) SLMALLOC (n);
   if (s == NULL) exit_malloc_error ();
   return s;
}

/*}}}*/


static void exit_unknown_object (void) /*{{{*/
{
   slrn_exit_error (_("%s: Error encountered processing line %d\n%s"),
		    This_File, This_Line_Num, This_Line);
}

/*}}}*/

static void issue_obsolete_message (void) /*{{{*/
{
   Slrn_Saw_Obsolete = 1;
   slrn_message (_("%s: Command is obsolete on line %d:\n%s"),
		 This_File, This_Line_Num, This_Line);
   slrn_message (_("The new usage is:\nset %s\n"), This_Line);
}

/*}}}*/

   
/*}}}*/
/*{{{ Set/Unset Key Functions */

static char *Group_Obsolete_Names [] =
{
   "down", "line_down",
     "group_bob", "bob",
     "group_eob", "eob",
     "pagedown", "page_down",
     "pageup", "page_up",
     "toggle_group_display", "toggle_group_formats",
     "uncatch_up", "uncatchup",
     "up", "line_up",
     NULL
};

static char *Art_Obsolete_Names [] =
{
   "art_bob", "article_bob",
     "art_eob", "article_eob",
     "art_xpunge", "article_expunge",
     "article_linedn", "article_line_down",
     "article_lineup", "article_line_up",
     "article_pagedn", "article_page_down",
     "article_pageup", "article_page_up",
     "down", "header_line_down",
     "enlarge_window", "enlarge_article_window",
     "goto_beginning", "article_bob",
     "goto_end", "article_eob",
     "left", "article_left",
     "locate_header_by_msgid", "locate_article",
     "pagedn", "header_page_down",
     "pageup", "header_page_up",
     "pipe_article", "pipe",
     "prev", "previous",
     "print_article", "print",
     "right", "article_right",
     "scroll_dn", "article_page_down",
     "scroll_up", "article_page_up",
     "shrink_window", "shrink_article_window",
     "skip_to_prev_group", "skip_to_previous_group",
     "toggle_show_author", "toggle_header_formats",
     "up", "header_line_up",
     NULL
};

static int setkey_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
   char *map = table->string_args[1];
   char *fun = table->string_args[2];
   char *key = table->string_args[3];
   SLKeyMap_List_Type *kmap = NULL;
   char **obsolete_names = NULL;
   int failure;
   
   (void) argc;

   if (!strcmp (map, "group"))
     {
	kmap = Slrn_Group_Keymap;
	obsolete_names = Group_Obsolete_Names;
     }
   else if (!strcmp (map, "article"))
     {
	kmap = Slrn_Article_Keymap;
	obsolete_names = Art_Obsolete_Names;
     }
   else if (!strcmp (map, "readline")) kmap = Slrn_RLine_Keymap;
   else slrn_exit_error (_("%s: line %d:\n%sNo such keymap: %s"), This_File, This_Line_Num, This_Line, map);
   
   if ((kmap == Slrn_Keymap_RLI->keymap) &&
       (NULL == SLang_find_key_function(fun, kmap)))
     {
	SLKeymap_Function_Type *tmp = kmap->functions;
	kmap->functions = Slrn_Custom_Readline_Functions;
	failure = SLang_define_key (key, fun, kmap);
	kmap->functions = tmp;
     }
   else
     failure = SLang_define_key (key, fun, kmap);
   
   if (failure)
     {
	slrn_exit_error (_("%s: line %d:\n%serror defining key."), This_File, This_Line_Num, This_Line);
     }
   else if (obsolete_names != NULL)
     {
	int i = 0;
	char *old_name = NULL, *new_name = NULL;
	
	while ((old_name = obsolete_names[i]) != NULL)
	  {
	     if (!strcmp (old_name, fun))
	       {
		  Slrn_Saw_Obsolete = 1;
		  new_name = obsolete_names[i+1];
		  break;
	       }
	     i += 2;
	  }
	
	if (new_name != NULL)
	  slrn_message (_("%s: Obsolete function name on line %d: %s\n"
			"The new name is: %s"),
			This_File, This_Line_Num, old_name, new_name);
     }

   return 0;
}

/*}}}*/

static int unsetkey_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
   char *map = table->string_args[1];
   char *key = table->string_args[2];
   SLKeyMap_List_Type *kmap = NULL;
   
   (void) argc;
   
   if (!strcmp (map, "group")) kmap = Slrn_Group_Keymap;
   else if (!strcmp (map, "article")) kmap = Slrn_Article_Keymap;
   else if (!strcmp (map, "readline")) kmap = Slrn_RLine_Keymap;
   else slrn_exit_error (_("%s: line %d:\n%sNo such keymap: %s"),
			 This_File, This_Line_Num, This_Line, map);
   
   SLang_undefine_key (key, kmap);
   return 0;
}

/*}}}*/

/*}}}*/

static int autobaud_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
   (void) argc; (void) table;
   Slrn_Autobaud = 1;
   return 0;
}

/*}}}*/

static SLRegexp_Type *compile_quote_regexp (char *str) /*{{{*/
{
   unsigned char *compiled_pattern_buf;
   SLRegexp_Type *r;
   
   compiled_pattern_buf = (unsigned char *) safe_malloc (512);
   r = (SLRegexp_Type *) safe_malloc (sizeof (SLRegexp_Type));
   
   r->pat = (unsigned char *) str;
   r->buf = compiled_pattern_buf;
   r->case_sensitive = 1;
   r->buf_len = 512;
   
   if (SLang_regexp_compile (r))
     {
	slrn_exit_error (_("%s: line %d:\n%sInvalid regular expression."),
			 This_File, This_Line_Num, This_Line);
     }
   
   return r;
}

/*}}}*/
void slrn_generic_regexp_fun (int argc, SLcmd_Cmd_Table_Type *cmd_table,
				     SLRegexp_Type **regexp_table) /*{{{*/
{
   unsigned int i;
   SLRegexp_Type *r;
   
   if (argc > SLRN_MAX_REGEXP + 1)
     {
	slrn_exit_error (_("%s: line %d:\n%sToo many expressions specified."),
			 This_File, This_Line_Num, This_Line);
     }
   
   for (i = 0; i < SLRN_MAX_REGEXP; i++)
     {
	r = regexp_table[i];
	if (r != NULL)
	  {
	     slrn_free ((char *) r->buf);
	     SLFREE (r);
	     regexp_table[i] = NULL;
	  }
     }
   
   for (i = 1; i < (unsigned int) argc; i++)
     {
	regexp_table[i-1] = compile_quote_regexp (cmd_table->string_args[i]);
     }
}

/*}}}*/

static int ignore_quote_fun (int argc, SLcmd_Cmd_Table_Type *table)
{
   slrn_generic_regexp_fun (argc, table, Slrn_Ignore_Quote_Regexp);
   return 0;
}

static int strip_re_fun (int argc, SLcmd_Cmd_Table_Type *table)
{
   slrn_generic_regexp_fun (argc, table, Slrn_Strip_Re_Regexp);
   return 0;
}

static int strip_sig_fun (int argc, SLcmd_Cmd_Table_Type *table)
{
   slrn_generic_regexp_fun (argc, table, Slrn_Strip_Sig_Regexp);
   return 0;
}

static int strip_was_fun (int argc, SLcmd_Cmd_Table_Type *table)
{
   slrn_generic_regexp_fun (argc, table, Slrn_Strip_Was_Regexp);
   return 0;
}

static int grouplens_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
   (void) argc;
#if SLRN_HAS_GROUPLENS
   (void) slrn_grouplens_add_group (table->string_args[1]);
#else 
   (void) table;
#endif
   return 0;
}

/*}}}*/


static int set_visible_headers_fun (int argc, SLcmd_Cmd_Table_Type *table) 
{
   (void) argc;
   return slrn_set_visible_headers (table->string_args[1]);
}

static int set_header_format_fun (int argc, SLcmd_Cmd_Table_Type *table) 
{
   (void) argc;
   return slrn_set_header_format (table->int_args[1], table->string_args[2]);
}

static int set_group_format_fun (int argc, SLcmd_Cmd_Table_Type *table) 
{
   (void) argc;
   return slrn_set_group_format (table->int_args[1], table->string_args[2]);
}

static int set_comp_charsets_fun (int argc, SLcmd_Cmd_Table_Type *table)
{
   (void) argc;
#if SLRN_HAS_MIME
   return slrn_set_compatible_charsets (table->string_args[1]);
#else
   (void) table;
   return -1;
#endif
}

/*{{{ Setting/Getting Variable Functions */

Slrn_Int_Var_Type Slrn_Int_Variables [] = /*{{{*/
{
     {"hide_verbatim_marks",	&Slrn_Verbatim_Marks_Hidden},
     {"hide_verbatim_text",	&Slrn_Verbatim_Hidden},
     {"hide_signature",	&Slrn_Signature_Hidden},
     {"hide_pgpsignature",	&Slrn_Pgp_Signature_Hidden},
     {"hide_quotes",            &Slrn_Quotes_Hidden_Mode},
     {"emphasized_text_mask",	&Slrn_Emphasized_Text_Mask},
     {"emphasized_text_mode",	&Slrn_Emphasized_Text_Mode},
     {"process_verbatim_marks", &Slrn_Process_Verbatim_Marks},

     {"use_flow_control",	&Slrn_Use_Flow_Control},
     {"abort_unmodified_edits", &Slrn_Abort_Unmodified},
     {"editor_uses_mime_charset", &Slrn_Editor_Uses_Mime_Charset},
     {"mail_editor_is_mua",	&Slrn_Mail_Editor_Is_Mua},
     {"auto_mark_article_as_read", &Slrn_Del_Article_Upon_Read},
     {"simulate_graphic_chars", &Slrn_Simulate_Graphic_Chars},
#if 0 /* This does not work yet */
     {"article_window_page_overlap", &Slrn_Article_Window_Border},
#endif
     {"new_subject_breaks_threads",	&Slrn_New_Subject_Breaks_Threads},
     {"scroll_by_page", &Slrn_Scroll_By_Page},
     {"use_color", &SLtt_Use_Ansi_Colors},
     {"ignore_signature", &Slrn_Sig_Is_End_Of_Article},
     {"reject_long_lines", &Slrn_Reject_Long_Lines},
     {"netiquette_warnings", &Slrn_Netiquette_Warnings},
     {"generate_date_header", &Slrn_Generate_Date_Header},
     {"generate_message_id", &Slrn_Generate_Message_Id},
     {"use_recommended_msg_id", &Slrn_Use_Recom_Id},
#if SLRN_HAS_STRICT_FROM
     {"generate_email_from", NULL},
#else
     {"generate_email_from", &Slrn_Generate_Email_From},
#endif
     {"display_cursor_bar", &Slrn_Display_Cursor_Bar},
#if SLRN_HAS_NNTP_SUPPORT
     {"broken_xref", &Slrn_Broken_Xref},
     {"force_authentication", &Slrn_Force_Authentication},
#else
     {"broken_xref", NULL},
     {"force_authentication", NULL},
#endif
     {"color_by_score", &Slrn_Color_By_Score},
     {"highlight_unread_subjects", &Slrn_Highlight_Unread},
     {"highlight_urls", &Slrn_Highlight_Urls},
     {"show_article", &Slrn_Startup_With_Article},
     {"smart_quote", &Slrn_Smart_Quote},
     {"no_backups", &Slrn_No_Backups},
     {"no_autosave", &Slrn_No_Autosave},
     {"beep", &SLtt_Ignore_Beep},
     {"unsubscribe_new_groups", &Slrn_Unsubscribe_New_Groups},
     {"check_new_groups", &Slrn_Check_New_Groups},
     {"show_thread_subject", &Slrn_Show_Thread_Subject},
     {"mouse", &Slrn_Use_Mouse},
     {"query_next_group", &Slrn_Query_Next_Group},
     {"query_next_article", &Slrn_Query_Next_Article},
     {"confirm_actions", &Slrn_User_Wants_Confirmation},
     {"cc_followup", &Slrn_Auto_CC_To_Poster},
     {"use_tmpdir", &Slrn_Use_Tmpdir},
     {"sorting_method", &Slrn_Sorting_Mode},
     {"custom_sort_by_threads", &Slrn_Sort_By_Threads},
     {"uncollapse_threads", &Slrn_Uncollapse_Threads},
     {"read_active", &Slrn_List_Active_File},
     {"drop_bogus_groups", &Slrn_Drop_Bogus_Groups},
     {"prefer_head", &Slrn_Prefer_Head},
#if SLRN_HAS_MIME
     {"use_metamail", &Slrn_Use_Meta_Mail},
#endif
#if SLRN_HAS_UUDEVIEW
     {"use_uudeview", &Slrn_Use_Uudeview},
#else
     {"use_uudeview", NULL},
#endif
     {"lines_per_update", &Slrn_Reads_Per_Update},
     {"min_high_score", &Slrn_High_Score_Min},
     {"max_low_score", &Slrn_Low_Score_Max},
     {"kill_score", &Slrn_Kill_Score_Max},
     {"followup_strip_signature", &Slrn_Followup_Strip_Sig},
#if !defined(IBMPC_SYSTEM)
     {"use_blink", &SLtt_Blink_Mode},
#endif
     {"warn_followup_to", &Slrn_Warn_Followup_To},
     {"wrap_flags", &Slrn_Wrap_Mode},
     {"wrap_method", &Slrn_Wrap_Method},
     {"write_newsrc_flags", &Slrn_Write_Newsrc_Flags},
     {"query_read_group_cutoff", &Slrn_Query_Group_Cutoff},
     {"max_queued_groups", &Slrn_Max_Queued_Groups},
     {"use_header_numbers", &Slrn_Use_Header_Numbers},
     {"use_localtime", &Slrn_Use_Localtime},
#if SLRN_HAS_SPOILERS
     {"spoiler_char", &Slrn_Spoiler_Char},
     {"spoiler_display_mode", &Slrn_Spoiler_Display_Mode},
#else
     {"spoiler_display_mode", NULL},
     {"spoiler_char", NULL},
#endif
#if SLRN_HAS_MIME
     {"use_mime", &Slrn_Use_Mime},
     {"fold_headers", &Slrn_Fold_Headers},
#else
     {"use_mime", NULL},
     {"fold_headers", NULL},
#endif
#if SLRN_HAS_GROUPLENS
     {"use_grouplens", &Slrn_Use_Group_Lens},
     {"grouplens_port", &Slrn_GroupLens_Port},
#else
     {"use_grouplens", NULL},
#endif
#if 0
#if SLRN_HAS_INEWS_SUPPORT
     {"use_inews", &Slrn_Use_Inews},
#else
     {"use_inews", NULL},
#endif
#endif
#if SLRN_HAS_PULL_SUPPORT
     {"use_slrnpull", &Slrn_Use_Pull_Post},
#else
     {"use_slrnpull", NULL},
#endif
     {"use_tilde", &Slrn_Use_Tildes},
#if SLRN_HAS_SPOOL_SUPPORT
     {"spool_check_up_on_nov", &Slrn_Spool_Check_Up_On_Nov},
#else
     {"spool_check_up_on_nov", NULL},
#endif
#if 1 /* FIXME: These will be removed before 1.0 */
     {"author_display", NULL},
     {"display_author_realname", NULL},
     {"display_score", NULL},
     {"group_dsc_start_column", NULL},
     {"process_verbatum_marks", &Slrn_Process_Verbatim_Marks},
     {"prompt_next_group", NULL},
     {"query_reconnect", NULL},
     {"show_descriptions", NULL},
     {"use_xgtitle", NULL},
#endif
     {NULL, NULL}
};

/*}}}*/

Slrn_Str_Var_Type Slrn_Str_Variables [] = /*{{{*/
{
     {"failed_posts_file", &Slrn_Failed_Post_Filename},
#if ! SLRN_HAS_STRICT_FROM
     {"hostname", &Slrn_User_Info.hostname},
     {"realname", &Slrn_User_Info.realname},
     {"username", &Slrn_User_Info.username},
#else
     {"hostname", NULL},
     {"realname", NULL},
     {"username", NULL},
#endif
#if SLRN_HAS_CANLOCK
     {"cansecret_file", &Slrn_User_Info.cancelsecret},
#else
     {"cansecret_file", NULL},
#endif
     {"art_help_line", &Slrn_Art_Help_Line},
     {"art_status_line", &Slrn_Art_Status_Line},
     {"header_help_line", &Slrn_Header_Help_Line},
     {"header_status_line", &Slrn_Header_Status_Line},
     {"group_help_line", &Slrn_Group_Help_Line},
     {"quote_string", &Slrn_Quote_String},
     {"replyto", &Slrn_User_Info.replyto},
     {"organization", &Slrn_User_Info.org},
     {"followup", &Slrn_User_Info.followup_string}, /* FIXME: obsolete */
     {"followup_string", &Slrn_User_Info.followup_string},
     {"followupto_string", &Slrn_User_Info.followupto_string},
     {"reply_string", &Slrn_User_Info.reply_string},
     {"cc_followup_string", NULL}, /* FIXME: obsolete */
     {"cc_post_string", &Slrn_CC_Post_Message},
     {"followup_date_format", &Slrn_Followup_Date_Format},
     {"overview_date_format", &Slrn_Overview_Date_Format},
     {"editor_command", &Slrn_Editor},
     {"post_editor_command", &Slrn_Editor_Post},
     {"score_editor_command", &Slrn_Editor_Score},
     {"mail_editor_command", &Slrn_Editor_Mail},
     {"non_Xbrowser", &Slrn_NonX_Browser},
     {"Xbrowser", &Slrn_X_Browser},
     {"save_posts", &Slrn_Save_Posts_File},
     {"save_replies", &Slrn_Save_Replies_File},
     {"save_directory", &Slrn_Save_Directory},
     {"postpone_directory", &Slrn_Postpone_Dir},
     {"signature", &Slrn_User_Info.signature},
     {"signoff_string", &Slrn_Signoff_String},
     {"custom_headers", &Slrn_Post_Custom_Headers},
     {"followup_custom_headers", &Slrn_Followup_Custom_Headers},
     {"reply_custom_headers", &Slrn_Reply_Custom_Headers},
     {"supersedes_custom_headers", &Slrn_Supersedes_Custom_Headers},
#if SLRN_HAS_GROUPLENS
     {"grouplens_pseudoname", &Slrn_GroupLens_Pseudoname},
     {"grouplens_host", &Slrn_GroupLens_Host},
#else
     {"grouplens_pseudoname", NULL},
     {"grouplens_host", NULL},
#endif
     {"decode_directory", 
#if SLRN_HAS_DECODE
	  &Slrn_Decode_Directory
#else
	  NULL
#endif
     },
   
     {"inews_program",
#if SLRN_HAS_INEWS_SUPPORT && SLRN_HAS_USER_INEWS
	  &Slrn_Inews_Pgm
#else
	  NULL
#endif
     },
     
#if SLRN_HAS_MIME
     {"mime_charset", &Slrn_Mime_Display_Charset},
     {"metamail_command", &Slrn_MetaMail_Cmd},
#else
     {"mime_charset", NULL},
     {"metamail_command", NULL},
#endif
#if SLRN_HAS_CHARACTER_MAP
     {"charset", &Slrn_Charset},
#else
     {"charset", NULL},
#endif
#ifndef VMS
     {"sendmail_command", &Slrn_SendMail_Command},
#endif
     {"spool_inn_root", 
#if SLRN_HAS_SPOOL_SUPPORT
     &Slrn_Inn_Root
#else
     NULL
#endif
     },
     {"spool_root",
#if SLRN_HAS_SPOOL_SUPPORT
     &Slrn_Spool_Root
#else
     NULL
#endif
     },
     {"spool_nov_root",
#if SLRN_HAS_SPOOL_SUPPORT
     &Slrn_Nov_Root
#else
     NULL
#endif
     },
     {"spool_nov_file",
#if SLRN_HAS_SPOOL_SUPPORT
     &Slrn_Nov_File
#else
     NULL
#endif
     },
     {"spool_active_file",
#if SLRN_HAS_SPOOL_SUPPORT
     &Slrn_Active_File
#else
     NULL
#endif
     },
     {"spool_activetimes_file",
#if SLRN_HAS_SPOOL_SUPPORT
     &Slrn_ActiveTimes_File
#else
     NULL
#endif
     },
     {"spool_newsgroups_file",
#if SLRN_HAS_SPOOL_SUPPORT
     &Slrn_Newsgroups_File
#else
     NULL
#endif
     },
     {"spool_overviewfmt_file",
#if SLRN_HAS_SPOOL_SUPPORT
     &Slrn_Overviewfmt_File
#else
     NULL
#endif
     },
     {"macro_directory",
#if SLRN_HAS_SLANG
	&Slrn_Macro_Dir
#else
	  NULL
#endif
     },
     {"server_object", &Server_Object},
     {"post_object", &Post_Object},
     {"printer_name", &Slrn_Printer_Name},
     {"top_status_line", &Slrn_Top_Status_Line},
     {"group_status_line", &Slrn_Group_Status_Line},
     {"scorefile", &Slrn_Score_File},
     {"custom_sort_order", &Slrn_Sort_Order},
     {NULL, NULL}
};

/*}}}*/

int slrn_set_string_variable (char *name, char *value) /*{{{*/
{
   Slrn_Str_Var_Type *sp = Slrn_Str_Variables;
	
   while (sp->what != NULL)
     {
	if (!strcmp (sp->what, name))
	  {
	     char *ss;
	     
	     if (This_File != NULL)
	       {
		  char *oldname = NULL, *newname = NULL;
		  if (!strcmp (name, "followup"))
		    {
		       oldname = "followup";
		       newname = "followup_string";
		    }
		  else if (!strcmp (name, "cc_followup_string"))
		    {
		       oldname = "cc_followup_string";
		       newname = "cc_post_string";
		    }
		  
		  if (oldname != NULL)
		    {
		       slrn_message (_("%s: Obsolete variable name on line %d: %s\n"
				       "The new name is %s"),
				     This_File, This_Line_Num, oldname, newname);
		       Slrn_Saw_Obsolete = 1;
		    }
	       }
	     
	     if (sp->svaluep == NULL)
	       {
		  if (This_File != NULL)
		    slrn_message (_("%s: In this version of slrn, setting variable\n"
				    "%s has no effect. Please refer to the manual for details."),
				  This_File, name);
		  Slrn_Saw_Warning = 1;
		  return 0;
	       }
	     
	     ss = *sp->svaluep;
		  
	     slrn_free (ss);
	     if (NULL == (ss = SLmake_string (value)))
	       exit_malloc_error ();
		  
	     *sp->svaluep = ss;
	     return 0;
	  }
	sp++;
     }
   return -1;
}

/*}}}*/

int slrn_set_integer_variable (char *name, int value) /*{{{*/
{
   Slrn_Int_Var_Type *ip = Slrn_Int_Variables;
   
   while (ip->what != NULL)
     {
	if (!strcmp (ip->what, name))
	  {
	     if (This_File != NULL) /*{{{ Test for obsolete variables */
	       {
		  if (!strcmp (name, "author_display") ||
		      !strcmp (name, "display_author_realname") ||
		      !strcmp (name, "display_score"))
		    {
		       slrn_message (_("%s: Obsolete variable on line %d: %s\n"
				     "Please make use of the header_display_format command instead."),
				     This_File, This_Line_Num, name);
		       Slrn_Saw_Obsolete = 1;
		    }
		  else if (!strcmp (name, "group_dsc_start_column") ||
			   !strcmp (name, "show_descriptions"))
		    {
		       slrn_message (_("%s: Obsolete variable on line %d: %s\n"
				     "Please make use of the group_display_format variable instead."),
				     This_File, This_Line_Num, name);
		       Slrn_Saw_Obsolete = 1;
		    }
		  else if (!strcmp (name, "process_verbatum_marks"))
		    {
		       slrn_message (_("%s: Obsolete spelling on line %d:\n"
				     "Use \"process_verbatim_marks\" instead of \"process_verbatum_marks\"."),
				     This_File, This_Line_Num);
		       Slrn_Saw_Obsolete = 1;
		    }
		  else if (!strcmp (name, "prompt_next_group") ||
			   !strcmp (name, "query_reconnect") ||
			   !strcmp (name, "use_xgtitle"))
		    {
		       slrn_message (_("%s: Obsolete variable on line %d: %s"),
				     This_File, This_Line_Num, name);
		       Slrn_Saw_Obsolete = 1;
		    }
	       } /*}}}*/
	     if (ip->valuep == NULL)
	       {
		  if (This_File != NULL)
		    slrn_message (_("%s: In this version of slrn, setting variable\n"
				    "%s has no effect. Please refer to the manual for details."),
				  This_File, name);
		  Slrn_Saw_Warning = 1;
		  return 0;
	       }
	     *ip->valuep = value;
	     return 0;
	  }
	ip++;
     }
   return -1;
}

/*}}}*/

int slrn_get_variable_value (char *name, int *type, char ***sval, int **ival) /*{{{*/
{
   Slrn_Str_Var_Type *sp;
   Slrn_Int_Var_Type *ip;
   
   sp = Slrn_Str_Variables;
   while (sp->what != NULL)
     {
	if (!strcmp (sp->what, name))
	  {
	     *sval = sp->svaluep;
	     *type = STRING_TYPE;
	     return 0;
	  }
	sp++;
     }
   
   ip = Slrn_Int_Variables;
   while (ip->what != NULL)
     {
	if (!strcmp (ip->what, name))
	  {
	     *ival = ip->valuep;
	     *type = INT_TYPE;
	     return 0;
	  }
	ip++;
     }
   
   return -1;
}

/*}}}*/

static int set_variable_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
   int ret;
   char *what = table->string_args[1];
   int ivalue = table->int_args[2];
   char *svalue = table->string_args[2];
   int type = table->arg_type[2];
   
   (void) argc;
   
   if (type == STRING_TYPE)
     ret = slrn_set_string_variable (what, svalue);
   else if (type == INT_TYPE) 
     ret = slrn_set_integer_variable (what, ivalue);
   else ret = -1;
   
   if (ret != 0) exit_unknown_object ();
   return 0;
}

/*}}}*/


/*}}}*/

/*{{{ Setting Color/Mono Attributes */

typedef struct /*{{{*/
{
   char *name;
   int value;
   char *fg, *bg;
   SLtt_Char_Type mono;
}

/*}}}*/
Color_Handle_Type;

/* default colors -- suitable for a color xterm */

#define DEF_BG		"black"
#define DEF_FG		"lightgray"

static Color_Handle_Type Color_Handles[] = /*{{{*/
{
     {"verbatum", 	VERBATIM_COLOR,		"green", 	DEF_BG, 0},
   /* FIXME: keep this as the first entry until it gets removed */
     {"article",	ARTICLE_COLOR,		DEF_FG,		DEF_BG, 0},
     {"author",		AUTHOR_COLOR,		"magenta",	DEF_BG, 0},
     {"boldtext", 	BOLD_COLOR,		"yellow", 	DEF_BG, SLTT_BOLD_MASK},
     {"box", 		BOX_COLOR, 		DEF_FG, 	"blue", 0},
     {"cursor",		CURSOR_COLOR,		"brightgreen",	DEF_BG, SLTT_REV_MASK},
     {"date",		DATE_COLOR,		"magenta",	DEF_BG, 0},
     {"description",	GROUP_DESCR_COLOR,	"magenta",	DEF_BG, 0},
     {"error",		ERROR_COLOR,		"red",		DEF_BG, SLTT_BLINK_MASK},
     {"frame", 		FRAME_COLOR,		"yellow", 	"blue", SLTT_REV_MASK},
     {"from_myself",	FROM_MYSELF_COLOR,	"brightmagenta", DEF_BG, SLTT_BOLD_MASK},
     {"group",		GROUP_COLOR,		DEF_FG,		DEF_BG, 0},
     {"grouplens_display",GROUPLENS_DISPLAY_COLOR,DEF_FG,	DEF_BG, 0},
     {"header_name",	SLRN_HEADER_KEYWORD_COLOR,"green", 	DEF_BG, SLTT_BOLD_MASK},
     {"header_number",	HEADER_NUMBER_COLOR,	"green",	DEF_BG, 0},
     {"headers",	HEADER_COLOR,		"brightcyan",	DEF_BG, 0},
     {"high_score",	HIGH_SCORE_COLOR,	"red",		DEF_BG, SLTT_BOLD_MASK},
     {"italicstext", 	ITALICS_COLOR,		"magenta", 	DEF_BG, 0},
     {"menu",		MENU_COLOR,		"yellow",	"blue", SLTT_REV_MASK},
     {"menu_press",	MENU_PRESS_COLOR,	DEF_FG,		"yellow", 0},
     {"message",	MESSAGE_COLOR,		DEF_FG,		DEF_BG, 0},
     {"neg_score",	NEG_SCORE_COLOR,	"green",	DEF_BG,	0},
     {"normal",		0,			DEF_FG,		DEF_BG, 0},
     {"pos_score",	POS_SCORE_COLOR,	"blue",		DEF_BG,	SLTT_REV_MASK},
     {"pgpsignature",	PGP_SIGNATURE_COLOR,	"red",		DEF_BG, 0},
     {"quotes",		QUOTE_COLOR,		"red",		DEF_BG, 0},
     {"response_char",	RESPONSE_CHAR_COLOR,	"green", 	DEF_BG, SLTT_BOLD_MASK},
     {"selection", 	SELECT_COLOR,		"yellow", 	"blue", SLTT_BOLD_MASK},
     {"signature",	SIGNATURE_COLOR,	"red",		DEF_BG, 0},
     {"status",		STATUS_COLOR,		"yellow",	"blue", SLTT_REV_MASK},
     {"subject",	SUBJECT_COLOR,		DEF_FG,		DEF_BG, 0},
     {"thread_number",	THREAD_NUM_COLOR,	DEF_FG,		DEF_BG, SLTT_BOLD_MASK},
     {"tilde",		SLRN_TILDE_COLOR,	"green",	DEF_BG, SLTT_BOLD_MASK},
     {"tree",		TREE_COLOR,		"red",		DEF_BG, 0},
     {"underlinetext", 	UNDERLINE_COLOR,	"magenta", 	DEF_BG, SLTT_ULINE_MASK},
     {"unread_subject",	UNREAD_SUBJECT_COLOR,	"white",	DEF_BG, SLTT_BOLD_MASK},
     {"url", 		URL_COLOR,		"white",	DEF_BG, SLTT_BOLD_MASK},
     {"verbatim", 	VERBATIM_COLOR,		"green", 	DEF_BG, 0},

     {NULL, -1, NULL, NULL, 0}
};

/*}}}*/

/* These are for internal use and automatically set by slrn */
static Color_Handle_Type Auto_Color_Handles[] = /*{{{*/
{
     {"high_score",	HL_HIGH_SCORE_COLOR,	"brightred",	DEF_BG,	SLTT_BOLD_MASK},
     {"neg_score",	HL_NEG_SCORE_COLOR,	"brightgreen",	DEF_BG,	SLTT_BOLD_MASK},
     {"pos_score",	HL_POS_SCORE_COLOR,	"brightblue",	DEF_BG, SLTT_REV_MASK | SLTT_BOLD_MASK},
     {"subject",	HL_SUBJECT_COLOR,	"white",	DEF_BG,	SLTT_BOLD_MASK},
     {NULL, -1, NULL, NULL, 0}
};

static char *Color_Names [16] =
{
   "black", "red", "green", "brown",
     "blue", "magenta", "cyan", "lightgray",
     "gray", "brightred", "brightgreen", "yellow",
     "brightblue", "brightmagenta", "brightcyan", "white"
};
/*}}}*/

int slrn_set_object_color (char *name, char *fg, char *bg,
			   SLtt_Char_Type attr) /*{{{*/
{
   Color_Handle_Type *ct = Color_Handles;

   while (ct->name != NULL)
     {
	if (!strcmp (ct->name, name))
	  {
	     Color_Handle_Type *ac = Auto_Color_Handles;
	     
	     if ((This_File != NULL) && (ct == Color_Handles))
	       {
		  slrn_message (_("%s: Obsolete spelling on line %d:\n"
				"Use \"verbatim\" instead of \"verbatum\""),
				This_File, This_Line_Num);
		  Slrn_Saw_Obsolete = 1;
	       }
	     
	     SLtt_set_color (ct->value, name, fg, bg);
#ifndef IBMPC_SYSTEM
	     SLtt_add_color_attribute (ct->value, attr);
#endif
	     
	     while (ac->name != NULL)
	       {
		  if (!strcmp (ac->name, name))
		    {
		       int i;
		       
		       for (i = 0; i < 16; ++i)
			 if (!strcmp (fg, Color_Names [i]))
			   break;
		       
		       if (i < 16)
			 SLtt_set_color (ac->value, NULL,
					 Color_Names [i | 0x8], bg);
		       else
			 SLtt_set_color (ac->value, NULL, fg, bg);
#ifndef IBMPC_SYSTEM
		       SLtt_add_color_attribute (ac->value, attr);
#endif
		       
		       break;
		    }
		  ac++;
	       }
	     
	     return 0;
	  }
	ct++;
     }

   if (!strncmp (name, "quotes", 6))
     {
	int level = atoi (name+6);
	if ((0<=level) && (level<MAX_QUOTE_LEVELS))
	  {
	     SLtt_set_color (QUOTE_COLOR + level, name, fg, bg);
#ifndef IBMPC_SYSTEM
	     SLtt_add_color_attribute (QUOTE_COLOR + level, attr);
#endif
	     return 0;
	  }
     }

   slrn_error (_("%s is not a color object"), name);
   return -1;
}
/*}}}*/

#if defined(__unix__) && !defined(IBMPC_SYSTEM)
static char *get_name_for_color (SLtt_Char_Type color, int want_bg) /*{{{*/
{
   /* 0xFF is the internal representation of "default" in S-Lang */
   if (((color >> (want_bg ? 16 : 8)) & 0xFF) == 0xFF)
     return "default";
   if (color & SLTT_BOLD_MASK)
     color |= 0x8 << 8;
   if (color & SLTT_BLINK_MASK)
     color |= 0x8 << 16;
   return Color_Names[(color >> (want_bg ? 16 : 8)) & 0xF];
}/*}}}*/
#endif

char *slrn_get_object_color (char *name, int want_bg) /*{{{*/
{
#if !defined(__unix__) || defined(IBMPC_SYSTEM)
   (void) name; (void) want_bg;
   slrn_error (_("Due to a limitation in S-Lang, this feature is only available on Unix."));
#else
   Color_Handle_Type *ct = Color_Handles;
   
   while (ct->name != NULL)
     {
	if (!strcmp (ct->name, name))
	  {
	     SLtt_Char_Type color = SLtt_get_color_object (ct->value);
	     return get_name_for_color (color, want_bg);
	  }
	ct++;
     }

   if (!strncmp (name, "quotes", 6))
     {
	int level = atoi (name+6);
	if ((0<=level) && (level<MAX_QUOTE_LEVELS))
	  {
	     SLtt_Char_Type color = SLtt_get_color_object (QUOTE_COLOR + level);
	     return get_name_for_color (color, want_bg);
	  }
     }
   
   slrn_error (_("%s is not a color object"), name);
#endif
   return NULL;
}
/*}}}*/

static SLtt_Char_Type read_mono_attributes (int argc, int start,
					    SLcmd_Cmd_Table_Type *table)/*{{{*/
{
   SLtt_Char_Type retval = 0;
   char *attr;
   int i;

   for (i = start; i < argc; i++)
     {
	attr = table->string_args[i];
	if (!strcmp (attr, "bold")) retval |= SLTT_BOLD_MASK;
	else if (!strcmp (attr, "blink")) retval |= SLTT_BLINK_MASK;
	else if (!strcmp (attr, "underline")) retval |= SLTT_ULINE_MASK;
	else if (!strcmp (attr, "reverse")) retval |= SLTT_REV_MASK;
	else if (!strcmp (attr, "none")) retval = 0;
	else exit_unknown_object ();
     }
   return retval;
}
/*}}}*/

static int color_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
   char *what = table->string_args[1];
   char *fg = table->string_args[2];
   char *bg = table->string_args[3];
   SLtt_Char_Type attrs = read_mono_attributes (argc, 4, table);
   
   if (-1 == slrn_set_object_color (what, fg, bg, attrs))
     exit_unknown_object ();
   return 0;
}

/*}}}*/

static int mono_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
   char *what = table->string_args[1];
   SLtt_Char_Type mono_attr;
   Color_Handle_Type *ct = Color_Handles;

   mono_attr = read_mono_attributes (argc, 2, table);
   
   while (ct->name != NULL)
     {
	if (!strcmp (ct->name, what))
	  {
	     Color_Handle_Type *ac = Auto_Color_Handles;
	     
	     SLtt_set_mono (ct->value, NULL, mono_attr);
	     
	     while (ac->name != NULL)
	       {
		  if (!strcmp (ac->name, what))
		    {
		       SLtt_set_mono (ac->value, NULL, mono_attr | SLTT_BOLD_MASK);
		       break;
		    }
		  ac++;
	       }
	     
	     return 0;
	  }
	ct++;
     }

   if (0 == strncmp (what, "quotes", 6))
     {
	SLtt_set_mono (QUOTE_COLOR + atoi (what+6), NULL, mono_attr);
	return 0;
     }

   exit_unknown_object ();
   return 0;
}

/*}}}*/

/*}}}*/

/*{{{ User Info Data (Hostname, etc) */

static int set_posting_host (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
   char *hostname = table->string_args[1];
   
   (void) argc;
   
   if (slrn_is_fqdn (hostname))
     {
	slrn_free (Slrn_User_Info.posting_host);
	if (NULL == (Slrn_User_Info.posting_host = SLmake_string (hostname)))
	  exit_malloc_error ();
	return 0;
     }
   
   slrn_error (_("%s is not a valid fully-qualified host name."), hostname);
   return -1;
}

/*}}}*/

/*----------------------------------------------------------------------*\
* static int user_data_fun ();
 *
 * convenient mechanism to set Slrn_User_Info fields without adding
 * extra environment variables
 * 
 * FIXME: This is obsolete and will be removed before 1.0
\*----------------------------------------------------------------------*/

typedef struct /*{{{*/
{
   char *name;
   char **addr;
   unsigned int size;
}

/*}}}*/
User_Info_Variable_Type;

static User_Info_Variable_Type User_Info_Variables[] = /*{{{*/
{
#if ! SLRN_HAS_STRICT_FROM
     {"hostname", &Slrn_User_Info.hostname, 0},
     {"username", &Slrn_User_Info.username, 0},
     {"realname", &Slrn_User_Info.realname, 0},
#else
     {"hostname", NULL, 0},
     {"username", NULL, 0},
     {"realname", NULL, 0},
#endif
     {"scorefile", &Slrn_Score_File, 0},
     {"replyto", &Slrn_User_Info.replyto, 0},
     {"organization", &Slrn_User_Info.org, 0},
     {"followup", &Slrn_User_Info.followup_string, 0},
     {"signature", &Slrn_User_Info.signature, 0},
     {"cc_followup_string", &Slrn_CC_Post_Message, 0},
#if SLRN_HAS_DECODE
     {"decode_directory", &Slrn_Decode_Directory, 0},
#endif
     {"editor_command", &Slrn_Editor, 0},
     {"quote_string", NULL, 0},
     {NULL, NULL, 0}
};

/*}}}*/

static int user_data_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
   char *what = table->string_args[0];
   char *field = table->string_args[1];
   User_Info_Variable_Type *u = User_Info_Variables;
   char **ptr, *contents;
   unsigned int n;
   
   (void) argc;
   
   while (u->name != NULL)
     {
	if (!strcmp (u->name, what))
	  {
	     n = strlen (field);
	     
	     if (u->size)
	       {
		  contents = (char *) u->addr;
		  strncpy (contents, field, u->size);
		  contents [u->size - 1] = 0;
	       }
	     else if (NULL != (ptr = u->addr))
	       {
		  contents = safe_malloc (n + 1);
		  strcpy (contents, field); /* safe */
		  
		  slrn_free (*ptr);
		  *ptr = contents;
	       }

	     issue_obsolete_message ();
	     
	     return 0;
	  }
	u++;
     }
   exit_unknown_object ();
   return -1;
}

/*}}}*/

/*}}}*/

/*{{{ Server Newsrc Mapping Functions */

typedef struct Server_List_Type /*{{{*/
{
   struct Server_List_Type *next;
   char *file;
   char *host;
   char *username;
   char *password;
}

/*}}}*/
Server_List_Type;

static Server_List_Type *Server_List;

static Server_List_Type *find_server (char *host) /*{{{*/
{
   Server_List_Type *s;
   char *port;
   int i, has_ssl = 0;
   
   if (0 == strncmp (host, "snews://", 8))
     {
	has_ssl = 1;
	host += 8;
     }
   else if (0 == strncmp (host, "news://", 7))
     host += 7;
   
   /* host may contain information about the port in the form "address:port"
    * and about whether to use SSL with a prefix like "snews://"
    * First try to find a match with exactly the same information.  If that
    * fails, find a match without it.
    */
   host = slrn_safe_strmalloc (host);
   port = slrn_strchr (host, ':');

   for (i = 0; i < 2; i++)
     {
	s = Server_List;
	while (s != NULL)
	  {
	     char *this_host = s->host;
	     int this_has_ssl = 0;
	     
	     if (0 == strncmp (this_host, "snews://", 8))
	       {
		  this_has_ssl = 1;
		  this_host += 8;
	       }
	     else if (0 == strncmp (this_host, "news://", 7))
	       this_host += 7;
	     
	     if ((0 == slrn_case_strcmp ((unsigned char *)host, 
					 (unsigned char *) this_host)) &&
		 ((i == 2) || (has_ssl == this_has_ssl)))
	       break;
	     s = s->next;
	  }

	if ((s != NULL) || (port == NULL))
	  break;
	
	/* Try again without port information */
	*port = 0;
     }

   slrn_free (host);
   return s;
}

/*}}}*/

int slrn_add_to_server_list (char *host, char *file, char *username, char *password) /*{{{*/
{
   Server_List_Type *s;
   
   if (NULL == (s = find_server (host)))
     {
	s = (Server_List_Type *) safe_malloc (sizeof (Server_List_Type));
	memset ((char *) s, 0, sizeof (Server_List_Type));
	s->next = Server_List;
	Server_List = s;
	s->host = slrn_safe_strmalloc (host);
     }
   
   if (file != NULL)
     {
	char pathbuf [SLRN_MAX_PATH_LEN];
	slrn_free (s->file);
	slrn_make_home_filename (file, pathbuf, sizeof (pathbuf));
	s->file = slrn_safe_strmalloc (pathbuf);
     }

   if (username != NULL)
     {
	slrn_free (s->username);
	s->username = slrn_safe_strmalloc (username);
     }

   if (password != NULL)
     {
	slrn_free (s->password);
	s->password = slrn_safe_strmalloc (password);
     }
   
   return 0;
}

/*}}}*/

static int server_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
   char *the_file = table->string_args[2], *the_host = table->string_args[1];
      
   (void) argc;
   
   return slrn_add_to_server_list (the_host, the_file, NULL, NULL);
}

/*}}}*/

char *slrn_map_file_to_host (char *host) /*{{{*/
{
   Server_List_Type *s;
   
   if (NULL == (s = find_server (host)))
     return NULL;
   
   return s->file;
}

/*}}}*/

/*}}}*/

/*{{{ Server Authorization Information */

/*----------------------------------------------------------------------*\
* static int nnrp_fun ();
 *
 * convenient mechanism to set nnrp Slrn_User_Info fields without adding
 * extra environment variables
 *
 * recognized fields
 *
 *   nnrpaccess         - used to log in to a server using authinfo
 *                        it has the following format.
 *                         "host  username  password"
\*----------------------------------------------------------------------*/

static int nnrp_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
#if SLRN_HAS_NNTP_SUPPORT
   char *server = table->string_args[1];
   char *name = table->string_args[2];
   char *pass = table->string_args[3];

   (void) argc;
   return slrn_add_to_server_list (server, NULL, name, pass);
#else
   (void) argc;
   (void) table;
   return 0;
#endif   
}
/*}}}*/

int slrn_get_authorization (char *host, char **name, char **pass) /*{{{*/
{
   Server_List_Type *s;
   char buf[SLRL_DISPLAY_BUFFER_SIZE];
   int do_newline = 1;

   *name = NULL;
   *pass = NULL;
   
   s = find_server (host);

   if (s == NULL)
     return 0;
   
   if ((s->username == NULL) || (*s->username == 0))
     {
	*buf = 0;
	if (do_newline)
	  {
	     if (0 == (Slrn_TT_Initialized  & SLRN_SMG_INIT))
	       putc ('\n', stdout);
	     do_newline = 0;
	  }

	if (-1 == slrn_read_input (_("Username: "), NULL, buf, 1, 0))
	  return -1;
	slrn_free (s->username);
	s->username = slrn_safe_strmalloc (buf);
     }

   if ((s->password == NULL) || (*s->password == 0))
     {
	*buf = 0;
	if (do_newline)
	  {
	     if (0 == (Slrn_TT_Initialized  & SLRN_SMG_INIT))
	       putc ('\n', stdout);
	  }

	if (-1 == slrn_read_input_no_echo (_("Password: "), NULL, buf, 1, 0))
	  return -1;
	slrn_free (s->password);
	s->password = slrn_safe_strmalloc (buf);
     }

   *name = s->username;
   *pass = s->password;

   return 0;
}

/*}}}*/

/*}}}*/

static void slrn_init_modes (void) /*{{{*/
{
   slrn_init_group_mode ();
   slrn_init_article_mode ();
}

/*}}}*/

void slrn_startup_initialize (void) /*{{{*/
{
   Color_Handle_Type *h;
   int i;

#if SLRN_HAS_SLANG
   Slrn_Macro_Dir = slrn_strdup_strcat (SHAREDIR, "/macros", NULL);
#endif

   slrn_init_modes ();
   SLang_init_case_tables ();
   
   Slrn_Ignore_Quote_Regexp[0] = compile_quote_regexp ("^ ? ?[><:=|]");
   Slrn_Strip_Sig_Regexp[0] = compile_quote_regexp ("^-- $");
   
   h = Color_Handles;
   while (h->name != NULL)
     {
	SLtt_set_color (h->value, NULL, h->fg, h->bg);
#ifndef IBMPC_SYSTEM
	SLtt_add_color_attribute (h->value, h->mono & ~SLTT_REV_MASK);
#endif	
	SLtt_set_mono (h->value, NULL, h->mono);
	h++;
     }
   h = Auto_Color_Handles;
   while (h->name != NULL)
     {
	SLtt_set_color (h->value, NULL, h->fg, h->bg);
#ifndef IBMPC_SYSTEM
	SLtt_add_color_attribute (h->value, h->mono & ~SLTT_REV_MASK);
#endif	
	SLtt_set_mono (h->value, NULL, h->mono);
	h++;
     }
   for (i = 1; i < MAX_QUOTE_LEVELS; i++)
     {
	SLtt_set_color (QUOTE_COLOR + i, NULL, "red", DEF_BG);
	SLtt_set_mono (QUOTE_COLOR + i, NULL, 0);
     }

#if !defined(IBMPC_SYSTEM)
   /* We are not using the blink characters (unless in mono) */
   SLtt_Blink_Mode = 0;
#endif

   /* The following are required by GNKSA */
   (void) slrn_set_visible_headers ("From:,Subject:,Newsgroups:,Followup-To:,Reply-To:");
   
   (void) slrn_set_header_format (0, "%F%B%-5S%G%-5l:[%12r]%t%s");
   (void) slrn_set_header_format (1, "%F%B%G%-5l:[%12r]%t%s");
   (void) slrn_set_header_format (2, "%F%B%-5l:%t%s");
   (void) slrn_set_header_format (3, "%F%B%-5S%-5l:%t%50s %r");
   (void) slrn_set_header_format (4, "%F%B%-5S [%10r]:%t%49s %-19g[%17d]");
   
   (void) slrn_set_group_format (0, "  %F%-5u  %n%45g%d");
   (void) slrn_set_group_format (1, "  %F%-5u  %n%50g%-8l-%h");
   (void) slrn_set_group_format (2, "  %F%-5u [%-6t]  %n");
}


/*}}}*/

int slrn_read_startup_file (char *name) /*{{{*/
{
   FILE *fp;
   char line [512];
   SLPreprocess_Type pt;
   int save_this_line_num;
   char *save_this_file;
   char *save_this_line;

   if (-1 == slrn_init_readline ())
     {
	slrn_exit_error (_("Unable to initialize S-Lang readline library."));
     }
   
   if (-1 == SLprep_open_prep (&pt))
     {
	slrn_exit_error (_("Error initializing S-Lang preprocessor."));
     }
   
   fp = fopen (name, "r");
   if (fp == NULL) return -1;
   
   slrn_message (_("Reading startup file %s."), name);
   
   save_this_file = This_File;
   save_this_line = This_Line;
   save_this_line_num = This_Line_Num;

   This_File = name;
   This_Line = line;
   This_Line_Num = 0;
   
   Slrn_Cmd_Table.table = Slrn_Startup_File_Cmds;
   
   while (NULL != fgets (line, sizeof(line) - 1, fp))
     {
	This_Line_Num++;
	if (SLprep_line_ok (line, &pt))
	  (void) SLcmd_execute_string (line, &Slrn_Cmd_Table);
	
	if (SLang_Error) exit_unknown_object ();
     }
   slrn_fclose (fp);
   
   SLprep_close_prep (&pt);
   
   if ((Server_Object != NULL)
       && (-1 == (Slrn_Default_Server_Obj = slrn_map_name_to_object_id (0, Server_Object))))
     slrn_exit_error (_("Server object '%s' is not supported."), Server_Object);
   
   if ((Post_Object != NULL) 
       && (-1 == (Slrn_Default_Post_Obj = slrn_map_name_to_object_id (1, Post_Object))))
     slrn_exit_error (_("Post object '%s' is not supported."), Post_Object);
   
   This_File = save_this_file;
   This_Line = save_this_line;
   This_Line_Num = save_this_line_num;
   return 0;
}

/*}}}*/

static int include_file_fun (int argc, SLcmd_Cmd_Table_Type *table)
{
   char file [SLRN_MAX_PATH_LEN];
   
   (void) argc;
   
   slrn_make_home_filename (table->string_args [1], file, sizeof (file));

   if (-1 == slrn_read_startup_file (file))
     slrn_exit_error (_("%s: line %d:\n%sFile not opened: %s"),
		      This_File, This_Line_Num, This_Line, file);

   return 0;
}

     
static int interpret_fun (int argc, SLcmd_Cmd_Table_Type *table) /*{{{*/
{
#if SLRN_HAS_SLANG
   char *file = table->string_args [1];
   
   (void) argc;
   if (Slrn_Use_Slang == 0) return 0;
   return slrn_eval_slang_file (file);
#else
   (void) argc; (void) table;
   return 0;
#endif
}

/*}}}*/
