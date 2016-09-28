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
#ifndef _SLRN_NNTPLIB_H
#define _SLRN_NNTPLIB_H
#include "sltcp.h"

typedef struct
{
#define NNTP_RECONNECT_OK	0x1
   unsigned int flags;
   int init_state;
   int can_post;
#define NNTP_MAX_GROUP_NAME_LEN 80
#define NNTP_MAX_CMD_LEN 512
   char group_name [NNTP_MAX_GROUP_NAME_LEN + 1];
#define NNTP_MAX_HOST_LEN	80
   char host [NNTP_MAX_HOST_LEN + 1];
   char name [NNTP_MAX_HOST_LEN];
   int port;
   int code;
   int use_ssh;
#define NNTP_RSPBUF_SIZE	512
   char rspbuf[NNTP_RSPBUF_SIZE];

   /* Capabilities-- if -1, probe server needs to be done */
   int can_xover;
   int can_xhdr;
   int can_xpat;
   int sv_id; /* type of server software */

   int (*auth_hook)(char *, char **, char **);

   int number_bytes_received;

   SLTCP_Type *tcp;
}
NNTP_Type;

extern void nntp_disconnect_server (NNTP_Type *);
extern int nntp_check_connection (NNTP_Type *);
extern int nntp_reconnect_server (NNTP_Type *);

extern int nntp_write_server (NNTP_Type *, char *, unsigned int);
extern int nntp_fgets_server (NNTP_Type *, char *, unsigned int);
extern int nntp_fputs_server (NNTP_Type *, char *);
extern int nntp_gets_server (NNTP_Type *, char *, unsigned int);
extern int nntp_puts_server (NNTP_Type *, char *);
extern int nntp_get_server_response (NNTP_Type *);
extern int nntp_start_server_cmd (NNTP_Type *, char *);
extern int nntp_start_server_vcmd (NNTP_Type *, char *, ...) ATTRIBUTE_PRINTF(2,3);
extern int nntp_server_cmd (NNTP_Type *, char *);
extern int nntp_server_vcmd (NNTP_Type *, char *, ...) ATTRIBUTE_PRINTF(2,3);

extern char *nntp_get_server_name (void);

extern int nntp_close_server (NNTP_Type *);
extern NNTP_Type *nntp_open_server (char *, int);

extern int nntp_read_line (NNTP_Type *s, char *, unsigned int);
extern int nntp_discard_output (NNTP_Type *s);

extern int nntp_has_cmd (NNTP_Type *, char *);
extern int nntp_list (NNTP_Type *, char *);
extern int nntp_list_newsgroups (NNTP_Type *);
extern int nntp_authorization (NNTP_Type *, int);
extern int nntp_end_post (NNTP_Type *);
extern int nntp_post_cmd (NNTP_Type *);
extern int nntp_list_active_cmd (NNTP_Type *, char *);

extern int nntp_select_group (NNTP_Type *, char *, NNTP_Artnum_Type *, NNTP_Artnum_Type *);
extern int nntp_refresh_groups (NNTP_Type *, Slrn_Group_Range_Type *, int);
extern int nntp_xpat_cmd (NNTP_Type *, char *, NNTP_Artnum_Type, NNTP_Artnum_Type, char *);
extern int nntp_one_xhdr_cmd (NNTP_Type *, char *, NNTP_Artnum_Type, char *, unsigned int);

extern int nntp_listgroup (NNTP_Type *, char *);
extern int nntp_head_cmd (NNTP_Type *, NNTP_Artnum_Type, char *, NNTP_Artnum_Type *);

extern int nntp_xover_cmd (NNTP_Type *, NNTP_Artnum_Type, NNTP_Artnum_Type);
extern int nntp_xhdr_cmd (NNTP_Type *, char *, NNTP_Artnum_Type, NNTP_Artnum_Type);
extern int nntp_next_cmd (NNTP_Type *s, NNTP_Artnum_Type *);
extern int nntp_body_cmd (NNTP_Type *s, NNTP_Artnum_Type, char *);
extern int nntp_article_cmd (NNTP_Type *s, NNTP_Artnum_Type, char *);

extern char *nntp_read_and_malloc (NNTP_Type *);

extern void (*NNTP_Connection_Lost_Hook) (NNTP_Type *);
extern int (*NNTP_Authorization_Hook) (char *, int, char **, char **);
extern FILE *NNTP_Debug_Fp;
extern char *nntp_map_code_to_string (int);

#endif				       /* _SLRN_NNTPLIB_H */
