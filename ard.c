/*-
 * Copyright (c) 2011 Yahoo! Inc.
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

#include <sys/types.h>
#include <sys/ata.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#if __FreeBSD_version >= 600000
#include <sys/resource.h>
#endif
#include <sys/time.h>
#include <sys/uio.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <err.h>
#include <fcntl.h>

#define	MAX_UNIT	16

#define VOLUME_DEGRADED(status)		((status) != AR_READY)

#if __FreeBSD_version < 600000
#define	ata_ioc_raid_status	raid_status
#endif
	
struct ata_volume {
	struct ata_ioc_raid_status status;
	int	present;
	int	generation;
	int	prev_status;
	int	unit;
	int	sentcnt;
	int	missing_drives;
	int	disks[16];
};

static struct ata_volume volumes[MAX_UNIT];
static int fd, generation;

static char hostname[MAXHOSTNAMELEN];
static char *mailto = "root@localhost";
static int notifyminutes = 720;         /* send mail every 12 hours by default */
static int dostdout;

static int
ataraid_fetch_status(int unit, struct ata_ioc_raid_status *status)
{
#if __FreeBSD_version < 600000
	struct ata_cmd cmd;
	int retval;

	bzero(&cmd, sizeof(cmd));
	cmd.channel = unit;
	cmd.cmd = ATARAIDSTATUS;
	retval = ioctl(fd, IOCATA, &cmd);
	if (retval >= 0)
		*status = cmd.u.raid_status;
	return (retval);
#else
	status->lun = unit;
	return (ioctl(fd, IOCATARAIDSTATUS, status));
#endif
}

static void
ataraid_scan_volume(int unit)
{
	struct ata_volume *vol;
	int i, status;

	vol = &volumes[unit];
	status = vol->status.status;
	if (ataraid_fetch_status(unit, &vol->status) < 0) {
		vol->present = 0;
		return;
	}
	vol->generation = generation;
	vol->missing_drives = 0;
	for (i = 0; i < vol->status.total_disks; i++)
		if (vol->status.disks[i].lun < 0)
			vol->missing_drives++;

	/* New volume arrived. */
	if (!vol->present) {
		vol->present = 1;
		vol->prev_status = AR_READY;
		for (i = 0; i < 16; i++)
			vol->disks[i] = vol->status.disks[i].lun;
		return;
	}

	/* See if any of the present disks differ. */
	for (i = 0; i < vol->status.total_disks; i++) {
		if (vol->status.disks[i].lun < 0 || vol->disks[i] < 0)
			continue;
		if (vol->status.disks[i].lun != vol->disks[i]) {
			/* Treat it as a new volume. */
			vol->prev_status = AR_READY;
			for (i = 0; i < 16; i++)
				vol->disks[i] = vol->status.disks[i].lun;
			return;
		}
	}

	/*
         * Copy over disks but don't replace a valid disk number with
         * a missing disk so we remember what disk is missing.
	 */
	vol->prev_status = status;
	for (i = 0; i < vol->status.total_disks; i++) {
		if (vol->status.disks[i].lun < 0)
			continue;
		vol->disks[i] = vol->status.disks[i].lun;
	}
}

static void
ataraid_scan_all(void)
{
	int i;

	generation++;
	for (i = 0; i < MAX_UNIT; i++)
		ataraid_scan_volume(i);
}

static void
ataraid_rebuild(struct ata_volume *vol)
{
#if __FreeBSD_version < 600000
	struct ata_cmd cmd;
#else
	char buf[32], title[32];
#endif
	int i, failed, spares;

	/* Make sure we have enough spares before trying a rebuild. */
	failed = 0;
	spares = 0;
	for (i = 0; i < vol->status.total_disks; i++) {
		if (vol->status.disks[i].state & AR_DISK_ONLINE)
			continue;
		if (vol->status.disks[i].state & AR_DISK_SPARE)
			spares++;
		else
			failed++;
	}
	if (dostdout)
		printf("found %d failed drives and %d spares for ar%d\n",
		    failed, spares, vol->unit);
	if (spares < failed)
		return;

	switch (fork()) {
	case 0:
		/* Child process does the actual rebuild. */
		setproctitle("rebuilding ar%d", vol->unit);
		if (dostdout)
			printf("%d: initiating rebuild for ar%d\n", getpid(),
			    vol->unit);
#if __FreeBSD_version < 600000
		cmd.channel = vol->unit;
		cmd.cmd = ATARAIDREBUILD;
		ioctl(fd, IOCATA, &cmd);
#else
		if (ioctl(fd, IOCATARAIDREBUILD, &vol->unit) >= 0) {
			setpriority(PRIO_PROCESS, 0, 20);
			snprintf(title, sizeof(title), "dd: rebuilding ar%d",
			    vol->unit);
			snprintf(buf, sizeof(buf), "if=/dev/ar%d", vol->unit);
			execl("/bin/dd", title, buf, "of=/dev/null", "bs=1m",
			    NULL);
		}
#endif
		exit(0);
	default:
		break;
	}
}

static int
ataraid_open(void)
{
	int i, nvolumes;

	fd = open("/dev/ata", O_RDWR);
	if (fd < 0)
		return (0);
	generation++;
	nvolumes = 0;
	for (i = 0; i < MAX_UNIT; i++) {
		volumes[i].unit = i;
		if (ataraid_fetch_status(i, &volumes[i].status) < 0)
			continue;
		nvolumes++;
	}

	return (nvolumes);
}

static FILE *
mailer_open(void)
{
	FILE *fp;

	if (dostdout)
		fp = stdout;
	else
		fp = popen("/usr/sbin/sendmail -t", "w");
	fprintf(fp, "To: %s\n", mailto);
	return fp;
}

static void
mailer_close(FILE *fp)
{

	if (dostdout == 0)
		pclose(fp);
	else
		fflush(fp);
}

static void
mailer_write(FILE *fp, const char *fmt, ...)
{
	va_list ap;
	char *mfmt, *pfmt = NULL;

	pfmt = mfmt = strdup(fmt);

	va_start (ap, fmt);
	vfprintf (fp, fmt, ap);
	va_end (ap);

	/* XXX: Hack for Subject: */
	if (strncmp(fmt, "Subject: ", 9) == 0) {
		char *p;
		pfmt += strlen("Subject: ");
		if ((p = strchr(pfmt, '\n')) != NULL)
			*p = '\0';
	}

	if (dostdout == 0) {
		va_start (ap, fmt);
		vsyslog(LOG_CRIT, pfmt, ap);
		va_end (ap);
	}

	if (mfmt)
		free(mfmt);
}

char *
ataraid_show_failed(struct ata_volume *vol)
{
	char *str, *p;
	int i, comma = 0, failed;

	failed = 0;
	for (i = 0; i < vol->status.total_disks; i++)
		if ((vol->status.disks[i].state & (AR_DISK_ONLINE |
		    AR_DISK_SPARE)) == 0 && vol->disks[i] >= 0)
			failed++;
	if (failed == 0)
		return (NULL);

	str = calloc(failed * 8, sizeof(char));
	if (str == NULL)
		return (NULL);

	p = str;
	*p++ = '(';
	for (i = 0; i < vol->status.total_disks; i++) {
		if ((vol->status.disks[i].state & (AR_DISK_ONLINE |
		    AR_DISK_SPARE)) != 0)
			continue;
		if (vol->disks[i] >= 0) {
			if (comma++)
				*p++ = ',';
			p += sprintf(p, "ad%d", vol->disks[i]);
		}
	}
	if ((p - str) == 1) {
		p += sprintf(p, "none");
	}
	*p = ')';

	return (str);
}

static void
ataraid_notify_failure(struct ata_volume *vol)
{
	FILE *fp;
	int *sentcnt;
	char *failed;

	sentcnt = &vol->sentcnt;
	if (vol->status.status == vol->prev_status &&
	    ((*sentcnt)++ % notifyminutes) != 0)
		return;
	*sentcnt = 1;

	failed = ataraid_show_failed(vol);

	fp = mailer_open();
	mailer_write(fp, "Subject: [ATA-RAID ALERT] vol ar%d on %s\n\n",
	    vol->unit, hostname);
	if (!VOLUME_DEGRADED(vol->status.status)) {
		mailer_write(fp,
		    "%s: volume ar%d is rebuilt and no longer has errors\n",
		    hostname, vol->unit);
	} else {
		if (vol->status.status ==
		    (AR_READY | AR_DEGRADED | AR_REBUILDING))
			mailer_write(fp,
			    "%s: rebuilding volume ar%d: %d%% completed\n",
			    hostname, vol->unit, vol->status.progress);
		else if (vol->status.status != (AR_READY | AR_DEGRADED))
			mailer_write(fp, "%s: volume ar%d is lost\n", hostname,
			    vol->unit);
		if (failed)
			mailer_write(fp,
		    "%s: disk(s) on volume ar%d need to be replaced: %s\n",
			    hostname, vol->unit, failed);
		else if (vol->missing_drives)
			mailer_write(fp,
		    "%s: %d disk(s) on volume ar%d need to be replaced\n",
			    hostname, vol->missing_drives, vol->unit);
		else if (vol->status.status == (AR_READY | AR_DEGRADED))
			mailer_write(fp, "%s: volume ar%d is degraded\n",
			    hostname, vol->unit);
	}

	if (failed)
		free(failed);

	mailer_close(fp);
}

static void
ataraid_check_volumes(void)
{
	int i;

	for (i = 0; i < MAX_UNIT; i++) {
		if (!volumes[i].present)
			continue;
		if (volumes[i].status.status == (AR_READY | AR_DEGRADED))
			ataraid_rebuild(&volumes[i]);
		if (VOLUME_DEGRADED(volumes[i].status.status) ||
		    VOLUME_DEGRADED(volumes[i].prev_status))
			ataraid_notify_failure(&volumes[i]);
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: ard [-ds] [-t minutes] [mailto]\n");
	exit(1);
}

int
main(int ac, char *av[])
{
	int			ch, daemonize = 1;

	while ((ch = getopt(ac, av, "dst:")) != -1) {
		switch (ch) {
		case 'd':
			daemonize = 0;
			break;

		case 't':
			notifyminutes = atoi(optarg);
			break;

		case 's':
			dostdout = 1;
			break;
		case '?':
			usage();
		}
	}

	av += optind;
	ac -= optind;

	if (ac > 1)
		usage();
	if (ac == 1)
		mailto = av[0];

	gethostname(hostname, sizeof(hostname));

	if (daemonize) {
		if (daemon(0, 0) < 0)
			err(1, "daemon");
	}

	if (ataraid_open() == 0)
		return (0);

	for (;;) {
		ataraid_scan_all();
		ataraid_check_volumes();
		sleep(60);
	}
}
