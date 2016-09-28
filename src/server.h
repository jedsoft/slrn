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
#ifndef _SLRN_SERVER_H
#define _SLRN_SERVER_H
#include "nntpcodes.h"
#include "ranges.h"

typedef struct
{
   int (*po_start)(void);
   int (*po_end)(void);
   int (*po_printf)(char *, ...) ATTRIBUTE_PRINTF(1,2);
   int (*po_vprintf)(const char *, va_list);
   int (*po_puts)(char *);
   char * (*po_get_recom_id)(void);
   int po_can_post;
} Slrn_Post_Obj_Type;

typedef struct
{
   int (*sv_select_group) (char *, NNTP_Artnum_Type *, NNTP_Artnum_Type *);
   int (*sv_refresh_groups) (Slrn_Group_Range_Type *, int);
   char * (*sv_current_group) (void);
   int (*sv_read_line) (char *, unsigned int);
   void (*sv_close) (void);
   /* sv_reset is somewhat like sv_close except that it only puts the
    * server in a state where the server can be given additional commands.
    */
   void (*sv_reset)(void);
   int (*sv_initialize) (void);
   int (*sv_select_article) (NNTP_Artnum_Type, char *);
   int (*sv_get_article_size) (NNTP_Artnum_Type);
   int (*sv_put_server_cmd) (char *, char *, unsigned int);
   int (*sv_xpat_cmd) (char *, NNTP_Artnum_Type, NNTP_Artnum_Type, char *);

   int (*sv_xhdr_command) (char *, NNTP_Artnum_Type, char *, unsigned int);

   int (*sv_has_cmd) (char *);
   int (*sv_list) (char *);
   int (*sv_list_newsgroups) (void);
   int (*sv_list_active) (char *);
   int (*sv_send_authinfo) (void);

   int sv_has_xhdr;
   int sv_has_xover;
   int sv_reset_has_xover;
   /* if non-zero, sv_has_xover is set to 1 when entering a group.
    * This is because some servers support XOVER but do not have overview
    * files for all groups.  See xover.c
    */
   int (*sv_nntp_xover) (NNTP_Artnum_Type, NNTP_Artnum_Type);
   int (*sv_nntp_xhdr) (char *, NNTP_Artnum_Type, NNTP_Artnum_Type);
   int (*sv_nntp_head) (NNTP_Artnum_Type, char *, NNTP_Artnum_Type *);
   int (*sv_nntp_next) (NNTP_Artnum_Type *);

   /* Returns number of bytes received.
    * If the int is non-zero, the counter is reset. */
   unsigned int (*sv_nntp_bytes) (int);

   /* Some server software has known bugs that we can work around. */
#define SERVER_ID_UNKNOWN	0
#define SERVER_ID_INN		1
   int sv_id;
   char *sv_name;
}
Slrn_Server_Obj_Type;

extern Slrn_Server_Obj_Type *Slrn_Server_Obj;
extern Slrn_Post_Obj_Type *Slrn_Post_Obj;

extern NNTP_Artnum_Type Slrn_Server_Min, Slrn_Server_Max;
extern char *Slrn_Current_Group_Name;

#if SLRN_HAS_NNTP_SUPPORT
extern int Slrn_Broken_Xref;
extern int Slrn_Query_Reconnect;
extern int Slrn_Force_Authentication;
extern char *Slrn_NNTP_Server_Name;
#endif

#if SLRN_HAS_INEWS_SUPPORT
extern char *Slrn_Inews_Pgm;
#endif

extern char *slrn_map_object_id_to_name (int, int);
extern int slrn_map_name_to_object_id (int, char *);

extern int slrn_init_objects (void);
extern int slrn_select_post_object (int);
extern int slrn_select_server_object (int);
extern int slrn_parse_object_args (char *, char **, int);

extern char *slrn_getserverbyfile(char *);

#if SLRN_HAS_SPOOL_SUPPORT
extern char *Slrn_Inn_Root;
extern char *Slrn_Spool_Root;
extern char *Slrn_Nov_Root;
extern char *Slrn_Nov_File;
extern char *Slrn_Active_File;
extern char *Slrn_ActiveTimes_File;
extern char *Slrn_Newsgroups_File;
extern char *Slrn_Overviewfmt_File;
extern int Slrn_Spool_Check_Up_On_Nov;

extern Slrn_Range_Type *slrn_spool_get_no_body_ranges (char *);
extern Slrn_Range_Type *slrn_spool_get_requested_ranges (char *);
extern int slrn_spool_set_requested_ranges (char*, Slrn_Range_Type*);
#endif

#if SLRN_HAS_PULL_SUPPORT
extern int Slrn_Use_Pull_Post;
#endif

extern int Slrn_Server_Id;
extern int Slrn_Post_Id;

#endif				       /* SLRN_SERVER_H */
