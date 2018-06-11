/*
 *  This program may be freely redistributed,
 *  but this entire comment MUST remain intact.
 *
 *  Copyright (c) 1984, 1989, William LeFebvre, Rice University
 *  Copyright (c) 1989, 1990, 1992, William LeFebvre, Northwestern University
 *
 * $FreeBSD$
 */

/*
 *  This file contains various handy utilities used by top.
 */

#include "top.h"
#include "utils.h"

#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <paths.h>
#include <kvm.h>

int
atoiwi(const char *str)
{
    size_t len;

    len = strlen(str);
    if (len != 0)
    {
	if (strncmp(str, "infinity", len) == 0 ||
	    strncmp(str, "all",      len) == 0 ||
	    strncmp(str, "maximum",  len) == 0)
	{
	    return(Infinity);
	}
	else if (str[0] == '-')
	{
	    return(Invalid);
	}
	else
	{
	    return(atoi(str));
	}
    }
    return(0);
}

/*
 *  itoa - convert integer (decimal) to ascii string for positive numbers
 *  	   only (we don't bother with negative numbers since we know we
 *	   don't use them).
 */

				/*
				 * How do we know that 16 will suffice?
				 * Because the biggest number that we will
				 * ever convert will be 2^32-1, which is 10
				 * digits.
				 */
_Static_assert(sizeof(int) <= 4, "buffer too small for this sized int");

char *
itoa(unsigned int val)
{
    char *ptr;
    static char buffer[16];	/* result is built here */
    				/* 16 is sufficient since the largest number
				   we will ever convert will be 2^32-1,
				   which is 10 digits. */

    ptr = buffer + sizeof(buffer);
    *--ptr = '\0';
    if (val == 0)
    {
	*--ptr = '0';
    }
    else while (val != 0)
    {
	*--ptr = (val % 10) + '0';
	val /= 10;
    }
    return(ptr);
}

/*
 *  itoa7(val) - like itoa, except the number is right justified in a 7
 *	character field.  This code is a duplication of itoa instead of
 *	a front end to a more general routine for efficiency.
 */

char *
itoa7(int val)
{
    char *ptr;
    static char buffer[16];	/* result is built here */
    				/* 16 is sufficient since the largest number
				   we will ever convert will be 2^32-1,
				   which is 10 digits. */

    ptr = buffer + sizeof(buffer);
    *--ptr = '\0';
    if (val == 0)
    {
	*--ptr = '0';
    }
    else while (val != 0)
    {
	*--ptr = (val % 10) + '0';
	val /= 10;
    }
    while (ptr > buffer + sizeof(buffer) - 7)
    {
	*--ptr = ' ';
    }
    return(ptr);
}

/*
 *  digits(val) - return number of decimal digits in val.  Only works for
 *	non-negative numbers.  If val <= 0 then digits(val) == 0.
 */

int __pure2
digits(int val)
{
    int cnt = 0;
	if (val == 0) {
		return 1;
	}

    while (val > 0) {
		cnt++;
		val /= 10;
    }
    return(cnt);
}

/*
 * string_index(string, array) - find string in array and return index
 */

int
string_index(const char *string, const char * const *array)
{
    size_t i = 0;

    while (*array != NULL)
    {
	if (strcmp(string, *array) == 0)
	{
	    return(i);
	}
	array++;
	i++;
    }
    return(-1);
}

/*
 * argparse(line, cntp) - parse arguments in string "line", separating them
 *	out into an argv-like array, and setting *cntp to the number of
 *	arguments encountered.  This is a simple parser that doesn't understand
 *	squat about quotes.
 */

const char * const *
argparse(char *line, int *cntp)
{
    const char **ap;
    static const char *argv[1024] = {0};

    *cntp = 1;
    ap = &argv[1];
    while ((*ap = strsep(&line, " ")) != NULL) {
        if (**ap != '\0') {
            (*cntp)++;
            if (*cntp >= (int)nitems(argv)) {
                break;
            }
	    ap++;
        }
    }
    return (argv);
}

/*
 *  percentages(cnt, out, new, old, diffs) - calculate percentage change
 *	between array "old" and "new", putting the percentages i "out".
 *	"cnt" is size of each array and "diffs" is used for scratch space.
 *	The array "old" is updated on each call.
 *	The routine assumes modulo arithmetic.  This function is especially
 *	useful on for calculating cpu state percentages.
 */

long
percentages(int cnt, int *out, long *new, long *old, long *diffs)
{
    int i;
    long change;
    long total_change;
    long *dp;
    long half_total;

    /* initialization */
    total_change = 0;
    dp = diffs;

    /* calculate changes for each state and the overall change */
    for (i = 0; i < cnt; i++)
    {
	if ((change = *new - *old) < 0)
	{
	    /* this only happens when the counter wraps */
	    change = (int)
		((unsigned long)*new-(unsigned long)*old);
	}
	total_change += (*dp++ = change);
	*old++ = *new++;
    }

    /* avoid divide by zero potential */
    if (total_change == 0)
    {
	total_change = 1;
    }

    /* calculate percentages based on overall change, rounding up */
    half_total = total_change / 2l;

    /* Do not divide by 0. Causes Floating point exception */
    if(total_change) {
        for (i = 0; i < cnt; i++)
        {
          *out++ = (int)((*diffs++ * 1000 + half_total) / total_change);
        }
    }

    /* return the total in case the caller wants to use it */
    return(total_change);
}

/* format_time(seconds) - format number of seconds into a suitable
 *		display that will fit within 6 characters.  Note that this
 *		routine builds its string in a static area.  If it needs
 *		to be called more than once without overwriting previous data,
 *		then we will need to adopt a technique similar to the
 *		one used for format_k.
 */

/* Explanation:
   We want to keep the output within 6 characters.  For low values we use
   the format mm:ss.  For values that exceed 999:59, we switch to a format
   that displays hours and fractions:  hhh.tH.  For values that exceed
   999.9, we use hhhh.t and drop the "H" designator.  For values that
   exceed 9999.9, we use "???".
 */

char *
format_time(long seconds)
{
    static char result[10];

    /* sanity protection */
    if (seconds < 0 || seconds > (99999l * 360l))
    {
	strcpy(result, "   ???");
    }
    else if (seconds >= (1000l * 60l))
    {
	/* alternate (slow) method displaying hours and tenths */
	sprintf(result, "%5.1fH", (double)seconds / (double)(60l * 60l));

	/* It is possible that the sprintf took more than 6 characters.
	   If so, then the "H" appears as result[6].  If not, then there
	   is a \0 in result[6].  Either way, it is safe to step on.
	 */
	result[6] = '\0';
    }
    else
    {
	/* standard method produces MMM:SS */
	/* we avoid printf as must as possible to make this quick */
	sprintf(result, "%3ld:%02ld",
	    (long)(seconds / 60), (long)(seconds % 60));
    }
    return(result);
}

/*
 * format_k(amt) - format a kilobyte memory value, returning a string
 *		suitable for display.  Returns a pointer to a static
 *		area that changes each call.  "amt" is converted to a
 *		string with a trailing "K".  If "amt" is 10000 or greater,
 *		then it is formatted as megabytes (rounded) with a
 *		trailing "M".
 */

/*
 * Compromise time.  We need to return a string, but we don't want the
 * caller to have to worry about freeing a dynamically allocated string.
 * Unfortunately, we can't just return a pointer to a static area as one
 * of the common uses of this function is in a large call to sprintf where
 * it might get invoked several times.  Our compromise is to maintain an
 * array of strings and cycle thru them with each invocation.  We make the
 * array large enough to handle the above mentioned case.  The constant
 * NUM_STRINGS defines the number of strings in this array:  we can tolerate
 * up to NUM_STRINGS calls before we start overwriting old information.
 * Keeping NUM_STRINGS a power of two will allow an intelligent optimizer
 * to convert the modulo operation into something quicker.  What a hack!
 */

#define NUM_STRINGS 8

char *
format_k(int amt)
{
    static char retarray[NUM_STRINGS][16];
    static int index = 0;
    char *p;
    char *ret;
    char tag = 'K';

    p = ret = retarray[index];
    index = (index + 1) % NUM_STRINGS;

    if (amt >= 10000)
    {
	amt = (amt + 512) / 1024;
	tag = 'M';
	if (amt >= 10000)
	{
	    amt = (amt + 512) / 1024;
	    tag = 'G';
	}
    }

    p = stpcpy(p, itoa(amt));
    *p++ = tag;
    *p = '\0';

    return(ret);
}

char *
format_k2(unsigned long long amt)
{
    static char retarray[NUM_STRINGS][16];
    static int index = 0;
    char *p;
    char *ret;
    char tag = 'K';

    p = ret = retarray[index];
    index = (index + 1) % NUM_STRINGS;

    if (amt >= 100000)
    {
	amt = (amt + 512) / 1024;
	tag = 'M';
	if (amt >= 100000)
	{
	    amt = (amt + 512) / 1024;
	    tag = 'G';
	}
    }

    p = stpcpy(p, itoa((int)amt));
    *p++ = tag;
    *p = '\0';

    return(ret);
}

int
find_pid(pid_t pid)
{
	kvm_t *kd = NULL;
	struct kinfo_proc *pbase = NULL;
	int nproc;
	int ret = 0;

	kd = kvm_open(NULL, _PATH_DEVNULL, NULL, O_RDONLY, NULL);
	if (kd == NULL) {
		fprintf(stderr, "top: kvm_open() failed.\n");
		quit(TOP_EX_SYS_ERROR);
	}

	pbase = kvm_getprocs(kd, KERN_PROC_PID, pid, &nproc);
	if (pbase == NULL) {
		goto done;
	}

	if ((nproc == 1) && (pbase->ki_pid == pid)) {
		ret = 1;
	}

done:
	kvm_close(kd);	
	return ret;
}
