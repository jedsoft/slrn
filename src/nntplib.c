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
#include <string.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdarg.h>
#include <slang.h>
#include "jdmacros.h"

#if SLRN_USE_SLTCP
# include "sltcp.h"
#else
# include "clientlib.h"
#endif

#include "nntpcodes.h"
#include "util.h"
#include "nntplib.h"
#include "ttymsg.h"
#include "snprintf.h"
#include "server.h"
#include "strutil.h"
#include "common.h"

void (*NNTP_Connection_Lost_Hook) (NNTP_Type *);
int (*NNTP_Authorization_Hook) (char *, int, char **, char **);
int NNTP_Try_Authentication = 0;

int Slrn_Broken_Xref;

static int _nntp_connect_server (NNTP_Type *);

extern int Slrn_Force_Authentication;
extern FILE *Slrn_Debug_Fp; /* we need this here when compiling slrnpull */

/* Note: nntp_allocate_nntp returns a pointer to a static object.  Eventually,
 * it should be changed to return a dynamic one.
 */
static NNTP_Type *nntp_allocate_nntp (void)
{
   static NNTP_Type nn;
   NNTP_Type *s;

   s = &nn;

   memset ((char *) s, 0, sizeof (NNTP_Type));
   s->can_xover = -1;
   s->can_xhdr = -1;
   s->can_xpat = -1;

   return s;
}

static void _nntp_deallocate_nntp (NNTP_Type *s)
{
   if (s == NULL) return;

   if (s->tcp != NULL)
     sltcp_close (s->tcp);

   /* Until nntp_allocate_nntp is modified to create a dynamic object, just
    * zero the memory here.
    */
   memset ((char *) s, 0, sizeof (NNTP_Type));
}

/* Call connection-lost hook, unless it is ok to reconnect */
static int check_connect_lost_hook (NNTP_Type *s)
{
   if (s->flags & NNTP_RECONNECT_OK)
     return 0;

   if (NNTP_Connection_Lost_Hook != NULL)
     (*NNTP_Connection_Lost_Hook) (s);

   return -1;
}

int nntp_fgets_server (NNTP_Type *s, char *buf, unsigned int len)
{
   if ((s == NULL) || (s->init_state <= 0))
     return -1;

   *buf = 0;

   if (-1 == sltcp_fgets (s->tcp, buf, len))
     {
	(void) nntp_disconnect_server (s);
	(void) check_connect_lost_hook (s);
	return -1;
     }

   if (Slrn_Debug_Fp != NULL)
     fprintf (Slrn_Debug_Fp, "<%s", buf);

   return 0;
}

int nntp_fputs_server (NNTP_Type *s, char *buf)
{
   if ((s == NULL) || (s->init_state == 0))
     return -1;

   if (-1 == sltcp_fputs (s->tcp, buf))
     {
	(void) nntp_disconnect_server (s);
	(void) check_connect_lost_hook (s);
	return -1;
     }

   if (Slrn_Debug_Fp != NULL)
     fprintf (Slrn_Debug_Fp, ">%s\n", buf);

   return 0;
}

int nntp_write_server (NNTP_Type *s, char *buf, unsigned int n)
{
   if ((s == NULL) || (s->init_state == 0))
     return -1;

   if (n != sltcp_write (s->tcp, buf, n))
     {
	(void) nntp_disconnect_server (s);
	(void) check_connect_lost_hook (s);
	return -1;
     }

   if (Slrn_Debug_Fp != NULL)
     {
	unsigned int i;
	fputc ('>', Slrn_Debug_Fp);
	for (i = 0; i < n; i++)
	  fputc (buf[i], Slrn_Debug_Fp);
	fputc ('\n', Slrn_Debug_Fp);
     }

   return 0;
}

int nntp_gets_server (NNTP_Type *s, char *buf, unsigned int len)
{
   if (-1 == nntp_fgets_server (s, buf, len))
     return -1;

   len = strlen (buf);

   /* Update bytes received */
   s->number_bytes_received += len;

   if (len && (buf[len - 1] == '\n'))
     {
	len--;
	buf[len] = 0;
	if (len && (buf[len - 1] == '\r'))
	  buf[len - 1] = 0;
     }

   return 0;
}

int nntp_puts_server (NNTP_Type *s, char *buf)
{
   if ((-1 == nntp_fputs_server (s, buf))
       || (-1 == nntp_fputs_server (s, "\r\n"))
       || (-1 == sltcp_flush_output (s->tcp)))
     {
	(void) check_connect_lost_hook (s);
	nntp_disconnect_server (s);
	return -1;
     }

   return 0;
}

static void _nntp_error_response (NNTP_Type *s, char *fmt)
{
   slrn_error ("%s", fmt);
   slrn_error (_("Reason: %s"), s->rspbuf);
}

static int _nntp_try_parse_timeout (char *str)
{
   /* I know of only two timeout responses:
    * 503 Timeout
    * 503 connection timed out
    *
    * Here the idea is to look for 'time' and then 'out'.
    */
   /* Some servers now have NLS, so this might fail.
    * I'm trying to handle that case in nntp_server_cmd.
    */
#if SLANG_VERSION < 20000
   static SLRegexp_Type re;
   unsigned char compiled_pattern_buf[256];

   re.pat = (unsigned char *) "time.*out";
   re.buf = compiled_pattern_buf;
   re.case_sensitive = 0;
   re.buf_len = sizeof (compiled_pattern_buf);

   (void) SLang_regexp_compile (&re);
   if (NULL == SLang_regexp_match ((unsigned char *) str, strlen (str), &re))
     return -1;
   return 0;
#else
   int status = -1;
   SLRegexp_Type *re = SLregexp_compile ("time.*out", SLREGEXP_CASELESS);
   if (re != NULL)
     {
	if (NULL != SLregexp_match (re, str, strlen (str)))
	  status = 0;

	SLregexp_free (re);
     }
   return status;
#endif
}

int nntp_get_server_response (NNTP_Type *s)
{
   int status;

   if ((s == NULL) || (s->init_state <= 0))
     return -1;

   if (-1 == nntp_gets_server (s, s->rspbuf, NNTP_RSPBUF_SIZE))
     return -1;

   status = atoi (s->rspbuf);

   switch (status)
     {
      case ERR_FAULT:
	if (-1 == _nntp_try_parse_timeout (s->rspbuf))
	  break;
	/* Drop */

      case 0:			       /* invalid code */
      case ERR_GOODBYE:
	nntp_disconnect_server (s);
	(void) check_connect_lost_hook (s);
	status = -1;
	break;
     }

   s->code = status;
   return status;
}

static int _nntp_reconnect (NNTP_Type *s)
{
   nntp_disconnect_server (s);

   if (-1 == check_connect_lost_hook (s))
     return -1;

   s->flags &= ~NNTP_RECONNECT_OK;

   slrn_message_now (_("Server %s dropped connection.  Reconnecting..."), s->host);

   if (-1 == _nntp_connect_server (s))
     {
	(void) check_connect_lost_hook (s);

	s->flags |= NNTP_RECONNECT_OK;
	return -1;
     }

   if ((s->group_name[0] != 0)
       && (-1 == nntp_server_vcmd (s, "GROUP %s", s->group_name)))
     {
	(void) check_connect_lost_hook (s);

	s->flags |= NNTP_RECONNECT_OK;
	return -1;
     }

   s->flags |= NNTP_RECONNECT_OK;
   return 0;
}

int nntp_start_server_cmd (NNTP_Type *s, char *cmd)
{
   int max_tries = 3;

   if (s == NULL)
     return -1;

   while ((s->init_state != 0) && max_tries
	  && (SLKeyBoard_Quit == 0))
     {
	if (-1 != nntp_puts_server (s, cmd))
	  return 0;

	if (-1 == _nntp_reconnect (s))
	  return -1;

	max_tries--;
     }

   return -1;
}

int nntp_server_cmd (NNTP_Type *s, char *cmd)
{
   int max_tries = 3, tried_auth;

   do
     {
	int code;
	tried_auth = 0;

	if (-1 == nntp_start_server_cmd (s, cmd))
	  return -1;

	if (-1 != (code = nntp_get_server_response (s)))
	  {
	     if ((code == ERR_NOAUTH) && NNTP_Try_Authentication)
	       {
		  NNTP_Try_Authentication = 0;
		  if (-1 == nntp_authorization (s, 1))
		    return -1;
		  tried_auth = 1;
		  continue;
	       }
	     else if (((code == ERR_FAULT) || (code == OK_GOODBYE))
		       && (max_tries == 3))
	       /* might be a timeout we didn't recognize;
		* SN sends OK_GOODBYE in this case; as this function isn't
		* used to send QUIT, we can assume a timeout. */
	       {
		  nntp_disconnect_server (s);
		  (void) check_connect_lost_hook (s);
		  max_tries = 1;
		  continue;
	       }
	     return code;
	  }

	max_tries--;
     }
   while ((max_tries && (s->flags & NNTP_RECONNECT_OK)) || tried_auth);

   return -1;
}

int nntp_server_vcmd (NNTP_Type *s, char *fmt, ...)
{
   char buf [NNTP_MAX_CMD_LEN];
   va_list ap;

   va_start (ap, fmt);
   (void) SLvsnprintf (buf, sizeof (buf), fmt, ap);
   va_end (ap);

   return nntp_server_cmd (s, buf);
}

int nntp_start_server_vcmd (NNTP_Type *s, char *fmt, ...)
{
   char buf [NNTP_MAX_CMD_LEN];
   va_list ap;

   va_start (ap, fmt);
   (void) SLvsnprintf (buf, sizeof (buf), fmt, ap);
   va_end (ap);

   return nntp_start_server_cmd (s, buf);
}

int nntp_close_server (NNTP_Type *s)
{
   if (s == NULL)
     return -1;

   if (s->init_state <= 0)
     return 0;

   s->init_state = -1;		       /* closing */

   /* This might be called from a connect_lost hook.  We also do not
    * want to reconnect to send the QUIT command so do not call any
    * of the *server_cmd functions.
    */
   (void) nntp_puts_server (s, "QUIT");

   if (Slrn_Debug_Fp != NULL)
     fputs ("!Closing the server connection.\n", Slrn_Debug_Fp);

   _nntp_deallocate_nntp (s);

   return 0;
}

int nntp_authorization (NNTP_Type *s, int auth_reqd)
{
   char *name = NULL, *pass = NULL;
   int status = 0;

   if (NNTP_Authorization_Hook != NULL)
     status = (*NNTP_Authorization_Hook) (s->name, auth_reqd, &name, &pass);

   if ((auth_reqd == 0) && (status == 0))
     return 0;			       /* not needed and info not present */

   if ((status == -1) || (name == NULL) || (pass == NULL))
     {
	slrn_exit_error (_("Authorization needed, but could not determine username / password."));
	return -1;
     }

   slrn_message_now (_("Authenticating %s ..."), name);

   if (-1 == nntp_server_vcmd (s, "AUTHINFO USER %s", name))
     return -1;

   if (s->code == OK_AUTH)
     return 0;
   if (s->code != NEED_AUTHDATA)
     return -1;

   switch (nntp_server_vcmd (s, "AUTHINFO PASS %s", pass))
     {
      case -1:
	return -1;
      case OK_AUTH:
	if (s->can_post == 1)
	  break;
	/* This is the only obvious way to find out whether we are
	 * able to post after successful authentication.
	 *
	 * However, at least one server will disconnect if an attempt
	 * is made to post an empty article.  Try to avoid this by
	 * looking at the response message.  Unfortunately, this method
	 * is not very robust.
	 */
	if (NULL != strstr (s->rspbuf, "Posting Allowed"))
	  {
	     s->can_post = 1;
	     break;
	  }

	if (-1 == nntp_post_cmd (s))
	  return -1;
	if (s->code == CONT_POST)
	  {
	     nntp_end_post (s);
	     s->can_post = 1;
	     break;
	  }
	break;
      case ERR_ACCESS:
      default:
	_nntp_error_response (s, _("Authorization failed."));
	return -1;
     }
   return 0;
}

static int _nntp_connect_server (NNTP_Type *s)
{
   if (s->tcp != NULL)
     sltcp_close (s->tcp);

   slrn_message_now (_("Connecting to host %s ..."), s->host);
   if (Slrn_Debug_Fp != NULL)
     fputs ("!Connecting to server...\n", Slrn_Debug_Fp);

   if (NULL == (s->tcp = sltcp_open_connection (s->host, s->port, s->use_ssh)))
     return -1;

   s->init_state = 1;
   s->number_bytes_received = 0;
   NNTP_Try_Authentication = 2;

   /* Read logon message. */
   switch (nntp_get_server_response (s))
     {
      case OK_CANPOST:
	s->can_post = 1;
	break;

      case OK_NOPOST:
	s->can_post = 0;
	break;

      default:
	goto failed;
     }

   /* Try to identify INN */
   if (NULL != strstr (s->rspbuf, "INN"))
     s->sv_id = SERVER_ID_INN;
   else
     s->sv_id = SERVER_ID_UNKNOWN;

   if ((-1 == nntp_server_cmd (s, "MODE READER"))
       || (ERR_ACCESS == s->code))
     goto failed;

   if (s->code == OK_NOPOST)
     s->can_post = 0;

   if (-1 == nntp_authorization (s, Slrn_Force_Authentication))
     return -1;

   slrn_message_now (_("Connected to host.  %s"),
		     (s->can_post ? _("Posting ok.") : _("Posting NOT ok.")));

   return 0;

   failed:

   _nntp_error_response (s, _("Failed to initialize server"));
   if (Slrn_Debug_Fp != NULL)
     fputs ("!Server connect failed.\n", Slrn_Debug_Fp);

   (void) sltcp_close (s->tcp);
   s->tcp = NULL;
   return -1;
}

#ifndef NNTPSERVER_FILE
# define NNTPSERVER_FILE NULL
#endif

static char *_nntp_getserverbyfile (char *file)
{
   FILE *fp;
   char *host;
   static char buf[256];

   host = getenv("NNTPSERVER");

   if (host != NULL)
     {
	slrn_strncpy (buf, host, sizeof (buf));
	return buf;
     }

   if (file == NULL)
     {
#ifdef NNTPSERVER_NAME
	slrn_strncpy (buf, NNTPSERVER_NAME, sizeof (buf));
	return buf;
#else
	return NULL;
#endif
     }

   if (NULL == (fp = fopen(file, "r")))
     return NULL;

   while (NULL != fgets(buf, sizeof (buf), fp))
     {
	char *b;

	b = slrn_skip_whitespace (buf);
	if ((*b == 0) || (*b == '#'))
	  continue;

	slrn_trim_string (b);
	(void) fclose(fp);
	return b;
     }

   (void) fclose(fp);
   return NULL;    /* No entry */
}

char *nntp_get_server_name (void)
{
   char *host;

   if (NULL != (host = _nntp_getserverbyfile(NNTPSERVER_FILE)))
     return host;

   slrn_stderr_strcat ("\n", _("You should set the NNTPSERVER environment variable to your server name."), "\n", NULL);
#ifdef VMS
   slrn_stderr_strcat (_("Example: $ define/job NNTPSERVER my.news.server"), "\n", NULL);
#else
# if defined(IBMPC_SYSTEM)
   slrn_stderr_strcat (_("Example: set NNTPSERVER=my.news.server"), "\n", NULL);
# else
   slrn_stderr_strcat (_("Example (csh): setenv NNTPSERVER my.news.server"), "\n",
		       _("Example (sh) : NNTPSERVER='my.news.server' && export NNTPSERVER"), "\n", NULL);
# endif
#endif
   slrn_stderr_strcat (_("For now, I'm going to try \"localhost\" as the default..."), "\n", NULL);
   return "localhost";
}

/* In general, host has the form: "address:port" or "[address]:port" (to
 * support IPv6 literal addresses).  If port is non-negative use its value
 * despite the value coded in the hostname. */
static int _nntp_setup_host_and_port (char *host, int port, NNTP_Type *s)
{
   char *a, *b, *bmax;
   int def_port = 119;
   char *def_service = "nntp";
   int quoteaddr=0;

   a = host;
   if (0 == strncmp (a, "snews://", 8))
     {
	s->use_ssh = 1;
	a += 8;
     }
   else if (0 == strncmp (a, "news://", 7))
     {
	s->use_ssh = 0;
	a += 7;
     }

   b = s->host;
   bmax = b + NNTP_MAX_HOST_LEN;
   while ((*a != 0) && ((*a != ':') || (quoteaddr)) && (b < bmax))
     {
	if(*a == '[')
	  {
	     quoteaddr=1;
	     a++;
	  }
	else if(*a == ']')
	  {
	     quoteaddr=0;
	     a++;
	  }
	else
	  *b++ = *a++;
     }
   *b = 0;
   slrn_strncpy (s->name, host, NNTP_MAX_HOST_LEN);

   if (s->use_ssh)
     {
	def_port = 563;
	def_service = "nntps";
     }

   if (port <= 0)
     {
	if (((*a != ':') || (!(port = atoi (a+1))
	     && (-1 == (port = sltcp_map_service_to_port (a+1)))))
	    && (-1 == (port = sltcp_map_service_to_port (def_service))))
	  port = def_port;
     }
   s->port = port;

   return 0;
}

NNTP_Type *nntp_open_server (char *host, int port)
{
   NNTP_Type *s;

   if (host == NULL)
     {
	host = nntp_get_server_name ();
	if (host == NULL)
	  return NULL;
     }

   if (NULL == (s = nntp_allocate_nntp ()))
     return NULL;

   (void) _nntp_setup_host_and_port (host, port, s);

   if (-1 == _nntp_connect_server (s))
     {
	_nntp_deallocate_nntp (s);
	return NULL;
     }

   return s;
}

/* This function removes the leading '.' from dot-escaped lines */
int nntp_read_line (NNTP_Type *s, char *buf, unsigned int len)
{
   if (-1 == nntp_gets_server (s, buf, len))
     return -1;

   if (buf[0] == '.')
     {
	char ch = buf[1];
	if (ch == 0)
	  return 0;
	if (ch == '.')
	  {
	     char *b = buf;
	     do
	       {
		  ch = b[1];
		  *b++ = ch;
	       }
	     while (ch != 0);
	  }
     }
   return 1;
}

int nntp_discard_output (NNTP_Type *s)
{
   char buf [NNTP_BUFFER_SIZE];
   int status;

   while (1 == (status = nntp_read_line (s, buf, sizeof (buf))))
     continue;

   return status;
}

int nntp_reconnect_server (NNTP_Type *s)
{
   unsigned int flag;
   int status;

   if (s == NULL)
     return -1;

   nntp_disconnect_server (s);

   flag = s->flags & NNTP_RECONNECT_OK;
   s->flags |= NNTP_RECONNECT_OK;

   status = _nntp_reconnect (s);

   if (flag == 0) s->flags &= ~NNTP_RECONNECT_OK;

   return status;
}

int nntp_check_connection (NNTP_Type *s)
{
   if ((s == NULL) || (-1 == sltcp_get_fd (s->tcp)))
     return -1;

   return 0;
}

void nntp_disconnect_server (NNTP_Type *s)
{
   if (s == NULL) return;

   if (Slrn_Debug_Fp != NULL)
     fputs ("!Disconnecting from server.\n", Slrn_Debug_Fp);

   sltcp_close_socket (s->tcp);
}

static int _nntp_probe_server (NNTP_Type *s, char *cmd)
{
   if (-1 == nntp_server_cmd (s, cmd))
     return -1;

   if (ERR_COMMAND == s->code)
     return 0;

   return 1;
}

#define PROBE_XCMD(s, var, cmd) \
   (((var) != -1) ? (var) : ((var) = _nntp_probe_server ((s),(cmd))))

int nntp_has_cmd (NNTP_Type *s, char *cmd)
{
   char buf [NNTP_BUFFER_SIZE];
   if (!strncmp (cmd, "XHDR", 4))
     {
	int ret;

	if (s->can_xhdr != -1)
	  return s->can_xhdr;

	ret = nntp_server_cmd (s, cmd);

	switch (ret)
	  {
	   default:
	     s->can_xhdr = 0;
	     break;

	   case ERR_CMDSYN:
	   case ERR_NCING:
	     s->can_xhdr = 1;
	     break;

	   case ERR_COMMAND:
	   case ERR_ACCESS:
	     s->can_xhdr = 0;
	     break;

	   case OK_HEAD:
	     s->can_xhdr = 1;
	     if (1 == nntp_read_line (s, buf, sizeof (buf)))
	       {
		  char *p = buf;
		  nntp_discard_output (s);
		  while (*p && (*p != ' ')) p++;
		  while (*p == ' ') p++;
		  if (!strcmp (p, "(none)"))
		    s->can_xhdr = 0;
	       }
	     break;
	  }
	return s->can_xhdr;
     }

   if (!strcmp (cmd, "XPAT"))
     return PROBE_XCMD(s, s->can_xpat, cmd);

   if (!strcmp (cmd, "XOVER"))
     return PROBE_XCMD(s, s->can_xover, cmd);

   return _nntp_probe_server (s, cmd);
}

static int _nntp_num_or_msgid_cmd (NNTP_Type *s, char *cmd, NNTP_Artnum_Type n, char *msgid)
{
   if ((n != -1) && ((Slrn_Broken_Xref == 0) || (msgid == NULL)))
     return nntp_server_vcmd (s, ("%s " NNTP_FMT_ARTNUM), cmd, n);
   else if (msgid == NULL)
     return nntp_server_cmd(s, cmd);
   else
     return nntp_server_vcmd (s, "%s %s", cmd, msgid);
}

int nntp_head_cmd (NNTP_Type *s, NNTP_Artnum_Type n, char *msgid, NNTP_Artnum_Type *real_id)
{
   int status;

   status = _nntp_num_or_msgid_cmd (s, "HEAD", n, msgid);
   if ((status == OK_HEAD) && (real_id != NULL))
     *real_id = NNTP_STR_TO_ARTNUM(s->rspbuf + 4);
   return status;
}

int nntp_xover_cmd (NNTP_Type *s, NNTP_Artnum_Type min, NNTP_Artnum_Type max)
{
   return nntp_server_vcmd (s, "XOVER " NNTP_FMT_ARTRANGE, min, max);
}

int nntp_xhdr_cmd (NNTP_Type *s, char *field, NNTP_Artnum_Type min, NNTP_Artnum_Type max)
{
   return nntp_server_vcmd (s, "XHDR %s " NNTP_FMT_ARTRANGE, field, min, max);
}

int nntp_article_cmd (NNTP_Type *s, NNTP_Artnum_Type n, char *msgid)
{
   return _nntp_num_or_msgid_cmd (s, "ARTICLE", n, msgid);
}

int nntp_next_cmd (NNTP_Type *s, NNTP_Artnum_Type *n)
{
   int status;

   if ((OK_NEXT == (status = nntp_server_cmd (s, "NEXT")))
       && (n != NULL))
     *n = NNTP_STR_TO_ARTNUM (s->rspbuf + 4);

   return status;
}

int nntp_select_group (NNTP_Type *s, char *name, NNTP_Artnum_Type *minp, NNTP_Artnum_Type *maxp)
{
   NNTP_Artnum_Type estim;
   NNTP_Artnum_Type min, max;

   switch (nntp_server_vcmd (s, "GROUP %s", name))
     {
      case -1:
	return -1;

      case OK_GROUP:

	if (3 != sscanf (s->rspbuf + 4, NNTP_FMT_ARTNUM_3, &estim, &min, &max))
	  return -1;

	if (minp != NULL) *minp = min;
	if (maxp != NULL) *maxp = max;

	slrn_strncpy (s->group_name, name, NNTP_MAX_GROUP_NAME_LEN);
	break;

      case ERR_ACCESS:
      default:
	break;
     }

   if (NNTP_Try_Authentication == 2)
     NNTP_Try_Authentication = 1;
   return s->code;
}

int nntp_refresh_groups (NNTP_Type *s, Slrn_Group_Range_Type *gr, int n)
{
   int reconnect, i, prev = 0, max_tries = 3;

   start_over:

   /* If we might still need authentication, use nntp_select_group first. */
   if ((NNTP_Try_Authentication == 2) && n)
     {
	int status;
	NNTP_Try_Authentication = 1;

	status = nntp_select_group (s, gr->name, &(gr->min), &(gr->max));
	if (status == -1) return -1;
	else if (status == ERR_NOGROUP)
	  gr->min = -1;
	n--; gr++;
     }

   max_tries--;
   reconnect = s->flags & NNTP_RECONNECT_OK;
   s->flags &= ~NNTP_RECONNECT_OK;
   /* disallow reconnect for the moment as we batch commands */

   i = prev;
   while (i < n)
     {
	if (-1 == nntp_start_server_vcmd (s, "GROUP %s", gr[i].name))
	  {
	     s->flags |= reconnect;
	     if ((max_tries == 0) || (-1 == _nntp_reconnect (s)))
	       return -1;
	     goto start_over;
	  }
	i++;
     }
   i = prev;
   while (i < n)
     {
	NNTP_Artnum_Type estim;

	prev = i;
	switch (nntp_get_server_response (s))
	  {
	   case -1:
	     s->flags |= reconnect;
	     if ((max_tries == 0) || (-1 == _nntp_reconnect (s)))
	       return -1;
	     goto start_over;

	   case ERR_NOAUTH:
	     s->flags |= reconnect;
	     while (++i < n)
	       nntp_get_server_response (s);
	     if (NNTP_Try_Authentication)
	       {
		  NNTP_Try_Authentication = 0;
		  if (-1 == nntp_authorization (s, 1))
		    return -1;
		  max_tries++;
		  goto start_over;
	       }
	     else
	       return -1;

	   case OK_GROUP:
	     if (3 != sscanf (s->rspbuf + 4, NNTP_FMT_ARTNUM_3, &estim,
			      &(gr[i].min), &(gr[i].max)))
	       {
		  s->flags |= reconnect;
		  while (++i < n)
		    nntp_get_server_response (s);
		  return -1;
	       }
	     slrn_strncpy (s->group_name, gr[i].name, NNTP_MAX_GROUP_NAME_LEN);
	     break;

	   case ERR_FAULT:
	   case OK_GOODBYE: /* SN */
	     if (max_tries == 2) /* might be a timeout we didn't recognize */
	       {
		  nntp_disconnect_server (s);
		  (void) check_connect_lost_hook (s);
		  max_tries = 0;
		  goto start_over;
	       }
	     /* Fall through */

	   default:
	     gr[i].min = -1;
	     break;
	  }
	i++;
     }
   s->flags |= reconnect;
   return 0;
}

int nntp_post_cmd (NNTP_Type *s)
{
   return _nntp_num_or_msgid_cmd (s, "POST", -1, NULL);
}

int nntp_end_post (NNTP_Type *s)
{
   if (-1 == nntp_puts_server (s, "."))
     return -1;

   return nntp_get_server_response (s);
}

int nntp_xpat_cmd (NNTP_Type *s, char *hdr, NNTP_Artnum_Type rmin, NNTP_Artnum_Type rmax, char *pat)
{
   if (0 == PROBE_XCMD(s, s->can_xpat, "XPAT"))
     return ERR_COMMAND;

   return nntp_server_vcmd (s, ("XPAT %s " NNTP_FMT_ARTRANGE " *%s*"),
			    hdr, rmin, rmax, pat);
}

/* hdr should not include ':' */
int nntp_one_xhdr_cmd (NNTP_Type *s, char *hdr, NNTP_Artnum_Type num, char *buf, unsigned int buflen)
{
   char tmpbuf[1024];
   int found;
   unsigned int colon;
   int status;

   status = PROBE_XCMD(s, s->can_xhdr, "XHDR");
   if (status == -1)
     return -1;

   if (status == 1)
     {
	char *b, ch;

	if (-1 == nntp_server_vcmd (s, ("XHDR %s " NNTP_FMT_ARTNUM), hdr, num))
	  return -1;

	if (OK_HEAD != s->code)
	  {
	     /* It comes as no surprise that Micro$oft apparantly makes
	      * buggy servers too.  Sigh.
	      */
	     if (s->code != 224) return -1;
	     s->code = OK_HEAD;
	  }

	status = nntp_read_line (s, tmpbuf, sizeof(tmpbuf));
	if (status != 1)
	  return -1;

	/* skip past article number */
	b = tmpbuf;
	while (((ch = *b++) >= '0') && (ch <= '9'))
	  ;
	strncpy (buf, b, buflen - 1);
	buf[buflen - 1] = 0;

	/* I should handle multi-line returns but I doubt that there will be
	 * any for our use of xhdr
	 */
	(void) nntp_discard_output (s);
	return 0;
     }

   /* Server does not have XHDR so, use HEAD */

   if (-1 == nntp_head_cmd (s, num, NULL, NULL))
     return -1;

   if (s->code != OK_HEAD)
     return -1;

   found = 0;
   colon = strlen (hdr);

   while (1 == (status = nntp_read_line (s, tmpbuf, sizeof (tmpbuf))))
     {
	char *b;

	if (found
	    || slrn_case_strncmp ( tmpbuf,  hdr, colon)
	    || (tmpbuf[colon] != ':'))
	  continue;

	found = 1;

	b = tmpbuf + (colon + 1);      /* skip past colon */
	if (*b == ' ') b++;
	strncpy (buf, b, buflen - 1);
	buf[buflen - 1] = 0;
     }

   return status;
}

int nntp_list (NNTP_Type *s, char* what)
{
   return nntp_server_vcmd (s, "LIST %s", what);
}

int nntp_list_newsgroups (NNTP_Type *s)
{
   return nntp_server_vcmd (s, "LIST NEWSGROUPS");
}

int nntp_list_active_cmd (NNTP_Type *s, char* pat)
{
   if (pat == NULL)
     return nntp_server_cmd (s, "LIST");
   else
     return nntp_server_vcmd (s, "LIST ACTIVE %s", pat);
}

int nntp_listgroup (NNTP_Type *s, char *group)
{
   return nntp_server_vcmd (s, "LISTGROUP %s", group);
   /*  OK_GROUP desired */
}

int nntp_body_cmd (NNTP_Type *s, NNTP_Artnum_Type n, char *msgid)
{
   return _nntp_num_or_msgid_cmd (s, "BODY", n, msgid);
}

char *nntp_read_and_malloc (NNTP_Type *s)
{
   char line [NNTP_BUFFER_SIZE];
   char *mbuf;
   unsigned int buffer_len, buffer_len_max;
   int status;

   mbuf = NULL;
   buffer_len_max = buffer_len = 0;

   while (1 == (status = nntp_read_line (s, line, sizeof(line))))
     {
	unsigned int len;

	len = strlen (line);

	if (len + buffer_len + 4 > buffer_len_max)
	  {
	     char *new_mbuf;

	     buffer_len_max += 4096 + len;
	     new_mbuf = slrn_realloc (mbuf, buffer_len_max, 0);

	     if (new_mbuf == NULL)
	       {
		  slrn_free (mbuf);
		  nntp_discard_output (s);
		  return NULL;
	       }
	     mbuf = new_mbuf;
	  }

	strcpy (mbuf + buffer_len, line); /* safe */
	buffer_len += len;
	mbuf [buffer_len++] = '\n';
	mbuf [buffer_len] = 0;
     }

   if (status == 0)
     {
	if (mbuf == NULL)
	  mbuf = slrn_strmalloc ("", 0);

	return mbuf;
     }

   slrn_free (mbuf);
   return NULL;
}

char *nntp_map_code_to_string (int code)
{
   switch (code)
     {
      case INF_HELP:	/* 100 */
	return _("Help text on way");
      case INF_AUTH:	/* 180 */
	return _("Authorization capabilities");
      case INF_DEBUG:	/* 199 */
	return _("Debug output");
      case OK_CANPOST:	/* 200 */
	return _("Hello; you can post");
      case OK_NOPOST:	/* 201 */
	return _("Hello; you can't post");
      case OK_SLAVE:	/* 202 */
	return _("Slave status noted");
      case OK_GOODBYE:	/* 205 */
	return _("Closing connection");
      case OK_GROUP:	/* 211 */
	return _("Group selected");
      case OK_GROUPS:	/* 215 */
	return _("Newsgroups follow");
      case OK_ARTICLE:	/* 220 */
	return _("Article (head & body) follows");
      case OK_HEAD:	/* 221 */
	return _("Head follows");
      case OK_BODY:	/* 222 */
	return _("Body follows");
      case OK_NOTEXT:	/* 223 */
	return _("No text sent -- stat, next, last");
      case OK_NEWNEWS:	/* 230 */
	return _("New articles by message-id follow");
      case OK_NEWGROUPS:/* 231 */
	return _("New newsgroups follow");
      case OK_XFERED:	/* 235 */
	return _("Article transferred successfully");
      case OK_POSTED:	/* 240 */
	return _("Article posted successfully");
      case OK_AUTHSYS:	/* 280 */
	return _("Authorization system ok");
      case OK_AUTH:	/* 281 */
	return _("Authorization (user/pass) ok");
      case OK_XGTITLE:	/* 282 */
	return _("Ok, XGTITLE info follows");
      case OK_XOVER:	/* 224 */
	return _("ok -- overview data follows");
      case CONT_XFER:	/* 335 */
	return _("Continue to send article");
      case CONT_POST:	/* 340 */
	return _("Continue to post article");
      case NEED_AUTHINFO:/* 380 */
	return _("authorization is required");
      case NEED_AUTHDATA:/* 381 */
	return _("<type> authorization data required");
      case ERR_GOODBYE:	/* 400 */
	return _("Have to hang up for some reason");
      case ERR_NOGROUP:	/* 411 */
	return _("No such newsgroup");
      case ERR_NCING:	/* 412 */
	return _("Not currently in newsgroup");
      case ERR_NOCRNT:	/* 420 */
	return _("No current article selected");
      case ERR_NONEXT:	/* 421 */
	return _("No next article in this group");
      case ERR_NOPREV:	/* 422 */
	return _("No previous article in this group");
      case ERR_NOARTIG:	/* 423 */
	return _("No such article in this group");
      case ERR_NOART:	/* 430 */
	return _("No such article at all");
      case ERR_GOTIT:	/* 435 */
	return _("Already got that article, don't send");
      case ERR_XFERFAIL:/* 436 */
	return _("Transfer failed");
      case ERR_XFERRJCT:/* 437 */
	return _("Article rejected, don't resend");
      case ERR_NOPOST:	/* 440 */
	return _("Posting not allowed");
      case ERR_POSTFAIL:/* 441 */
	return _("Posting failed");
      case ERR_NOAUTH:	/* 480 */
	return _("authorization required for command");
      case ERR_AUTHSYS:	/* 481 */  /* ERR_XGTITLE has same code */
	return _("Authorization system invalid");
      case ERR_AUTHREJ:	/* 482 */
	return _("Authorization data rejected");
      case ERR_COMMAND:	/* 500 */
	return _("Command not recognized");
      case ERR_CMDSYN:	/* 501 */
        return _("Command syntax error");
      case ERR_ACCESS:	/* 502 */
	return _("Access to server denied");
      case ERR_FAULT:	/* 503 */
	return _("Program fault, command not performed");
      case ERR_AUTHBAD:	/* 580 */
	return _("Authorization Failed");
     }

   return _("Unknown NNTP code");
}
