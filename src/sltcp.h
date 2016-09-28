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
#ifndef _SLTCP_H_LOADED_
#define _SLTCP_H_LOADED_

#include <stdarg.h>

typedef struct _SLTCP_Type SLTCP_Type;

extern int sltcp_close_socket (SLTCP_Type *);
extern SLTCP_Type *sltcp_open_connection (char *, int, int);
extern int sltcp_close (SLTCP_Type *);
extern unsigned int sltcp_read (SLTCP_Type *, char *, unsigned int);
extern unsigned int sltcp_write (SLTCP_Type *, char *, unsigned int);
extern int sltcp_flush_output (SLTCP_Type *);
extern int sltcp_map_service_to_port (char *);
extern int (*SLTCP_Interrupt_Hook) (void);

extern int sltcp_fputs (SLTCP_Type *, char *);
extern int sltcp_vfprintf (SLTCP_Type *, char *, va_list);
extern int sltcp_fgets (SLTCP_Type *, char *, unsigned int);

extern int sltcp_open_sltcp (void);
extern int sltcp_close_sltcp (void);
extern void (*SLtcp_Verror_Hook) (char *, va_list);

extern unsigned int sltcp_get_num_input_bytes (SLTCP_Type *);
extern unsigned int sltcp_get_num_output_bytes (SLTCP_Type *);
extern int sltcp_reset_num_io_bytes (SLTCP_Type *);
extern int sltcp_get_fd (SLTCP_Type *);
#endif
