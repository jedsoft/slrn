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
extern void slrn_add_signature (FILE *);
extern void slrn_add_date_header (FILE *);
extern int slrn_add_custom_headers (FILE *, char *, int (*)(char *, FILE *));
extern int slrn_add_references_header (FILE *, char *);
extern int slrn_post (char *, char *, char *);
extern int slrn_post_file (char *, char *, int);
extern void slrn_post_postponed (void);
extern int slrn_save_file_to_mail_file (char *, char *);
extern char *Slrn_CC_Post_Message;
extern char *Slrn_Save_Posts_File;
extern char *Slrn_Save_Replies_File;
extern char *Slrn_Post_Custom_Headers;
extern int Slrn_Reject_Long_Lines;
extern char *Slrn_Postpone_Dir;
extern int Slrn_Generate_Message_Id;
extern int Slrn_Generate_Date_Header;
extern char *Slrn_Signoff_String;
extern int Slrn_Netiquette_Warnings;
extern int Slrn_Use_Recom_Id;
