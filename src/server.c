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

#ifdef VMS
# include <file.h>
#else
# include <sys/types.h>
# include <sys/stat.h>
#endif
#ifdef __MINGW32__
# include <process.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif
#ifdef HAVE_SYS_FCNTL_H
# include <sys/fcntl.h>
#endif

#include <slang.h>
#include "jdmacros.h"

#include "slrn.h"
#include "util.h"
#include "misc.h"
#include "server.h"
#include "startup.h"
#include "snprintf.h"
#if SLRN_USE_SLTCP
# if SLRN_HAS_NNTP_SUPPORT || SLRN_HAS_GROUP_LENS
#  include "sltcp.c"
# endif
#endif

#if SLRN_HAS_NNTP_SUPPORT
# if !SLRN_USE_SLTCP
#  include "clientlib.c"
# endif
# include "nntplib.c"
# include "nntp.c"
#endif

#if SLRN_HAS_SPOOL_SUPPORT
# include "spool.c"
#endif

int Slrn_Server_Id;
int Slrn_Post_Id;

Slrn_Server_Obj_Type *Slrn_Server_Obj;
Slrn_Post_Obj_Type *Slrn_Post_Obj;

#if SLRN_HAS_PULL_SUPPORT
static int pull_init_objects (void);
static int pull_select_post_object (void);
static int pull_parse_args (char **, int);
int Slrn_Use_Pull_Post;
#endif

#if SLRN_HAS_INEWS_SUPPORT
static FILE *Fp_Inews;
char *Slrn_Inews_Pgm;

#if defined(IBMPC_SYSTEM)
static char Inews_Outfile [SLRN_MAX_PATH_LEN];
#define MAX_LINE_BUFLEN	2048
#endif

static int inews_start_post (void)
{
#if defined(IBMPC_SYSTEM)
   if (NULL == (Fp_Inews = slrn_open_tmpfile (Inews_Outfile,
					      sizeof (Inews_Outfile))))
#else
   /* pipe a message to inews. Its done this way because inews expects the
    * article on stdin. Inews errors WILL mess up the screen.
    */
   /* !HACK! should we use slrn_popen() and slrn_pclose()?
    * They stop the screen getting messed up, but the error messages aren't
    * very appropriate
    */
   if (NULL == (Fp_Inews = popen (Slrn_Inews_Pgm, "w")))
#endif
     {
	slrn_error (_("Couldn't open pipe to inews! -- Article not posted."));
	return -1;
     }
   return CONT_POST;
}

static int inews_end_post (void)
{
   int res = 0;

#if defined(IBMPC_SYSTEM)
   char buf [MAX_LINE_BUFLEN];
   slrn_fclose (Fp_Inews);
   slrn_snprintf (buf, sizeof (buf), "%s %s", Slrn_Inews_Pgm, Inews_Outfile);
   slrn_posix_system (buf, 0);
#else
   if (-1 == pclose (Fp_Inews))
     {
	slrn_error (_("pclose() failed -- check if article was posted")); /* !HACK! can we do better? */
	res = -1;
     }
#endif
   Fp_Inews=NULL;

   return res;
}

static int inews_puts (char *s)
{
   fputs (s, Fp_Inews );
   return 0;
}

static int inews_vprintf (const char *fmt, va_list ap)
{
   char *line;
   int retval;

   line = slrn_strdup_vprintf (fmt, ap);
   retval = inews_puts (line);
   slrn_free (line);

   return retval;
}
static int inews_printf (char *fmt, ...)
{
   va_list ap;
   int retval;

   va_start (ap, fmt);
   retval = inews_vprintf (fmt, ap);
   va_end (ap);

   return retval;
}
static char *inews_get_recom_id (void)
{
   return NULL;
}

static Slrn_Post_Obj_Type Inews_Post_Obj;

static int inews_init_objects (void)
{
   Inews_Post_Obj.po_start = inews_start_post;
   Inews_Post_Obj.po_end = inews_end_post;
   Inews_Post_Obj.po_vprintf = inews_vprintf;
   Inews_Post_Obj.po_printf = inews_printf;
   Inews_Post_Obj.po_puts = inews_puts;
   Inews_Post_Obj.po_get_recom_id = inews_get_recom_id;
   Inews_Post_Obj.po_can_post = 1;
   Slrn_Inews_Pgm = slrn_safe_strmalloc (SLRN_INEWS_COMMAND);
   return 0;
}

static int inews_select_post_object (void)
{
   char inews [SLRN_MAX_PATH_LEN + 1];
   char *p;

   strncpy (inews, Slrn_Inews_Pgm, SLRN_MAX_PATH_LEN);
   inews[SLRN_MAX_PATH_LEN - 1] = 0;

   p = inews;
   while (*p && (*p != ' ')) p++;
   *p = 0;

   if (1 != slrn_file_exists (inews))
     {
	slrn_error (_("Unable to locate inews program \"%s\""), inews);
	return -1;
     }

   Slrn_Post_Obj = &Inews_Post_Obj;
   return 0;
}

#endif				       /* HAS_INEWS_SUPPORT */

int slrn_init_objects (void)
{
   char *server;

#if SLRN_HAS_NNTP_SUPPORT
   if (-1 == nntp_init_objects ())
     return -1;
#endif
#if SLRN_HAS_SPOOL_SUPPORT
   if (-1 == spool_init_objects ())
     return -1;
#endif
#if SLRN_HAS_INEWS_SUPPORT
   if (-1 == inews_init_objects ())
     return -1;
#endif
#if SLRN_HAS_PULL_SUPPORT
   if (-1 == pull_init_objects ())
     return -1;
#endif

   switch (Slrn_Server_Id)
     {
      case SLRN_SERVER_ID_NNTP:
	server = "NNTP";
	break;

      case SLRN_SERVER_ID_SPOOL:
	server = "SPOOL";
	break;

      default:
	server = NULL;
     }

   if ((server != NULL)
       && (-1 == SLdefine_for_ifdef (server)))
     {
	slrn_exit_error (_("Unable to add preprocessor name %s."), server);
     }

   return 0;
}

#if !SLRN_HAS_INEWS_SUPPORT
# undef SLRN_FORCE_INEWS
# define SLRN_FORCE_INEWS 0
#endif

int slrn_select_post_object (int id)
{
   char *post;

   switch (id)
     {
#if SLRN_HAS_INEWS_SUPPORT
      case SLRN_POST_ID_INEWS:
# if SLRN_HAS_PULL_SUPPORT
	if (Slrn_Use_Pull_Post
	    && (Slrn_Server_Id == SLRN_SERVER_ID_SPOOL))
	  {
	     slrn_error (_("inews cannot be used with an slrnpull spool."));
	     return -1;
	  }
# endif
	return inews_select_post_object ();
#endif

#if !SLRN_FORCE_INEWS
# if SLRN_HAS_NNTP_SUPPORT
      case SLRN_POST_ID_NNTP:
	return nntp_select_post_object ();
# endif
#endif

#if SLRN_HAS_PULL_SUPPORT
      case SLRN_POST_ID_PULL:
	return pull_select_post_object ();
#endif

      default:
	post = slrn_map_object_id_to_name (1, id);
	if (post == NULL)
	  post = "UNKNOWN";

	slrn_error (_("%s is not a supported posting agent."), post);
     }

   return -1;
}

int slrn_select_server_object (int id)
{
#if SLRN_HAS_SPOOL_SUPPORT
   int ret;
#endif
   char *server;

   switch (id)
     {
#if SLRN_HAS_NNTP_SUPPORT
      case SLRN_SERVER_ID_NNTP:
	return nntp_select_server_object ();
#endif

#if SLRN_HAS_SPOOL_SUPPORT
      case SLRN_SERVER_ID_SPOOL:
	ret = spool_select_server_object ();

# if SLRN_HAS_PULL_SUPPORT
	if (ret == -1)
	  return ret;

	if (Slrn_Use_Pull_Post)
	  Slrn_Post_Id = SLRN_POST_ID_PULL;
# endif
	return ret;
#endif

      default:
	server = slrn_map_object_id_to_name (0, id);

	if (server == NULL)
	  server = "UNKNOWN";

	slrn_error (_("%s is not a supported server object."), server);
     }
   return -1;
}

int slrn_parse_object_args (char *name, char **argv, int argc)
{
   int num_parsed = -1;

   if (name == NULL) return -1;

   if (!strcmp (name, "nntp"))
     {
#if SLRN_HAS_NNTP_SUPPORT
	num_parsed = nntp_parse_args (argv, argc);
	if (Slrn_Server_Id == 0)
	  Slrn_Server_Id = SLRN_SERVER_ID_NNTP;
# if !SLRN_FORCE_INEWS
	if (Slrn_Post_Id == 0)
	  Slrn_Post_Id = SLRN_POST_ID_NNTP;
# endif
	return num_parsed;
#else
	return -2;
#endif
     }

   if (!strcmp (name, "spool"))
     {
#if SLRN_HAS_SPOOL_SUPPORT
	Slrn_Server_Id = SLRN_SERVER_ID_SPOOL;
	return 0;
#else
	return -2;
#endif
     }

   if (!strcmp (name, "inews"))
     {
#if SLRN_HAS_INEWS_SUPPORT
	Slrn_Post_Id = SLRN_POST_ID_INEWS;
	return 0;
#else
	return -2;
#endif
     }

   if (!strcmp (name, "pull"))
     {
#if SLRN_HAS_PULL_SUPPORT
	num_parsed = pull_parse_args (argv, argc);
	Slrn_Post_Id = SLRN_POST_ID_PULL;
	return num_parsed;
#else
	return -2;
#endif
     }

   return num_parsed;
}

typedef struct
{
   char *name;
   int id;
}
Object_Name_Id_Type;

static Object_Name_Id_Type Server_Objects_and_Ids [] =
{
   {"nntp", SLRN_SERVER_ID_NNTP},
   {"spool", SLRN_SERVER_ID_SPOOL},
   {NULL, 0}
};

static Object_Name_Id_Type Post_Objects_and_Ids [] =
{
   {"nntp", SLRN_POST_ID_NNTP},
   {"inews", SLRN_POST_ID_INEWS},
   {"slrnpull", SLRN_POST_ID_PULL},
   {NULL, 0}
};

/* These functions should not attempt to write any error messages */
static Object_Name_Id_Type *map_object_type (int type, int *def_id)
{
   Object_Name_Id_Type *obj;

   switch (type)
     {
      case 0:
	obj = Server_Objects_and_Ids;
	if (def_id != NULL)
	  *def_id = SLRN_DEFAULT_SERVER_OBJ;
	break;

      case 1:
	obj = Post_Objects_and_Ids;
	if (def_id != NULL)
	  *def_id = SLRN_DEFAULT_POST_OBJ;
	break;

      default:
	obj = NULL;
     }

   return obj;
}

char *slrn_map_object_id_to_name (int type, int id)
{
   Object_Name_Id_Type *obj;

   if (NULL == (obj = map_object_type (type, NULL)))
     return NULL;

   while (obj->name != NULL)
     {
	if (obj->id == id)
	  return obj->name;
	obj++;
     }

   return NULL;
}

int slrn_map_name_to_object_id (int type, char *name)
{
   Object_Name_Id_Type *obj;
   int default_object_id;

   if (NULL == (obj = map_object_type (type, &default_object_id)))
     return -1;

   if (name == NULL)
     {
	name = slrn_map_object_id_to_name (type, default_object_id);
	if (name == NULL)
	  return -1;
     }

   while (obj->name != NULL)
     {
	if (0 == strcmp (name, obj->name))
	  return obj->id;
	obj++;
     }
   return -1;
}

#if SLRN_HAS_PULL_SUPPORT
static Slrn_Post_Obj_Type Pull_Post_Obj;
static FILE *Pull_Fp;
unsigned int Pull_Num_Posted;
char Pull_Post_Filename [SLRN_MAX_PATH_LEN + 1];
char Pull_Post_Dir [SLRN_MAX_PATH_LEN + 1];

static int pull_make_tempname (char *file, size_t n, char *prefix,
			       unsigned int num)
{
   int pid;
   time_t now;
   char name[256];
   char *login_name;

   pid = getpid ();
   time (&now);

   login_name = Slrn_User_Info.login_name;
   if ((login_name == NULL) || (*login_name == 0))
     login_name = "unknown";

   slrn_snprintf (name, sizeof (name), "%s%lu-%d-%u.%s", prefix,
		  (unsigned long) now, pid, num, login_name);

   if (-1 == slrn_dircat (Pull_Post_Dir, name, file, n))
     return -1;

   return 0;
}

static int pull_start_post (void)
{
   int fd;
   unsigned int n = 0;

   do
     {
	if (-1 == pull_make_tempname (Pull_Post_Filename,
				      sizeof (Pull_Post_Filename), "_", n++))
	  return ERR_FAULT;
	fd = open (Pull_Post_Filename, O_WRONLY | O_CREAT | O_EXCL,
		   S_IREAD | S_IWRITE);
	if ((-1 == fd) && (n == 100))
          {
	     slrn_error (_("Unable to open file in %s."), Pull_Post_Dir);
	     return ERR_FAULT;
	  }
     }
   while (-1 == fd);

   if (NULL == (Pull_Fp = fdopen (fd, "w")))
     {
	close (fd);
	return ERR_FAULT;
     }

   return CONT_POST;
}

static int pull_end_post (void)
{
   char file [SLRN_MAX_PATH_LEN + 1];
   unsigned int n = 0, m = 0;

   if (Pull_Fp == NULL)
     return ERR_FAULT;

   if (-1 == slrn_fclose (Pull_Fp))
     {
	Pull_Fp = NULL;
	return ERR_FAULT;
     }
   Pull_Fp = NULL;

#if SLRNPULL_USE_SETGID_POSTS
    /*
     * 1998/07/07 Sylvain Robitaille (syl@alcor.concordia.ca)
     *            We have our out.going directory setgid to news, and we
     *            want to be sure the file is readable to
     *            group news. This way, the news user will have
     *            appropriate permissions for manipulating this file.
     *            (NOTE: This has only been tested on Linux-2.0.xx)
     *
     * As of version 0.9.7.3, the files is not made group writeable any
     * longer. slrnpull should not need that permission.
     */
    if (-1 == chmod (Pull_Post_Filename, S_IRUSR|S_IWUSR|S_IRGRP))
      {
	 return ERR_FAULT;
      }
#endif

   while (m != 10)
     {
	do
	  {
	     if ((100 == n) || (-1 == pull_make_tempname (file, sizeof (file),
							  "X", n++)))
	       return ERR_FAULT;
	  }
	while (0 != slrn_file_exists (file));

	if (0 == rename (Pull_Post_Filename, file))
	  break;

#ifdef EEXIST
	if (errno == EEXIST)
	  {
	     m++;
	     continue;
	  }
#endif

	slrn_error (_("Unable to rename file. errno = %d."), errno);
	return ERR_FAULT;
     }

   return 0;
}

static int pull_puts (char *s)
{
   unsigned int len;

   len = strlen (s);

   if (len != fwrite (s, 1, len, Pull_Fp))
     {
	slrn_error (_("Write error.  File system full?"));
	return -1;
     }

   return 0;
}

static int pull_vprintf (const char *fmt, va_list ap)
{
   int ret;

   ret = vfprintf (Pull_Fp, fmt, ap);

   if (EOF == ret)
     {
	slrn_error (_("Write failed.  File system full?"));
	return -1;
     }

   return 0;
}

static int pull_printf (char *fmt, ...)
{
   int ret;

   va_list ap;

   va_start (ap, fmt);
   ret = pull_vprintf (fmt, ap);
   va_end (ap);

   return ret;
}

static char *pull_get_recom_id (void)
{
   return NULL;
}

static int pull_init_objects (void)
{
   Pull_Post_Obj.po_start = pull_start_post;
   Pull_Post_Obj.po_end = pull_end_post;
   Pull_Post_Obj.po_vprintf = pull_vprintf;
   Pull_Post_Obj.po_printf = pull_printf;
   Pull_Post_Obj.po_puts = pull_puts;
   Pull_Post_Obj.po_get_recom_id = pull_get_recom_id;
   Pull_Post_Obj.po_can_post = 1;

   return 0;
}

static int pull_select_post_object (void)
{
   if (-1 == slrn_dircat (Slrn_Inn_Root, SLRNPULL_OUTGOING_DIR,
			  Pull_Post_Dir, sizeof (Pull_Post_Dir)))
     return -1;

   if (2 != slrn_file_exists (Pull_Post_Dir))
     {
	slrn_error (_("Posting directory %s does not exist."), Pull_Post_Dir);
	return -1;
     }

   Slrn_Post_Obj = &Pull_Post_Obj;

   return 0;
}

static int pull_parse_args (char **argv, int argc)
{
   (void) argv;
   (void) argc;

   return 0;
}

#endif
