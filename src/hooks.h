/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.
 Copyright (c) 2001 Robin Sommer <rsommer@uni-paderborn.de>
 Copyright (c) 2001 Thomas Schultz <tststs@gmx.de>

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

#define HOOK_ARTICLE_MODE           0
#define HOOK_ARTICLE_MODE_QUIT      1
#define HOOK_ARTICLE_MODE_STARTUP   2
#define HOOK_CC                     3
#define HOOK_FOLLOWUP               4
#define HOOK_FORWARD                5
#define HOOK_GROUP_MODE             6
#define HOOK_GROUP_MODE_STARTUP     7
#define HOOK_HEADER_NUMBER          8
#define HOOK_MAKE_FROM_STRING       9
#define HOOK_MAKE_SAVE_FILENAME    10
#define HOOK_POST_FILE             11
#define HOOK_POST_FILTER           12
#define HOOK_POST                  13
#define HOOK_PRE_ARTICLE_MODE      14
#define HOOK_RESIZE_SCREEN         15
#define HOOK_READ_ARTICLE          16
#define HOOK_REPLY		   17
#define HOOK_STARTUP               18
#define HOOK_SUBJECT_COMPARE       19
#define HOOK_SUPERSEDE             20
/* Number of different hooks */
#define HOOK_NUMBER                21

extern int slrn_run_hooks( unsigned int hook, unsigned int num_args, ... );
extern int slrn_is_hook_defined( unsigned int hook );
extern int slrn_register_hook( unsigned int hook, const char *func );   
extern int slrn_register_hook_by_name( const char *hook, const char *func );   
extern int slrn_unregister_hook( unsigned int hook, const char *func );   
extern int slrn_unregister_hook_by_name( const char *hook, const char *func );   
