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
#include <ssdef.h>
#include <string.h>

#include "vms.h"
#include "vmsmail.h"

extern int mail$send_add_bodypart ();
extern int mail$send_begin ();
extern int mail$send_add_attribute ();
extern int mail$send_add_address ();
extern int mail$send_message ();
extern int mail$send_end ();

static void fill_struct(Mail_Type *m, int act, char *s)
{
   m->code = act;
   m->buflen = strlen(s);
   m->addr = (long) s;
   m->junk = m->ret = 0;
}

/* to might be a comma separated list--- parse it too */
int vms_send_mail(char *to, char *subj, char *file)
{
   Mail_Type mt0, mt;
   int context = 0;
   char *p;

   mt0.code = mt0.buflen = mt0.addr = mt0.ret = mt0.junk = 0;

   if (SS$_NORMAL != mail$send_begin(&context, &mt0, &mt0))
     {
	return(0);
     }
#if 0
   fill_struct(&mt, MAIL$_SEND_TO_LINE, to);
   if (SS$_NORMAL != mail$send_add_attribute(&context, &mt, &mt0))
     {
	return(0);
     }

   fill_struct(&mt, MAIL$_SEND_USERNAME, to);
   if (SS$_NORMAL != mail$send_add_address(&context, &mt, &mt0))
     {
	return(0);
     }
#endif
   while (1)
     {
	while (*to && ((*to <= ' ') || (*to == ','))) to++;
	if (*to == 0) break;
	p = to;
	while ((*p > ' ') && (*p != ',')) p++;

        mt.code = MAIL$_SEND_TO_LINE;
	mt.buflen = p - to;
	mt.ret = mt.junk = 0;
	mt.addr = (long) to;

	if (SS$_NORMAL != mail$send_add_attribute(&context, &mt, &mt0))
	  {
	     return(0);
	  }

	mt.code = MAIL$_SEND_USERNAME;
	mt.buflen = p - to;
	mt.ret = mt.junk = 0;
	mt.addr = (long) to;

	if (SS$_NORMAL != mail$send_add_address(&context, &mt, &mt0))
	  {
	     return(0);
	  }
	to = p;
     }

   fill_struct(&mt, MAIL$_SEND_SUBJECT, subj);
   if (SS$_NORMAL != mail$send_add_attribute(&context, &mt, &mt0))
     {
	return(0);
     }

   fill_struct(&mt, MAIL$_SEND_FILENAME, file);
   if (SS$_NORMAL != mail$send_add_bodypart(&context, &mt, &mt0))
     {
	return(0);
     }

   if (SS$_NORMAL != mail$send_message(&context, &mt0, &mt0))
     {
	return(0);
     }

   if (SS$_NORMAL != mail$send_end(&context, &mt0, &mt0))
     {
	return(0);
     }
   return(1);
}
