/*-
 * Copyright (c) 1992-2009 Edwin Groothuis. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: user/edwin/calendar/day.c 200813 2009-12-21 21:17:59Z edwin $");

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "calendar.h"

static char *showflags(int flags);
static int isonlydigits(char *s, int star);
static char *getmonthname(int i);
static int checkmonth(char *s, int *len, int *offset, char **month);
static char *getdayofweekname(int i);
static int checkdayofweek(char *s, int *len, int *offset, char **dow);
static int isonlydigits(char *s, int nostar);
static int indextooffset(char *s);

/*
 * Expected styles:
 *
 * Date			::=	Month . ' ' . DayOfMonth |
 *				Month . ' ' . DayOfWeek . ModifierIndex |
 *				Month . '/' . DayOfMonth |
 *				Month . '/' . DayOfWeek . ModifierIndex |
 *				DayOfMonth . ' ' . Month |
 *				DayOfMonth . '/' . Month |
 *				DayOfWeek . ModifierIndex . ' ' .Month |
 *				DayOfWeek . ModifierIndex . '/' .Month |
 *				DayOfWeek . ModifierIndex |
 *				SpecialDay . ModifierOffset
 *
 * Month		::=	MonthName | MonthNumber | '*'
 * MonthNumber		::=	'0' ... '9' | '00' ... '09' | '10' ... '12'
 * MonthName		::=	MonthNameShort | MonthNameLong
 * MonthNameLong	::=	'January' ... 'December'
 * MonthNameShort	::=	'Jan' ... 'Dec' | 'Jan.' ... 'Dec.'
 *
 * DayOfWeek		::=	DayOfWeekShort | DayOfWeekLong
 * DayOfWeekShort	::=	'Mon' .. 'Sun'
 * DayOfWeekLong	::=	'Monday' .. 'Sunday'
 * DayOfMonth		::=	'0' ... '9' | '00' ... '09' | '10' ... '29' |
 *				'30' ... '31' | '*'
 *
 * ModifierOffset	::=	'' | '+' . ModifierNumber | '-' . ModifierNumber
 * ModifierNumber	::=	'0' ... '9' | '00' ... '99' | '000' ... '299' |
 *				'300' ... '359' | '360' ... '365'
 * ModifierIndex	::=	'Second' | 'Third' | 'Fourth' | 'Fifth' |
 *				'First' | 'Last'
 * 
 * SpecialDay		::=	'Easter' | 'Pashka' | 'ChineseNewYear'
 *
 */
int
determinestyle(char *date, int *flags,
    char *month, int *imonth, char *dayofmonth, int *idayofmonth,
    char *dayofweek, int *idayofweek, char *modifieroffset,
    char *modifierindex, char *specialday)
{
	char *p, *dow, *pmonth, *p1, *p2;
	char pold;
	int len, offset;

	*flags = F_NONE;
	*month = '\0';
	*imonth = 0;
	*dayofmonth = '\0';
	*idayofmonth = 0;
	*dayofweek = '\0';
	*idayofweek = 0;
	*modifieroffset = '\0';
	*modifierindex = '\0';
	*specialday = '\0';

#define CHECKSPECIAL(s1, s2, lens2, type)				\
	if (s2 != NULL && strncmp(s1, s2, lens2) == 0) {		\
		*flags |= F_SPECIALDAY;					\
		*flags |= type;						\
		*flags |= F_VARIABLE;					\
		if (strlen(s1) == lens2) {				\
			strcpy(specialday, s1);				\
			return (1);					\
		}							\
		strncpy(specialday, s1, lens2);				\
		specialday[lens2] = '\0';				\
		strcpy(modifieroffset, s1 + lens2);			\
		*flags |= F_MODIFIEROFFSET;				\
		return (1);						\
	}

	if ((p = strchr(date, ' ')) == NULL) {
		if ((p = strchr(date, '/')) == NULL) {
			CHECKSPECIAL(date, STRING_CNY, strlen(STRING_CNY),
			    F_CNY);
			CHECKSPECIAL(date, ncny.name, ncny.len, F_CNY);
			CHECKSPECIAL(date, STRING_PASKHA,
			    strlen(STRING_PASKHA), F_PASKHA);
			CHECKSPECIAL(date, npaskha.name, npaskha.len, F_PASKHA);
			CHECKSPECIAL(date, STRING_EASTER,
			    strlen(STRING_EASTER), F_EASTER);
			CHECKSPECIAL(date, neaster.name, neaster.len, F_EASTER);
			if (checkdayofweek(date, &len, &offset, &dow) != 0) {
				*flags |= F_DAYOFWEEK;
				*flags |= F_VARIABLE;
				*idayofweek = offset;
				if (strlen(date) == len) {
					strcpy(dayofweek, date);
					return (1);
				}
				strncpy(dayofweek, date, len);
				dayofweek[len] = '\0';
				strcpy(modifierindex, date + len);
				*flags |= F_MODIFIERINDEX;
				return (1);
			}
			if (isonlydigits(date, 1)) {
				/* Assume month number only */
				*flags |= F_MONTH;
				*imonth = (int)strtol(date, (char **)NULL, 10);
				strcpy(month, getmonthname(*imonth));
				return(1);
			}
			return (0);
		}
	}

	/*
	 * AFTER this, leave by goto-ing to "allfine" or "fail" to restore the
	 * original data in `date'.
	 */
	pold = *p;
	*p = 0;
	p1 = date;
	p2 = p + 1;
	/* Now p2 points to the next field and p1 to the first field */

	/* Check if there is a month-string in the date */
	if ((checkmonth(p1, &len, &offset, &pmonth) != 0)
	 || (checkmonth(p2, &len, &offset, &pmonth) != 0 && (p2 = p1))) {
		/* p2 is the non-month part */
		*flags |= F_MONTH;
		*imonth = offset;

		strcpy(month, getmonthname(offset));
		if (isonlydigits(p2, 1)) {
			strcpy(dayofmonth, p2);
			*idayofmonth = (int)strtol(p2, (char **)NULL, 10);
			*flags |= F_DAYOFMONTH;
			goto allfine;
		}
		if (strcmp(p2, "*") == 0) {
			*flags |= F_ALLDAY;
			goto allfine;
		}

		if (checkdayofweek(p2, &len, &offset, &dow) != 0) {
			*flags |= F_DAYOFWEEK;
			*flags |= F_VARIABLE;
			*idayofweek = offset;
			strcpy(dayofweek, getdayofweekname(offset));
			if (strlen(p2) == len)
				goto allfine;
			strcpy(modifierindex, p2 + len);
			*flags |= F_MODIFIERINDEX;
			goto allfine;
		}

		goto fail;
	}

	/* Check if there is an every-day or every-month in the string */
	if ((strcmp(p1, "*") == 0 && isonlydigits(p2, 1))
	 || (strcmp(p2, "*") == 0 && isonlydigits(p1, 1) && (p2 = p1))) {
		int d;

		*flags |= F_ALLMONTH;
		*flags |= F_DAYOFMONTH;
		d = (int)strtol(p2, (char **)NULL, 10);
		*idayofmonth = d;
		sprintf(dayofmonth, "%d", d);
		goto allfine;
	}

	/* Month as a number, then a weekday */
	if (isonlydigits(p1, 1)
	 && checkdayofweek(p2, &len, &offset, &dow) != 0) {
		int d;

		*flags |= F_MONTH;
		*flags |= F_DAYOFWEEK;
		*flags |= F_VARIABLE;

		*idayofweek = offset;
		d = (int)strtol(p1, (char **)NULL, 10);
		*imonth = d;
		strcpy(month, getmonthname(d));

		strcpy(dayofweek, getdayofweekname(offset));
		if (strlen(p2) == len)
			goto allfine;
		strcpy(modifierindex, p2 + len);
		*flags |= F_MODIFIERINDEX;
		goto allfine;
	}

	/* If both the month and date are specified as numbers */
	if (isonlydigits(p1, 1) && isonlydigits(p2, 0)) {
		/* Now who wants to be this ambigious? :-( */
		int m, d;

		if (strchr(p2, '*') != NULL)
			*flags |= F_VARIABLE;

		m = (int)strtol(p1, (char **)NULL, 10);
		d = (int)strtol(p2, (char **)NULL, 10);

		*flags |= F_MONTH;
		*flags |= F_DAYOFMONTH;

		if (m > 12) {
			*imonth = d;
			*idayofmonth = m;
			strcpy(month, getmonthname(d));
			sprintf(dayofmonth, "%d", m);
		} else {
			*imonth = m;
			*idayofmonth = d;
			strcpy(month, getmonthname(m));
			sprintf(dayofmonth, "%d", d);
		}
		goto allfine;
	}

	/* FALLTHROUGH */
fail:
	*p = pold;
	return (0);
allfine:
	*p = pold;
	return (1);
	
}

static void
remember(int index, int *y, int *m, int *d, int yy, int mm, int dd)
{

	y[index] = yy;
	m[index] = mm;
	d[index] = dd;
}

void
debug_determinestyle(int dateonly, char *date, int flags, char *month,
    int imonth, char *dayofmonth, int idayofmonth, char *dayofweek,
    int idayofweek, char *modifieroffset, char *modifierindex, char *specialday)
{

	if (dateonly != 0) {
		printf("-------\ndate: |%s|\n", date);
		if (dateonly == 1)
			return;
	}
	printf("flags: %x - %s\n", flags, showflags(flags));
	if (modifieroffset[0] != '\0')
		printf("modifieroffset: |%s|\n", modifieroffset);
	if (modifierindex[0] != '\0')
		printf("modifierindex: |%s|\n", modifierindex);
	if (month[0] != '\0')
		printf("month: |%s| (%d)\n", month, imonth);
	if (dayofmonth[0] != '\0')
		printf("dayofmonth: |%s| (%d)\n", dayofmonth, idayofmonth);
	if (dayofweek[0] != '\0')
		printf("dayofweek: |%s| (%d)\n", dayofweek, idayofweek);
	if (specialday[0] != '\0')
		printf("specialday: |%s|\n", specialday);
}


/*
 * Possible date formats include any combination of:
 *	3-charmonth			(January, Jan, Jan)
 *	3-charweekday			(Friday, Monday, mon.)
 *	numeric month or day		(1, 2, 04)
 *
 * Any character may separate them, or they may not be separated.  Any line,
 * following a line that is matched, that starts with "whitespace", is shown
 * along with the matched line.
 */
int
parsedaymonth(char *date, int *yearp, int *monthp, int *dayp, int *flags)
{
	char month[100], dayofmonth[100], dayofweek[100], modifieroffset[100];
	char modifierindex[100], specialday[100];
	int idayofweek, imonth, idayofmonth, year, index;

	int *mondays, d, m, dow, rm, rd, offset;

	/*
	 * CONVENTION
	 *
	 * Month:     1-12
	 * Monthname: Jan .. Dec
	 * Day:       1-31
	 * Weekday:   Mon .. Sun
	 *
	 */

	*flags = 0;

	if (debug)
		debug_determinestyle(1, date, *flags, month, imonth,
		    dayofmonth, idayofmonth, dayofweek, idayofweek,
		    modifieroffset, modifierindex, specialday);
	if (determinestyle(date, flags, month, &imonth, dayofmonth,
	    &idayofmonth, dayofweek, &idayofweek, modifieroffset,
	    modifierindex, specialday) == 0) {
		if (debug)
			printf("Failed!\n");
		return (0);
	}

	if (debug)
		debug_determinestyle(0, date, *flags, month, imonth,
		    dayofmonth, idayofmonth, dayofweek, idayofweek,
		    modifieroffset, modifierindex, specialday);

	index = 0;
	for (year = year1; year <= year2; year++) {
		mondays = mondaytab[isleap(year)];

		/* Same day every year */
		if (*flags == (F_MONTH | F_DAYOFMONTH)) {
			if (!remember_ymd(year, imonth, idayofmonth))
				continue;
			remember(index++, yearp, monthp, dayp,
			    year, imonth, idayofmonth);
			continue;
		}

		/* Same day every month */
		if (*flags == (F_ALLMONTH | F_DAYOFMONTH)) {
			for (m = 1; m <= 12; m++) {
				if (!remember_ymd(year, m, idayofmonth))
					continue;
				remember(index++, yearp, monthp, dayp,
				    year, m, idayofmonth);
			}
			continue;
		}

		/* Every day of a month */
		if (*flags == (F_ALLDAY | F_MONTH)) {
			for (d = 1; d <= mondays[imonth]; d++) {
				if (!remember_ymd(year, imonth, d))
					continue;
				remember(index++, yearp, monthp, dayp,
				    year, imonth, d);
			}
			continue;
		}

		/* One day of every month */
		if (*flags == (F_ALLMONTH | F_DAYOFWEEK)) {
			for (m = 1; m <= 12; m++) {
				if (!remember_ymd(year, m, idayofmonth))
					continue;
				remember(index++, yearp, monthp, dayp,
				    year, m, idayofmonth);
			}
			continue;
		}

		/* Every dayofweek of the year */
		if (*flags == (F_DAYOFWEEK | F_VARIABLE)) {
			dow = first_dayofweek_of_year(year);
			d = (idayofweek - dow + 8) % 7;
			while (d <= 366) {
				if (remember_yd(year, d, &rm, &rd))
					remember(index++, yearp, monthp, dayp,
					    year, rm, rd);
				d += 7;
			}
			continue;
		}

		/* A certain dayofweek of a month */
		if (*flags ==
		    (F_MONTH | F_DAYOFWEEK | F_MODIFIERINDEX | F_VARIABLE)) {
			offset = indextooffset(modifierindex);
			dow = first_dayofweek_of_month(year, imonth);
			d = (idayofweek - dow + 8) % 7;

			if (offset > 0) {
				while (d <= mondays[imonth]) {
					if (--offset == 0
					 && remember_ymd(year, imonth, d)) {
						remember(index++, yearp,
						    monthp, dayp, year, imonth,
						    d);
						continue;
					}
					d += 7;
				}
				continue;
			}
			if (offset < 0) {
				while (d <= mondays[imonth])
					d += 7;
				while (offset != 0) {
					offset++;
					d -= 7;
				}
				if (remember_ymd(year, imonth, d))
					remember(index++, yearp,
					    monthp, dayp, year, imonth, d);
				continue;
			}
			continue;
		}

		/* Every dayofweek of the month */
		if (*flags == (F_DAYOFWEEK | F_MONTH | F_VARIABLE)) {
			dow = first_dayofweek_of_month(year, imonth);
			d = (idayofweek - dow + 8) % 7;
			while (d <= mondays[imonth]) {
				if (remember_ymd(year, imonth, d))
					remember(index++, yearp, monthp, dayp,
					    year, imonth, d);
				d += 7;
			}
			continue;
		}

		printf("Unprocessed:\n");
		debug_determinestyle(2, date, *flags, month, imonth,
		    dayofmonth, idayofmonth, dayofweek, idayofweek,
		    modifieroffset, modifierindex, specialday);

	}

	return (index);

#ifdef NOTDEF
	if (!(v1 = getfield(date, &flags)))
		return (0);

	/* Easter or Easter depending days */
	if (flags & F_EASTER)
		day = v1 - 1; /* days since January 1 [0-365] */

	 /*
	  * 1. {Weekday,Day} XYZ ...
	  *
	  *    where Day is > 12
	  */
	else if (flags & F_ISDAY || v1 > 12) {

		/* found a day; day: 1-31 or weekday: 1-7 */
		day = v1;

		/* {Day,Weekday} {Month,Monthname} ... */
		/* if no recognizable month, assume just a day alone
		 * in other words, find month or use current month */
		if (!(month = getfield(endp, &flags)))
			month = tp->tm_mon + 1;
	}

	/* 2. {Monthname} XYZ ... */
	else if (flags & F_ISMONTH) {
		month = v1;

		/* Monthname {day,weekday} */
		/* if no recognizable day, assume the first day in month */
		if (!(day = getfield(endp, &flags)))
			day = 1;
	}

	/* Hm ... */
	else {
		v2 = getfield(endp, &flags);

		/*
		 * {Day} {Monthname} ...
		 * where Day <= 12
		 */
		if (flags & F_ISMONTH) {
			day = v1;
			month = v2;
			*varp = 0;
		}

		/* {Month} {Weekday,Day} ...  */
		else {
			/* F_ISDAY set, v2 > 12, or no way to tell */
			month = v1;
			/* if no recognizable day, assume the first */
			day = v2 ? v2 : 1;
			*varp = 0;
		}
	}

	/* convert Weekday into *next*  Day,
	 * e.g.: 'Sunday' -> 22
	 *       'SundayLast' -> ??
	 */
	if (flags & F_ISDAY) {
#ifdef DEBUG
		fprintf(stderr, "\nday: %d %s month %d\n", day, endp, month);
#endif

		*varp = 1;
		/* variable weekday, SundayLast, MondayFirst ... */
		if (day < 0 || day >= 10) {

			/* negative offset; last, -4 .. -1 */
			if (day < 0) {
				v1 = day / 10 - 1;	/* offset -4 ... -1 */
				day = 10 + (day % 10);	/* day 1 ... 7 */

				/* day, eg '22nd' */
				v2 = tp->tm_mday +
				    (((day - 1) - tp->tm_wday + 7) % 7);

				/* (month length - day)	/ 7 + 1 */
				if (cumdays[month + 1] - cumdays[month] >= v2
				    && ((int)((cumdays[month + 1] -
				    cumdays[month] - v2) / 7) + 1) == -v1)
					day = v2;	/* bingo ! */

				/* set to yesterday */
				else {
					day = tp->tm_mday - 1;
					if (day == 0)
						return (0);
				}
			}

			/* first, second ... +1 ... +5 */
			else {
				/* offset: +1 (first Sunday) ... */
				v1 = day / 10;
				day = day % 10;

				/* day, eg '22th' */
				v2 = tp->tm_mday +
				    (((day - 1) - tp->tm_wday + 7) % 7);

				/* Hurrah! matched */
				if (((v2 - 1 + 7) / 7) == v1 )
					day = v2;

				else {
					/* set to yesterday */
					day = tp->tm_mday - 1;
					if (day == 0)
						return (0);
				}
			}
		} else {
			/* wired */
			day = tp->tm_mday + (((day - 1) - tp->tm_wday + 7) % 7);
			*varp = 1;
		}
	}

	if (!(flags & F_EASTER)) {
		if (day + cumdays[month] > cumdays[month + 1]) {
			/* off end of month, adjust */
			day -= (cumdays[month + 1] - cumdays[month]);
			/* next year */
			if (++month > 12)
				month = 1;
		}
		*monthp = month;
		*dayp = day;
		day = cumdays[month] + day;
	} else {
		for (v1 = 0; day > cumdays[v1]; v1++)
			;
		*monthp = v1 - 1;
		*dayp = day - cumdays[v1 - 1];
		*varp = 1;
	}

#ifdef DEBUG
	fprintf(stderr, "day2: day %d(%d-%d) yday %d\n",
	    *dayp, day, cumdays[month], tp->tm_yday);
#endif

	/* When days before or days after is specified */
	/* no year rollover */
	if (day >= tp->tm_yday - f_dayBefore &&
	    day <= tp->tm_yday + f_dayAfter)
		return (1);

	/* next year */
	if (tp->tm_yday + f_dayAfter >= yrdays) {
		int end = tp->tm_yday + f_dayAfter - yrdays;
		if (day <= end)
			return (1);
	}

	/* previous year */
	if (tp->tm_yday - f_dayBefore < 0) {
		int before = yrdays + (tp->tm_yday - f_dayBefore);
		if (day >= before)
			return (1);
	}
#endif
	return (0);
}

static char *
showflags(int flags)
{
	static char s[1000];
	s[0] = '\0';

	if ((flags & F_MONTH) != 0)
		strcat(s, "month ");
	if ((flags & F_DAYOFWEEK) != 0)
		strcat(s, "dayofweek ");
	if ((flags & F_DAYOFMONTH) != 0)
		strcat(s, "dayofmonth ");
	if ((flags & F_MODIFIERINDEX) != 0)
		strcat(s, "modifierindex ");
	if ((flags & F_MODIFIEROFFSET) != 0)
		strcat(s, "modifieroffset ");
	if ((flags & F_SPECIALDAY) != 0)
		strcat(s, "specialday ");
	if ((flags & F_ALLMONTH) != 0)
		strcat(s, "allmonth ");
	if ((flags & F_ALLDAY) != 0)
		strcat(s, "allday ");
	if ((flags & F_VARIABLE) != 0)
		strcat(s, "variable ");
	if ((flags & F_CNY) != 0)
		strcat(s, "chinesenewyear ");
	if ((flags & F_PASKHA) != 0)
		strcat(s, "paskha ");
	if ((flags & F_EASTER) != 0)
		strcat(s, "easter ");

	return s;
}

static char *
getmonthname(int i)
{
	if (nmonths[i - 1].len != 0 && nmonths[i - 1].name != NULL)
		return (nmonths[i - 1].name);
	return ((char *)months[i - 1]);
}

static int
checkmonth(char *s, int *len, int *offset, char **month)
{
	struct fixs *n;
	int i;

	for (i = 0; fnmonths[i].name != NULL; i++) {
		n = fnmonths + i;
		if (strncasecmp(s, n->name, n->len) == 0) {
			*len = n->len;
			*month = n->name;
			*offset = i + 1;
			return (1);
		}
	}
	for (i = 0; nmonths[i].name != NULL; i++) {
		n = nmonths + i;
		if (strncasecmp(s, n->name, n->len) == 0) {
			*len = n->len;
			*month = n->name;
			*offset = i + 1;
			return (1);
		}
	}
	for (i = 0; fmonths[i] != NULL; i++) {
		*len = strlen(fmonths[i]);
		if (strncasecmp(s, fmonths[i], *len) == 0) {
			*month = (char *)fmonths[i];
			*offset = i + 1;
			return (1);
		}
	}
	for (i = 0; months[i] != NULL; i++) {
		if (strncasecmp(s, months[i], 3) == 0) {
			*len = 3;
			*month = (char *)months[i];
			*offset = i + 1;
			return (1);
		}
	}
	return (0);
}

static char *
getdayofweekname(int i)
{
	if (ndays[i].len != 0 && ndays[i].name != NULL)
		return (ndays[i].name);
	return ((char *)days[i]);
}

static int
checkdayofweek(char *s, int *len, int *offset, char **dow)
{
	struct fixs *n;
	int i;

	for (i = 0; fndays[i].name != NULL; i++) {
		n = fndays + i;
		if (strncasecmp(s, n->name, n->len) == 0) {
			*len = n->len;
			*dow = n->name;
			*offset = i;
			return (1);
		}
	}
	for (i = 0; ndays[i].name != NULL; i++) {
		n = ndays + i;
		if (strncasecmp(s, n->name, n->len) == 0) {
			*len = n->len;
			*dow = n->name;
			*offset = i;
			return (1);
		}
	}
	for (i = 0; fdays[i] != NULL; i++) {
		*len = strlen(fdays[i]);
		if (strncasecmp(s, fdays[i], *len) == 0) {
			*dow = (char *)fdays[i];
			*offset = i;
			return (1);
		}
	}
	for (i = 0; days[i] != NULL; i++) {
		if (strncasecmp(s, days[i], 3) == 0) {
			*len = 3;
			*dow = (char *)days[i];
			*offset = i;
			return (1);
		}
	}
	return (0);
}

static int
isonlydigits(char *s, int nostar)
{
	int i;
	for (i = 0; s[i] != '\0'; i++) {
		if (nostar == 0 && s[i] == '*' && s[i + 1] == '\0')
			return 1;
		if (!isdigit(s[i]))
			return (0);
	}
	return (1);
}

static int
indextooffset(char *s)
{
	if (strcasecmp(s, "first") == 0)
		return (1);
	if (strcasecmp(s, "second") == 0)
		return (2);
	if (strcasecmp(s, "third") == 0)
		return (3);
	if (strcasecmp(s, "fourth") == 0)
		return (4);
	if (strcasecmp(s, "fifth") == 0)
		return (5);
	if (strcasecmp(s, "last") == 0)
		return (-1);
	return (0);
}
