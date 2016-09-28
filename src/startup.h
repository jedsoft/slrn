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
extern int slrn_read_startup_file (char *);
extern void slrn_startup_initialize (void);
extern char *slrn_map_file_to_host (char *);
extern int Slrn_Autobaud;
extern char *Slrn_Score_File;
extern int Slrn_Scroll_By_Page;
extern int Slrn_Saw_Obsolete;

extern int slrn_set_string_variable (char *, char *, char *);
extern int slrn_set_integer_variable (char *, int);
extern int slrn_get_variable_value (char *, SLtype *, char **, int *);
extern int slrn_get_authorization (char *, int, char **, char **);
extern int slrn_set_object_color (char *, char *, char *, SLtt_Char_Type);
extern char *slrn_get_object_color (char *, int);
extern int slrn_add_to_server_list (char *, char *, char *, char *);

extern char *slrn_get_charset (char *name);

/* struct SLcmd_Cmd_Table_Type; */
/* struct SLRegexp_Type; */
extern void slrn_generic_regexp_fun (int, SLcmd_Cmd_Table_Type *, SLRegexp_Type **);

typedef struct Slrn_Int_Var_Type
{
   char *what;
   int *ivalp;
   int (*get_set_func)(struct Slrn_Int_Var_Type *, int, int *);
}
Slrn_Int_Var_Type;

typedef struct Slrn_Str_Var_Type
{
   char *what;
   char **svalp;
   int (*get_set_func)(struct Slrn_Str_Var_Type *, int, char **);
}
Slrn_Str_Var_Type;

extern Slrn_Int_Var_Type Slrn_Int_Variables [];
extern Slrn_Str_Var_Type Slrn_Str_Variables [];

extern void slrn_print_config (FILE *);
