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
extern int slrn_add_signature (FILE *);
extern char *slrn_gen_date_header (void);
extern int slrn_add_custom_headers (FILE *, char *, int (*)(char *, FILE *));
extern char *slrn_trim_references_header (char *);
extern int slrn_prepare_file_for_posting (char *, unsigned int *, Slrn_Article_Type *, char *, int);
extern int slrn_post (char *, char *, char *);
extern int slrn_post_file (char *, char *, int);
extern void slrn_post_postponed (void);
extern int slrn_save_article_to_mail_file (Slrn_Article_Type *, char *);
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
