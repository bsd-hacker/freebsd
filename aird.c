/*-
 * Copyright (c) 2007 Rui Paulo <rpaulo@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $P4: //depot/user/rpaulo/aird/aird.c#1 $
 *
 */

/*
 * aird - a deamon to interact with an USB Apple IR receiver.
 *        Runs commands for each key pressed.
 *
 * This daemon reads an uhid(4) device 6 bytes each time.
 * For example, when the user presses the volume up, the read buffer will
 * contain:
 *  0x25 0x87 0xee 0x[any] 0x0a/0x0b
 * 0x[any] means that there can be any value in that byte (as it's remote
 * controller dependent). The last byte is also hardware dependent.
 *
 * When I key is repeated the read will return:
 *  0x26 0x00 0x00 0x00 0x00
 *
 * For flat batteries we only check the value of the third byte (0xe0) and
 * send a syslog warning.
 *
 * All the magic numbers came from the Linux appleir driver (now integrated
 * into LIRC).
 *
 */

#if 0
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#endif

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>

#include <inttypes.h>

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

#include <libutil.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbhid.h>

static struct pidfh *pfh;

static void	sighandler(int sig);
static void 	usage(void);
static void 	runcmd(const char *cmd, int fd);


static void
sighandler(__unused int sig)
{

	if (pfh)
		pidfile_remove(pfh);

	exit(EXIT_SUCCESS);
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-vd] [-p pidfile] -f device "
	    "[-M menu command]\n\t[-P play command] [-F forward command] "
	    "[-B backward command]\n\t[-U volume up command] "
	    "[-D volume down command]\n", getprogname());

	exit(1);
}

static void
runcmd(const char *cmd, int fd)
{
	int r;

	if (fork() == 0) {
		close(fd);
		signal(SIGCHLD, SIG_DFL);
		r = system(cmd);
		_exit(r);
	}
}

int
main(int argc, char *argv[])
{
	int fd;
	int inpair;
	int repeating;
	char ch;
	unsigned char buf[5];
	unsigned char prevbuf[5];
	int foreground;
	int verbose;
	char *cmd_menu, *cmd_play, *cmd_forward, *cmd_backward,
	    *cmd_volup, *cmd_voldown;
	pid_t otherpid;
	const char *pidfile;
	const char *deventry;
	unsigned char key;

	pfh = NULL;

	signal(SIGHUP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGCHLD, SIG_IGN);

	deventry     = NULL;
	foreground   = 0;
	verbose	     = 0;
	repeating    = 0;
	cmd_menu     = NULL;
	cmd_play     = NULL;
	cmd_forward  = NULL;
	cmd_backward = NULL;
	cmd_volup    = NULL;
	cmd_voldown  = NULL;
	pidfile      = "/var/run/aird.pid";
	key	     = 0;
	inpair	     = 0;
	
	while ((ch = getopt(argc, argv, "f:dvk:p:M:P:F:B:U:D:?")) != -1)
		switch (ch) {
		case 'f':
			deventry = optarg;
			break;
		case 'd':
			foreground = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'k':
			key = atoi(optarg);
			if (key == 0) {
				foreground = 1;
				inpair = 1;
			}
			break;
		case 'M':
			cmd_menu = optarg;
			break;
		case 'P':
			cmd_play = optarg;
			break;
		case 'F':
			cmd_forward = optarg;
			break;
		case 'B':
			cmd_backward = optarg;
			break;
		case 'U':
			cmd_volup = optarg;
			break;
		case 'D':
			cmd_voldown = optarg;
			break;
		case 'p':
			pidfile = optarg;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
        argc -= optind;
        argv += optind;

	if (!deventry)
		usage();

	fd = open(deventry, O_RDONLY);
	if (fd < 0)
		err(EXIT_FAILURE, "open %s", deventry);

	if (!foreground) {
		pfh = pidfile_open(pidfile, 0600, &otherpid);
		if (pfh == NULL) {
			if (errno == EEXIST) {
				errx(EXIT_FAILURE,
				    "Daemon already running, pid: %jd.",
				    (intmax_t)otherpid);
			}
			/* If we cannot create pidfile from other reasons,
			   only warn. */
			warn("Cannot open or create pidfile");
		}
		
		if (daemon(0, 0) < 0) {
			pidfile_remove(pfh);
			err(EXIT_FAILURE, "daemon");
		}
		pidfile_write(pfh);
	}
	
	memset(prevbuf, 0, sizeof(prevbuf));
	while (read(fd, &buf, sizeof(buf)) >= 0) {
		if (verbose)
			fprintf(stderr, "%x %x %x %x %x\n", buf[0], buf[1],
			    buf[2], buf[3], buf[4]);

		/*
		 * Handle pairing.
		 */
		if (inpair && buf[2] == 0xe0) {
			fprintf(stderr, "Your Apple remote pairing key is: "
			    "%d\n"
			    "Next time you run aird, pass this number as the "
			    "-k argument.\n", buf[3]);
			exit(EXIT_SUCCESS);
		}

		if (key && buf[3] != key)
			continue;
		
		/*
		 * Check for key repeats.
		 */
		if (buf[0] == 0x26)
			repeating++;
		else {
			memcpy(prevbuf, buf, sizeof(prevbuf));
			repeating = 0;
		}

		/*
		 * The controller can generate repeating key codes
		 * very fast and this makes a lot of users upset.
		 *
		 * This check makes the program only consider repeats
		 * if the key has been pressed for actually 1 second.
		 */
		if (repeating >= 5) {
			if (verbose)
				fprintf(stderr, "repeating key: %x %x %x %x "
				    "%x\n", prevbuf[0], prevbuf[1],
				    prevbuf[2], prevbuf[3], prevbuf[4]);
			
			memcpy(buf, prevbuf, sizeof(buf));
			repeating = 0;
		}
		
		switch (buf[4]) {
		/* Menu */	
		case 0x02:
		case 0x03:
			runcmd(cmd_menu, fd);
			break;
		/* Play/Pause */
		case 0x04:
		case 0x05:
			runcmd(cmd_play, fd);
			break;
		/* Forward */
		case 0x06:
		case 0x07:
			runcmd(cmd_forward ,fd);
			break;
		/* Backward */
		case 0x08:
		case 0x09:
			runcmd(cmd_backward, fd);
			break;
		/* Volume Up */
		case 0x0a:
		case 0x0b:
			runcmd(cmd_volup, fd);
			break;
		/* Volume Down */
		case 0x0c:
		case 0x0d:
			runcmd(cmd_voldown, fd);
			break;

		}

	}
	pidfile_remove(pfh);
	close(fd);

	return (0);
}
