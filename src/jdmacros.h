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
#ifndef _JD_MACROS_H_
#define _JD_MACROS_H_
/* This file defines some macros that I use with programs that link to
 * the slang library.
 */

#ifdef HAVE_MALLOC_H
# if !defined(__FreeBSD__)
#  include <malloc.h>
# endif
#endif

#ifdef HAVE_MEMORY_H
# include <memory.h>
#endif

#ifndef SLMEMSET
# ifdef HAVE_MEMSET
#  define SLMEMSET memset
# else
#  define SLMEMSET SLmemset
# endif
#endif

#ifndef SLMEMCHR
# ifdef HAVE_MEMCHR
#  define SLMEMCHR memchr
# else
#  define SLMEMCHR SLmemchr
# endif
#endif

#ifndef SLMEMCPY
# ifdef HAVE_MEMCPY
#  define SLMEMCPY memcpy
# else
#  define SLMEMCPY SLmemcpy
# endif
#endif

/* Note:  HAVE_MEMCMP requires an unsigned memory comparison!!!  */
#ifndef SLMEMCMP
# ifdef HAVE_MEMCMP
#  define SLMEMCMP memcmp
# else
#  define SLMEMCMP SLmemcmp
# endif
#endif

#ifndef SLFREE
# define SLFREE free
#endif

#ifndef SLMALLOC
# define SLMALLOC malloc
#endif

#ifndef SLCALLOC
# define SLCALLOC calloc
#endif

#ifndef SLREALLOC
# define SLREALLOC realloc
#endif

#include <ctype.h>

#if !defined(HAVE_ISALPHA) && !defined(isalpha)
# define isalpha(x) \
  ((((x) <= 'Z') && ((x) >= 'A')) \
   || (((x) <= 'z') && ((x) >= 'a')))
#endif

#if !defined(HAVE_ISSPACE) && !defined(isspace)
# define isspace(x) (((x)==' ')||((x)=='\t')||((x)=='\n')||((x)=='\r'))
#endif

#if !defined(HAVE_ISDIGIT) && !defined(isdigit)
# define isdigit(x) (((x) <= '9') && ((x) >= '0'))
#endif

#if !defined(HAVE_ISALNUM) && !defined(isalnum)
# define isalnum(x) (isalpha(x)||isdigit(x))
#endif

#if !defined(HAVE_ISPUNCT) && !defined(ispunct)
# define ispunct(x)  (((x)=='.')||((x)==',')||((x)==';')||((x)=='!')||((x)=='?'))
#endif

#ifdef __GNUC__
# define ATTRIBUTE_(x) __attribute__ (x)
#else
# define ATTRIBUTE_(x)
#endif
#define ATTRIBUTE_PRINTF(a,b) ATTRIBUTE_((format(printf,a,b)))

#endif				       /* _JD_MACROS_H_ */
