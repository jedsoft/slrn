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
extern int slrn_get_yesno (int, char *, ...) ATTRIBUTE_PRINTF(2,3);
extern int slrn_get_yesno_cancel (int, char *str, ...) ATTRIBUTE_PRINTF(2,3);
extern void slrn_clear_message (void);
extern void slrn_clear_error (void);
extern char *slrn_print_percent (char *, SLscroll_Window_Type *, int);

typedef char *(PRINTF_CB)(char, void *, int *, int *);
extern void slrn_custom_printf (char *, PRINTF_CB, void *, int, int);
extern void slrn_write_nbytes (char *, unsigned int);

extern int slrn_set_display_format (char **, unsigned int, char *);
extern unsigned int slrn_toggle_format (char **, unsigned int);

extern FILE *slrn_open_home_file (char *, char *, char *, size_t, int);
extern VFILE *slrn_open_home_vfile (char *, char *, size_t);
extern void slrn_suspend_cmd (void);
extern int slrn_read_artnum_int (char *, NNTP_Artnum_Type *, NNTP_Artnum_Type *);
extern int slrn_read_input (char *, char *, char *, int, int);
extern int slrn_read_input_no_echo (char *, char *, char *, int, int);
extern int slrn_read_filename (char *, char *, char *, int, int);
extern int slrn_read_variable (char *, char *, char *, int, int);
extern void slrn_evaluate_cmd (void);
extern void slrn_update_top_status_line (void);
extern void slrn_set_color (int);
extern char slrn_map_translated_char (char *, char *, char);
extern char slrn_get_response (char *, char *, char *str, ...) ATTRIBUTE_PRINTF(3,4);
extern int slrn_is_fqdn (char *);
extern int slrn_init_readline (void);
extern int slrn_check_batch (void);

extern unsigned char *slrn_regexp_match (SLRegexp_Type *, char *);
extern SLRegexp_Type *slrn_compile_regexp_pattern (char *);
extern SLRegexp_Type *slrn_free_regexp (SLRegexp_Type *);

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

#define SLRN_CONFIRM_CATCHUP	0x01
#define SLRN_CONFIRM_PRINT	0x02
#define SLRN_CONFIRM_POST	0x04
#define SLRN_CONFIRM_URL	0x08
#define SLRN_CONFIRM_QUIT	0x10
#define SLRN_CONFIRM_ALL	0xFF
extern int Slrn_User_Wants_Confirmation;
extern void slrn_get_mouse_rc (int *, int *);
#ifndef VMS
extern char *Slrn_SendMail_Command;
#endif

#if SLANG_VERSION < 20000
extern int SLang_get_error (void);
extern int SLang_set_error (int);
typedef struct SLKeyMap_List_Type SLkeymap_Type;
typedef SLang_RLine_Info_Type SLrline_Type;
extern SLkeymap_Type *SLrline_get_keymap (SLrline_Type *);
typedef unsigned char SLuchar_Type;
#endif

extern SLKeyMap_List_Type *Slrn_RLine_Keymap;
extern SLang_RLine_Info_Type *Slrn_Keymap_RLI;
extern int slrn_rline_setkey (char *, char *, SLkeymap_Type *);

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

extern int Slrn_Abort_Unmodified;

#endif				       /* _SLRN_MISC_H */
