/* -*- mode: C; mode: fold; -*- */
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
#include "config.h"
#include "slrnfeat.h"

/*{{{ Include Files */

#include <stdio.h>
#include <string.h>


#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include "slrn.h"
#include "menu.h"
#include "misc.h"
#include "util.h"
#include "slrndir.h"
#include "snprintf.h"

/*}}}*/

/*{{{ Menu Routines */

typedef struct /*{{{*/
{
   char *menu_name;
   char *function_name;
}

/*}}}*/
Menu_Type;

static Menu_Type Group_Mode_Menu [] = /*{{{*/
{
     {N_("Quit"), "quit"},
     {N_("Refresh"), "refresh_groups"},
     {N_("Top"), "bob"},
     {N_("Bot"), "eob"},
     {N_("Post"), "post"},
     {N_("Help"), "help"},
     {NULL, NULL}
};

/*}}}*/
	
static Menu_Type Article_Mode_Menu [] = /*{{{*/
{
     {N_("Quit"), "quit"},
     {N_("Catchup"), "catchup_all"},
     {N_("NextGrp"), "skip_to_next_group"},
     {N_("NextArt"), "next"},
     {N_("Top"), "goto_beginning"},
     {N_("Bot"), "goto_end"},
     {N_("Post"), "post"},
     {N_("Reply"), "reply"},
     {N_("Followup"), "followup"},
     {N_("Help"), "help"},
     {NULL, NULL}
};

/*}}}*/

static Menu_Type *Current_Menu;

static void update_menu (Menu_Type *m) /*{{{*/
{
   int col;
   
   Current_Menu = m;
   /* if (Slrn_Full_Screen_Update == 0) return; */
   SLsmg_gotorc (0, 0);
   slrn_set_color (MENU_COLOR);
   if (m != NULL) while (m->menu_name != NULL)
     {
	SLsmg_write_string (_(m->menu_name));
	SLsmg_write_string ("   ");
	m++;
     }
   
   SLsmg_erase_eol ();

   col = SLtt_Screen_Cols - 16;
   if (SLsmg_get_column () < col)
     SLsmg_gotorc (0, col);
   
   SLsmg_write_string ("slrn ");
   SLsmg_write_string (Slrn_Version);
   
   slrn_set_color (0);
}

/*}}}*/

int slrn_execute_menu (int want_col) /*{{{*/
{
   Menu_Type *m;
   int col;
   int color;
   
   if ((want_col < 0) || (want_col >= SLtt_Screen_Cols)) return -1;
   
   m = Current_Menu;
   if (m == NULL) return -1;
   
   col = -1;
   while (m->menu_name != NULL)
     {
	int dcol = 2 + strlen (_(m->menu_name));
	if ((want_col > col) 
	    && (want_col <= col + dcol))
	  break;
	col += dcol + 1;
	m++;
     }
   if (m->menu_name == NULL) return -1;
   
   slrn_push_suspension (0);
   /* redraw menu item so that user sees that it has been pressed */
   if (col == -1) col = 0;
   color = MENU_PRESS_COLOR;
   SLsmg_gotorc (0, col);
   while (1)
     {
	slrn_set_color (color);
	if (col) SLsmg_write_char (' ');
	SLsmg_write_string (_(m->menu_name));
	SLsmg_write_char (' ');
	SLsmg_gotorc (0, col);
	slrn_smg_refresh ();
	if (color == MENU_COLOR) break;
	(void) SLang_input_pending (1);	       /* 1/10 sec */
	color = MENU_COLOR;
     }
   slrn_set_color (0);
   slrn_pop_suspension ();

   slrn_call_command (m->function_name);

   return 0;
}

/*}}}*/
   
void slrn_update_article_menu (void) /*{{{*/
{
   update_menu (Article_Mode_Menu);
}

/*}}}*/

void slrn_update_group_menu (void) /*{{{*/
{
   update_menu (Group_Mode_Menu);
}

/*}}}*/

/*}}}*/

/*{{{ Selection Box Routines */

static char *Sort_Selections [] = /*{{{*/
{
   N_("No sorting"),		       /* 000 */
     N_("Thread Headers"),		       /* 001 */
     N_("Sort by subject"),		       /* 010 */
     N_("Thread, then sort by subject."),  /* 011 */
     N_("Sort by scores."),		       /* 100 */
     N_("Thread, then sort by scores."),   /* 101 */
     N_("Sort by score and subject"),      /* 110 */
     N_("Thread, then sort by score and subject"),   /* 111 */
     N_("Sort by date (most recent first)"),		       /* 1000 */
     N_("Thread, then sort by date (most recent first)"),      /* 1001 */
     N_("Sort by date (most recent last)"),      /* 1010 */
     N_("Thread, then Sort by date (most recent last)"),      /* 1011 */
     N_("Custom sorting (see manual)"),	/* 1100 */
     NULL
};

/*}}}*/

static Slrn_Select_Box_Type Slrn_Sort_Select_Box = /*{{{*/
{
   N_("Sorting Method"), Sort_Selections
};

/*}}}*/

static void center_string_column (char *title, int row, int col, int num_columns) /*{{{*/
{
   int c;
   int len = strlen (title);
   
   c = (num_columns - len) / 2;
   if (c < 0) c = 0;
   
   c += col;
   SLsmg_gotorc (row, c); 
   SLsmg_write_string (title);
}

/*}}}*/

static int draw_select_box (Slrn_Select_Box_Type *sb) /*{{{*/
{
   int num_selections, max_selection_len;
   int num_rows, num_columns;
   int len;
   int row, column, r, c;
   char **lines, *line, *title;
   static Slrn_Select_Box_Type *last_sb;
   
   if (((sb == NULL) && ((sb = last_sb) == NULL)) ||
       (sb->lines == NULL))
     return -1;
   
   last_sb = sb;
   
   lines = sb->lines;
   if (sb->title == NULL)
     title = "";
   else
     title = _(sb->title);

   slrn_push_suspension (0);
   
   max_selection_len = strlen (title);
   num_selections = 0;
   while ((line = *lines) != NULL)
     {
	len = strlen (_(line));
	if (len > max_selection_len) max_selection_len = len;
	num_selections++;
	lines++;
     }

   /* allow room for title, blank line, top, bottom */
   num_rows = num_selections + 4 + 2;
   num_columns = max_selection_len + (3 + 3);
   
   row = (SLtt_Screen_Rows - num_rows) / 2;
   if (row < 0) row = 0;
   column = (SLtt_Screen_Cols - num_columns) / 2;
   if (column < 0) column = 0;

   slrn_set_color (BOX_COLOR);
   SLsmg_fill_region (row, column, num_rows, num_columns, ' ');
   
   r = row + 1;
   center_string_column (title, r, column, num_columns);
   
   lines = sb->lines;
   
   
   num_selections = 0;
   c = column + 1;
   r += 1;
   while ((line = *lines) != NULL)
     {
	lines++;
	r++;
	SLsmg_gotorc (r, c);
	SLsmg_printf ("%2X %s", num_selections, _(line));
	num_selections++;
     }
   
   r += 2;
   center_string_column (_("(Select One)"), r, column, num_columns);
				      
   slrn_set_color (FRAME_COLOR);
   SLsmg_draw_box (row, column, num_rows, num_columns);
   slrn_set_color (0);
   
   slrn_smg_refresh ();
   
   slrn_pop_suspension ();
   
   return num_selections;
}

/*}}}*/

static void select_box_redraw (void) /*{{{*/
{
   slrn_push_suspension (0);
   draw_select_box (NULL);
   Slrn_Full_Screen_Update = 1;
   slrn_pop_suspension ();
}

/*}}}*/

static Slrn_Mode_Type Menu_Mode_Cap = 
{
   NULL,			       /* keymap */
   select_box_redraw,		       /* redraw_fun */
   NULL,			       /* sigwinch_fun */
   NULL,			       /* hangup_fun */
   NULL,			       /* enter_mode_hook */
   SLRN_MENU_MODE
};

int slrn_select_box (Slrn_Select_Box_Type *sb) /*{{{*/
{
   int num_selections;
   int rsp;
   
   if (Slrn_Batch)
     return -1;

   slrn_push_mode (&Menu_Mode_Cap);

   num_selections = draw_select_box (sb);
   
   Slrn_Full_Screen_Update = 1;
   rsp = SLang_getkey ();

   slrn_pop_mode ();
   
   if (rsp >= 'A')
     {
	rsp = 10 + ((rsp | 0x20) - 'a');
     }
   else rsp = rsp - '0';
   
   if ((rsp < 0) || (rsp >= num_selections))
     {
	slrn_error (_("Cancelled."));

	while (SLang_input_pending (2))
	  (void) SLang_getkey ();
	
	return -1;
     }
   
   return rsp;
}

/*}}}*/

int slrn_sbox_sorting_method (void) /*{{{*/
{
   return slrn_select_box (&Slrn_Sort_Select_Box);
}

/*}}}*/

/*}}}*/

typedef struct Select_List_Type
{
   struct Select_List_Type *next;
   struct Select_List_Type *prev;
   unsigned int flags;
   char *data;
}
Select_List_Type;

static SLscroll_Window_Type *Select_Window;
static unsigned int Select_List_Window_Row;
static unsigned int Select_List_Window_Col;
static unsigned int Select_List_Window_Ncols;
static int Select_List_Active_Color = SELECT_COLOR;
static char *Select_List_Title = ".";
static int Is_Popup_Window; /* set to 2 if full text is visible */
static int Use_Shortcuts; /* do we use shortcuts to "jump" to entries? */

static void draw_select_list (void)
{
   unsigned int r;
   Select_List_Type *l;
   int row, column, top_showing = 0, bot_showing = 0;
   unsigned int num_rows, num_columns;

   if (Select_Window == NULL)
     return;

   slrn_push_suspension (0);

   SLscroll_find_top (Select_Window);
   l = (Select_List_Type *) Select_Window->top_window_line;
   num_rows = Select_Window->nrows;
   
   row = Select_List_Window_Row;
   column = Select_List_Window_Col;
   num_columns = Select_List_Window_Ncols;
   if (num_columns + 5 > (unsigned int) SLtt_Screen_Cols)
     num_columns = SLtt_Screen_Cols;
   column = (SLtt_Screen_Cols - num_columns) / 2;
   
   slrn_set_color (FRAME_COLOR);
   SLsmg_draw_box (row, column, num_rows + 2, num_columns + 2);
   SLsmg_gotorc (row + num_rows + 1, column + num_columns - 10);
   if (Is_Popup_Window)
     {
	unsigned int bot_number;
	
	bot_number = Select_Window->line_num +
	  (Select_Window->nrows - Select_Window->window_row) - 1;
	bot_showing = ((Select_Window->bot_window_line == NULL)
		       || (Select_Window->num_lines == bot_number));
	top_showing = (Select_Window->line_num == Select_Window->window_row + 1);
	
	if (top_showing)
	  {
	     SLsmg_write_string (bot_showing ? _("[All]") : _("[Top]"));
	     Is_Popup_Window = bot_showing ? 2 : 1;
	  }
	else if (bot_showing)
	  SLsmg_write_string (_("[Bot]"));
	else
	  SLsmg_printf ("[%u/%u]", Select_Window->line_num, Select_Window->num_lines);
     }
   else
     SLsmg_printf ("[%u/%u]", Select_Window->line_num, Select_Window->num_lines);

   if (Select_List_Title != NULL)
     {
	SLsmg_gotorc (row, column + 1);
	SLsmg_printf ("[%s]", Select_List_Title);
     }
   
   slrn_set_color (BOX_COLOR);
   row++;
   column++;
   
   for (r = 0; r < num_rows; r++)
     {
	char *data;
	SLsmg_gotorc (row + r, column);
	
	if (l == NULL) data = NULL;
	else 
	  {
	     if (!Is_Popup_Window &&
		 (l == (Select_List_Type *) Select_Window->current_line))
	       slrn_set_color (Select_List_Active_Color);

	     data = l->data;
	     l = l->next;
	  }

	SLsmg_write_nstring (data, num_columns);
	slrn_set_color (BOX_COLOR);
     }

   if (Is_Popup_Window)
     {
	if (top_showing && bot_showing)
	  slrn_message (_("Press any key to close the window."));
	else
	  slrn_message (_("Use UP/DOWN to move, any other key to close the window."));
     }
   else
     {
	if (Use_Shortcuts)
	  slrn_message (_("Use UP/DOWN to move, RETURN to select, Ctrl-G to cancel"));
	else
	  slrn_message (_("Use UP/DOWN to move, RETURN to select, q to quit"));
	SLsmg_gotorc (row + Select_Window->window_row, column);
     }
   slrn_pop_suspension ();
}

static void free_select_list (Select_List_Type *l, int free_data)
{
   Select_List_Type *next;
   
   while (l != NULL)
     {
	next = l->next;
	if (free_data)
	  slrn_free (l->data);
	slrn_free ((char *)l);
	l = next;
     }
}

static void select_list_winch (int oldr, int oldc)
{
   (void) oldr;
   (void) oldc;
   
   if (Select_Window == NULL)
     return;
   
   if (Select_Window->num_lines + 5 >= (unsigned int) SLtt_Screen_Rows)
     {
	if (SLtt_Screen_Rows > 5)
	  Select_Window->nrows = (unsigned int) SLtt_Screen_Rows - 5;
	else Select_Window->nrows = 1;
     }
   
   Select_List_Window_Row = (SLtt_Screen_Rows - Select_Window->nrows) / 2;
}

static Slrn_Mode_Type Select_List_Mode_Cap =
{
   NULL,			       /* keymap */
   draw_select_list,
   select_list_winch,
   NULL,
   NULL,
   -1
};

static int Select_List_Quit;

static void sl_cancel (void)
{
   Select_List_Quit = -1;
}

static void sl_up (void)
{
   if (Is_Popup_Window == 2)
     {
	sl_cancel ();
	return;
     }
   if ((0 == SLscroll_prev_n (Select_Window, 1)) &&
       (0 == Is_Popup_Window))
     {
	while (0 != SLscroll_next_n (Select_Window, 1000))
	  ;
     }
   if (Is_Popup_Window)
     Select_Window->top_window_line = Select_Window->current_line;
}

static void sl_pageup (void)
{
   if (Is_Popup_Window == 2)
     {
	sl_cancel ();
	return;
     }
   if (Is_Popup_Window)
     {
	SLscroll_prev_n (Select_Window, Select_Window->nrows - 1);
	Select_Window->top_window_line = Select_Window->current_line;
     }
   else
     SLscroll_pageup (Select_Window);
}

static void sl_pagedn (void)
{
   if (Is_Popup_Window == 2)
     {
	sl_cancel ();
	return;
     }
   SLscroll_pagedown (Select_Window);
   if (Is_Popup_Window)
     Select_Window->top_window_line = Select_Window->current_line;
}

static void sl_down (void)
{
   if (Is_Popup_Window == 2)
     {
	sl_cancel ();
	return;
     }
   if ((0 == SLscroll_next_n (Select_Window, 1)) &&
       (0 == Is_Popup_Window))
     {
	while (0 != SLscroll_prev_n (Select_Window, 1000))
	  ;
     }
   if (Is_Popup_Window)
     Select_Window->top_window_line = Select_Window->current_line;
}

static void sl_jump (void)
{
   char ch = (char) SLang_Last_Key_Char;
   int dir = (ch & 0x20);
   SLscroll_Type *startpos = Select_Window->current_line;
   Select_List_Type *curline;
   
   ch |= 0x20;
   do
     {
	if (dir)
	  Select_Window->current_line = Select_Window->current_line->next;
	else
	  Select_Window->current_line = Select_Window->current_line->prev;
	if (Select_Window->current_line == NULL)
	  {
	     Select_Window->current_line = Select_Window->lines;
	     if (!dir)
	       while (Select_Window->current_line->next != NULL)
		 Select_Window->current_line = Select_Window->current_line->next;
	  }
	
	if (Select_Window->current_line == startpos)
	  {
	     SLtt_beep ();
	     return;
	  }
	curline = (Select_List_Type *) Select_Window->current_line;
     }
   while (ch != (*curline->data | 0x20));
   
   select_list_winch (0, 0);
   SLscroll_find_line_num (Select_Window);
}

static void sl_select (void)
{
   Select_List_Active_Color = MENU_PRESS_COLOR;
   slrn_update_screen ();
   Select_List_Active_Color = SELECT_COLOR;
   (void) SLang_input_pending (1);	       /* 1/10 sec */
   slrn_update_screen ();
   Select_List_Quit = 1;
}

static void sl_right (void)
{
   sl_select ();
   Select_List_Quit = 2;
}
static void sl_left (void)
{
   /* Select_List_Quit = -1; */
}

static void sl_mouse (void)
{
   int r;
   slrn_get_mouse_rc (&r, NULL);
   r--;
   
   if (((unsigned int) r <= Select_List_Window_Row)
       || ((unsigned int) r > Select_List_Window_Row + Select_Window->nrows))
     {
	SLtt_beep ();
	return;
     }
   r -= (Select_List_Window_Row + 1);
   r -= Select_Window->window_row;
   if (r < 0)
     SLscroll_prev_n (Select_Window, -r);
   else
     SLscroll_next_n (Select_Window, r);
   sl_select ();
   Select_List_Quit = 2;
}

   
static SLKeyMap_List_Type *Select_List_Keymap;
static SLKeyMap_List_Type *Popup_Keymap;

static void adapt_shortcuts (void)
{
   char buf[2];
   buf[1] = 0;
   if (Use_Shortcuts)
     {
	for (*buf = 'a'; *buf <= 'z'; (*buf)++)
	  SLkm_define_key (buf, (FVOID_STAR) sl_jump, Select_List_Keymap);
	for (*buf = 'A'; *buf <= 'Z'; (*buf)++)
	  SLkm_define_key (buf, (FVOID_STAR) sl_jump, Select_List_Keymap);
     }
   else
     {
	for (*buf = 'a'; *buf <= 'z'; (*buf)++)
	  SLang_undefine_key (buf, Select_List_Keymap);
	for (*buf = 'A'; *buf <= 'Z'; (*buf)++)
	  SLang_undefine_key (buf, Select_List_Keymap);
	SLkm_define_key ("Q", (FVOID_STAR) sl_cancel, Select_List_Keymap);
     }
}

static int init_select_list_mode (int select_list)
{
   SLKeyMap_List_Type **kmap;
   char *int_name;
   
   slrn_free ((char *) Select_Window);
   Select_Window = (SLscroll_Window_Type *) slrn_malloc (sizeof (SLscroll_Window_Type), 1, 1);
   Is_Popup_Window = !select_list;
   if (Select_Window == NULL)
     return -1;
   Select_Window->nrows = 5;
   select_list_winch (0, 0);
   
   if (select_list)
     {
	kmap = &Select_List_Keymap;
	int_name = "list";
     }
   else
     {
	kmap = &Popup_Keymap;
	int_name = "popup";
     }
   
   if (*kmap != NULL)
     {
	Select_List_Mode_Cap.keymap = *kmap;
	if (select_list) adapt_shortcuts ();
	return 0;
     }
   
   if (NULL == (*kmap = SLang_create_keymap (int_name, NULL)))
     return -1;
   
#if defined(IBMPC_SYSTEM)
   SLkm_define_key  ("^@H", (FVOID_STAR) sl_up, *kmap);
   SLkm_define_key  ("\xE0H", (FVOID_STAR) sl_up, *kmap);
   SLkm_define_key  ("^@P", (FVOID_STAR) sl_down, *kmap);
   SLkm_define_key  ("\xE0P", (FVOID_STAR) sl_down, *kmap);
   SLkm_define_key  ("^@I", (FVOID_STAR) sl_pageup, *kmap);
   SLkm_define_key  ("\xE0I", (FVOID_STAR) sl_pageup, *kmap);
   SLkm_define_key  ("^@Q", (FVOID_STAR) sl_pagedn, *kmap);
   SLkm_define_key  ("\xE0Q", (FVOID_STAR) sl_pagedn, *kmap);
#else
   SLkm_define_key  ("\033[5~", (FVOID_STAR) sl_pageup, *kmap);
   SLkm_define_key  ("\033[6~", (FVOID_STAR) sl_pagedn, *kmap);
   SLkm_define_key  ("\033[A", (FVOID_STAR) sl_up, *kmap);
   SLkm_define_key  ("\033OA", (FVOID_STAR) sl_up, *kmap);
   SLkm_define_key  ("\033[B", (FVOID_STAR) sl_down, *kmap);
   SLkm_define_key  ("\033OB", (FVOID_STAR) sl_down, *kmap);
# ifdef __unix__
   SLkm_define_key  ("^(kd)", (FVOID_STAR) sl_down, *kmap);
   SLkm_define_key  ("^(ku)", (FVOID_STAR) sl_up, *kmap);
# endif
#endif
   
   /* In select lists, these bindings collide with the new "jump" feature */
   SLkm_define_key  ("k", (FVOID_STAR) sl_up, *kmap);
   SLkm_define_key  ("j", (FVOID_STAR) sl_down, *kmap);
   
   if (select_list)
     {
#if defined(IBMPC_SYSTEM)
	SLkm_define_key  ("^@M", (FVOID_STAR) sl_right, Select_List_Keymap);
	SLkm_define_key  ("\xE0M", (FVOID_STAR) sl_right, Select_List_Keymap);
	SLkm_define_key  ("^@K", (FVOID_STAR) sl_left, Select_List_Keymap);
	SLkm_define_key  ("\xE0K", (FVOID_STAR) sl_left, Select_List_Keymap);
#else
	SLkm_define_key  ("\033[C", (FVOID_STAR) sl_right, Select_List_Keymap);
	SLkm_define_key  ("\033OC", (FVOID_STAR) sl_right, Select_List_Keymap);
	SLkm_define_key  ("\033OD", (FVOID_STAR) sl_left, Select_List_Keymap);
	SLkm_define_key  ("\033[D", (FVOID_STAR) sl_left, Select_List_Keymap);
# ifdef __unix__
	SLkm_define_key  ("^(kr)", (FVOID_STAR) sl_right, Select_List_Keymap);
	SLkm_define_key  ("^(kl)", (FVOID_STAR) sl_left, Select_List_Keymap);
# endif
#endif
	SLkm_define_key  ("\r", (FVOID_STAR) sl_select, Select_List_Keymap);
	SLkm_define_key  ("^G", (FVOID_STAR) sl_cancel, Select_List_Keymap);
	
	SLkm_define_key  ("\033[M\040", (FVOID_STAR) sl_mouse, Select_List_Keymap);
	SLkm_define_key  ("\033[M\041", (FVOID_STAR) sl_mouse, Select_List_Keymap);
	SLkm_define_key  ("\033[M\042", (FVOID_STAR) sl_mouse, Select_List_Keymap);
	
	adapt_shortcuts ();
     }
   
   if (SLang_Error)
     return -1;
   
   Select_List_Mode_Cap.keymap = *kmap;
   return 0;
}

int slrn_select_list_mode (char *title,
			   unsigned int argc, char **argv, unsigned int active_num,
			   int want_shortcuts, int *want_edit)
{
   unsigned int num_lines;
   Select_List_Type *root, *curr, *last, *active_line;

   Use_Shortcuts = want_shortcuts;
   if (want_edit != NULL) *want_edit = 1;

   if (argv == 0)
     return -1;

   if (-1 == init_select_list_mode (1))
     return -1;
   
   Select_List_Title = title;

   if (title == NULL) 
     Select_List_Window_Ncols = 1;
   else 
     Select_List_Window_Ncols = 2 + strlen (title);

   active_line = root = last = NULL;
   for (num_lines = 0; num_lines < argc; num_lines++)
     {
	unsigned int len;

	if (NULL == (curr = (Select_List_Type *) slrn_malloc (sizeof (Select_List_Type), 1, 1)))
	  {
	     free_select_list (root, 0);
	     return -1;
	  }

	if (root == NULL)
	  root = curr;
	else
	  {
	     last->next = curr;
	     curr->prev = last;
	  }
	last = curr;
	
	if (num_lines == active_num)
	  active_line = curr;

	curr->data = argv [num_lines];
	len = strlen (curr->data);
	if (len > Select_List_Window_Ncols)
	  Select_List_Window_Ncols = len;
     }

   if (active_line == NULL) 
     active_line = root;

   Select_Window->num_lines = num_lines;
   Select_Window->nrows = num_lines;
   Select_Window->current_line = (SLscroll_Type *) active_line;
   Select_Window->lines = (SLscroll_Type *) root;
   select_list_winch (0, 0);
   SLscroll_find_line_num (Select_Window);
   
   slrn_push_mode (&Select_List_Mode_Cap);
   Select_List_Quit = 0;
   while (Select_List_Quit == 0)
     {
	slrn_update_screen ();
	slrn_do_keymap_key (Select_List_Keymap);
     }
   active_num = Select_Window->line_num - 1;
   free_select_list (root, 0);
   slrn_pop_mode ();
   
   slrn_message ("");

   if (Select_List_Quit == -1)
     return -1;

   if ((Select_List_Quit == 2)
       && (want_edit != NULL))
     *want_edit = 0;

   return (int) active_num;
}

/* This function will modify "text"! */
int slrn_popup_win_mode (char *title, char *text)
{
   unsigned int num_lines = 0;
   Select_List_Type *root, *curr, *last;

   if ((text == NULL) || (*text == 0) ||
       (-1 == init_select_list_mode (0)))
     return -1;
   
   Select_List_Title = title;

   if (title == NULL) 
     Select_List_Window_Ncols = 1;
   else 
     Select_List_Window_Ncols = 2 + strlen (title);

   root = last = NULL;
   
   do
     {
	unsigned int len, pos;
	char *newline, *tab;
	
	if (NULL == (curr = (Select_List_Type *) slrn_malloc (sizeof (Select_List_Type), 1, 1)))
	  {
	     free_select_list (root, 1);
	     return -1;
	  }
	
	if (root == NULL)
	  root = curr;
	else
	  {
	     last->next = curr;
	     curr->prev = last;
	  }
	last = curr;
	
	if (NULL != (newline = strchr (text, '\n')))
	  {
	     *newline++ = 0;
	     if (*newline == 0)
	       newline = NULL;
	  }
	len = strlen (text);
	
	/* Here, we handle TABs (expand to spaces): */
	tab = text;
	while (NULL != (tab = strchr (tab, '\t')))
	  {
	     tab++;
	     len += 8;
	  }
	if (NULL == (curr->data = slrn_malloc (len+1, 1, 1)))
	  {
	     free_select_list (root, 1);
	     return -1;
	  }
	tab = curr->data;
	pos = 0;
	while (*text)
	  {
	     if (*text == '\t')
	       do
	       {
		  *tab++ = ' ';
		  pos++;
	       } while (pos % 8);
	     else
	       {
		  *tab++ = *text;
		  pos++;
	       }
	     text++;
	  }
	
	len = strlen (curr->data);
	if (len > Select_List_Window_Ncols)
	  Select_List_Window_Ncols = len;
	text = newline;
	num_lines++;
     }
   while (text != NULL);
   
   Select_Window->num_lines = num_lines;
   Select_Window->nrows = num_lines;
   Select_Window->current_line = Select_Window->lines = (SLscroll_Type *) root;
   select_list_winch (0, 0);
   SLscroll_find_line_num (Select_Window);
   Select_Window->top_window_line = Select_Window->current_line;
   
   slrn_push_mode (&Select_List_Mode_Cap);
   Select_List_Quit = 0;
   while (Select_List_Quit == 0)
     {
	SLang_Key_Type *key;
	
	slrn_update_screen ();
	key = SLang_do_key (Popup_Keymap, (int (*)(void)) SLang_getkey);
	if ((key == NULL) 
	    || (key->type == SLKEY_F_INTERPRET))
	  Select_List_Quit = 1;
	else
	  key->f.f ();
     }
   free_select_list (root, 1);
   slrn_pop_mode ();
   
   slrn_message ("");
   
   if (Select_List_Quit == -1)
     return -1;
   
   return SLang_Last_Key_Char;;
}

static int file_strcmp (char **ap, char **bp)
{
   char *a, *b;
   
   a = *ap; b = *bp;
   /* Put the '.' files first */
   if (*a == '.')
     {
	if (*b != '.')
	  return -1;
     }
   else if (*b == '.')
     return 1;
   
   return strcmp (a, b);
}

#define MAX_DIR_FILES 1024

static char *browse_dir (char *dir)
{
   char *argv [MAX_DIR_FILES];
   unsigned int argc;
   Slrn_Dir_Type *d;
   Slrn_Dirent_Type *de;
   int selected;
   char title [45];
   unsigned int len;
   void (*qsort_fun) (char **, unsigned int,
		      unsigned int, int (*)(char **, char **));
   
   /* This is a silly hack to make up for braindead compilers and the lack of
    * uniformity in prototypes for qsort.
    */
   qsort_fun = (void (*) (char **, unsigned int,
			 unsigned int, int (*)(char **, char **)))
     qsort;

   if (dir == NULL)
     {
	dir = slrn_getcwd (NULL, 0);
	if (dir == NULL)
	  return NULL;
     }
   
   slrn_message_now (_("Creating directory list..."));
   if (NULL == (d = slrn_open_dir (dir)))
     return NULL;
   
   len = strlen (dir) + 1;
   if (len >= sizeof (title))
     {
	dir = dir + 4 + (len - sizeof(title));
	sprintf (title, "....%s", dir); /* safe */
     }
   else strcpy (title, dir); /* safe */
   
   argc = 0;
   while ((argc < MAX_DIR_FILES)
	  && (NULL != (de = slrn_read_dir (d))))
     {
	int status;

	status = slrn_file_exists (de->name);
	if (status == 0)
	  continue;
	
	len = de->name_len;
	if (NULL == (argv[argc] = slrn_malloc (len + 2, 0, 1)))
	  {
	     slrn_free_argc_argv_list (argc, argv);
	     return NULL;
	  }
	strcpy (argv[argc], de->name); /* safe */
	if (status == 2)
	  {
	     argv[argc][len] = '/';
	     argv[argc][len + 1] = 0;
	  }
	argc++;
     }
   
   slrn_close_dir (d);
   
   if (argc > 1)
     {
	qsort_fun (argv, argc, sizeof (char *), file_strcmp);
     }

   if (-1 == (selected = slrn_select_list_mode (title, argc, argv,
						((argc > 2) ? 2 : 0), 1,
						NULL)))
     {
	slrn_free_argc_argv_list (argc, argv);
	return NULL;
     }
   
   dir = argv[selected];
   argv[selected] = NULL;
   slrn_free_argc_argv_list (argc, argv);
   return dir;
}

char *slrn_browse_dir (char *dir)
{
   char *file;
   char *cwd;

   cwd = slrn_getcwd (NULL, 0);
   if (cwd == NULL)
     return NULL;

   if (NULL == (cwd = slrn_strmalloc (cwd, 1)))
     return NULL;

   if (-1 == slrn_chdir (dir))
     {
	(void) slrn_chdir (cwd);
	slrn_free (cwd);
	return NULL;
     }

   file = NULL;
   while (1)
     {
	unsigned int len;

	file = browse_dir (NULL);
	if (file == NULL)
	  break;
	
	len = strlen (file);
	if ((len == 0) || (file [len - 1] != '/'))
	  {
	     char *cwd1, *dir_file;
	     cwd1 = slrn_getcwd (NULL, 0);
	     if ((cwd1 == NULL)
		 || (NULL == (dir_file = slrn_spool_dircat (cwd1, file, 0))))
	       {
		  slrn_free (file);
		  file = NULL;
		  break;
	       }
	     slrn_free (file);
	     file = dir_file;
	     break;
	  }
	
	file [len - 1] = 0;	       /* knock off slash */
	(void) slrn_chdir (file);
	slrn_free (file);
	file = NULL;
     }
   
   (void) slrn_chdir (cwd);
   slrn_free (cwd);
   return file;
}
