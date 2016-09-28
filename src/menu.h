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
extern void slrn_update_article_menu (void);
extern void slrn_update_group_menu (void);
extern int slrn_execute_menu (int);
extern int slrn_sbox_sorting_method (void);

typedef struct
{
   char *title;
   char **lines;
}
Slrn_Select_Box_Type;

extern int slrn_select_box (Slrn_Select_Box_Type *);

extern int slrn_select_list_mode (char *, unsigned int, char **, unsigned int, int, int *);
extern int slrn_popup_win_mode (char *, char *);
extern char *slrn_browse_dir (char *);
