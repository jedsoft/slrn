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
#ifndef _SLRN_MISC_H
#define _SLRN_MISC_H
#include <stdio.h>
#include <stdarg.h>
#include <slang.h>

#include "ttymsg.h"
#include "vfile.h"

extern void slrn_make_home_filename (char *, char *, size_t);
extern void slrn_make_home_dirname (char *, char *, size_t);
extern void slrn_redraw (void);
extern void slrn_update_screen (void);
extern int Slrn_Full_Screen_Update;
extern int slrn_get_yesno (int, char *, ...);
extern int slrn_get_yesno_cancel (char *str, ...);
extern void slrn_clear_message (void);
extern char *slrn_print_percent (char *, SLscroll_Window_Type *, int);

typedef char *(PRINTF_CB)(char, void *, int *, int *);
extern void slrn_custom_printf (char *, PRINTF_CB, void *, int, int);
extern void slrn_write_nchars (char *, unsigned int);

extern int slrn_set_display_format (char **, unsigned int, char *);
extern unsigned int slrn_toggle_format (char **, unsigned int);

extern FILE *slrn_open_home_file (char *, char *, char *, size_t, int);
extern VFILE *slrn_open_home_vfile (char *, char *, size_t);
extern void slrn_suspend_cmd (void);
extern int slrn_read_integer (char *, int *, int *);
extern int slrn_read_input (char *, char *, char *, int, int);
extern int slrn_read_input_no_echo (char *, char *, char *, int, int);
extern int slrn_read_filename (char *, char *, char *, int, int);
extern int slrn_read_variable (char *, char *, char *, int, int);
extern void slrn_evaluate_cmd (void);
extern void slrn_update_top_status_line (void);
extern void slrn_set_color (int);
extern char slrn_map_translated_char (char *, char *, char);
extern char slrn_get_response (char *, char *, char *str, ...);
extern int slrn_is_fqdn (char *);
extern int slrn_init_readline (void);
extern int slrn_check_batch (void);

extern unsigned char *slrn_regexp_match (SLRegexp_Type *, char *);
extern SLRegexp_Type *slrn_compile_regexp_pattern (char *);

#define MAX_HOST_NAME_LEN 256
typedef struct
{
   char *realname;
   char *username;
   char *hostname;
   char *replyto;
   char *org;
   char *followup_string;
   char *followupto_string;
   char *reply_string;
   char *signature;
#if SLRN_HAS_CANLOCK
   char *cancelsecret;
#endif
   char *posting_host;		       /* FQDN or NULL */
   char *login_name;
}
Slrn_User_Info_Type;

extern Slrn_User_Info_Type Slrn_User_Info;
extern void slrn_get_user_info (void);
extern int slrn_edit_file (char *, char *, unsigned int, int);
extern  int slrn_mail_file (char *, int, unsigned int, char *, char *);

extern void slrn_article_help (void);
extern void slrn_group_help (void);

/* Both of these must be malloced strings */
void slrn_set_input_string (char *);
void slrn_set_input_chars (char *);

extern int Slrn_Message_Present;

#define SLRN_CONFIRM_CATCHUP	0x1
#define SLRN_CONFIRM_PRINT	0x2
#define SLRN_CONFIRM_POST	0x4
#define SLRN_CONFIRM_URL	0x8
#define SLRN_CONFIRM_QUIT	0x10
#define SLRN_CONFIRM_ALL	0x1F
extern int Slrn_User_Wants_Confirmation;
extern void slrn_get_mouse_rc (int *, int *);
#ifndef VMS
extern char *Slrn_SendMail_Command;
#endif
extern SLKeyMap_List_Type *Slrn_RLine_Keymap;
extern SLang_RLine_Info_Type *Slrn_Keymap_RLI;
extern SLKeymap_Function_Type Slrn_Custom_Readline_Functions [];

extern void slrn_va_message (char *, va_list);

#if SLRN_HAS_PIPING
extern int _slrn_pclose (FILE *);
#endif
extern FILE *slrn_popen (char *, char *);
extern int slrn_pclose (FILE *);


extern int Slrn_Use_Tmpdir;
extern FILE *slrn_open_tmpfile_in_dir (char *, char *, size_t);
extern FILE *slrn_open_tmpfile (char *, size_t);

extern int slrn_posix_system (char *, int);
extern char *Slrn_Editor;
extern char *Slrn_Editor_Post;
extern char *Slrn_Editor_Score;
extern char *Slrn_Editor_Mail;
extern int Slrn_Editor_Uses_Mime_Charset;
extern int Slrn_Mail_Editor_Is_Mua;

extern char *Slrn_Failed_Post_Filename;

extern char *Slrn_Top_Status_Line;

extern void slrn_push_keymap (SLKeyMap_List_Type *);
extern void slrn_pop_keymap (void);
extern SLKeyMap_List_Type *Slrn_Current_Keymap;

extern int Slrn_Abort_Unmodified;
#endif				       /* _SLRN_MISC_H */
