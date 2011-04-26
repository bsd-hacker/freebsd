/* ptimeout: executes command but kills it after a specified timeout
 * usage: ptimeout timeout command args ...
 *
 * $FreeBSD: ports/Tools/portbuild/scripts/ptimeout.c,v 1.2 2009/05/21 16:17:15 pav Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#if defined(DEBUG)
# define DPRINTF(fmt, ...) printf(fmt, __VA_ARGS__)
#else
# define DPRINTF(fmt, ...)
#endif

int
main(int argc, char *argv[])
{
    	int timeout, status, i;
	pid_t pid1, pid2, child;
	char *command, args[MAXPATHLEN + 1];
	time_t t;

	if (argc < 3) {
		printf("usage: %s timeout command [args ...]\n", argv[0]);
		exit(1);
	}

	timeout = atoi(argv[1]);
	command = argv[2];

	bzero(args, MAXPATHLEN + 1);
	for (i = 2; i < argc; i++) {
	    	strlcat(args, argv[i], MAXPATHLEN - strlen(args));
		strlcat(args, " ", MAXPATHLEN - strlen(args));
	}

	DPRINTF("timeout is %d\n", timeout);
	DPRINTF("arguments are %s\n", args);

	if ((pid1 = fork()) > 0) {
	    	if ((pid2 = fork()) > 0) {
		    /* parent */
		    DPRINTF("child pids are %d %d\n", pid1, pid2);
		    child = wait(&status);
		    DPRINTF("exited child is %d, status is %d\n", child, status);
		    if (pid1 == child) {
			DPRINTF("killing process %d\n", pid2);
			kill(pid2, SIGKILL);
		    } else {
			DPRINTF("killing process %d\n", pid1);
			kill(pid1, SIGKILL);
		    }
		    /* exit status in upper 8 bits, killed signal (if any)
		     * in lower 8 bits
		     */
		    exit((status >> 8) | (status & 0xff));
		} else {
		    /* second child */
		    sleep(timeout);
		    t = time(NULL);
		    printf("ptimeout: killing %s (pid %d) since timeout of %d expired at %s", args, pid1, timeout, ctime(&t));
		    kill(pid1, SIGKILL);
		    exit(1);
		}
	} else {
	    	/* first child */
		DPRINTF("executing %s\n", args);
		execvp(command, argv + 2);
	}

	/* Shouldn't be reached. */
	return 0;
}
