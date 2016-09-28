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
#ifndef _SLRN_ART_H
#define _SLRN_ART_H
#ifndef SLRNPULL_CODE
extern int slrn_select_article_mode (Slrn_Group_Type *, NNTP_Artnum_Type, int);
extern void slrn_init_article_mode (void);
extern void slrn_subject_strip_was (char *);
extern SLKeyMap_List_Type *Slrn_Article_Keymap;
extern char *Slrn_Quote_String;
extern char *Slrn_Save_Directory;
extern char *Slrn_Header_Help_Line;
extern char *Slrn_Header_Status_Line;
extern char *Slrn_Art_Help_Line;
extern char *Slrn_Art_Status_Line;
extern char *Slrn_Followup_Custom_Headers;
extern char *Slrn_Reply_Custom_Headers;
extern char *Slrn_Supersedes_Custom_Headers;
extern char *Slrn_Overview_Date_Format;
extern char *Slrn_Followup_Date_Format;
extern int Slrn_Use_Localtime;

extern int Slrn_Article_Window_Border;
extern int Slrn_Startup_With_Article;
extern int Slrn_Show_Thread_Subject;
extern int Slrn_Query_Next_Article;
extern int Slrn_Query_Next_Group;
extern int Slrn_Auto_CC_To_Poster;
extern int Slrn_Score_After_XOver;
extern int Slrn_Use_Tmpdir;
extern int Slrn_Wrap_Mode;
extern int Slrn_Wrap_Width;
extern int Slrn_Wrap_Method;
extern int Slrn_Use_Header_Numbers;
extern int Slrn_Reads_Per_Update;
extern int Slrn_High_Score_Min;
extern int Slrn_Low_Score_Max;
extern int Slrn_Kill_Score_Max;
extern FILE *Slrn_Kill_Log_FP;
extern int Slrn_Signature_Hidden;
extern int Slrn_Pgp_Signature_Hidden;
extern int Slrn_Quotes_Hidden_Mode;
extern int Slrn_Verbatim_Marks_Hidden;
extern int Slrn_Verbatim_Hidden;
extern int Slrn_Warn_Followup_To;

extern char *Slrn_X_Browser;

extern char *Slrn_NonX_Browser;
#if SLRN_HAS_SPOILERS
extern int Slrn_Spoiler_Char;
extern int Slrn_Spoiler_Display_Mode;
#endif
extern int Slrn_Use_Tildes;
extern int Slrn_Generate_Email_From;
extern int Slrn_Emphasized_Text_Mode;
extern int Slrn_Emphasized_Text_Mask;
extern int Slrn_Highlight_Urls;
extern int Slrn_Sig_Is_End_Of_Article;
extern int Slrn_Del_Article_Upon_Read;
extern int Slrn_Followup_Strip_Sig;
extern int Slrn_Smart_Quote;
extern int Slrn_Pipe_Type;
extern int Slrn_Process_Verbatim_Marks;
#if SLRN_HAS_UUDEVIEW
extern int Slrn_Use_Uudeview;
#endif

#endif				       /* NOT SLRNPULL_CODE */

extern int Slrn_Invalid_Header_Score;

typedef struct Slrn_Header_Line_Type
{
   char *name;
   unsigned int name_len;
   char *value;
   struct Slrn_Header_Line_Type *next;
}
Slrn_Header_Line_Type;

typedef struct Slrn_Header_Type
{
   struct Slrn_Header_Type *next, *prev;  /* threaded next/prev */
   unsigned int flags;
#define HEADER_READ			0x0001
#define HEADER_TAGGED			0x0004
#define HEADER_HIGH_SCORE		0x0008
#define HEADER_LOW_SCORE		0x0010
#define HEADER_HARMLESS_FLAGS_MASK	0x001F
#define HEADER_REQUEST_BODY		0x0020
#define HEADER_DONT_DELETE_MASK		0x0024
#define HEADER_WITHOUT_BODY		0x0040
#define HEADER_HIDDEN			0x0100
#define HEADER_NTAGGED			0x0200
#define FAKE_PARENT			0x0400
#define FAKE_CHILDREN			0x0800
#define FAKE_HEADER_HIGH_SCORE		0x1000
#define HEADER_CHMAP_PROCESSED		0x2000
#define HEADER_HAS_PARSE_PROBLEMS	0x4000

#define HEADER_PROCESSED		0x8000
   struct Slrn_Header_Type *real_next, *real_prev;
   struct Slrn_Header_Type *parent, *child, *sister;  /* threaded relatives */
   struct Slrn_Header_Type *hash_next;  /* next in hash table */
   unsigned int num_children;
   unsigned long hash;		       /* based on msgid */
   NNTP_Artnum_Type number;			       /* server number */
   int lines;
   int bytes;
   char *subject;		       /* malloced */
   char *from;			       /* malloced */
   char *date;			       /* malloced */
   char *msgid;			       /* malloced */
   char *refs;			       /* malloced */
   char *xref;			       /* malloced */
   char *realname;		       /* malloced */
   unsigned int tag_number;
   Slrn_Header_Line_Type *add_hdrs;
#define MAX_TREE_SIZE 256
   char *tree_ptr;		       /* malloced -- could be NULL */
#if SLRN_HAS_GROUPLENS
   int gl_rating;
   int gl_pred;
#endif
   int score;
   int thread_score;
} Slrn_Header_Type;

extern Slrn_Header_Type *Slrn_First_Header;
extern Slrn_Header_Type *Slrn_Current_Header;

extern int slrn_goto_header (Slrn_Header_Type *, int);
extern void slrn_set_header_flags (Slrn_Header_Type *, unsigned int);
extern void slrn_request_header (Slrn_Header_Type *);
extern void slrn_unrequest_header (Slrn_Header_Type *);
extern int slrn_locate_header_by_msgid (char *, int, int);

typedef struct Slrn_Article_Line_Type
{
   struct Slrn_Article_Line_Type *next, *prev;
   unsigned int flags;
#define HEADER_LINE		0x0001
#define QUOTE_LINE		0x0002
#define SIGNATURE_LINE		0x0004
#define PGP_SIGNATURE_LINE	0x0008
#define VERBATIM_LINE		0x0010
#define VERBATIM_MARK_LINE	0x0020
#define LINE_TYPE_MASK		0x00FF

#define WRAPPED_LINE		0x0100
#define SPOILER_LINE		0x0200
#define HIDDEN_LINE		0x0400
#define LINE_ATTRIBUTES_MASK	0x0700

#define LINE_HAS_8BIT_FLAG	0x8000
   union
     {
	unsigned int quote_level;
     }
   v;
   char *buf;
}
Slrn_Article_Line_Type;

typedef struct
{
    char *charset;		       /* malloced */
    int content_type;
    int content_subtype;
    int was_modified;
    int was_parsed;
    int needs_metamail;
}
Slrn_Mime_Type;

typedef struct
{
   Slrn_Article_Line_Type *lines;
   Slrn_Article_Line_Type *cline;      /* current line */
   Slrn_Article_Line_Type *raw_lines;  /* unmodified article (as read from server) */
   int is_modified;		       /* non-zero if different from server */
   int is_wrapped;
   /* Eventually *_hidden will be replaced by a bitmapped quantity */
   int quotes_hidden;
   int headers_hidden;
   int signature_hidden;
   int pgp_signature_hidden;
   int verbatim_hidden;
   int verbatim_marks_hidden;
   Slrn_Mime_Type mime;
   int needs_sync;		       /* non-zero if line number/current line needs updated */
}
Slrn_Article_Type;

extern Slrn_Article_Type *Slrn_Current_Article;

extern Slrn_Header_Type *slrn_find_header_with_msgid (char *);
extern int slrn_set_visible_headers (char *);

extern char *Slrn_Visible_Headers_String;
extern int slrn_is_hidden_headers_mode (void);

extern SLRegexp_Type *Slrn_Ignore_Quote_Regexp[];
extern SLRegexp_Type *Slrn_Strip_Re_Regexp[];
extern SLRegexp_Type *Slrn_Strip_Sig_Regexp[];
extern SLRegexp_Type *Slrn_Strip_Was_Regexp[];

extern int slrn_pipe_article_to_cmd (char *);
extern int slrn_save_current_article (char *);

extern Slrn_Article_Line_Type *slrn_search_article (char *, char **, int, int, int);
extern int slrn_header_cursor_pos (void);
extern unsigned int slrn_header_down_n (unsigned int, int);
extern unsigned int slrn_header_up_n (unsigned int, int);

extern unsigned int slrn_art_linedn_n (unsigned int);
extern unsigned int slrn_art_lineup_n (unsigned int);
extern unsigned int slrn_art_count_lines (void);
extern unsigned int slrn_art_cline_num (void);

extern void slrn_collapse_this_thread (Slrn_Header_Type *, int);
extern void slrn_collapse_threads (int);
extern void slrn_uncollapse_threads (int);
extern void slrn_uncollapse_this_thread (Slrn_Header_Type *, int);
extern unsigned int slrn_thread_size (Slrn_Header_Type *);
extern int slrn_is_thread_collapsed (Slrn_Header_Type *);

extern int slrn_next_unread_header (int);
extern int slrn_goto_num_tagged_header (int *);
extern int slrn_next_tagged_header (void);
extern int slrn_prev_tagged_header (void);
extern void slrn_set_article_window_size (int);
extern int slrn_get_article_window_size (void);
extern char *slrn_extract_header (char *, unsigned int);

/* Third argument must be zero unless caller deals with Slrn_Current_Header. */
#include "score.h"
extern Slrn_Header_Type *slrn_set_header_score (Slrn_Header_Type *, int, int,
						Slrn_Score_Debug_Info_Type *);
extern void slrn_apply_scores (int);

extern int slrn_is_article_visible (void);
extern int slrn_is_article_win_zoomed (void);

extern int slrn_edit_score (Slrn_Header_Type *, char *);

int slrn_get_next_pagedn_action (void);

extern int slrn_string_to_article (char *str, int handle_mime, int cooked);

extern int Slrn_Color_By_Score;
extern int Slrn_Highlight_Unread;

extern int slrn_set_header_format (unsigned int, char *);
extern char *slrn_get_header_format (int num);   /* un-malloced */

extern void slrn_art_sync_article (Slrn_Article_Type *);
extern void slrn_art_free_line (Slrn_Article_Line_Type *);
extern void slrn_art_free_article_line_list (Slrn_Article_Line_Type *);
extern void slrn_art_free_article (Slrn_Article_Type *);
extern int slrn_art_get_unread (void);

/* These are in art_sort.c : */
#ifndef SLRNPULL_CODE
extern int Slrn_New_Subject_Breaks_Threads;
extern int Slrn_Sorting_Mode;
extern int Slrn_Sort_By_Threads;
extern char *Slrn_Sort_Order;
extern int Slrn_Uncollapse_Threads;
#endif				       /* NOT SLRNPULL_CODE */
extern void slrn_sort_headers (void);

/* These are in art_misc.c : */
extern int _slrn_art_unhide_quotes (Slrn_Article_Type *a);
extern int _slrn_art_hide_quotes (Slrn_Article_Type *a, int);
#if SLRN_HAS_SPOILERS
extern void slrn_art_mark_spoilers (Slrn_Article_Type *a);
#endif
extern void slrn_art_mark_quotes (Slrn_Article_Type *a);
extern void slrn_art_mark_signature (Slrn_Article_Type *a);
extern int _slrn_art_unwrap_article (Slrn_Article_Type *a);
extern int _slrn_art_wrap_article (Slrn_Article_Type *a);
extern void _slrn_art_skip_quoted_text (Slrn_Article_Type *a);
extern int _slrn_art_skip_digest_forward (Slrn_Article_Type *a);
extern char *slrn_art_extract_header (char *, unsigned int);
extern char *slrn_cur_extract_header (char *, unsigned int);
extern void _slrn_art_hide_headers (Slrn_Article_Type *a);
extern void _slrn_art_unhide_headers (Slrn_Article_Type *a);
extern int _slrn_art_unfold_header_lines (Slrn_Article_Type *a);

extern void slrn_mark_header_lines (Slrn_Article_Type *a);
extern void _slrn_art_hide_signature (Slrn_Article_Type *a);
extern void _slrn_art_unhide_signature (Slrn_Article_Type *a);
extern void _slrn_art_hide_pgp_signature (Slrn_Article_Type *a);
extern void _slrn_art_unhide_pgp_signature (Slrn_Article_Type *a);
extern void slrn_art_mark_pgp_signature (Slrn_Article_Type *a);

extern void _slrn_art_hide_verbatim (Slrn_Article_Type *a);
extern void _slrn_art_unhide_verbatim (Slrn_Article_Type *a);
extern void slrn_art_mark_verbatim (Slrn_Article_Type *a);

#endif				       /* _SLRN_ART_H */
