/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.
 Copyright (c) 2001 Robin Sommer <rsommer@uni-paderborn.de>
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

#include "config.h"
#include <string.h>
#include "slrnfeat.h"
#include "slrn.h"
#include "util.h"
#include "hooks.h"

typedef struct Slrn_Hook_Function_Type {
   char *function;
   struct Slrn_Hook_Function_Type *next;
   } Slrn_Hook_Function_Type;

typedef struct Slrn_Hook_Type {
   const char *name;
   int multi; /* whether multiple hooks may be registered */
   Slrn_Hook_Function_Type *first;
   } Slrn_Hook_Type;

typedef Slrn_Hook_Type Slrn_Hook_Table_Type[HOOK_NUMBER];

/* Order as given by constants HOOK_*
 * Define whether or not multiple registration is allowed: */

static Slrn_Hook_Table_Type Hooks = {
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
   { "resize_screen_hook", 1, NULL },
   { "read_article_hook", 1, NULL },
   { "reply_hook", 1, NULL },
   { "startup_hook", 1, NULL },
   { "subject_compare_hook", 0, NULL },
   { "supersede_hook", 1, NULL }
   
};

int slrn_register_hook( unsigned int hook, const char *func )
{
   Slrn_Hook_Function_Type *h = Hooks[hook].first;
   Slrn_Hook_Function_Type **ins = &(Hooks[hook].first);
   
   while (h != NULL)
     {
	if (!strcmp (h->function, func))
	  return 2; /* Was already registered. */
	ins = &h->next;
	h = h->next;
     }
   
   if ((Hooks[hook].multi == 0) && slrn_is_hook_defined (hook))
     return 0;
   
   h = (Slrn_Hook_Function_Type *)slrn_safe_malloc( sizeof( Slrn_Hook_Function_Type ) );
   h->function = slrn_safe_strmalloc( (char *)func );
   h->next = NULL;
   *ins = h;
   return (0 < SLang_is_defined(h->function)) ? 1 : 3;
}

int slrn_register_hook_by_name( const char *hook, const char *func )
{
   int i;
   for( i = 0; i < HOOK_NUMBER; i++ )
      if( strcmp( hook, Hooks[i].name ) == 0  )
         return slrn_register_hook( i, func );
   return 0;
}

int slrn_unregister_hook( unsigned int hook, const char *func )
{
   Slrn_Hook_Function_Type *h, **prev;
   
   prev = &Hooks[hook].first;
   
   for( h = Hooks[hook].first; h; h = h->next ){
      if( strcmp( h->function, func ) == 0 ){
         *prev = h->next;
	 slrn_free( h->function );
         slrn_free( (char *)h );
         return 1;
      }
      prev = &h->next;
   }
      
   return 0;
}

int slrn_unregister_hook_by_name( const char *hook, const char *func )
{
   int i;
   for( i = 0; i < HOOK_NUMBER; i++ )
      if( strcmp( hook, Hooks[i].name ) == 0 )
         return slrn_unregister_hook( i, func );
   return 0;
}

/* Copied and pasted from slang 
 * Modified to take va_list as arg (instead of "...")
 */
static int my_SLang_run_hooks(char *hook, unsigned int num_args, va_list ap)
{
   unsigned int i;

   if (SLang_Error) return -1;
  
   if (0 == SLang_is_defined (hook))
     return 0;

   for (i = 0; i < num_args; i++)
     {
        char *arg;
        arg = va_arg (ap, char *);
        if (-1 == SLang_push_string (arg))
          break;
     }

   if (SLang_Error) return -1;
   return SLang_execute_function (hook);
}


int slrn_run_hooks( unsigned int hook, unsigned int num_args, ... )
{
   Slrn_Hook_Function_Type *h;
   va_list ap;
   int hook_defined, error = 1;
   
   hook_defined = SLang_is_defined ((char *)Hooks[hook].name);
   
   if (!hook_defined && (NULL == Hooks[hook].first))
     return 0;
   
   if (!hook_defined || Hooks[hook].multi)
     for( h = Hooks[hook].first; h; h = h->next ){
	va_start( ap, num_args );
	if( my_SLang_run_hooks( h->function, num_args, ap ) <= 0 )
	  error = -1;
	va_end( ap );
     }
   
   /* For compatibility */
   if (hook_defined){
      va_start( ap, num_args );
      if( my_SLang_run_hooks( (char *)Hooks[hook].name, num_args, ap ) <= 0 )
	error = -1;
      va_end( ap );
   }
   
   return error;
}

int slrn_is_hook_defined( unsigned int hook )
{
   if( Hooks[hook].first ) 
      return 1;
   
   return (2 == SLang_is_defined( (char *)Hooks[hook].name ));
}
