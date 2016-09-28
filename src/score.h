/*
 This file is part of SLRN.

 Copyright (c) 1994, 1999, 2007-2016 John E. Davis <jed@jedsoft.org>
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
#ifndef _SLRN_SCORE_H
#define _SLRN_SCORE_H
extern int Slrn_Apply_Score;
extern char *Slrn_Scorefile_Open;
extern int slrn_read_score_file (char *);
extern void slrn_close_score (void);
extern int slrn_open_score (char *);
#define SLRN_XOVER_SCORING 1
#define SLRN_EXPENSIVE_SCORING 2
extern int Slrn_Perform_Scoring;
extern int Slrn_Prefer_Head;

/* This struct holds the information which scores match the selected article */
typedef struct Slrn_Score_Debug_Info_Type
{
   const char *filename;
   int linenumber;
   const char *description;
   int score;
   int stop_here;
   struct Slrn_Score_Debug_Info_Type *next;
}
Slrn_Score_Debug_Info_Type;

extern int slrn_score_header (Slrn_Header_Type *, char *,
			      Slrn_Score_Debug_Info_Type **);
#endif /* _SLRN_SCORE_H */
