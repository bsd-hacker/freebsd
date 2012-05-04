#include <sys/param.h>
#include <sys/event.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sysexits.h>
#include <syslog.h>
#include <unistd.h>

/*
 * Assumptions made and corners cut:
 *
 * XXX rc.d script name == $name set in it
 *
 *   This is mostly true except in several historical cases.
 *   One big exception is sendmail.  It effectively handles
 *   several services with different names.  Ideally, those
 *   should have separate rc.d scripts.
 *   To work around exceptions, this utility accepts absolute
 *   paths to rc.d scripts as well.  Now it's preferred way
 *   to invoke it from rc.subr.
 *
 * there is no stale pidfile left from an earlier instance
 * of the service with a pid value now reused by an unrelated
 * process
 *
 *   This is an obvious race condition: Should a stale pid
 *   file exist, it will be impossible to reliably tell if
 *   it came from the current or previous instance of the
 *   service.  Hence the assumption.  Of course, it would
 *   be better just to remove any stale pidfiles in rc.subr
 *   before starting the service.
 *
 * pid value is written atomically
 *
 *   E.g., there should be no chance to read just "12" from
 *   the pidfile if the pid value is 12345.
 *
 * no pidfile or other lock mechanism used here -- relying
 * on the monitored process pidfile checked by rc.d
 *
 *   rc.d won't try to start a service if it's already running.
 */

#define MY_NAME		"rcguard"
#define PIDFILE_SUFFIX	MY_NAME

#define PATH_SERVICE	"/usr/sbin/service"

int foreground = 0;
struct pidfh *pfh = NULL;
long pidfile_timeout = 60;	/* seconds */
const char *service_command;
const char *service_name;
const char *service_pidfile = NULL;
int sig_stop = -1;		/* no signal means clean exit by default */
int verbose = 0;

void cleanup(void);
pid_t get_pid_from_file(const char *, long);
void usage(void);
int str2sig(const char *);
int watch_pid(pid_t);

int
main(int argc, char **argv)
{
	char *ep;
	char *mypidfile;
	const char *shortname;
	int c;
	int restart;
	pid_t pid;

	atexit(cleanup);

	while ((c = getopt(argc, argv, "fp:s:t:v")) != -1) {
		switch (c) {
		case 'f':
			foreground = 1;
			break;
		case 'p':
			service_pidfile = optarg;
			if (service_pidfile[0] == '\0')
				errx(EX_USAGE, "null pidfile name");
			break;
		case 's':
			if (optarg[0] == '\0')
				errx(EX_USAGE, "null signal name");
			if ((sig_stop = str2sig(optarg)) == -1)
				errx(EX_USAGE,
				    "invalid signal name %s", optarg);
			break;
		case 't':
			pidfile_timeout = strtol(optarg, &ep, 10);
			if (pidfile_timeout <= 0 || *ep != '\0')
				errx(EX_USAGE,
				    "invalid timeout value: %s", optarg);
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Can't monitor a service w/o knowing its pidfile */
	if (service_pidfile == NULL)
		usage();

	if (argc != 2)
		usage();
	service_name = argv[0];
	service_command = argv[1];
	if (service_name[0] == '\0')
		errx(EX_USAGE, "null service name");
	if (service_command[0] == '\0')
		errx(EX_USAGE, "null service command");

	/* Get basename for a nicer proctitle and messages */
	shortname = strrchr(service_name, '/');
	if (shortname == NULL || *(++shortname) == '\0')
		shortname = service_name;

	if (verbose) {
		printf("Service: %s\n", service_name);
		printf("Command: %s\n", service_command);
		printf("Pidfile: %s\n", service_pidfile);
		printf("Signal: %d\n", sig_stop);
		printf("Timeout: %ld\n", pidfile_timeout);
	}

	asprintf(&mypidfile, "%s.%s", service_pidfile, PIDFILE_SUFFIX);
	if (mypidfile == NULL)
		errx(EX_UNAVAILABLE, "out of memory in asprintf");
	if ((pfh = pidfile_open(mypidfile, 0644, &pid)) == NULL) {
		if (errno == EEXIST)
			errx(EX_UNAVAILABLE,
			    "already monitoring %s with pid %ld",
			    shortname, (long)pid);
		else
			err(EX_CANTCREAT, "failed to create own pidfile %s",
			    mypidfile);
	}

	/*
	 * It's OK to (re)write pidfile more than once, so do it here
	 * before a possibly long wait for the service pidfile
	 * to make our pid known for troubleshooting purposes.
	 */
	if (pidfile_write(pfh) == -1)
		errx(EX_UNAVAILABLE, "failed to write to own pidfile %s",
		    mypidfile);

	pid = get_pid_from_file(service_pidfile, pidfile_timeout);

	if (!foreground) {
		if (verbose)
			printf("Daemonizing; further messages in syslog\n");
		verbose = 0;	/* no stdio after daemon() */
		if (daemon(0, 0) == -1)
			err(EX_OSERR, "Failed to daemonize");
	}

	openlog(MY_NAME, LOG_CONS | LOG_PID, LOG_DAEMON);

	/* Now that we've daemonized, rewrite our pidfile with the new pid. */
	if (pidfile_write(pfh) == -1)
		syslog(LOG_ERR, "failed to write to own pidfile %s",
		    mypidfile);

	setproctitle("%s", shortname);

	c = watch_pid(pid);
	if (WIFSIGNALED(c)) {
		syslog(LOG_NOTICE, "%s terminated on signal %d",
		    shortname, WTERMSIG(c));
		restart = WTERMSIG(c) != sig_stop;
	} else if (WIFEXITED(c)) {
		syslog(LOG_NOTICE, "%s exited with status %d",
		    shortname, WEXITSTATUS(c));
		restart = 0;
	} else {
		syslog(LOG_WARNING, "%s ceased with unknown status %d",
		    shortname, c);
		restart = 1;
	}

	if (restart) {
		syslog(LOG_NOTICE, "Restarting %s", shortname);
		if (verbose)
			printf("Restarting %s\n", service_name);

		/*
		 * Although pidfile_open() sets O_CLOEXEC and so it could
		 * be OK to leave our pidfile open here, the exec'ed script
		 * will inherit our pid easily confusing programs that don't
		 * try to lock the pidfile and only check pid existence.
		 *
		 * No race condition created here as the new instance(s)
		 * will be locking the pidfile anyway.
		 */
		pidfile_remove(pfh);
		pfh = NULL;		/* for cleanup() */

		if (service_name[0] == '/') {
			if (verbose)
				printf("Running '%s %s'\n",
				    service_name, service_command);
			c = execl(service_name, service_name, service_command,
			    (char *)NULL);
		} else {
			if (verbose)
				printf("Running '%s %s %s'\n",
				    PATH_SERVICE, service_name,
				    service_command);
			c = execl(PATH_SERVICE, PATH_SERVICE,
			    service_name, service_command, (char *)NULL);
		}
		if (c == -1)
			syslog(LOG_ERR, "exec failed: %m");
		else
			syslog(LOG_ERR, "exec returned %d", c);
		exit(EX_OSERR);
	} else
		syslog(LOG_NOTICE, "%s stopped", shortname);

	exit(EX_OK);

	return (0);	/* dummy */
}

void
cleanup(void)
{

	if (pfh) {
		pidfile_remove(pfh);
		pfh = NULL;	/* in case there is another atexit() handler */
	}
}

pid_t
get_pid_from_file(const char *pidfile, long timeout)
{
	char buf[32];
	char *ep;
	FILE *fp;
	long pid;	/* will be cast to pid_t on return */
	long slept;
	long t;
	struct stat st;

	for (pid = slept = 0;;) {
		if ((fp = fopen(pidfile, "r")) == NULL) {
			if (verbose)
				printf("Failed to open %s: %s\n",
				    pidfile, strerror(errno));
			goto retry;	/* Not created yet? */
		}
		if (fgets(buf, sizeof(buf), fp) == NULL) {
			if (verbose)
				printf("Read nothing from %s\n", pidfile);
			fclose(fp);
			goto retry;	/* Not written yet? */
		}
		if (verbose > 1)
			printf("Got 1st line from pidfile %s:\n%s\n",
			    pidfile, buf);
		pid = strtol(buf, &ep, 10);
		if (pid <= 0 || !(*ep == '\0' || *ep == '\n' ||
		    *ep == '\t' || *ep == ' '))
			errx(EX_DATAERR,
			    "no pid in pidfile %s", pidfile);
		if (verbose)
			printf("Got pid %ld from %s\n", pid, pidfile);
		if (fstat(fileno(fp), &st) != 0) {
			if (verbose)
				printf("Failed to stat %s: %s\n",
				    pidfile, strerror(errno));
			fclose(fp);
			goto retry;	/* File system gone? */
		}
		fclose(fp);
		if (kill(pid, 0) != 0) {
			if (errno != ESRCH)
				err(EX_NOPERM, "failed to check pid %ld", pid);
			if (verbose)
				printf("No process with pid %ld yet\n", pid);
			goto retry;	/* Stale pidfile? */
		}
		t = time(NULL) - st.st_mtime;
		if (t >= timeout)
			warnx("pidfile %s might be stale, age %ld seconds",
			    pidfile, t);
		break;
retry:
		if (slept >= timeout)
			errx(EX_UNAVAILABLE,
			    "timeout waiting for pidfile %s", pidfile);
		/* Exponential backoff */
		t = slept ? slept : 1;
		if (verbose)
			printf("Sleeping for %ld seconds...\n", t);
		sleep(t);
		slept += t;
		if (verbose > 1)
			printf("Slept for %ld seconds so far\n", slept);
		if (verbose)
			printf("Retrying...\n");
	}

	return (pid);
}

int
str2sig(const char *s)
{
	char *ep;
	int i;

	/* First, check if it's numeric */
	i = (int)strtol(s, &ep, 10);
	if (i > 0 && *ep == '\0')
		return (i);

	/* Drop SIG prefix if present */
	if (strncmp(s, "SIG", 3) == 0 && strlen(s) > 3)
		s += 3;

	/* Search the table of signal names */
	for (i = 1; i < NSIG; i++) {
		if (strcmp(s, sys_signame[i]) == 0)
			return (i);
	}

	return (-1);
}

void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-fv] [-s sig_stop] [-t timeout] " \
	    "-p pidfile service command\n", MY_NAME);
	exit(EX_USAGE);
}

int
watch_pid(pid_t pid)
{
	int kq;
	struct kevent kev;

	if ((kq = kqueue()) == -1) {
		syslog(LOG_ERR, "kqueue: %m");
		exit(EX_OSERR);
	}

	EV_SET(&kev, pid, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT, 0, NULL);

	if (verbose)
		printf("Waiting for kevent on pid %ld...\n", (long)pid);

	switch (kevent(kq, &kev, 1, &kev, 1, NULL)) {
	case -1:
		syslog(LOG_ERR, "kevent: %m");
		exit(EX_OSERR);
	case 0:
		syslog(LOG_ERR, "kevent returned 0");
		exit(EX_OSERR);
	}

	if ((long)kev.ident != (long)pid) {
		syslog(LOG_ERR, "kevent fired on pid %ld not %ld",
		    (long)kev.ident, (long)pid);
		exit(EX_OSERR);
	}

	if (verbose)
		printf("Got exit status %d\n", (int)kev.data);

	return (kev.data);
}
