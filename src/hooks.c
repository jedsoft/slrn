/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.
 Copyright (c) 2001 Robin Sommer <rsommer@uni-paderborn.de>
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
#include <string.h>
#include "slrnfeat.h"
#include "slrn.h"
#include "util.h"
#include "hooks.h"
#include "strutil.h"

typedef struct Hook_Function_Type
{
   SLang_Name_Type *func;
   struct Hook_Function_Type *next;
}
Hook_Function_Type;

typedef struct Hook_Type
{
   const char *name;
   int multi; /* whether multiple hooks may be registered */
   Hook_Function_Type *first;
}
Hook_Type;

/* Order as given by constants HOOK_*
 * Define whether or not multiple registration is allowed: */

static Hook_Type Hooks [HOOK_NUMBER+1] =
{
   { "article_mode_hook", 1, NULL },
   { "article_mode_quit_hook", 1, NULL },
   { "article_mode_startup_hook", 1, NULL },
   { "cc_hook", 0, NULL },
   { "followup_hook", 1, NULL },
   { "forward_hook", 1, NULL },
   { "group_mode_hook", 1, NULL },
   { "group_mode_startup_hook", 1, NULL },
   { "header_number_hook", 1, NULL },
   { "make_from_string_hook", 0, NULL },
   { "make_save_filename_hook", 0, NULL },
   { "post_file_hook", 1, NULL },
   { "post_filter_hook", 1, NULL },
   { "post_hook", 1, NULL },
   { "pre_article_mode_hook", 1, NULL },
   { "quit_hook", 1, NULL },
   { "resize_screen_hook", 1, NULL },
   { "read_article_hook", 1, NULL },
   { "reply_hook", 1, NULL },
   { "startup_hook", 1, NULL },
   { "subject_compare_hook", 0, NULL },
   { "supersede_hook", 1, NULL },

   {NULL, 0, NULL}
};

static Hook_Type *find_hook (char *name)
{
   Hook_Type *h;

   h = Hooks;
   while (h->name != NULL)
     {
	if (0 == strcmp (name, h->name))
	  return h;
	h++;
     }
   return NULL;
}

static void free_hook_function_type (Hook_Function_Type *f)
{
   if (f == NULL)
     return;

   if (f->func != NULL)
     SLang_free_function (f->func);

   slrn_free ((char *) f);
}

int slrn_register_hook (char *name, SLang_Name_Type *func)
{
   Hook_Type *h;
   Hook_Function_Type *f;

   if (NULL == (h = find_hook (name)))
     return -1;

   /* Do not register the same hook more than once */
   f = h->first;
   while (f != NULL)
     {
	if (f->func == func)
	  return 1;
	f = f->next;
     }

   f = (Hook_Function_Type *) SLmalloc (sizeof (Hook_Function_Type));
   if (f == NULL)
     return -1;

   if (NULL == (f->func = SLang_copy_function (func)))
     {
	SLfree ((char *) f);
	return -1;
     }

   if (h->multi == 0)
     {
	free_hook_function_type (h->first);
	h->first = NULL;
     }

   f->next = h->first;
   h->first = f;

   return 1;
}

int slrn_unregister_hook (char *name, SLang_Name_Type *func)
{
   Hook_Type *h;
   Hook_Function_Type *f, *prev;

   if (NULL == (h = find_hook (name)))
     return -1;

   prev = NULL;
   f = h->first;
   while (f != NULL)
     {
	if (f->func != func)
	  {
	     prev = f;
	     f = f->next;
	     continue;
	  }

	if (prev != NULL)
	  prev->next = f->next;
	else
	  h->first = f->next;

	free_hook_function_type (f);
	return 1;
     }

   return 0;
}

static int call_slang_function (SLang_Name_Type *func, unsigned int num_args, va_list ap)
{
   unsigned int i;

   if (-1 == SLang_start_arg_list ())
     return -1;

   for (i = 0; i < num_args; i++)
     {
	char *arg = va_arg (ap, char *);
	if (-1 == SLang_push_string (arg))
	  return -1;
     }

   if ((-1 == SLang_end_arg_list ())
       || (-1 == SLexecute_function (func)))
     return -1;

   return 0;
}

int slrn_run_hooks (unsigned int hook, unsigned int num_args, ...)
{
   SLang_Name_Type *func;
   Hook_Function_Type *f;
   Hook_Type *h;
   va_list ap;
   int num_called;

   num_called = 0;

   if (hook > HOOK_NUMBER)
     return num_called;

   h = Hooks + hook;
   if (h->name == NULL)
     return num_called;

   f = h->first;
   while (f != NULL)
     {
	va_start (ap, num_args);
	if (-1 == call_slang_function (f->func, num_args, ap))
	  {
	     va_end (ap);
	     return -1;
	  }
	va_end (ap);
	f = f->next;
	num_called++;
     }

   if (num_called && (h->multi == 0))
     return num_called;

   /* Compatibility */
   if (NULL == (func = SLang_get_function ((char *)h->name)))
     return num_called;

   va_start (ap, num_args);
   if (-1 == call_slang_function (func, num_args, ap))
     {
	va_end (ap);
	SLang_free_function (func);
	return -1;
     }
   va_end (ap);
   SLang_free_function (func);
   num_called++;

   return num_called;
}

int slrn_is_hook_defined (unsigned int hook)
{
   Hook_Type *h;

   if (hook >= HOOK_NUMBER)
     return 0;

   h = Hooks + hook;

   if (h->first != NULL)
     return 1;

   return ((h->name != NULL) && (SLang_is_defined ((char *)h->name) > 0));
}
