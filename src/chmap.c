/* -*- mode: C; mode: fold; -*- */
/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999 John E. Davis <davis@space.mit.edu>
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
#include "slrnfeat.h"

#include <stdio.h>
#include <string.h>

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef VMS
# include "vms.h"
#endif

#include <slang.h>
#include "jdmacros.h"

#include "misc.h"
#include "util.h"
#include "group.h"
#include "art.h"
#include "chmap.h"

#if SLRN_HAS_CHARACTER_MAP
char *Slrn_Charset;

static unsigned char *ChMap_To_Iso_Map;
static unsigned char *ChMap_From_Iso_Map;

/* This include file contains static globals */
# include "charmaps.h"

static void chmap_map_string (char *str, unsigned char *map)
{
   unsigned char ch;

   if (map == NULL)
     return;

   while (0 != (ch = *str))
     *str++ = map[ch];
}

static void chmap_map_string_from_iso (char *str)
{
   chmap_map_string (str, ChMap_From_Iso_Map);
}

static void chmap_map_string_to_iso (char *str)
{
   chmap_map_string (str, ChMap_To_Iso_Map);
}

#endif

/* Fix a single header; the rest of the header lines are dealt with
 * later in hide_art_headers() */
void slrn_chmap_fix_header (Slrn_Header_Type *h)
{
#if SLRN_HAS_CHARACTER_MAP
   if ((h->flags & HEADER_CHMAP_PROCESSED) == 0)
     {
	chmap_map_string_from_iso (h->subject);
	chmap_map_string_from_iso (h->from);
	chmap_map_string_from_iso (h->realname);
	h->flags |= HEADER_CHMAP_PROCESSED;
     }
#endif
}

void slrn_chmap_fix_body (Slrn_Article_Type *a, int revert)
{
#if SLRN_HAS_CHARACTER_MAP
   Slrn_Article_Line_Type *l;
   
   if (a == NULL)
     return;
   l = a->lines;

   while (l != NULL)
     {
	if (revert)
	  chmap_map_string_to_iso (l->buf);
	else
	  chmap_map_string_from_iso (l->buf);
        l = l->next;
     }
#endif
}

int slrn_chmap_fix_file (char *file, int reverse)
{
#if SLRN_HAS_CHARACTER_MAP
   FILE *fp, *tmpfp;
   char buf [4096];
   char tmp_file [SLRN_MAX_PATH_LEN];
   char *name;
   unsigned int len;
   int ret;

   name = slrn_basename (file);
   len = (unsigned int) (name - file);
   if (len != 0)
     {
	strncpy (tmp_file, file, len);
# ifndef VMS
	/* It appears that some non-unix systems cannot handle pathnames that
	 * end in a trailing slash.
	 */
	if (len > 1) len--;
# endif
     }
   else
     {
	tmp_file [0] = '.';
	len++;
     }
   tmp_file [len] = 0;

   if (NULL == (fp = fopen (file, "r")))
     {
        slrn_error (_("File error: %s --- message not posted."), file);
        return -1;
     }

   if (NULL == (tmpfp = slrn_open_tmpfile_in_dir (tmp_file, tmp_file,
						  sizeof (tmp_file))))
     {
	slrn_error (_("File error: %s --- message not posted."), tmp_file);
	fclose (fp);
	return -1;
     }

   ret = 0;
   while (NULL != fgets (buf, sizeof (buf), fp))
     {
	if (reverse) chmap_map_string_from_iso (buf);
	else chmap_map_string_to_iso (buf);
	if (EOF == fputs (buf, tmpfp))
	  {
	     slrn_error (_("Write Error. Disk Full? --- message not posted."));
	     ret = -1;
	     break;
	  }
     }

   slrn_fclose (fp);
   if (-1 == slrn_fclose (tmpfp))
     ret = -1;

   if (ret == -1)
     {
	(void) slrn_delete_file (tmp_file);
	return -1;
     }

   ret = slrn_move_file (tmp_file, file);
   return ret;
#else
   (void) file;
   return 0;
#endif
}

#if SLRN_HAS_CHARACTER_MAP
void slrn_chmap_show_supported (void)
{
   CharMap_Type *map;
   unsigned int i;

   printf (_("  Default character set:     %s\n"), DEFAULT_CHARSET_NAME);
   
   printf (_(" SUPPORTED CHARACTER SETS:\n"));
   printf ("  isolatin");

   for (i = 0; i < MAX_CHARMAPS; i++)
     {
	map = Char_Maps[i];
	if (map == NULL) continue;
	printf (" %s", map->map_name);
     }
   putc ('\n', stdout);
}
#endif

int slrn_set_charset (char *name)
{
#if SLRN_HAS_CHARACTER_MAP
   CharMap_Type *map;
   unsigned int i;

   if (name == NULL)
     name = DEFAULT_CHARSET_NAME;

   ChMap_To_Iso_Map = ChMap_From_Iso_Map = NULL;
   SLsmg_Display_Eight_Bit = 160;

   if (0 == slrn_case_strcmp ((unsigned char *)name, (unsigned char *)"isolatin"))
     return 0;

   for (i = 0; i < MAX_CHARMAPS; i++)
     {
	map = Char_Maps[i];
	if (map == NULL) continue;
	if (0 == slrn_case_strcmp ((unsigned char *)map->map_name, (unsigned char *)name))
	  {
	     SLsmg_Display_Eight_Bit = map->display_eight_bit;
	     ChMap_From_Iso_Map = map->from_iso_map;
	     ChMap_To_Iso_Map = map->to_iso_map;
	     return 0;
	  }
     }

   slrn_error (_("Unsupport character set: %s"), name);
   return -1;

#else
   (void) name;
   return -1;
#endif
}
