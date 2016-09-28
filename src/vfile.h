#ifndef _DAVIS_VFILE_H_
#define _DAVIS_VFILE_H_
/* Copyright (c) 1992, 1998, 2000, 2007-2016 John E. Davis
 * This file is part of JED editor library source.
 *
 * You may distribute this file under the terms the GNU General Public
 * License.  See the file COPYING for more information.
 */

#define VFILE_TEXT  1
#define VFILE_BINARY  2
extern unsigned int VFile_Mode;

typedef struct VFILE VFILE;

extern char *vgets(VFILE *, unsigned int *);
extern VFILE *vopen(char *, unsigned int, unsigned int);
extern void vclose(VFILE *);
extern VFILE *vstream(int, unsigned int, unsigned int);
extern int vungets (VFILE *v);

#endif
