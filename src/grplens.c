/* -*- mode: C; mode: fold -*- */
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
#include "config.h"
#include "slrnfeat.h"

#include <stdio.h>
#include <string.h>

#if SLRN_HAS_GROUPLENS
/* Rest of file in this #if statement */

#include <slang.h>
#include <stdarg.h>
#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif
#include <ctype.h>

#include "jdmacros.h"

#include "slrn.h"
#include "group.h"
#include "art.h"
#include "misc.h"
#include "decode.h"
#include "grplens.h"
#include "sltcp.h"
#include "util.h"
#include "server.h"
#include "snprintf.h"

#define GL_MAX_RESPONSE_LINE_LEN	1024

/*{{{ GL_Type structure */

typedef struct GL_Type /*{{{*/
{
   char *hostname;
   int port;

   char *token;
   char *pseudoname;
   int logged_in;
   SLTCP_Type *tcp;
   struct GL_Type *next;
}

/*}}}*/

GL_Type;

static GL_Type GL_Nameserver;
static GL_Type *GL_Server_List;
/*}}}*/

typedef struct /*{{{*/
{
   char *msgid;
   float pred;
   float confhigh;
   float conflow;
}

/*}}}*/
GL_Prediction_Type;

typedef struct /*{{{*/
{
   char *msgid;
   int rating;
   int saw_article;
}

/*}}}*/
GL_Rating_Type;

char *Slrn_GroupLens_Host;
int Slrn_GroupLens_Port;
char *Slrn_GroupLens_Pseudoname;

typedef struct GL_Newsgroup_List_Type /*{{{*/
{
   char *group;
   GL_Type *gl;
   struct GL_Newsgroup_List_Type *next;
}

/*}}}*/
GL_Newsgroup_List_Type;

static GL_Newsgroup_List_Type *Newsgroup_List = NULL;

/*{{{ Error codes and Error functions */

static int GL_Error;
#define GL_ERROR_UNKNOWN	       -1
#define GL_ELOGIN_UNREGISTERED		1
#define GL_ELOGIN_BUSY			2
#define GL_ELOGIN_UNAVAILABLE		3
#define GL_ELOGIN_ALREADY_LOGGEDIN	4
#define GL_ERROR_PROPER_RESPONSE	6
#define GL_ERROR_MALLOC			7
#define GL_ESERVER_WRITE		8
#define GL_ESERVER_READ			9
#define GL_ERROR_TOKEN		       10
#define GL_ERROR_NEWSGROUP	       11

typedef struct /*{{{*/
{
   char *err;
   unsigned int len;
   int errcode;
}

/*}}}*/

GL_Error_Type;

/*{{{ Utility functions */

static char *make_nstring (char *s, unsigned int len) /*{{{*/
{
   char *s1;

   if (s == NULL) return s;

   if (NULL == (s1 = (char *) malloc (len + 1)))
     {
	GL_Error = GL_ERROR_MALLOC;
	return NULL;
     }
   strncpy (s1, s, len);
   s1[len] = 0;
   return s1;
}

/*}}}*/

static char *make_string (char *s) /*{{{*/
{
   if (s == NULL) return s;
   return make_nstring (s, strlen (s));
}

/*}}}*/

static char *skip_whitespace (char *s) /*{{{*/
{
   char ch;

   while (((ch = *s) != 0) && isspace(ch)) s++;
   return s;
}

/*}}}*/

static char *skip_nonwhitespace (char *s) /*{{{*/
{
   char ch;

   while (((ch = *s) != 0) && (0 == isspace(ch))) s++;
   return s;
}

/*}}}*/

/*}}}*/

static char *gl_get_error (void) /*{{{*/
{
   switch (GL_Error)
     {
      case GL_ELOGIN_UNREGISTERED: return _("User is Unregistered");
      case GL_ELOGIN_BUSY: return _("Service is Busy");
      case GL_ELOGIN_UNAVAILABLE: return _("Service Unavailable");
      case GL_ELOGIN_ALREADY_LOGGEDIN: return _("Already logged in");
      case GL_ERROR_PROPER_RESPONSE: return _("Server failed to return proper response");
      case GL_ERROR_MALLOC: return _("Not enough memory");
      case GL_ESERVER_WRITE: return _("Error writing to server");
      case GL_ESERVER_READ: return _("Error reading from server");
      case GL_ERROR_TOKEN: return _("Token is invalid");
      case GL_ERROR_NEWSGROUP: return _("Newsgroup not supported");
     }

   return _("Unknown Error");
}

/*}}}*/

static void close_gl (GL_Type *gl) /*{{{*/
{
   if (gl == NULL) return;
   sltcp_close (gl->tcp);
   gl->tcp = NULL;
}

/*}}}*/

static int get_gl_response (GL_Type *gl, char *buf, unsigned int len) /*{{{*/
{
   char *b;

   (void) len;

   if (-1 == sltcp_fgets (gl->tcp, buf, GL_MAX_RESPONSE_LINE_LEN))
     {
	GL_Error = GL_ESERVER_READ;
	close_gl (gl);
	return -1;
     }

   b = buf + strlen (buf);
   if (b != buf)
     {
	b--;
	if (*b == '\n') *b = 0;
	if (b != buf)
	  {
	     b--;
	     if (*b == '\r') *b = 0;
	  }
     }
   return 0;
}

/*}}}*/

static void handle_error_response (GL_Type *gl, GL_Error_Type *tbl) /*{{{*/
{
   char *s, *s1;
   char buf [GL_MAX_RESPONSE_LINE_LEN];

   if (-1 == get_gl_response (gl, buf, sizeof (buf)))
     return;

   if (tbl == NULL)
     return;

   s = skip_whitespace (buf);

   GL_Error = GL_ERROR_UNKNOWN;

   while (*s != 0)
     {
	unsigned int len;
	GL_Error_Type *t;

	s1 = skip_nonwhitespace (s);

	len = s1 - s;

	t = tbl;
	while (t->err != NULL)
	  {
	     if ((t->len == len)
		 && (0 == slrn_case_strncmp ((unsigned char *)s, (unsigned char *) t->err, len)))
	       {
		  GL_Error = t->errcode;
		  if (GL_Error == GL_ERROR_TOKEN)
		    {
		       if (gl->token != NULL)
			 free (gl->token);
		       gl->token = NULL;
		       gl->logged_in = 0;
		    }
		  break;
	       }
	     t++;
	  }
	s = skip_whitespace (s1);
     }
}

/*}}}*/

static int is_error (char *buf) /*{{{*/
{
   if (!slrn_case_strncmp ((unsigned char *) buf, (unsigned char *) "ERROR", 5))
     return 1;

   return 0;
}

/*}}}*/

/*}}}*/

/*{{{ parse_keyword_eqs_value */

/* Scan buf looking for keyword=VALUE */
static int parse_keyword_eqs_value (char *buf, char *kw, char **value, unsigned int *len) /*{{{*/
{
   char *b, *b1, *v, *v1;
   char ch;
   unsigned int kwlen;

   *value = NULL;

   kwlen = strlen (kw);

   b = buf;
   while (1)
     {
	b = skip_whitespace (b);
	if (*b == 0) return -1;

	b1 = b;
	while (((ch = *b1) != 0) && (0 == isspace (ch)) && (ch != '='))
	  b1++;

	if (ch == 0) return -1;
	if (ch != '=')
	  {
	     b = b1;
	     continue;
	  }

	v = b1 + 1;
	if (*v == '"')
	  {
	     v++;
	     v1 = v;
	     while (((ch = *v1) != 0) && (ch != '"')) v1++;
	  }
	else v1 = skip_nonwhitespace (v);

	if ((b + kwlen == b1)
	    && (0 == slrn_case_strncmp ((unsigned char *) kw, (unsigned char *) b, kwlen)))
	  {
	     *value = v;
	     *len = v1 - v;
	     return 0;
	  }

	if (*v1 == '"') v1++;
	b = v1;
     }
}

/*}}}*/

/*}}}*/
/*{{{ low level GL server functions */

static int send_gl_line (GL_Type *gl, char *msg, int flush) /*{{{*/
{
   if (msg == NULL) return 0;

   if ((-1 == sltcp_fputs (gl->tcp, msg))
       || (-1 == sltcp_fputs (gl->tcp, "\r\n"))
       || (flush && (-1 == sltcp_flush_output (gl->tcp))))
     {
	GL_Error = GL_ESERVER_WRITE;
	close_gl (gl);
	return -1;
     }

   return 0;
}

/*}}}*/

static int connect_to_gl_host (GL_Type *gl, char *cmd) /*{{{*/
{
   char buf [GL_MAX_RESPONSE_LINE_LEN];
   char *host;
   int port;

   GL_Error = 0;

   port = gl->port;
   if (NULL == (host = gl->hostname))
     {
	host = Slrn_GroupLens_Host;
	port = Slrn_GroupLens_Port;
     }

   if ((host == NULL) || (port <= 0))
     {
	GL_Error = GL_ERROR_UNKNOWN;
	return -1;
     }

   if (NULL == (gl->tcp = sltcp_open_connection (host, port, 0)))
     return -1;

   /* We should be able to read OK ...  If not, we failed to make proper
    * connection.
    */

   if (-1 == get_gl_response (gl, buf, sizeof (buf)))
     return -1;

   if (0 != slrn_case_strncmp ((unsigned char *) buf, (unsigned char *) "OK", 2))
     {
	GL_Error = GL_ERROR_PROPER_RESPONSE;
	close_gl (gl);
	return -1;
     }

   return send_gl_line (gl, cmd, 1);
}

/*}}}*/

static int start_command (GL_Type *gl, char *fmt, ...) /*{{{*/
{
   va_list ap;
   int ret;

   if (-1 == connect_to_gl_host (gl, NULL))
     return -1;

   va_start(ap, fmt);
   ret = sltcp_vfprintf (gl->tcp, fmt, ap);
   va_end (ap);

   if ((ret == -1)
       || (-1 == sltcp_fputs (gl->tcp, "\r\n")))
     {
	close_gl (gl);
	return -1;
     }

   return 0;
}

/*}}}*/

static int check_for_error_response (GL_Type *gl, GL_Error_Type *tbl) /*{{{*/
{
   char response [GL_MAX_RESPONSE_LINE_LEN];

   if (-1 == get_gl_response (gl, response, sizeof (response)))
     return -1;

   if (is_error (response))
     {
	handle_error_response (gl, tbl);
	close_gl (gl);
	return -1;
     }
   return 0;
}

/*}}}*/

/* Open connection, send command check for error and handle it via table
 * if error occurs.
 */
static int gl_send_command (GL_Type *gl, GL_Error_Type *tbl, char *fmt, ...) /*{{{*/
{
   va_list ap;
   int ret;

   if (-1 == connect_to_gl_host (gl, NULL))
     return -1;

   va_start(ap, fmt);
   ret = sltcp_vfprintf (gl->tcp, fmt, ap);
   va_end (ap);

   if ((ret == -1)
       || (-1 == sltcp_flush_output (gl->tcp)))
     {
	GL_Error = GL_ESERVER_WRITE;
	close_gl (gl);
	return -1;
     }

   return check_for_error_response (gl, tbl);
}

/*}}}*/

/*}}}*/

/*{{{ login functions */

static GL_Error_Type Login_Error_Table [] = /*{{{*/
{
   {":busy",		5,	GL_ELOGIN_BUSY},
   {":unavailable",	12,	GL_ELOGIN_UNAVAILABLE},
   {":unregistered",	13,	GL_ELOGIN_UNREGISTERED},
   {":alreadyLogin",	13,	GL_ELOGIN_ALREADY_LOGGEDIN},
   {NULL, 0, 0}
};

/*}}}*/

static int login (GL_Type *gl) /*{{{*/
{
   char response [GL_MAX_RESPONSE_LINE_LEN];
   char *value;
   unsigned int value_len;

   gl->logged_in = 0;

   if (-1 == gl_send_command (gl, Login_Error_Table, "LOGIN %s :client=\"slrn %s\"\r\n",
			      gl->pseudoname, Slrn_Version))
     return -1;

   /* parse response to get token, etc... */
   if (-1 == get_gl_response (gl, response, sizeof (response)))
     return -1;

   /* CLose server then parse response. */
   close_gl (gl);

   /* expecting 1 or more fields of:
    * :token=SOMETHING
    * :version=SOMETHING
    * :rkeys="BLA BLA BLA"
    * :pkeys="BLA BLA"
    */

   /* We only require token. */
   if (-1 == parse_keyword_eqs_value (response, ":token", &value, &value_len))
     {
	GL_Error = GL_ERROR_PROPER_RESPONSE;
	return -1;
     }

   if (NULL == (gl->token = make_nstring (value, value_len)))
     return -1;

   gl->logged_in = 1;

   return 0;
}

/*}}}*/

static int validate_token (GL_Type *gl) /*{{{*/
{
   if ((gl->token == NULL) || (gl->logged_in == 0))
     return login (gl);

   return 0;
}

/*}}}*/

/*}}}*/
/*{{{ logout functions */

static void logout (GL_Type *gl) /*{{{*/
{
   if (gl->logged_in == 0) return;

   (void) gl_send_command (gl, NULL, "LOGOUT %s\r\n", gl->token);
   close_gl (gl);
}

/*}}}*/

/*}}}*/

static int gl_open_predictions (GL_Type *gl, char *group) /*{{{*/
{
   if (-1 == validate_token (gl))
     return -1;

   if (-1 == start_command (gl, "GetPredictions %s %s", gl->token, group))
     return -1;

   return 0;
}

/*}}}*/

static int terminate_command (GL_Type *gl, GL_Error_Type *tbl) /*{{{*/
{
   if (-1 == send_gl_line (gl, ".", 1))
     return -1;

   return check_for_error_response (gl, tbl);
}

/*}}}*/

static int gl_want_prediction (GL_Type *gl, char *msgid) /*{{{*/
{
   return send_gl_line (gl, msgid, 0);
}

/*}}}*/

static GL_Error_Type Predictions_Error_Table [] = /*{{{*/
{
   {":invalidToken",	13,	GL_ERROR_TOKEN},
   {":invalidGroup",	13,	GL_ERROR_NEWSGROUP},
   {NULL, 0, 0}
};

/*}}}*/

static int gl_close_predictions (GL_Type *gl, void (*f) (GL_Prediction_Type *)) /*{{{*/
{
   char buf [GL_MAX_RESPONSE_LINE_LEN];
   GL_Prediction_Type st;

   if (-1 == terminate_command (gl, Predictions_Error_Table))
     return -1;

   while (-1 != get_gl_response (gl, buf, sizeof (buf)))
     {
	char *value;
	unsigned int value_len;
	int ok;

	ok = 0;
	if ((*buf == '.') && (buf[1] == 0))
	  {
	     close_gl (gl);
	     return 0;
	  }

	st.pred = -1.0;
	st.conflow = -1.0;
	st.confhigh = -1.0;

	if (0 == parse_keyword_eqs_value (buf, ":nopred", &value, &value_len))
	  continue;

	if ((0 == parse_keyword_eqs_value (buf, ":pred", &value, &value_len))
	    && (1 == sscanf (value, "%f", &st.pred)))
	  ok++;

	if ((0 == parse_keyword_eqs_value (buf, ":conflow", &value, &value_len))
	    && (1 == sscanf (value, "%f", &st.conflow)))
	  ok++;

	if ((0 == parse_keyword_eqs_value (buf, ":confhigh", &value, &value_len))
	    && (1 == sscanf (value, "%f", &st.confhigh)))
	  ok++;

	if (ok && (st.pred > 0.0))
	  {
	     char *b;
	     /* Now get message id.  It is first thing on line. */
	     st.msgid = skip_whitespace (buf);
	     b = skip_nonwhitespace (st.msgid);
	     *b = 0;

	     if (*st.msgid == '<') (*f) (&st);
	  }
     }
   return -1;
}

/*}}}*/

static int gl_open_ratings (GL_Type *gl, char *group) /*{{{*/
{
   if (-1 == validate_token (gl))
     return -1;

   if (-1 == start_command (gl, "PutRatings %s %s", gl->token, group))
     return -1;
   return 0;
}

/*}}}*/

static int gl_put_rating (GL_Type *gl, GL_Rating_Type *rt) /*{{{*/
{
   char *buf;
   int retval;

   if (rt->rating <= 0) return 0;
   buf = slrn_strdup_printf ("%s :rating=%4.2f :sawHeader=1%s",
			     rt->msgid, (float) rt->rating,
			     rt->saw_article ? " :sawArticle=1" : "");
   retval = send_gl_line (gl, buf, 0);
   slrn_free (buf);

   return retval;
}

/*}}}*/

static int gl_close_ratings (GL_Type *gl) /*{{{*/
{
   if (-1 == terminate_command (gl, Predictions_Error_Table))
     return -1;

   close_gl (gl);
   return 0;
}

/*}}}*/

int Slrn_Use_Group_Lens = 0;

static void do_error (void) /*{{{*/
{
   slrn_error (_("Failed: %s"), gl_get_error ());
}

/*}}}*/

int slrn_grouplens_add_group (char *group) /*{{{*/
{
   GL_Newsgroup_List_Type *g;

   if (group == NULL) return -1;
   g = (GL_Newsgroup_List_Type *) malloc (sizeof (GL_Newsgroup_List_Type));
   if (g == NULL)
     return -1;

   memset ((char *) g, 0, sizeof (GL_Newsgroup_List_Type));

   g->group = slrn_safe_strmalloc (group);
   g->next = Newsgroup_List;
   Newsgroup_List = g;

   return 0;
}

/*}}}*/

static void free_gl (GL_Type *gl) /*{{{*/
{
   if (gl == NULL)
     return;

   if (gl->pseudoname != NULL)
     free (gl->pseudoname);

   if (gl->hostname != NULL)
     free (gl->hostname);

   if (gl->token != NULL)
     free (gl->token);

   free (gl);
}

/*}}}*/

static GL_Type *create_gl_for_group (char *group) /*{{{*/
{
   GL_Type *gl;
   char response [GL_MAX_RESPONSE_LINE_LEN];
   unsigned int value_len;
   char *value;
   int port;

   gl = &GL_Nameserver;

   if (-1 == gl_send_command (gl, NULL,
			      "lookupbroker :partitionname=%s\r\n", group))
     return NULL;

   if (-1 == get_gl_response (gl, response, sizeof(response)))
     return NULL;

   close_gl (gl);

   /* We expect something like:
    * lookupbroker :partitionname=comp.unix.questions \
    *   :host=grouplens.cs.umn.edu :port=42828  :protocol_version=a1
    */

   if (-1 == parse_keyword_eqs_value (response, ":port", &value, &value_len))
     {
	GL_Error = GL_ERROR_PROPER_RESPONSE;
	return NULL;
     }
   port = atoi (value);

   if (-1 == parse_keyword_eqs_value (response, ":host", &value, &value_len))
     {
	GL_Error = GL_ERROR_PROPER_RESPONSE;
	return NULL;
     }

   gl = GL_Server_List;
   while (gl != NULL)
     {
	if ((gl->port == port)
	    && (0 == strncmp (value, gl->hostname, value_len))
	    && (gl->hostname [value_len] == 0))
	  return gl;

	gl = gl->next;
     }

   if (NULL == (gl = (GL_Type *) malloc (sizeof (GL_Type))))
     return NULL;

   memset ((char *) gl, 0, sizeof (GL_Type));
   gl->port = port;

   if (NULL == (gl->hostname = make_nstring (value, value_len)))
     {
	free_gl (gl);
	return NULL;
     }

   if (NULL == (gl->pseudoname = make_string (Slrn_GroupLens_Pseudoname)))
     {
	free_gl (gl);
	return NULL;
     }

   gl->next = GL_Server_List;
   GL_Server_List = gl;

   return gl;
}

/*}}}*/

static GL_Type *is_group_valid (char *name) /*{{{*/
{
   GL_Newsgroup_List_Type *g;

   g = Newsgroup_List;
   while (g != NULL)
     {
	if (0 == slrn_case_strcmp ((unsigned char *)name, (unsigned char *)g->group))
	  {
	     if (g->gl == NULL)
	       g->gl = create_gl_for_group (name);
	     return g->gl;
	  }
	g = g->next;
     }
   return NULL;
}

/*}}}*/

static int gl_pseudonym_fun (unsigned int argc, char **argv)
{
   switch (argc)
     {
      case 4:
	if (slrn_case_strcmp ((unsigned char *)argv[1], (unsigned char *)"GROUP"))
	  break;
	return 0;

      case 3:
	if (slrn_case_strcmp ((unsigned char *)argv[1], (unsigned char *)"DEFAULT"))
	  break;
	/* drop */
      case 2:
	slrn_free (Slrn_GroupLens_Pseudoname);
	Slrn_GroupLens_Pseudoname = slrn_safe_strmalloc (argv[argc - 1]);
	return 0;
     }

   slrn_error (_("%s usage: %s [default] PSEUDONYM"), argv[0], argv[0]);
   return -1;
}

static int gl_glnshost_fun (unsigned int argc, char **argv)
{
   if (argc != 2)
     {
	slrn_error (_("%s: expecting a single argument"), argv[0]);
	return -1;
     }
   slrn_free (Slrn_GroupLens_Host);
   Slrn_GroupLens_Host = slrn_safe_strmalloc (argv[1]);
   return 0;
}

static int gl_glnsport_fun (unsigned int argc, char **argv)
{
   if (argc != 2)
     {
	slrn_error (_("%s: expecting a single argument"), argv[0]);
	return -1;
     }
   if (1 != sscanf (argv[1], "%d", &Slrn_GroupLens_Port))
     Slrn_GroupLens_Port = -1;
   return 0;
}

typedef struct
{
#define GL_MAX_CONFIG_FILE_ARGC 10
   char *name;
   int (*fun) (unsigned int, char **);
}
GL_Config_File_Type;

static int parse_to_argc_argv (char *line, unsigned int *argc_p,
			       char **argv, unsigned int max_argc)
{
   unsigned int argc;

   argc = 0;
   while (argc <= max_argc)
     {
	line = slrn_skip_whitespace (line);
	if (*line == 0)
	  {
	     argv[argc] = NULL;
	     *argc_p = argc;
	     return 0;
	  }

	/* Later I will modify this to handle quotes. */
	argv[argc++] = line;
	while (*line
	       && (*line != ' ')
	       && (*line != '\t')
	       && (*line != '\n'))
	  line++;
	if (*line) *line++ = 0;
     }

   slrn_error (_("Too many arguments"));
   return -1;
}

static GL_Config_File_Type GL_Config [] =
{
   {"PSEUDONYM", gl_pseudonym_fun},
   {"GLNSHOST", gl_glnshost_fun},
   {"GLNSPORT", gl_glnsport_fun},
   {"BBBHOST", gl_glnshost_fun},
   {"BBBPORT", gl_glnsport_fun},
   {"DISPLAYTYPE", NULL},
   {NULL, NULL}
};

static int read_grplens_file (void) /*{{{*/
{
   char file [SLRN_MAX_PATH_LEN];
   char line [1024];
   FILE *fp;
   char *argv [GL_MAX_CONFIG_FILE_ARGC + 1];
   unsigned int argc;
   unsigned int linenum;
   GL_Config_File_Type *cfg;

   fp = slrn_open_home_file (".grouplens", "r", file, sizeof (file), 0);
   if (fp == NULL)
     {
	fp = slrn_open_home_file (".grplens", "r", file, sizeof (file), 0);
	if (fp == NULL)
	  return -1;
     }

   slrn_message (_("Reading %s"), file);
   linenum = 0;

   while (NULL != fgets (line, sizeof (line), fp))
     {
	char *b;

	linenum++;
	b = slrn_skip_whitespace (line);
	if ((*b == 0) || (*b == '#') || (*b == '%'))
	  continue;

	if (-1 == parse_to_argc_argv (b, &argc, argv, GL_MAX_CONFIG_FILE_ARGC))
	  {
	     slrn_error (_("Error processing line %u of %s"), linenum, file);
	     return -1;
	  }

	if (argc == 0)
	  continue;

	cfg = GL_Config;
	while (cfg->name != NULL)
	  {
	     if (!slrn_case_strcmp ((unsigned char *)cfg->name,
				     (unsigned char *)argv[0]))
	       break;
	     cfg++;
	  }

	if (cfg->name == NULL)
	  {
	     /* Anything else is a newsgroup */
	     (void) slrn_grouplens_add_group (argv[0]);
	     continue;
	  }

	if ((cfg->fun != NULL)
	    && (-1 == (*cfg->fun) (argc, argv)))
	  {
	     slrn_error (_("Error processing line %u of %s"), linenum, file);
	     return -1;
	  }
     }

   fclose (fp);
   return 0;
}

/*}}}*/

int slrn_init_grouplens (void) /*{{{*/
{
   Slrn_Use_Group_Lens = 0;

   (void) read_grplens_file ();

   if ((Slrn_GroupLens_Host == NULL)
       || (Slrn_GroupLens_Port <= 0)
       || (Slrn_GroupLens_Pseudoname == NULL))
     {
	return -1;
     }

   slrn_message (_("Checking to see if server %s:%d is alive..."),
		 Slrn_GroupLens_Host, Slrn_GroupLens_Port);
   if (-1 == connect_to_gl_host (&GL_Nameserver, NULL))
     return -1;
   close_gl (&GL_Nameserver);

   Slrn_Use_Group_Lens = 1;
   slrn_message (_("GroupLens support initialized."));
   return 0;
}

/*}}}*/

static void gl_close_servers (void) /*{{{*/
{
   GL_Newsgroup_List_Type *g, *gnext;
   GL_Type *gl, *glnext;

   g = Newsgroup_List;
   while (g != NULL)
     {
	gnext = g->next;
	if (g->group != NULL)
	  free (g->group);

	free (g);
	g = gnext;
     }
   Newsgroup_List = NULL;

   gl = GL_Server_List;
   while (gl != NULL)
     {
	glnext = gl->next;
	(void) logout (gl);
	free_gl (gl);
	gl = glnext;
     }
   GL_Server_List = NULL;
}

/*}}}*/

void slrn_close_grouplens (void) /*{{{*/
{
   if (Slrn_Use_Group_Lens == 0) return;
   slrn_message (_("Logging out of GroupLens servers ..."));

   Slrn_Message_Present = 0;
   Slrn_Use_Group_Lens = 0;

   gl_close_servers ();
   slrn_message ("");
}

/*}}}*/

static int Did_Rating = 0;
static int Prediction_Count;

static void prediction_callback (GL_Prediction_Type *s) /*{{{*/
{
   Slrn_Header_Type *h;
   int i;

   h = slrn_find_header_with_msgid (s->msgid);

   if (h == NULL)
     return;			       /* not supposed to happen */

   i = (int) (s->pred + 0.5);

   if (i < 0) i = 0;

   h->gl_pred = i;
   Prediction_Count++;
}

/*}}}*/

int slrn_get_grouplens_scores (void) /*{{{*/
{
   Slrn_Header_Type *h;
   int ret;
   GL_Type *gl;

   if ((Slrn_Use_Group_Lens == 0)
       || (Slrn_First_Header == NULL))
     return -1;

   if (NULL == (gl = is_group_valid (Slrn_Current_Group_Name)))
     return -1;

   Prediction_Count = 0;

   slrn_message_now (_("Getting GroupLens predictions from %s:%d..."),
		     gl->hostname, gl->port);

   if (-1 == gl_open_predictions (gl, Slrn_Current_Group_Name))
     {
	do_error ();
	return -1;
     }

   h = Slrn_First_Header;

   ret = 0;
   while (h != NULL)
     {
	if (ret == 0) ret = gl_want_prediction (gl, h->msgid);
	h = h->real_next;
     }

   if (ret == -1)
     {
	do_error ();
	return -1;
     }

   if (-1 == gl_close_predictions (gl, prediction_callback))
     {
	do_error ();
	return -1;
     }

   return Prediction_Count;
}

/*}}}*/

int slrn_put_grouplens_scores (void) /*{{{*/
{
   Slrn_Header_Type *h;
   GL_Type *gl;

   if (Slrn_Use_Group_Lens == 0) return 0;
   if (Did_Rating == 0) return 0;
   Did_Rating = 0;

   h = Slrn_First_Header;
   if (h == NULL) return 0;

   if (NULL == (gl = is_group_valid (Slrn_Current_Group_Name)))
     return -1;

   slrn_message_now (_("Sending GroupLens ratings to %s:%d..."),
		     gl->hostname, gl->port);

   if (-1 == gl_open_ratings (gl, Slrn_Current_Group_Name))
     {
	do_error ();
	return -1;
     }

   while (h != NULL)
     {
	GL_Rating_Type r;

	if (h->gl_rating > 0)
	  {
	     r.msgid = h->msgid;
	     if (h->gl_rating >= 10)
	       {
		  r.saw_article = 1;
		  r.rating = h->gl_rating / 10;
	       }
	     else
	       {
		  r.saw_article = 0;
		  r.rating = h->gl_rating;
	       }

	     if (-1 == gl_put_rating (gl, &r))
	       {
		  do_error ();
		  return -1;
	       }
	  }
	h = h->real_next;
     }

   if (-1 == gl_close_ratings (gl))
     {
	do_error ();
	return -1;
     }

   return 0;
}

/*}}}*/

void slrn_group_lens_rate_article (Slrn_Header_Type *h, int score, int saw_article) /*{{{*/
{
   if (saw_article) score = score * 10;
   h->gl_rating = score;
   Did_Rating++;
}

/*}}}*/

#endif				       /* SLRN_HAS_GROUPLENS */
