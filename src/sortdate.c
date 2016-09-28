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
#include "config.h"
#include "slrnfeat.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "jdmacros.h"
#include "sortdate.h"
#include "strutil.h"

/* This routine parses the date field of a header and returns it as a time_t
 * object.  The main difficulity is the lack of uniformity in the date formats
 * and the fact that there are leap years, etc...
 *
 * Grepping my news spool of more than 12000 articles revealed that the
 * date headers of the vast majority of headers appear in one of the
 * following forms:
 *
 *   03 Feb 1997 10:09:58 +0100
 *   5 Feb 1997 15:32:05 GMT
 *   Wed, 05 Feb 1997 11:58:33 -0800
 *
 * A few look like:
 *   Fri, 3 Jan 1997 07:35:15 -0800 (PST)
 *
 * and very few (less than 1 percent) of the articles had Date headers of the
 * form:
 *
 *   Tue, 21 Jan 1997 14:17:31 UNDEFINED
 *   02 Feb 97 14:20:35 EDT
 *   Tue, 4 Feb 1997 19:06:46 LOCAL
 *   Tue, 3 Feb 1997 11:51:27 UT
 *
 * This means that the timezone can be assumed to be GMT or specified via an
 * offset from GMT.  All of the above dates have the format:
 *
 *    [Weekday,] Day Month Year Hour:Min:Sec Timezone
 *
 * which will make the parsing somewhat simple.
 *
 * Beware: the calculations involving leap years, etc... are naive.
 */

#define THE_YEAR_0		1970L  /* no leap year */
#define IS_LEAP_YEAR(y)		(((y) % 4) == 0)

typedef struct
{
   char name[5];
   int tz;
}
Timezone_Table_Type;

static Timezone_Table_Type Timezone_Table [] =
{
   {"EST", -500},		/* US Eastern Standard Time*/
   {"CST", -600},		/* US Central */
   {"MST", -700},		/* US Mountain */
   {"PST", -800},		/* US Pacific  */
   {"EDT", -400},		/* US Eastern Daylight Time */
   {"CDT", -500},		/* US Central Daylight Time */
   {"MDT", -600},		/* US Mountain Daylight Time */
   {"PDT", -700},		/* US Pacific Daylight Time */
   {"CET", 100},		       /* Central European */
   {"MET", 100},		       /* Middle European */
   {"MEZ", 100},		       /* Middle European */
   {"MSK", 300},		       /* Moscow */
   {"HKT", 800},
   {"JST", 900},
   {"KST", 900},		       /* Korean Standard */
   {"CAST", 930},		       /* Central Autsralian */
   {"EAST", 1000},		       /* Eastern Autsralian */
   {"NZST", 1200},		       /* New Zealand Autsralian */
   {"EET", 200},		       /* Eastern European */
   {"", 0}
};

static int parse_timezone (char *t)
{
   char ch;
   Timezone_Table_Type *table;

   table = Timezone_Table;

   while (0 != (ch = table->name [0]))
     {
	if ((*t == ch) && (0 == strncmp (t, table->name, sizeof (table->name))))
	  return table->tz;

	table++;
     }

   return 0;
}

long slrn_date_to_order_parm (char *date)
{
   char ch;
   long day, month, year, minutes, seconds, hours, tz;
   int sign;

   date = slrn_skip_whitespace (date);
   if ((date == NULL) || (*date == 0))
     return 0;

   /* Look for a weekday, if found skip it */
   while (isalpha (*date))
     date++;
   if (*date == ',') date++;

   date = slrn_skip_whitespace (date);

   /* expecting "03 Feb 1997 10:09:58 +0100" */
   day = 0;
   while (ch = *date, isdigit (ch))
     {
	day = 10 * day + (long) (ch - '0');
	date++;
     }
   if ((day == 0) || (day > 31))
     return 0;
   day--;

   date = slrn_skip_whitespace (date);
   month = 0;
   switch (*date++)
     {
      default:
	return 0;
      case 'J':			       /* Jan, Jun, Jul */
	ch = *date++;
	if ((ch == 'u') || (ch == 'U'))
	  {
	     ch = *date++;
	     if ((ch == 'n') || (ch == 'N'))
	       month = 6;
	     else if ((ch == 'l') || (ch == 'L'))
	       month = 7;
	  }
	else if ((ch == 'a') || (ch == 'A'))
	  month = 1;
	break;

      case 'F': case 'f':	       /* Feb */
	month = 2;
	break;
      case 'M': case 'm':	       /* May, Mar */
	ch = *date++;
	if ((ch == 'a') || (ch == 'A'))
	  {
	     ch = *date++;
	     if ((ch == 'y') || (ch == 'Y')) month = 5;
	     else if ((ch == 'r') || (ch == 'R')) month = 3;
	  }
	break;
      case 'A': case 'a':	       /* Apr, Aug */
	ch = *date++;
	if ((ch == 'p') || (ch == 'P')) month = 4;
	else if ((ch == 'u') || (ch == 'U')) month = 8;
	break;

      case 'S': case 's':	       /* Sep */
	month = 9;
	break;
      case 'O': case 'o':	       /* Oct */
	month = 10;
	break;
      case 'N': case 'n':	       /* Nov */
	month = 11;
	break;
      case 'D': case 'd':	       /* Dec */
	month = 12;
	break;
     }

   if (month == 0)
     return 0;

   month--;

   /* skip past month onto year. */
   while (isalpha (*date))
     date++;
   date = slrn_skip_whitespace (date);

   year = 0;
   while (ch = *date, isdigit (ch))
     {
	year = year * 10 + (long) (ch - '0');
	date++;
     }
#if 1
   /* Sigh.  Y2K problem handling dates of the form 02 Feb 97 14:20:35 EDT. */
   if (year < 100)
     {
	if (year > 50)
	  year += 1900;
	else
	  year += 2000;
     }
#endif

   date = slrn_skip_whitespace (date);

   /* Now parse hh:mm:ss */
   hours = 0;
   while (ch = *date, isdigit (ch))
     {
	hours = hours * 10 + (long) (ch - '0');
	date++;
     }
   if ((ch != ':') || (hours >= 24)) return 0;
   date++;

   minutes = 0;
   while (ch = *date, isdigit (ch))
     {
	minutes = minutes * 10 + (long) (ch - '0');
	date++;
     }
   if (minutes >= 60)
     return 0;

   /* Seconds may not be present */
   seconds = 0;
   if (ch == ':')
     {
	date++;
	while (ch = *date, isdigit (ch))
	  {
	     seconds = seconds * 10 + (long) (ch - '0');
	     date++;
	  }
	if (seconds >= 60) return 0;
     }

   /* Now timezone */
   date = slrn_skip_whitespace (date);

   sign = 1;
   if (*date == '+')
     date++;
   else if (*date == '-')
     {
	sign = -1;
	date++;
     }

   tz = 0;
   while (ch = *date, isdigit(ch))
     {
	tz = tz * 10 + (long) (ch - '0');
	date++;
     }
   tz = sign * tz;

   date = slrn_skip_whitespace (date);
   if (isalpha (*date))
     {
	sign = 1;
	if (((*date != 'G') || (*(date+1) != 'M') || (*(date+2) != 'T')) &&
	    ((*date != 'U') || (*(date+1) != 'T')))
	  tz = parse_timezone (date);
     }

   /* Compute the number of days since beginning of year. */
   day = 31 * month + day;
   if (month > 1)
     {
	day -= (month * 4 + 27)/10;
	if (IS_LEAP_YEAR(year)) day++;
     }

   /* add that to number of days since beginning of time */
   year -= THE_YEAR_0;
   day += year * 365 + year / 4;
   if ((year % 4) == 3) day++;

   /* Adjust hours for timezone */
   hours -= tz / 100;
   minutes -= tz % 100;		       /* ?? */

   /* Now convert to secs */
   seconds += 60L * (minutes + 60L * (hours +  24L * day));

   return seconds;
}

/* Formats a given "date" (as understood by slrn_date_to_order_parm) according
 * to the format specification "format" (see strftime(3)) */
void slrn_strftime (char *s, size_t max, const char *format, char *date,
		    int use_localtime)
{
   time_t date_t;
   struct tm *date_tm;

   date_t = (time_t) slrn_date_to_order_parm (date);

#if defined(VMS) || defined(__BEOS__)
   (void) use_localtime; /* gmtime is broken on BEOS */
#else
   if (0 == use_localtime)
     date_tm = gmtime (&date_t);
   else
#endif
     date_tm = localtime (&date_t);

   (void) strftime (s, max, format, date_tm);
}
