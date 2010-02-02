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
#include <err.h>

#include "calendar.h"

static char *showflags(int flags);
static int isonlydigits(char *s, int star);
static char *getmonthname(int i);
static int checkmonth(char *s, int *len, int *offset, char **month);
static char *getdayofweekname(int i);
static int checkdayofweek(char *s, int *len, int *offset, char **dow);
static int isonlydigits(char *s, int nostar);
static int indextooffset(char *s);
static int parseoffset(char *s);

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
static int
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
			CHECKSPECIAL(date, STRING_NEWMOON,
			    strlen(STRING_NEWMOON), F_NEWMOON);
			CHECKSPECIAL(date, nnewmoon.name, nnewmoon.len,
			    F_NEWMOON);
			CHECKSPECIAL(date, STRING_FULLMOON,
			    strlen(STRING_FULLMOON), F_FULLMOON);
			CHECKSPECIAL(date, nfullmoon.name, nfullmoon.len,
			    F_FULLMOON);
			CHECKSPECIAL(date, STRING_PASKHA,
			    strlen(STRING_PASKHA), F_PASKHA);
			CHECKSPECIAL(date, npaskha.name, npaskha.len, F_PASKHA);
			CHECKSPECIAL(date, STRING_EASTER,
			    strlen(STRING_EASTER), F_EASTER);
			CHECKSPECIAL(date, neaster.name, neaster.len, F_EASTER);
			CHECKSPECIAL(date, STRING_MAREQUINOX,
			    strlen(STRING_MAREQUINOX), F_MAREQUINOX);
			CHECKSPECIAL(date, nmarequinox.name, nmarequinox.len,
			    F_SEPEQUINOX);
			CHECKSPECIAL(date, STRING_SEPEQUINOX,
			    strlen(STRING_SEPEQUINOX), F_SEPEQUINOX);
			CHECKSPECIAL(date, nsepequinox.name, nsepequinox.len,
			    F_SEPEQUINOX);
			CHECKSPECIAL(date, STRING_JUNSOLSTICE,
			    strlen(STRING_JUNSOLSTICE), F_JUNSOLSTICE);
			CHECKSPECIAL(date, njunsolstice.name, njunsolstice.len,
			    F_JUNSOLSTICE);
			CHECKSPECIAL(date, STRING_DECSOLSTICE,
			    strlen(STRING_DECSOLSTICE), F_DECSOLSTICE);
			CHECKSPECIAL(date, ndecsolstice.name, ndecsolstice.len,
			    F_DECSOLSTICE);
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

static void
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

struct yearinfo {
	int year;
	int ieaster, ipaskha, firstcnyday;
	int ifullmoon[MAXMOONS], inewmoon[MAXMOONS],
	    ichinesemonths[MAXMOONS], equinoxdays[2], solsticedays[2];
	int *mondays;
	struct yearinfo *next;
};
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
	int d, m, dow, rm, rd, offset;

	static struct yearinfo *years, *yearinfo;

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
		/* Get important dates for this year */
		yearinfo = years;
		while (yearinfo != NULL) {
			if (yearinfo->year == year)
				break;
			yearinfo = yearinfo -> next;
		}
		if (yearinfo == NULL) {
			yearinfo = (struct yearinfo *)calloc(1,
			    sizeof(struct yearinfo));
			if (yearinfo == NULL)
				errx(1, "Unable to allocate more years");
			yearinfo->year = year;
			yearinfo->next = years;
			years = yearinfo;

			yearinfo->mondays = mondaytab[isleap(year)];
			yearinfo->ieaster = easter(year);
			pom(year, yearinfo->ifullmoon, yearinfo->inewmoon);
			equinoxsolstice(year, 0.0,
			    yearinfo->equinoxdays, yearinfo->solsticedays);

			/*
			 * CNY: Match day with sun longitude at 330` with new
			 * moon
			 */
			yearinfo->firstcnyday = calculatesunlongitude30(year,
			    120, yearinfo->ichinesemonths);
			for (m = 0; yearinfo->inewmoon[m] != 0; m++) {
				if (yearinfo->inewmoon[m] >
				    yearinfo->firstcnyday) {
					yearinfo->firstcnyday =
					    yearinfo->inewmoon[m - 1];
					break;
				}
			}
		}

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
			for (d = 1; d <= yearinfo->mondays[imonth]; d++) {
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
				while (d <= yearinfo->mondays[imonth]) {
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
				while (d <= yearinfo->mondays[imonth])
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
			while (d <= yearinfo->mondays[imonth]) {
				if (remember_ymd(year, imonth, d))
					remember(index++, yearp, monthp, dayp,
					    year, imonth, d);
				d += 7;
			}
			continue;
		}

		/* Easter */
		if ((*flags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_EASTER)) {
			offset = 0;
			if ((*flags & F_MODIFIEROFFSET) != 0)
				offset = parseoffset(modifieroffset);
			if (remember_yd(year, yearinfo->ieaster + offset,
			    &rm, &rd))
				remember(index++, yearp, monthp, dayp,
				    year, rm, rd);
			continue;
		}

		/* Paskha */
		if ((*flags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_PASKHA)) {
			offset = 0;
			if ((*flags & F_MODIFIEROFFSET) != 0)
				offset = parseoffset(modifieroffset);
			if (remember_yd(year, yearinfo->ipaskha + offset,
			    &rm, &rd))
				remember(index++, yearp, monthp, dayp,
				    year, rm, rd);
			continue;
		}

		/* Chinese New Year */
		if ((*flags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_CNY)) {
			offset = 0;
			if ((*flags & F_MODIFIEROFFSET) != 0)
				offset = parseoffset(modifieroffset);
			if (remember_yd(year, yearinfo->firstcnyday + offset,
			    &rm, &rd))
				remember(index++, yearp, monthp, dayp,
				    year, rm, rd);
			continue;
		}

		/* FullMoon */
		if ((*flags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_FULLMOON)) {
			int i;

			offset = 0;
			if ((*flags & F_MODIFIEROFFSET) != 0)
				offset = parseoffset(modifieroffset);
			for (i = 0; yearinfo->ifullmoon[i] != 0; i++) {
				if (remember_yd(year,
				    yearinfo->ifullmoon[i] + offset, &rm, &rd))
					remember(index++, yearp, monthp, dayp,
						year, rm, rd);
			}
			continue;
		}

		/* NewMoon */
		if ((*flags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_NEWMOON)) {
			int i;

			offset = 0;
			if ((*flags & F_MODIFIEROFFSET) != 0)
				offset = parseoffset(modifieroffset);
			for (i = 0; yearinfo->ifullmoon[i] != 0; i++) {
				if (remember_yd(year,
				    yearinfo->inewmoon[i] + offset, &rm, &rd))
					remember(index++, yearp, monthp, dayp,
						year, rm, rd);
			}
			continue;
		}

		/* (Mar|Sep)Equinox */
		if ((*flags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_MAREQUINOX)) {
			offset = 0;
			if ((*flags & F_MODIFIEROFFSET) != 0)
				offset = parseoffset(modifieroffset);
			if (remember_yd(year, yearinfo->equinoxdays[0] + offset,
			    &rm, &rd))
				remember(index++, yearp, monthp, dayp,
				    year, rm, rd);
			continue;
		}
		if ((*flags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_SEPEQUINOX)) {
			offset = 0;
			if ((*flags & F_MODIFIEROFFSET) != 0)
				offset = parseoffset(modifieroffset);
			if (remember_yd(year, yearinfo->equinoxdays[1] + offset,
			    &rm, &rd))
				remember(index++, yearp, monthp, dayp,
				    year, rm, rd);
			continue;
		}

		/* (Jun|Dec)Solstice */
		if ((*flags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_JUNSOLSTICE)) {
			offset = 0;
			if ((*flags & F_MODIFIEROFFSET) != 0)
				offset = parseoffset(modifieroffset);
			if (remember_yd(year,
			    yearinfo->solsticedays[0] + offset, &rm, &rd))
				remember(index++, yearp, monthp, dayp,
				    year, rm, rd);
			continue;
		}
		if ((*flags & ~F_MODIFIEROFFSET) ==
		    (F_SPECIALDAY | F_VARIABLE | F_DECSOLSTICE)) {
			offset = 0;
			if ((*flags & F_MODIFIEROFFSET) != 0)
				offset = parseoffset(modifieroffset);
			if (remember_yd(year,
			    yearinfo->solsticedays[1] + offset, &rm, &rd))
				remember(index++, yearp, monthp, dayp,
				    year, rm, rd);
			continue;
		}

		printf("Unprocessed:\n");
		debug_determinestyle(2, date, *flags, month, imonth,
		    dayofmonth, idayofmonth, dayofweek, idayofweek,
		    modifieroffset, modifierindex, specialday);
	}

	return (index);
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
	if ((flags & F_FULLMOON) != 0)
		strcat(s, "fullmoon ");
	if ((flags & F_NEWMOON) != 0)
		strcat(s, "newmoon ");
	if ((flags & F_MAREQUINOX) != 0)
		strcat(s, "marequinox ");
	if ((flags & F_SEPEQUINOX) != 0)
		strcat(s, "sepequinox ");
	if ((flags & F_JUNSOLSTICE) != 0)
		strcat(s, "junsolstice ");
	if ((flags & F_DECSOLSTICE) != 0)
		strcat(s, "decsolstice ");

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

static int
parseoffset(char *s)
{

	return strtol(s, NULL, 10);
}
