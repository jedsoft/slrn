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
extern int slrn_read_startup_file (char *);
extern void slrn_startup_initialize (void);
extern char *slrn_map_file_to_host (char *);
extern int Slrn_Autobaud;
extern char *Slrn_Score_File;
extern int Slrn_Scroll_By_Page;
extern int Slrn_Saw_Obsolete;

extern int slrn_set_string_variable (char *, char *);
extern int slrn_set_integer_variable (char *, int);
extern int slrn_get_variable_value (char *, int *, char ***, int **);
extern int slrn_get_authorization (char *, char **, char **);
extern int slrn_set_object_color (char *, char *, char *, SLtt_Char_Type);
extern char *slrn_get_object_color (char *, int);
extern int slrn_add_to_server_list (char *, char *, char *, char *);

struct SLcmd_Cmd_Table_Type;
struct SLRegexp_Type;
extern void slrn_generic_regexp_fun (int, SLcmd_Cmd_Table_Type *, SLRegexp_Type **);

typedef struct
{
   char *what;
   int *valuep;
}
Slrn_Int_Var_Type;

typedef struct
{
   char *what;
   char **svaluep;
}
Slrn_Str_Var_Type;

extern Slrn_Int_Var_Type Slrn_Int_Variables [];
extern Slrn_Str_Var_Type Slrn_Str_Variables [];
