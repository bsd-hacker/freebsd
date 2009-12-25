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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/uio.h>

#define	SECSPERDAY	24 * 60 * 60

/* Not yet categorized */

extern struct passwd *pw;
extern int doall;
extern struct iovec header[];
extern struct tm tp1, tp2;
extern time_t t1, t2;
extern const char *calendarFile;
extern int yrdays;
extern struct fixs neaster, npaskha, ncny;

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

/* Flags to determine the returned values by determinestyle() in parsedata.c */
#define	F_NONE			0x000
#define	F_MONTH			0x001
#define	F_DAYOFWEEK		0x002
#define	F_DAYOFMONTH		0x004
#define	F_MODIFIERINDEX		0x008
#define	F_MODIFIEROFFSET	0x010
#define	F_SPECIALDAY		0x020
#define	F_ALLMONTH		0x040
#define	F_ALLDAY		0x080
#define	F_VARIABLE		0x100
#define	F_EASTER		0x200
#define	F_CNY			0x400
#define	F_PASKHA		0x800

#define	STRING_EASTER	"Easter"
#define	STRING_PASKHA	"Paskha"
#define	STRING_CNY	"ChineseNewYear"

extern int	debug;		/* show parsing of the input */
extern int	f_dayAfter;	/* days after current date */
extern int	f_dayBefore;	/* days before current date */
extern int	Friday;		/* day before weekend */

/* events.c */
/*
 * Event sorting related functions:
 * - Use event_add() to create a new event
 * - Use event_continue() to add more text to the last added event
 * - Use event_print_all() to display them in time chronological order
 */
struct event *event_add(struct event *, int, int, char *, int, char *);
void	event_continue(struct event *events, char *txt);
void	event_print_all(FILE *fp, struct event *events);
struct event {
	int	month;
	int	day;
	int	var;
	char	*date;
	char	*text;
	struct event *next;
};

/* locale.c */

struct fixs {
	char	*name;
	int	len;
};

extern struct fixs fndays[8];		/* full national days names */
extern struct fixs ndays[8];		/* short national days names */
extern struct fixs fnmonths[13];	/* full national months names */
extern struct fixs nmonths[13];		/* short national month names */
extern const char *months[];
extern const char *fmonths[];
extern const char *days[];
extern const char *fdays[];

void	setnnames(void);

/* day.c */
extern const struct tm tm0;
void	settimes(time_t,int, int, struct tm *tp1, struct tm *tp2);
time_t	Mktime(char *);

/* parsedata.c */
int	parsedaymonth(char *, int *, int *, int *);

/* io.c */
void	cal(void);
void	closecal(FILE *);
FILE	*opencal(void);

/* ostern.c / pashka.c */
int	paskha(int);
int	easter(int);

/* dates.c */
extern int cumdaytab[][14];
void	generatedates(struct tm *tp1, struct tm *tp2);
void	dumpdates(void);
