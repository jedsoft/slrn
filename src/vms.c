/*
 *  Project   : tin - a Usenet reader
 *  Module    : vms.c
 *  Author    : Andrew Greer
 *  Created   : 19-06-95
 *  Updated   : 19-06-95
 *  Notes     :
 *  Copyright : (c) Copyright 1991-95 by Iain Lea & Andrew Greer
 *              You may  freely  copy or  redistribute  this software,
 *              so  long as there is no profit made from its use, sale
 *              trade or  reproduction.  You may not change this copy-
 *              right notice, and it must be included in any copy made
 * 
 * 
 * Note: This file was originally donated for use in slrn by Andrew Greer.
 *       Since then, it has been modified and made more bullet-proof for 
 *       use in slrn.
 */

#include "config.h"
#include "slrnfeat.h"

#ifdef VMS

#include <stdio.h>
#include <ctype.h>
#include <descrip.h>
#include <iodef.h>
#include <ssdef.h>
#include <uaidef.h>
#include <string.h>
#include <stdlib.h>
#include <file.h>

#include "vms.h"

char *slrn_vms_getlogin (void)
{
   return getenv ("USER");
}

static struct dsc$descriptor *c$dsc(char *c$_str)
{
   static struct dsc$descriptor c$_tmpdesc;
   
   c$_tmpdesc.dsc$w_length = strlen(c$_str);
   c$_tmpdesc.dsc$b_dtype  = DSC$K_DTYPE_T;
   c$_tmpdesc.dsc$b_class  = DSC$K_CLASS_S;
   c$_tmpdesc.dsc$a_pointer= c$_str;
   return(&c$_tmpdesc);
}

char *slrn_vms_get_uaf_fullname (void)
{
   static char uaf_owner[40];
   char loc_username[13];
   unsigned int i, pos, max_len;
   char *user;

   struct item_list 
     {
	short bl, ic;
	char *ba;
	short *rl;
     }
   getuai_itmlist[] = 
     {
	  {
	     sizeof(uaf_owner),
	       UAI$_OWNER,
	       &uaf_owner[0],
	       0
	  },
	  { 0, 0, 0, 0}
     };

   if (NULL == (user = slrn_vms_getlogin ()))
     user = "";

   /* Apparantly, loc_username must be padded with blanks */
   max_len = sizeof (loc_username) - 1;
   strncpy (loc_username, user, max_len);
   i = strlen (user);
   while (i < max_len)
     {
	loc_username [i] = ' ';
	i++;
     }
   loc_username[max_len] = 0;

   sys$getuai(0,0,c$dsc(loc_username),getuai_itmlist,0,0,0);
   
   pos=1;
   if (uaf_owner[pos]=='|')
     pos += 3;
   while (uaf_owner[pos] == ' ')
     pos++;
   uaf_owner[uaf_owner[0] + 1] = '\0';
   return(uaf_owner + pos);
}

/* Converts "TOD_MCQUILLIN" to "Tod McQuillin" */
char *slrn_vms_fix_fullname(char *p)
{
   unsigned char cc = 0;
   char *q, ch;
   
   q = p;
   
   while ((ch = *q) != 0)
     {
	if (cc == 0)
	  ch = toupper (ch);
	else
	  {
	     if ((cc == 2) && (*(q-1) == 'c') && (*(q - 2) == 'M'))
	       ch = toupper (ch);
	     else 
	       ch = tolower (ch);
	  }
	
	if (ch == '_') ch = ' ';
	
	*q = ch;

	if (ch == ' ') 
	  cc = 0;
	else 
	  cc++;

	q++;
     }
   return p;
}

#ifndef __CRTL_VER
# define __CRTL_VER 00000000
#endif

#if __VMS_VER < 70000000
FILE *
popen (
       char *command,
       char *mode)
{
   return NULL;
}


int
pclose (FILE *pipe)
{
   return 1;
}
#endif

#if __CRTL_VER < 70000000
void tzset(void)
{
   return;
}
#endif

#endif /* VMS */
