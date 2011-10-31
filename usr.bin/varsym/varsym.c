/*
 * Copyright (c) 2003,2004 The DragonFly Project.  All rights reserved.
 * Copyright (c) 2007-2009 The Aerospace Corporation.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Matthew Dillon <dillon@backplane.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/varsym.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void listvars(int type);
static void printvar(int scope, const char* name);
static void usage(void);

int
main(int argc, char **argv)
{
	char *p;
	int ch;
	int scope;
	int printenv_mode;
	int want_clear, want_delete, want_proc, want_proc_priv, want_system;

	printenv_mode = 0;
	want_clear = want_delete = 0;
	want_proc = want_proc_priv = want_system = 0;
	while ((ch = getopt(argc, argv, "deiPps")) != -1)
		switch(ch) {
		case 'd':
			want_delete = 1;
			break;
		case 'e':
			printenv_mode = 1;
			break;
		case 'i':
			want_clear = 1;
			break;
		case 'P':
			want_proc_priv = 1;
			break;
		case 'p':
			want_proc = 1;
			break;
		case 's':
			want_system = 1;
			break;
		case '?':
		default:
			usage();
		}
	if (want_proc && want_proc_priv)
		errx(EXIT_FAILURE, "-p and -P are incompatable");
	if (want_proc && want_system)
		errx(EXIT_FAILURE, "-p and -s are incompatable");
	if (want_proc_priv && want_system)
		errx(EXIT_FAILURE, "-P and -s are incompatable");
	if (want_clear && want_delete)
		errx(EXIT_FAILURE, "-d and -i are incompatable");
	if (printenv_mode && want_delete)
		errx(EXIT_FAILURE, "-d and -P are incompatable");
	if (printenv_mode && want_clear)
		errx(EXIT_FAILURE, "-i and -P are incompatable");

	if (want_proc)
		scope = VARSYM_PROC;
	else if (want_proc_priv)
		scope = VARSYM_PROC_PRIV;
	else if (want_system)
		scope = VARSYM_SYS;
	else
		scope = VARSYM_ALL;

	argv += optind;
	if (*argv == NULL) {
		if (!want_clear) {
			listvars(scope);
			exit(0);
		} else if (scope != VARSYM_SYS) {
			errx(EXIT_FAILURE,
			    "-i requires a command or -s");
		}
		/*
		 * If we want to clear the system set, we fall through and
		 *  do that from here.
		 */
	}

	if (printenv_mode) {
		if (argv[1] != NULL)
			errx(EXIT_FAILURE, "Too many arguments.");
		printvar(scope, *argv);
	}

	/*
	 * If we're not listing variables, we actually want to default to
	 * process scope.
	 */
	if (scope == VARSYM_ALL)
		scope = VARSYM_PROC;

	if (want_delete) {
		for (; *argv != NULL; ++argv) {
			if (varsym_set(scope, 0, *argv, NULL) != 0 &&
			    errno != ENOENT)
				warn("failed to delete '%s'", *argv);
		}
		exit(0);
	}

	if (want_clear)
		if (varsym_set(scope, 0, NULL, NULL) != 0)
			err(EXIT_FAILURE, "varsym_clear()");

	for (; *argv && (p = strchr(*argv, '=')); ++argv) {
		*p = '\0';
		if (varsym_set(scope, 0, *argv, ++p) != 0)
			err(EXIT_FAILURE, "varsym_set(%s)", *argv);
		*p = '=';
	}

	if (*argv != NULL) {
		if (want_system)
			errx(EXIT_FAILURE, "too many arguments for -s");
		execvp(*argv, argv);
		err(errno == ENOENT ? 127 : 126, "%s", *argv);
	}

	exit(0);
}

static void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: varsym [-ips] [name=value ...] [utility [argument ...]]\n"
	    "       varsym -d [-ps] [name ...]\n");
	exit(1);
}

static int
isinbuf(char *name, char *buf, int bytes)
{
	int b;
	int i;
	char *vname = NULL;

	for (b = i = 0; i < bytes; i++) {
		if (buf[i] == '\0') {
			if (vname == NULL) {
				vname = buf + b;
				if (strcmp(vname, name) == 0)
					return(1);
			} else {
				vname = NULL;
			}
			b = i + 1;
		}
	}
	return(0);
}

static void
dumpvars(char *buf, int bytes)
{
	int b;
	int i;
	char *vname = NULL;
	char *vdata = NULL;

	for (b = i = 0; i < bytes; i++) {
		if (buf[i] == 0) {
			if (vname == NULL) {
				vname = buf + b;
			} else {
				vdata = buf + b;
				if (!isinbuf(vname, buf, vname - buf))
					printf("%s=%s\n", vname, vdata);
				vname = vdata = NULL;
			}
			b = i + 1;
		}
	}
}

static void
listvars(int scope)
{
	char buf[3*128*1024]; /* XXX: MAXPHYS */
	size_t size, tmpsize;

	size = sizeof(buf);
	if (scope != VARSYM_ALL) {
		if (varsym_list(scope, 0, buf, &size) != 0)
			err(1, "varsym_list failed");
	} else {
		if (varsym_list(VARSYM_SYS, 0, buf, &size) != 0)
			err(1, "varsym_list failed");
		tmpsize = sizeof(buf) - size;
		if (varsym_list(VARSYM_PROC_PRIV, 0, buf+size, &tmpsize) != 0)
			err(1, "varsym_list failed");
		size += tmpsize;
		tmpsize = sizeof(buf) - size;
		if (varsym_list(VARSYM_PROC, 0, buf+size, &tmpsize) != 0)
			err(1, "varsym_list failed");
		size += tmpsize;
	}
	dumpvars(buf, size);
}

static void
printvar(int scope, const char* name)
{
	char buf[MAXVARSYM_DATA];
	size_t bufsize;

	bufsize = sizeof(buf);
	if (scope != VARSYM_ALL) {
		if (varsym_get(scope, 0, name, buf, &bufsize) != 0) {
			if (errno == ENOENT)
				exit(1);
			else
				err(EXIT_FAILURE, "varsym_get()");
		}
	} else {
		if (varsym_get(VARSYM_SYS, 0, name, buf, &bufsize) != 0) {
			if (errno != ENOENT)
				err(EXIT_FAILURE, "varsym_get()");
			if (varsym_get(VARSYM_PROC_PRIV, 0, name, buf,
			    &bufsize) != 0) {
				if (errno != ENOENT)
					err(EXIT_FAILURE, "varsym_get()");
				if (varsym_get(VARSYM_PROC, 0, name, buf,
				    &bufsize) != 0) {
					if (errno == ENOENT)
						exit(1);
					else
						err(EXIT_FAILURE,
						    "varsym_get()");
				}
			}
		}
	}
	printf("%s\n", buf);
	exit(0);
}
