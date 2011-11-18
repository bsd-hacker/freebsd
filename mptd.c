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
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/param.h>
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

#include "mptd.h"

#define	MAX_UNIT	16

#define	DRIVE_FAILED(Status)						\
	((Status).State != MPI_PHYSDISK0_STATUS_ONLINE &&		\
	    (Status).State != MPI_PHYSDISK0_STATUS_MISSING &&		\
	    (Status).State != MPI_PHYSDISK0_STATUS_NOT_COMPATIBLE &&	\
	    (Status).State != MPI_PHYSDISK0_STATUS_INITIALIZING)

#define	DRIVE_MISSING(Status)						\
	((Status).State == MPI_PHYSDISK0_STATUS_MISSING)

#define	DRIVE_REBUILDING(Status)					\
	((Status).State == MPI_PHYSDISK0_STATUS_ONLINE &&		\
	    ((Status).Flags & MPI_PHYSDISK0_STATUS_FLAG_OUT_OF_SYNC))

#define VOLUME_DEGRADED(Status)						\
	((Status.State) != MPI_RAIDVOL0_STATUS_STATE_OPTIMAL)

static char hostname[MAXHOSTNAMELEN];
static char *mailto = "root@localhost";
static int notifyminutes = 720;         /* send mail every 12 hours by default */
static int dostdout;

/* Maximum target_id and device_id of volumes and drives, respectively. */
#define	MPT_MAX_VOL_ID			65536
#define	MPT_MAX_PD_ID			256

struct mpt_physdisk {
	uint32_t	generation;
	U8		PhysDiskBus;
	U8		PhysDiskID;
	U8		PhysDiskNum;
	RAID_PHYS_DISK0_STATUS Status;
	uint8_t		spare;
};

struct mpt_volume {
	CONFIG_PAGE_RAID_VOL_0 *config;
	RAID_VOL0_STATUS prev_status;
	int		sentcnt;
	uint32_t	generation;
	U8		VolumeBus;
	U8		VolumeID;
	int		missing_drives;
	int		prev_missing_drives;
};

struct mpt_controller {
	int		fd;
	int		unit;
	uint32_t	generation;
	int		missing_drives;
	int		prev_missing_drives;
	int		bad_drives;
	int		prev_bad_drives;
	int		sentcnt;
	struct mpt_volume *volumes[MPT_MAX_VOL_ID];
	struct mpt_physdisk *physdisks[MPT_MAX_PD_ID];
};

static struct mpt_controller controllers[MAX_UNIT];
static int ncontrollers;

static int
mpt_drive_location(char *p, struct mpt_physdisk *pd)
{

	return (sprintf(p, "bus %d id %d", pd->PhysDiskBus, pd->PhysDiskID));
}

static void
mpt_scan_drive(struct mpt_controller *c, U8 PhysDiskNum, struct mpt_volume *v)
{
	CONFIG_PAGE_RAID_PHYS_DISK_0 *info;
	struct mpt_physdisk *pd;

	info = mpt_pd_info(c->fd, PhysDiskNum);
	if (info == NULL) {
		warn("mpt%d:disk%d: failed to fetch disk info", c->unit,
		    PhysDiskNum);
		return;
	}

	/* See if we have seen this drive before. */
	pd = c->physdisks[PhysDiskNum];
	if (pd == NULL) {
		pd = calloc(1, sizeof(struct mpt_physdisk));
		pd->PhysDiskNum = PhysDiskNum;
		c->physdisks[PhysDiskNum] = pd;
		pd->PhysDiskBus = info->PhysDiskBus;
		pd->PhysDiskID = info->PhysDiskID;
	}

	/* Update generation count and other state. */
	pd->generation = c->generation;
	pd->Status = info->PhysDiskStatus;
	pd->spare = (info->PhysDiskSettings.HotSparePool != 0);
	if (DRIVE_MISSING(info->PhysDiskStatus)) {
		if (v != NULL)
			v->missing_drives++;
		else
			c->missing_drives++;
	}
	free(info);
}

static void
mpt_scan_volume(struct mpt_controller *c, CONFIG_PAGE_IOC_2_RAID_VOL *vol)
{
	CONFIG_PAGE_RAID_VOL_0 *info;
	RAID_VOL0_PHYS_DISK *disk;
	struct mpt_volume *v;
	int i;

	info = mpt_vol_info(c->fd, vol->VolumeBus, vol->VolumeID);
	if (info == NULL) {
		warn("mpt%d:%d:%d: failed to fetch volume info", c->unit,
		    vol->VolumeBus, vol->VolumeID);
		return;
	}

	/* See if we have seen this drive before. */
	v = c->volumes[vol->VolumeBus * 256 + vol->VolumeID];
	if (v == NULL) {
		v = calloc(1, sizeof(struct mpt_volume));
		v->VolumeBus = vol->VolumeBus;
		v->VolumeID = vol->VolumeID;
		c->volumes[v->VolumeBus * 256 + vol->VolumeID] = v;

		v->prev_status = info->VolumeStatus;
	} else {
		v->prev_status = v->config->VolumeStatus;
		free(v->config);
	}

	/* Update generation count and other state. */
	v->generation = c->generation;
	v->config = info;

	/* Scan all the drives this volume spans. */
	v->prev_missing_drives = v->missing_drives;
	v->missing_drives = 0;
	disk = info->PhysDisk;
	for (i = 0; i < info->NumPhysDisks; disk++, i++)
		mpt_scan_drive(c, disk->PhysDiskNum, v);
}

static void
mpt_scan_volumes(struct mpt_controller *c)
{
	CONFIG_PAGE_IOC_2 *ioc2;
	CONFIG_PAGE_IOC_2_RAID_VOL *vol;
	struct mpt_volume *mv;
	int i;

	/* Get the volume list from the controller. */
	ioc2 = mpt_read_ioc_page(c->fd, 2);
	if (ioc2 == NULL) {
		warn("mpt%d: Failed to get volume list", c->unit);
		return;
	}

	/* Scan all the volumes. */
	vol = ioc2->RaidVolume;
	for (i = 0; i < ioc2->NumActiveVolumes; vol++, i++) {
		mpt_scan_volume(c, vol);
	}

	/* Throw away all the volumes that disappeared. */
	for (i = 0; i < MPT_MAX_VOL_ID; i++) {
		mv = c->volumes[i];
		if (mv == NULL)
			continue;
		if (mv->generation != c->generation) {
			c->volumes[i] = NULL;
			free(mv);
		}
	}
	free(ioc2);
}

static void
mpt_scan_drives(struct mpt_controller *c)
{
	CONFIG_PAGE_IOC_5 *ioc5;
	IOC_5_HOT_SPARE *spare;
	struct mpt_physdisk *pd;
	int i;

	/*
	 * Drives from active volumes are scanned when the volumes are
	 * scanned.  The only thing left for us to look at are the
	 * spare drives.
	 */
	ioc5 = mpt_read_ioc_page(c->fd, 5);
	if (ioc5 == NULL) {
		warn("mpt%d: Failed to get spare drive list", c->unit);
		return;
	}

	/* Scan all the spares. */
	c->prev_missing_drives = c->missing_drives;
	c->missing_drives = 0;
	spare = ioc5->HotSpare;
	for (i = 0; i < ioc5->NumHotSpares; spare++, i++)
		mpt_scan_drive(c, spare->PhysDiskNum, NULL);
	free(ioc5);

	/*
	 * If a drive fails when there is a hot spare, the failing
	 * drive swaps places with the hot spare.  In this case, the
	 * failed drive won't be associated with a volume, so we track
	 * them via a controller-wide bad drives count.
	 */
	c->prev_bad_drives = c->bad_drives;
	c->bad_drives = 0;
	for (i = 0; i < MPT_MAX_PD_ID; i++) {
		if (c->physdisks[i] == NULL)
			continue;
		if (!DRIVE_FAILED(c->physdisks[i]->Status))
			continue;
		if (!c->physdisks[i]->spare)
			continue;
		c->bad_drives++;
	}

	/* Throw away all the drives that disappeared. */
	for (i = 0; i < MPT_MAX_PD_ID; i++) {
		pd = c->physdisks[i];
		if (pd == NULL)
			continue;
		if (pd->generation != c->generation) {
			c->physdisks[i] = NULL;
			free(pd);
		}
	}
}

static void
mpt_scan_controller(struct mpt_controller *c)
{

	/* Bump the overall generation count. */
	c->generation++;

	mpt_scan_volumes(c);
	mpt_scan_drives(c);
}

static void
mpt_scan_all(void)
{
	int i;

	for (i = 0; i < ncontrollers; i++)
		mpt_scan_controller(&controllers[i]);
}

static int
mpt_open(void)
{
	CONFIG_PAGE_IOC_2 *ioc2;
	char path[MAXPATHLEN];
	int fd, unit;

	ncontrollers = 0;
	for (unit = 0; unit < MAX_UNIT; unit++) {
		snprintf(path, sizeof(path), "/dev/mpt%d", unit);
		fd = open(path, O_RDWR);
		if (fd < 0)
			continue;

		/*
		 * Don't bother monitoring controllers that don't
		 * support RAID volumes.  The emulated mpt(4)
		 * controllers in VMWare crash the VM when queried for
		 * a list of hot spare drives via IOC page 5, so this
		 * test lets us avoid them altogether.
		 */
		ioc2 = mpt_read_ioc_page(fd, 2);
		if (ioc2 == NULL || ioc2->MaxPhysDisks == 0) {
			if (ioc2)
				free(ioc2);
			close(fd);
			continue;
		}
		free(ioc2);
		controllers[ncontrollers].fd = fd;
		controllers[ncontrollers].unit = unit;
		ncontrollers++;
	}
	if (ncontrollers == 0)
		return (ncontrollers);

	mpt_scan_all();

	return (ncontrollers);
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

/* Look for any failed disks in this volume. */
char *
mpt_show_failed(struct mpt_controller *c, struct mpt_volume *v)
{
	RAID_VOL0_PHYS_DISK *disk;
	struct mpt_physdisk *pd;
	int i, comma = 0, instate;
	char *str, *p;

	instate = 0;
	disk = v->config->PhysDisk;
	for (i = 0; i < v->config->NumPhysDisks; disk++, i++) {
		pd = c->physdisks[disk->PhysDiskNum];
		if (pd == NULL)
			continue;
		if (DRIVE_FAILED(pd->Status))
			instate++;
	}

	if (instate == 0)
		return (NULL);

	str = calloc(instate * 64, sizeof(char));
	if (str == NULL)
		return (NULL);

	p = str;
	*p++ = '(';

	disk = v->config->PhysDisk;
	for (i = 0; i < v->config->NumPhysDisks; disk++, i++) {
		pd = c->physdisks[disk->PhysDiskNum];
		if (pd == NULL)
			continue;
		if (DRIVE_FAILED(pd->Status)) {
			if (comma++)
				*p++ = ',';
			p += sprintf(p, "drive %u at ", pd->PhysDiskNum);
			p += mpt_drive_location(p, pd);
		}
	}

	if ((p - str) == 1) {
		int n = sprintf(p, "none");
		p += n;
	}
	*p = ')';

	return (str);
}

/* Look for any rebuilding disks in this volume. */
char *
mpt_show_rebuild(struct mpt_controller *c, struct mpt_volume *v)
{
	RAID_VOL0_PHYS_DISK *disk;
	struct mpt_physdisk *pd;
	int i, comma = 0, instate;
	char *str, *p;

	instate = 0;
	disk = v->config->PhysDisk;
	for (i = 0; i < v->config->NumPhysDisks; disk++, i++) {
		pd = c->physdisks[disk->PhysDiskNum];
		if (pd == NULL)
			continue;
		if (DRIVE_REBUILDING(pd->Status))
			instate++;
	}

	if (instate == 0)
		return (NULL);

	str = calloc(instate * 64, sizeof(char));
	if (str == NULL)
		return (NULL);

	p = str;
	*p++ = '(';

	disk = v->config->PhysDisk;
	for (i = 0; i < v->config->NumPhysDisks; disk++, i++) {
		pd = c->physdisks[disk->PhysDiskNum];
		if (pd == NULL)
			continue;
		if (DRIVE_REBUILDING(pd->Status)) {
			if (comma++)
				*p++ = ',';
			p += sprintf(p, "drive %u at ", pd->PhysDiskNum);
			p += mpt_drive_location(p, pd);
		}
	}

	if ((p - str) == 1) {
		int n = sprintf(p, "none");
		p += n;
	}
	*p = ')';

	return (str);
}

char *
mpt_show_missing_vol(struct mpt_controller *c, struct mpt_volume *v)
{
	RAID_VOL0_PHYS_DISK *disk;
	struct mpt_physdisk *pd;
	int i, comma = 0, instate;
	char *str, *p;

	if (v->missing_drives == 0)
		return (NULL);

	instate = 0;
	disk = v->config->PhysDisk;
	for (i = 0; i < v->config->NumPhysDisks; disk++, i++) {
		pd = c->physdisks[disk->PhysDiskNum];
		if (pd == NULL)
			continue;
		if (DRIVE_MISSING(pd->Status))
			instate++;
	}

	if (instate == 0)
		return (NULL);

	str = calloc(instate * 64, sizeof(char));
	if (str == NULL)
		return (NULL);

	p = str;
	*p++ = '(';

	disk = v->config->PhysDisk;
	for (i = 0; i < v->config->NumPhysDisks; disk++, i++) {
		pd = c->physdisks[disk->PhysDiskNum];
		if (pd == NULL)
			continue;
		if (DRIVE_MISSING(pd->Status)) {
			if (comma++)
				*p++ = ',';
			p += sprintf(p, "drive %u at ", pd->PhysDiskNum);
			p += mpt_drive_location(p, pd);
		}
	}

	if ((p - str) == 1) {
		int n = sprintf(p, "none");
		p += n;
	}
	*p = ')';

	return (str);
}

char *
mpt_show_missing_controller(struct mpt_controller *c)
{
	struct mpt_physdisk *pd;
	char *str, *p;
	int i, comma = 0, instate;

	if (c->missing_drives == 0)
		return (NULL);

	instate = 0;
	for (i = 0; i < MPT_MAX_PD_ID; i++) {
		pd = c->physdisks[i];
		if (pd == NULL)
			continue;
		if (pd->spare && DRIVE_MISSING(pd->Status))
			instate++;
	}
	if (instate == 0)
		return (NULL);

	str = calloc(instate * 64, sizeof(char));
	if (str == NULL)
		return (NULL);

	p = str;
	*p++ = '(';
	for (i = 0; i < MPT_MAX_PD_ID; i++) {
		pd = c->physdisks[i];
		if (pd == NULL)
			continue;
		if (pd->spare && DRIVE_MISSING(pd->Status)) {
			if (comma++)
				*p++ = ',';
			p += sprintf(p, "drive %u at ", pd->PhysDiskNum);
			p += mpt_drive_location(p, pd);
		}
	}
	if ((p - str) == 1) {
		int n = sprintf(p, "none");
		p += n;
	}
	*p = ')';

	return (str);
}

char *
mpt_show_bad(struct mpt_controller *c)
{
	struct mpt_physdisk *pd;
	char *str, *p;
	int i, comma = 0, instate;

	if (c->bad_drives == 0)
		return (NULL);

	instate = 0;
	for (i = 0; i < MPT_MAX_PD_ID; i++) {
		pd = c->physdisks[i];
		if (pd == NULL)
			continue;
		if (pd->spare && DRIVE_FAILED(pd->Status))
			instate++;
	}
	if (instate == 0)
		return (NULL);

	str = calloc(instate * 64, sizeof(char));
	if (str == NULL)
		return (NULL);

	p = str;
	*p++ = '(';
	for (i = 0; i < MPT_MAX_PD_ID; i++) {
		pd = c->physdisks[i];
		if (pd == NULL)
			continue;
		if (pd->spare && DRIVE_FAILED(pd->Status)) {
			if (comma++)
				*p++ = ',';
			p += sprintf(p, "drive %u at ", pd->PhysDiskNum);
			p += mpt_drive_location(p, pd);
		}
	}
	if ((p - str) == 1) {
		int n = sprintf(p, "none");
		p += n;
	}
	*p = ')';

	return (str);
}

static void
mpt_notify_failure(struct mpt_controller *c, struct mpt_volume *v)
{
	FILE *fp;
	int *sentcnt;
	char *bad, *failed, *missingc, *missingv, *rebuild;

	sentcnt = &v->sentcnt;
	if (v->config->VolumeStatus.State == v->prev_status.State &&
	    v->missing_drives == v->prev_missing_drives &&
	    c->bad_drives == c->prev_bad_drives &&
	    c->missing_drives == c->prev_missing_drives &&
	    ((*sentcnt)++ % notifyminutes) != 0)
		return;
	*sentcnt = 1;
	c->sentcnt = 1;

	bad = mpt_show_bad(c);
	missingc = mpt_show_missing_controller(c);
	rebuild = mpt_show_rebuild(c, v);
	failed = mpt_show_failed(c, v);
	missingv = mpt_show_missing_vol(c, v);

	fp = mailer_open();
	mailer_write(fp, "Subject: [MPT ALERT] controller %d vol %s on %s\n\n",
	    c->unit, mpt_volume_name(c->fd, c->unit, v->VolumeBus, v->VolumeID),
	    hostname);
	if (!VOLUME_DEGRADED(v->config->VolumeStatus)) {
		mailer_write(fp,
	    "%s: controller %d volume %s is rebuilt and no longer has errors\n",
		    hostname, c->unit, mpt_volume_name(c->fd, c->unit,
		    v->VolumeBus, v->VolumeID));
	} else {
		if (rebuild)
			mailer_write(fp,
		    "%s: recovering to %s on controller %d volume %s\n",
			    hostname, rebuild, c->unit, mpt_volume_name(c->fd,
			    c->unit, v->VolumeBus, v->VolumeID));
		if (failed)
			mailer_write(fp,
	    "%s: disk(s) on controller %d volume %s needs to be replaced: %s\n",
			    hostname, c->unit, mpt_volume_name(c->fd, c->unit,
			    v->VolumeBus, v->VolumeID), failed);
		if (missingv)
			mailer_write(fp,
		    "%s: disk(s) on controller %d volume %s are missing: %s\n",
			    hostname, c->unit, mpt_volume_name(c->fd, c->unit,
			    v->VolumeBus, v->VolumeID), missingv);
		else if (v->missing_drives)
			mailer_write(fp,
		    "%s: %d disk(s) on controller %d volume %s are missing\n",
			    hostname, v->missing_drives, c->unit,
			    mpt_volume_name(c->fd, c->unit, v->VolumeBus,
			    v->VolumeID));
	}
	if (bad)
		mailer_write(fp, "%s: disk(s) on controller %d are bad: %s\n",
		    hostname, c->unit, bad);
	else if (c->bad_drives)
		mailer_write(fp, "%s: %d disk(s) on controller %d are bad\n",
		    hostname, c->bad_drives, c->unit);
	else if (c->prev_bad_drives)
		mailer_write(fp,
		    "%s: controller %d no longer has any bad disks\n",
		    hostname, c->unit);
	if (missingc)
		mailer_write(fp,
		    "%s: disk(s) on controller %d are missing: %s\n",
		    hostname, c->unit, missingc);
	else if (c->missing_drives)
		mailer_write(fp,
		    "%s: %d disk(s) on controller %d are missing\n",
		    hostname, c->missing_drives, c->unit);
	else if (c->prev_missing_drives)
		mailer_write(fp,
		    "%s: controller %d no longer has any missing disks\n",
		    hostname, c->unit);

	if (bad)
		free(bad);
	if (missingc)
		free(missingc);
	if (failed)
		free(failed);
	if (rebuild)
		free(rebuild);
	if (missingv)
		free(missingv);

	mailer_close(fp);
}

static void
mpt_notify_controller(struct mpt_controller *c)
{
	FILE *fp;
	int *sentcnt;
	char *bad, *missing;

	sentcnt = &c->sentcnt;
	if (c->bad_drives == c->prev_bad_drives &&
	    c->missing_drives == c->prev_missing_drives &&
	    ((*sentcnt)++ % notifyminutes) != 0)
		return;
	*sentcnt = 1;

	bad = mpt_show_bad(c);
	missing = mpt_show_missing_controller(c);

	fp = mailer_open();
	mailer_write(fp, "Subject: [MPT ALERT] controller %d on %s\n\n",
	    c->unit, hostname);
	if (bad)
		mailer_write(fp, "%s: disk(s) on controller %d are bad: %s\n",
		    hostname, c->unit, bad);
	else if (c->bad_drives)
		mailer_write(fp, "%s: %d disk(s) on controller %d are bad\n",
		    hostname, c->bad_drives, c->unit);
	else if (c->prev_bad_drives)
		mailer_write(fp,
		    "%s: controller %d no longer has any bad disks\n",
		    hostname, c->unit);
	if (missing)
		mailer_write(fp, "%s: disk(s) on controller %d are missing: %s\n",
		    hostname, c->unit, missing);
	else if (c->missing_drives)
		mailer_write(fp, "%s: %d disk(s) on controller %d are missing\n",
		    hostname, c->missing_drives, c->unit);
	else if (c->prev_missing_drives)
		mailer_write(fp,
		    "%s: controller %d no longer has any missing disks\n",
		    hostname, c->unit);

	if (bad)
		free(bad);
	if (missing)
		free(missing);

	mailer_close(fp);
}

static void
mpt_check_volumes(void)
{
	struct mpt_volume *v;
	struct mpt_controller *c;
	int i, j, notified;

	for (i = 0; i < ncontrollers; i++) {
		c = &controllers[i];
		notified = 0;
		for (j = 0; j < MPT_MAX_VOL_ID; j++) {
			v = c->volumes[j];
			if (v == NULL || v->config == NULL)
				continue;

			if (VOLUME_DEGRADED(v->config->VolumeStatus) ||
			    VOLUME_DEGRADED(v->prev_status)) {
				mpt_notify_failure(c, v);
				notified = 1;
			}
		}
		if (!notified &&
		    (c->bad_drives != 0 || c->prev_bad_drives != 0 ||
		    c->missing_drives != 0 || c->prev_missing_drives != 0))
			mpt_notify_controller(c);
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: mptd [-ds] [-t minutes] [mailto]\n");
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

	mpt_open();

	if (ncontrollers == 0)
		return (0);

	for (;;) {
		mpt_scan_all();
		mpt_check_volumes();
		sleep(60);
	}
}
