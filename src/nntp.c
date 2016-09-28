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
#include "config.h"
#include "slrnfeat.h"

#include <stdio.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <string.h>
#include <errno.h>
#include <slang.h>

#include "jdmacros.h"
#include "server.h"
#include "snprintf.h"

#ifdef VMS
# include "vms.h"
#endif

extern int Slrn_Prefer_Head;

int Slrn_Force_Authentication = 0;

static Slrn_Server_Obj_Type NNTP_Server_Obj;
static Slrn_Post_Obj_Type NNTP_Post_Obj;

static NNTP_Type *NNTP_Server;
static int NNTP_Port = -1;
static char *NNTP_Server_Name;

/* If 1, abort.  If -1, abort unless keyboard quit. */
static int _NNTP_Abort_On_Disconnection;

static void _nntp_connection_lost_hook (NNTP_Type *s)
{
   if (_NNTP_Abort_On_Disconnection)
     {
	if ((_NNTP_Abort_On_Disconnection == -1) && SLKeyBoard_Quit)
	  return;

	slrn_exit_error (_("Server connection to %s lost.  Cannot recover."),
			 s->host);
     }
}

static int _nntp_interrupt_handler (void)
{
   (void) slrn_handle_interrupts ();

   if ((SLang_get_error () == USER_BREAK) || SLKeyBoard_Quit)
     return -1;

   return 0;
}

static void _nntp_reset (void)
{
   NNTP_Type *s = NNTP_Server;

   nntp_disconnect_server (s);
   /* The next NNTP command will actually perform the reset. */
}

static void _nntp_close_server (void)
{
   NNTP_Type *s = NNTP_Server;

   NNTP_Server = NULL;

   if (s != NULL) nntp_close_server (s);
}

static int _nntp_initialize_server (void)
{
   NNTP_Authorization_Hook = slrn_get_authorization;
   SLTCP_Interrupt_Hook = _nntp_interrupt_handler;

   if (NNTP_Server != NULL)
     nntp_close_server (NNTP_Server);

   if (NULL == (NNTP_Server = nntp_open_server (NNTP_Server_Name, NNTP_Port)))
     return -1;

   NNTP_Server_Obj.sv_has_xover = 0;
   if (Slrn_Prefer_Head & 2)
     NNTP_Server->can_xover = 0;
   else
     {
	if (-1 == nntp_has_cmd (NNTP_Server, "XOVER"))
	  {
	     _nntp_close_server ();
	     return -1;
	  }

	if (NNTP_Server->can_xover == 0)
	  {
	     slrn_message_now (_("Server %s does not implement the XOVER command."),
			       NNTP_Server->host);
	  }
	else NNTP_Server_Obj.sv_has_xover = 1;
     }

   NNTP_Server_Obj.sv_has_xhdr = 0;
   if (Slrn_Prefer_Head & 1)
     NNTP_Server->can_xhdr = 0;
   else
     {
	if (-1 == nntp_has_cmd (NNTP_Server, "XHDR Path"))
	  {
	     _nntp_close_server ();
	     return -1;
	  }

	if (NNTP_Server->can_xhdr == 0)
	  {
	     slrn_message_now (_("Server %s does not implement the XHDR command."),
			       NNTP_Server->host);
	  }
	else NNTP_Server_Obj.sv_has_xhdr = 1;
     }

   NNTP_Server_Obj.sv_id = NNTP_Server->sv_id;

   NNTP_Post_Obj.po_can_post = NNTP_Server->can_post;
   NNTP_Server->flags |= NNTP_RECONNECT_OK;

   NNTP_Connection_Lost_Hook = _nntp_connection_lost_hook;
   return 0;
}

static int _nntp_read_line (char *buf, unsigned int len)
{
   int status;

   status = nntp_read_line (NNTP_Server, buf, len);

   if (status == 1)
     return status;

   if (status == 0)
     {
	_NNTP_Abort_On_Disconnection = 0;
	return 0;
     }

   /* Read fail or user break.  Either way, we have to shut it down. */
   _nntp_connection_lost_hook (NNTP_Server);
   return -1;
}

static int _nntp_get_article_size (NNTP_Artnum_Type id)
{
   /* no nntp-command for this, reading the article takes too much time */
   (void) id;
   return 0;
}

static int _nntp_head_cmd (NNTP_Artnum_Type id, char *msgid, NNTP_Artnum_Type *real_idp)
{
   return nntp_head_cmd (NNTP_Server, id, msgid, real_idp);
}

static int _nntp_select_article (NNTP_Artnum_Type n, char *msgid)
{
   return nntp_article_cmd (NNTP_Server, n, msgid);
}

static int _nntp_select_group (char *grp, NNTP_Artnum_Type *min, NNTP_Artnum_Type *max)
{
   return nntp_select_group (NNTP_Server, grp, min, max);
}

static int _nntp_refresh_groups (Slrn_Group_Range_Type *gr, int n)
{
   return nntp_refresh_groups (NNTP_Server, gr, n);
}

static char *_nntp_current_group (void)
{
   return NNTP_Server->group_name;
}

static int _nntp_put_server_cmd (char *cmd, char *buf, unsigned int len)
{
   int code;

   if (-1 != (code = nntp_server_cmd (NNTP_Server, cmd)))
     {
	strncpy (buf, NNTP_Server->rspbuf, len);
	if (len) buf[len - 1] = 0;
     }
   return code;
}

static int _nntp_xpat_cmd (char *hdr, NNTP_Artnum_Type rmin, NNTP_Artnum_Type rmax, char *pat)
{
   return nntp_xpat_cmd (NNTP_Server, hdr, rmin, rmax, pat);
}

static int _nntp_one_xhdr_cmd (char *hdr, NNTP_Artnum_Type num,
			       char *buf, unsigned int buflen)
{
   return nntp_one_xhdr_cmd (NNTP_Server, hdr, num, buf, buflen);
}

static int _nntp_has_cmd (char *cmd)
{
   return nntp_has_cmd (NNTP_Server, cmd);
}

static int _nntp_list (char *what)
{
   return nntp_list (NNTP_Server, what);
}

static int _nntp_list_newsgroups (void)
{
   return nntp_list_newsgroups (NNTP_Server);
}

static int _nntp_list_active (char *pat)
{
   int code;

   code = nntp_list_active_cmd (NNTP_Server, pat);

   if (OK_GROUPS == code)
     _NNTP_Abort_On_Disconnection = (pat == NULL) ? 1 : -1;

   return code;
}

static int _nntp_send_authinfo (void)
{
   return nntp_authorization (NNTP_Server, Slrn_Force_Authentication);
}

static int _nntp_start_post (void)
{
   /* Make sure we're connected to a server -- won't be the case the first
    * time we post if we're reading news from a local spool.  In this case
    * a late connect is better as we don't bother the server until we need
    * to.
    */
   if (NNTP_Server == NULL)
     {
	if (-1 == _nntp_initialize_server ())
	  return -1;
     }

   return nntp_post_cmd (NNTP_Server);
}

static int _nntp_end_post (void)
{
   int status;

   status = nntp_end_post (NNTP_Server);

   if (status == -1)
     {
	slrn_error (_("Error writing to server."));
	return -1;
     }

   if (NNTP_Server->code != OK_POSTED)
     {
	slrn_error (_("Article rejected: %s"), NNTP_Server->rspbuf);
	return -1;
     }

   return 0;
}

static int _nntp_po_puts (char *buf)
{
   char *b;

   /* make sure \n --> \r\n */
   b = buf;
   while (NULL != (b = slrn_strbyte (buf, '\n')))
     {
	unsigned int len;

	len = (unsigned int) (b - buf);
	if ((-1 == nntp_write_server (NNTP_Server, buf, len))
	    || (-1 == nntp_write_server (NNTP_Server, "\r\n", 2)))
	  return -1;

	buf = b + 1;
     }

   return nntp_fputs_server (NNTP_Server, buf);
}

static int _nntp_po_vprintf (const char *fmt, va_list ap)
{
   char buf[NNTP_BUFFER_SIZE];

   slrn_vsnprintf (buf, sizeof (buf), fmt, ap);

   return _nntp_po_puts (buf);
}

static int _nntp_po_printf (char *fmt, ...)
{
   va_list ap;
   int retval;

   va_start(ap, fmt);
   retval = _nntp_po_vprintf (fmt, ap);
   va_end(ap);

   return retval;
}

static int _nntp_xover_cmd (NNTP_Artnum_Type min, NNTP_Artnum_Type max)
{
   int status;

   if (OK_XOVER == (status = nntp_xover_cmd (NNTP_Server, min, max)))
     {
	_NNTP_Abort_On_Disconnection = -1;
     }

   return status;
}

static int _nntp_xhdr_cmd (char *field, NNTP_Artnum_Type min, NNTP_Artnum_Type max)
{
   int status;

   if (OK_HEAD == (status = nntp_xhdr_cmd (NNTP_Server, field, min, max)))
     {
	_NNTP_Abort_On_Disconnection = -1;
     }

   return status;
}

static int _nntp_next_cmd (NNTP_Artnum_Type *id)
{
   return nntp_next_cmd (NNTP_Server, id);
}

static unsigned int _nntp_get_bytes (int clear)
{
   unsigned int temp;

   temp = NNTP_Server->number_bytes_received;
   if (clear)
     NNTP_Server->number_bytes_received = 0;

   return temp;
}

static char * _nntp_get_recom_id (void)
{
   SLRegexp_Type *re;

   if ((re=slrn_compile_regexp_pattern("<.*@.*\\..*>")) != NULL)
     {
	char *t, *msgid = NULL, *post_rsp;
	post_rsp = NNTP_Server->rspbuf;
#if SLANG_VERSION < 20000
	t=(char*)SLang_regexp_match((unsigned char*) post_rsp,
				    strlen(post_rsp), re);
#else
	t = SLregexp_match (re, post_rsp, strlen (post_rsp));
#endif
	if (t != NULL)
	  {
	     unsigned int len;
#if SLANG_VERSION < 20000
	     len = re->end_matches[0];
#else
	     (void) SLregexp_nth_match (re, 0, NULL, &len);
#endif

	     msgid=slrn_strnmalloc(t, len, 1);
	  }
#if SLANG_VERSION >= 20000
	SLregexp_free (re);
#endif
	return msgid;
     }
   return NULL;
}

static int nntp_init_objects (void)
{
   NNTP_Post_Obj.po_start = _nntp_start_post;
   NNTP_Post_Obj.po_end = _nntp_end_post;
   NNTP_Post_Obj.po_printf = _nntp_po_printf;
   NNTP_Post_Obj.po_vprintf = _nntp_po_vprintf;
   NNTP_Post_Obj.po_puts = _nntp_po_puts;
   NNTP_Post_Obj.po_get_recom_id = _nntp_get_recom_id;
   NNTP_Post_Obj.po_can_post = 1;

   NNTP_Server_Obj.sv_select_group = _nntp_select_group;
   NNTP_Server_Obj.sv_refresh_groups = _nntp_refresh_groups;
   NNTP_Server_Obj.sv_current_group = _nntp_current_group;
   NNTP_Server_Obj.sv_read_line = _nntp_read_line;
   NNTP_Server_Obj.sv_close = _nntp_close_server;
   NNTP_Server_Obj.sv_get_article_size = _nntp_get_article_size;
   NNTP_Server_Obj.sv_initialize = _nntp_initialize_server;
   NNTP_Server_Obj.sv_select_article = _nntp_select_article;
   NNTP_Server_Obj.sv_put_server_cmd = _nntp_put_server_cmd;
   NNTP_Server_Obj.sv_xpat_cmd = _nntp_xpat_cmd;
   NNTP_Server_Obj.sv_xhdr_command = _nntp_one_xhdr_cmd;
   NNTP_Server_Obj.sv_has_cmd = _nntp_has_cmd;
   NNTP_Server_Obj.sv_list = _nntp_list;
   NNTP_Server_Obj.sv_list_newsgroups = _nntp_list_newsgroups;
   NNTP_Server_Obj.sv_list_active = _nntp_list_active;
   NNTP_Server_Obj.sv_send_authinfo = _nntp_send_authinfo;

   NNTP_Server_Obj.sv_has_xover = 0;
   NNTP_Server_Obj.sv_has_xhdr = -1;
   NNTP_Server_Obj.sv_nntp_xover = _nntp_xover_cmd;
   NNTP_Server_Obj.sv_nntp_xhdr = _nntp_xhdr_cmd;
   NNTP_Server_Obj.sv_nntp_head = _nntp_head_cmd;
   NNTP_Server_Obj.sv_nntp_next = _nntp_next_cmd;
   NNTP_Server_Obj.sv_reset = _nntp_reset;
   NNTP_Server_Obj.sv_nntp_bytes = _nntp_get_bytes;
   NNTP_Server_Obj.sv_id = SERVER_ID_UNKNOWN;
   return 0;
}

static int _nntp_get_server_name (void)
{
   char *host;

   if (NULL == (host = NNTP_Server_Name))
     {
	host = nntp_get_server_name ();
	if (host == NULL)
	  return -1;
     }

   NNTP_Server_Name = NNTP_Server_Obj.sv_name = slrn_safe_strmalloc (host);
   return 0;
}

static int nntp_select_server_object (void)
{
   if (NNTP_Server_Obj.sv_select_group == NULL)
     nntp_init_objects ();

   Slrn_Server_Obj = &NNTP_Server_Obj;

   if (NNTP_Server_Obj.sv_name == NULL)
     return _nntp_get_server_name ();

   return 0;
}

static int nntp_select_post_object (void)
{
   if (NNTP_Post_Obj.po_start == NULL)
     nntp_init_objects ();

   Slrn_Post_Obj = &NNTP_Post_Obj;

   if (NNTP_Server_Obj.sv_name == NULL)
     return _nntp_get_server_name ();

   return 0;
}

static void nntp_usage (void)
{
   fputs (_("--nntp options:\n\
-h nntp-host    Host name to connect to.  This overrides NNTPSERVER variable.\n\
-p NNTP-PORT    Set the NNTP port to NNTP-PORT. The default value is 119.\n\
                 Note: This option has no effect on some systems.\n\
"),
	  stdout);
   exit (0);
}

/* returns number parsed */
static int nntp_parse_args (char **argv, int argc)
{
   int i;

   for (i = 0; i < argc; i++)
     {
	if (!strcmp (argv[i], "--help"))
	  nntp_usage ();
	else if (i + 1 < argc)
	  {
	     char *arg, *arg1;

	     arg = argv[i];
	     arg1 = argv [i + 1];
	     if (!strcmp ("-p", arg))
	       {
		  NNTP_Port = atoi (arg1);
	       }
	     else if (!strcmp ("-h", arg))
	       {
		  NNTP_Server_Name = arg1;
	       }
	     else break;

	     i++;
	  }
	else break;
     }

   return i;
}
