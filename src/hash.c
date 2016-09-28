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
#ifndef SLRNPULL_CODE
# include "config.h"
# include "slrnfeat.h"
#endif

#include <stdio.h>
#include <string.h>

#include <slang.h>
#include "jdmacros.h"

#include "hash.h"
#include "snprintf.h"
#include "strutil.h"

unsigned long slrn_compute_hash (unsigned char *s, unsigned char *smax)
{
   register unsigned long h = 0, g;
   register unsigned long sum = 0;

   while (s < smax)
     {
	sum += *s++ | 0x20;	       /* case-insensitive */

	h = sum + (h << 3);
	if ((g = h & 0xE0000000U) != 0)
	  {
	     h = h ^ (g >> 24);
	     h = h ^ g;
	  }
     }
   return h;
}

#if SLRN_HAS_MSGID_CACHE
typedef struct Msg_Id_Cache_Type
{
   char *msgid;
   char *newsgroup;
   struct Msg_Id_Cache_Type *next;
#ifdef SLRNPULL_CODE
   NNTP_Artnum_Type num;
#endif
}
Msg_Id_Cache_Type;

typedef struct Newsgroup_Cache_Type
{
   char *newsgroup;
   struct Newsgroup_Cache_Type *next;
}
Newsgroup_Cache_Type;

#define MAX_MSGID_HASH	8017
#define MAX_NG_HASH	2909

static Msg_Id_Cache_Type *Msg_Id_Cache [MAX_MSGID_HASH];
static Newsgroup_Cache_Type *Newsgroup_Cache [MAX_NG_HASH];

static unsigned int simple_hash (unsigned char *s, unsigned char *smax,
				 unsigned int mod)
{
   register unsigned int h = 0;

   while (s < smax)
     {
	h = ((h << 5) + *s) % mod;
	s++;
     }
   return h;
}

static char *allocate_newsgroup (char *newsgroup)
{
   Newsgroup_Cache_Type *node;
   unsigned int hash_index;
   unsigned int len;

   len = strlen (newsgroup);

   hash_index = simple_hash ((unsigned char *) newsgroup,
			     (unsigned char *) newsgroup + len,
			     MAX_NG_HASH);

   node = Newsgroup_Cache [hash_index];

   while (node != NULL)
     {
	if (!strcmp (node->newsgroup, newsgroup))
	  return node->newsgroup;
	node = node->next;
     }

   node = (Newsgroup_Cache_Type *) slrn_malloc (sizeof (Newsgroup_Cache_Type),1,1);
   if (node == NULL) return NULL;
   if (NULL == (node->newsgroup = (char *) slrn_malloc (len + 1,0,1)))
     {
	slrn_free ((char *)node);
	return NULL;
     }
   strcpy (node->newsgroup, newsgroup); /* safe */

   node->next = Newsgroup_Cache [hash_index];
   Newsgroup_Cache [hash_index] = node;

   return node->newsgroup;
}

static Msg_Id_Cache_Type *allocate_msgid_node (char *msgid, unsigned int msgid_len,
					       char *newsgroup)
{
   Msg_Id_Cache_Type *node;
   char *buf;

   newsgroup = allocate_newsgroup (newsgroup);
   if (newsgroup == NULL)
     return NULL;

   buf = (char *) slrn_malloc (msgid_len + 1, 0, 1);
   if (buf == NULL) return NULL;
   slrn_strncpy (buf, msgid, msgid_len);

   node = (Msg_Id_Cache_Type *) slrn_malloc (sizeof (Msg_Id_Cache_Type), 1, 1);
   if (node == NULL)
     {
	slrn_free (buf);
	return NULL;
     }
   node->msgid = buf;
   node->newsgroup = newsgroup;
   return node;
}

static int Dont_Grow = 0;
static Msg_Id_Cache_Type *is_msgid_cached (char *msgid, char *newsgroup,
					   NNTP_Artnum_Type num, int add)
{
   Msg_Id_Cache_Type *node;
   unsigned int hash_index;
   unsigned int msgid_len;

#ifndef SLRNPULL_CODE
   (void) num;
#endif

   msgid_len = strlen (msgid);

   hash_index = simple_hash ((unsigned char *) msgid,
			     (unsigned char *) msgid + msgid_len,
			     MAX_MSGID_HASH);

   node = Msg_Id_Cache [hash_index];

   while (node != NULL)
     {
	if (!strcmp (node->msgid, msgid))
	  return node;
	node = node->next;
     }

   if (add && (Dont_Grow == 0) && (newsgroup != NULL))
     {
	node = allocate_msgid_node (msgid, msgid_len, newsgroup);

	if (node == NULL) Dont_Grow = 1;
	else
	  {
#ifdef SLRNPULL_CODE
	     node->num = num;
#endif
	     node->next = Msg_Id_Cache [hash_index];
	     Msg_Id_Cache [hash_index] = node;
	  }
     }

   return NULL;
}

char *slrn_is_msgid_cached (char *msgid, char *newsgroup, int add)
{
   Msg_Id_Cache_Type *node;

   node = is_msgid_cached (msgid, newsgroup, 0, add);

   if (node != NULL)
     return node->newsgroup;

   return NULL;
}

#endif				       /* SLRN_HAS_MSGID_CACHE */

#if 0
#define SIZE 1250
unsigned int bin[SIZE];

int main (int argc, char **argv)
{
   unsigned char buf[0x7FFF];
   unsigned char *b, ch;
   unsigned long hash;
   int i;

   while (NULL != fgets ((char *) buf, sizeof (buf) - 1, stdin))
     {
	b = buf;
	while (((ch = *b) != '!') && (ch != ':') && (ch > ' '))
	  b++;

	hash = slrn_compute_hash (buf, b);
	hash = hash % SIZE;

	/* fprintf (stdout, "%X\n", hash); */
	bin[hash] += 1;
     }

   for (i = 0; i < SIZE; i++)
     {
	fprintf (stdout, "%d\t%d\n", i, bin[i]);
     }
   return 0;
}
#endif
