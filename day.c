/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "calendar.h"

struct tm		tp1, tp2;
time_t			time1, time2;
static const struct tm	tm0;
int			*cumdays, yrdays;
char			dayname[10];


/* 1-based month, 0-based days, cumulative */
int	daytab[][14] = {
	{0, -1, 30, 58, 89, 119, 150, 180, 211, 242, 272, 303, 333, 364},
	{0, -1, 30, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
};


void
settimes(time_t now, int before, int after)
{
	char *oldl, *lbufp;
	struct tm tp;

	localtime_r(&now, &tp);
	if (isleap(tp.tm_year + 1900)) {
		yrdays = 366;
		cumdays = daytab[1];
	} else {
		yrdays = 365;
		cumdays = daytab[0];
	}
	/* Friday displays Monday's events */
	if (f_dayAfter == 0 && f_dayBefore == 0 && Friday != -1)
		f_dayAfter = tp.tm_wday == Friday ? 3 : 1;

	time1 = now - SECSPERDAY * f_dayBefore;
	localtime_r(&time1, &tp1);
	time2 = now + SECSPERDAY * f_dayAfter;
	localtime_r(&time2, &tp2);

	header[5].iov_base = dayname;

	oldl = NULL;
	lbufp = setlocale(LC_TIME, NULL);
	if (lbufp != NULL && (oldl = strdup(lbufp)) == NULL)
		errx(1, "cannot allocate memory");
	(void)setlocale(LC_TIME, "C");
	header[5].iov_len = strftime(dayname, sizeof(dayname), "%A", &tp);
	(void)setlocale(LC_TIME, (oldl != NULL ? oldl : ""));
	if (oldl != NULL)
		free(oldl);

	setnnames();
}

/* convert Day[/Month][/Year] into unix time (since 1970)
 * Day: two digits, Month: two digits, Year: digits
 */
time_t
Mktime(char *dp)
{
	time_t t;
	int d, m, y;
	struct tm tm, tp;

	(void)time(&t);
	localtime_r(&t, &tp);

	tm = tm0;
	tm.tm_mday = tp.tm_mday;
	tm.tm_mon = tp.tm_mon;
	tm.tm_year = tp.tm_year;

	switch (sscanf(dp, "%d.%d.%d", &d, &m, &y)) {
	case 3:
		if (y > 1900)
			y -= 1900;
		tm.tm_year = y;
		/* FALLTHROUGH */
	case 2:
		tm.tm_mon = m - 1;
		/* FALLTHROUGH */
	case 1:
		tm.tm_mday = d;
	}

#ifdef DEBUG
	fprintf(stderr, "Mktime: %d %d %s\n",
	    (int)mktime(&tm), (int)t, asctime(&tm));
#endif
	return (mktime(&tm));
}



/* return offset for variable weekdays
 * -1 -> last weekday in month
 * +1 -> first weekday in month
 * ... etc ...
 */
int
getdayvar(char *s)
{
	int offs;

	offs = strlen(s);

	/* Sun+1 or Wednesday-2
	 *    ^              ^   */

	/* fprintf(stderr, "x: %s %s %d\n", s, s + offs - 2, offs); */
	switch (*(s + offs - 2)) {
	case '-':
		return (-(atoi(s + offs - 1)));
	case '+':
		return (atoi(s + offs - 1));
	}

	/*
	 * some aliases: last, first, second, third, fourth
	 */

	/* last */
	if      (offs > 4 && !strcasecmp(s + offs - 4, "last"))
		return (-1);
	else if (offs > 5 && !strcasecmp(s + offs - 5, "first"))
		return (+1);
	else if (offs > 6 && !strcasecmp(s + offs - 6, "second"))
		return (+2);
	else if (offs > 5 && !strcasecmp(s + offs - 5, "third"))
		return (+3);
	else if (offs > 6 && !strcasecmp(s + offs - 6, "fourth"))
		return (+4);

	/* no offset detected */
	return (0);
}

