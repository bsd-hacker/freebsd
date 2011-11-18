/*-
 * Copyright (c) 2011 Yahoo! Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Yahoo! Inc. nor the name of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
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

#include <dev/mfi/mfireg.h>
#include <dev/mfi/mfi_ioctl.h>

#define	MAX_UNIT	16

#define VOLUME_DEGRADED(state)						\
	((state) == MFI_LD_STATE_PARTIALLY_DEGRADED ||			\
	 (state) == MFI_LD_STATE_DEGRADED)

static char hostname[MAXHOSTNAMELEN];
static char *mailto = "root@localhost";
static int notifyminutes = 720;         /* send mail every 12 hours by default */
static int dostdout;

/* Maximum target_id and device_id of volumes and drives, respectively. */
#define	MFI_MAX_LD_ID			256
#define	MFI_MAX_PD_ID			65536

struct mfi_physical_drive {
	uint32_t			generation;
	uint16_t			device_id;
	enum mfi_pd_state		state;
	uint16_t			encl_device_id;
	uint8_t				encl_index;
	uint8_t				slot_number;
	uint8_t				missing;
};

struct mfi_logical_drive {
	enum mfi_ld_state		state;
	enum mfi_ld_state		prev_state;
	int				sentcnt;
	uint32_t			generation;
	uint8_t				target_id;
	int				missing_drives;
};

struct mfi_controller {
	int				fd;
	int				unit;
	uint32_t			generation;
	uint32_t			config_size;
	int				config_valid;
	int				missing_drives;
	int				bad_drives;
	int				prev_bad_drives;
	int				sentcnt;
	struct mfi_config_data		*config;
	struct mfi_logical_drive	*ldrives[MFI_MAX_LD_ID];
	struct mfi_physical_drive	*pdrives[MFI_MAX_PD_ID];
};

static struct mfi_array *mfi_find_array(struct mfi_controller *c,
    uint16_t array_ref);

static struct mfi_controller controllers[MAX_UNIT];
static int ncontrollers;

static int
mfi_query_disk(struct mfi_controller *c, uint8_t target_id,
    struct mfi_query_disk *info)
{

	info->array_id = target_id;
	if (ioctl(c->fd, MFIIO_QUERY_DISK, info) < 0)
		return (-1);
	if (!info->present) {
		errno = ENXIO;
		return (-1);
	}
	return (0);
}

static const char *
mfi_volume_name(struct mfi_controller *c, uint8_t target_id)
{
	static struct mfi_query_disk info;
	static char buf[4];

	if (mfi_query_disk(c, target_id, &info) < 0) {
		snprintf(buf, sizeof(buf), "%d", target_id);
		return (buf);
	}
	return (info.devname);
}

static int
mfi_dcmd_command(struct mfi_controller *c, uint32_t opcode, void *buf,
    size_t bufsize, uint8_t *mbox, size_t mboxlen, uint8_t *statusp)
{
	struct mfi_ioc_passthru ioc;
	struct mfi_dcmd_frame *dcmd;
	int r;

	if ((mbox != NULL && (mboxlen == 0 || mboxlen > MFI_MBOX_SIZE)) ||
	    (mbox == NULL && mboxlen != 0)) {
		errno = EINVAL;
		return (-1);
	}

	bzero(&ioc, sizeof(ioc));
	dcmd = &ioc.ioc_frame;
	if (mbox)
		bcopy(mbox, dcmd->mbox, mboxlen);
	dcmd->header.cmd = MFI_CMD_DCMD;
	dcmd->header.timeout = 0;
	dcmd->header.flags = 0;
	dcmd->header.data_len = bufsize;
	dcmd->opcode = opcode;

	ioc.buf = buf;
	ioc.buf_size = bufsize;
	r = ioctl(c->fd, MFIIO_PASSTHRU, &ioc);
	if (r < 0)
		return (r);

	if (statusp != NULL)
		*statusp = dcmd->header.cmd_status;
	else if (dcmd->header.cmd_status != MFI_STAT_OK) {
		warnx("mfi%d: command %x returned error status %x", c->unit,
		    opcode, dcmd->header.cmd_status);
		errno = EIO;
		return (-1);
	}
	return (0);
}

static void
mbox_store_device_id(uint8_t *mbox, uint16_t device_id)
{

	mbox[0] = device_id & 0xff;
	mbox[1] = device_id >> 8;
}

static int
mfi_pd_get_list(struct mfi_controller *c, struct mfi_pd_list **listp,
    uint8_t *statusp)
{
	struct mfi_pd_list *list;
	uint32_t list_size;

	/*
	 * Keep fetching the list in a loop until we have a large enough
	 * buffer to hold the entire list.
	 */
	list = NULL;
	list_size = 1024;
fetch:
	list = reallocf(list, list_size);
	if (list == NULL)
		return (-1);
	if (mfi_dcmd_command(c, MFI_DCMD_PD_GET_LIST, list, list_size, NULL, 0,
	    statusp) < 0) {
		free(list);
		return (-1);
	}

	if (list->size > list_size) {
		list_size = list->size;
		goto fetch;
	}

	*listp = list;
	return (0);
}

static int
mfi_pd_get_info(struct mfi_controller *c, uint16_t device_id,
    struct mfi_pd_info *info, uint8_t *statusp)
{
	uint8_t mbox[2];

	mbox_store_device_id(&mbox[0], device_id);
	return (mfi_dcmd_command(c, MFI_DCMD_PD_GET_INFO, info,
	    sizeof(struct mfi_pd_info), mbox, 2, statusp));
}

int
mfi_drive_location(char *p, struct mfi_physical_drive *pd)
{

	if (pd->encl_device_id == 0xffff)
		return (sprintf(p, "slot %d", pd->slot_number));
	else if (pd->encl_device_id == pd->device_id)
		return (sprintf(p, "enclosure %d", pd->encl_index));
	else
		return (sprintf(p, "enclosure %d, slot %d", pd->encl_index,
		    pd->slot_number));
}

static void
mfi_scan_volume(struct mfi_controller *c, struct mfi_ld_config *ldc)
{
	struct mfi_logical_drive *ld;
	struct mfi_array *ar;
	uint8_t state;
	int i, span;

	state = ldc->params.state;

	/* See if we have seen this drive before. */
	ld = c->ldrives[ldc->properties.ld.v.target_id];
	if (ld == NULL) {
		ld = calloc(1, sizeof(struct mfi_logical_drive));
		ld->target_id = ldc->properties.ld.v.target_id;
		c->ldrives[ld->target_id] = ld;

		ld->prev_state = state;
	} else
		ld->prev_state = ld->state;

	/* Update generation count and other state. */
	ld->generation = c->generation;
	ld->state = state;

	/*
	 * Scan all the arrays this volume spans to see if this volume
	 * is missing any drives.
	 */
	ld->missing_drives = 0;
	for (span = 0; span < ldc->params.span_depth; span++) {
		ar = mfi_find_array(c, ldc->span[span].array_ref);

		/* Walk the array to find the backing drives. */
		for (i = 0; i < ar->num_drives; i++)
			/* Missing drive. */
			if (ar->pd[i].ref.v.device_id == 0xffff)
				ld->missing_drives++;
	}
}

static void
mfi_scan_volumes(struct mfi_controller *c)
{
	struct mfi_logical_drive *ld;
	char *p;
	int i;

	/* Find the first config. */
	p = (char *)c->config->array +
	    c->config->array_count * c->config->array_size;

	/* Scan all the volumes. */
	for (i = 0; i < c->config->log_drv_count; i++) {
		mfi_scan_volume(c, (struct mfi_ld_config *)p);
		p += c->config->log_drv_size;
	}

	/* Throw away all the volumes that disappeared. */
	for (i = 0; i < MFI_MAX_LD_ID; i++) {
		ld = c->ldrives[i];
		if (ld == NULL)
			continue;
		if (ld->generation != c->generation) {
			c->ldrives[i] = NULL;
			free(ld);
		}
	}
}

static void
mfi_scan_drive(struct mfi_controller *c, uint16_t device_id, uint16_t state)
{
	struct mfi_physical_drive *pd;
	struct mfi_pd_info info;

	/* See if we have seen this drive before. */
	pd = c->pdrives[device_id];
	if (pd == NULL) {
		pd = calloc(1, sizeof(struct mfi_physical_drive));
		pd->device_id = device_id;
		c->pdrives[device_id] = pd;

		if (mfi_pd_get_info(c, device_id, &info, NULL) < 0)
			warn("mfi%d: Failed to get info for drive %u", c->unit,
			    device_id);
		else {
			pd->encl_device_id = info.encl_device_id;
			pd->encl_index = info.encl_index;
			pd->slot_number = info.slot_number;
		}
	}

	/* Update generation count and other state. */
	pd->generation = c->generation;
	pd->state = state;
	pd->missing = 0;
}

static void
mfi_scan_drives(struct mfi_controller *c)
{
	struct mfi_physical_drive *pd;
	struct mfi_pd_list *list;
	struct mfi_pd_info info;
	struct mfi_array *ar;
	char *p;
	int i, j, count;

	/* Find the first array. */
	p = (char *)c->config->array;

	/* Scan all the arrays. */
	c->missing_drives = 0;
	for (i = 0; i < c->config->array_count; i++) {
		ar = (struct mfi_array *)p;

		/* Scan each drive in the array. */
		for (j = 0; j < ar->num_drives; j++) {
			/* Missing drive. */
			if (ar->pd[j].ref.v.device_id == 0xffff) {
				c->missing_drives++;
				continue;
			}
			mfi_scan_drive(c, ar->pd[j].ref.v.device_id,
			    ar->pd[j].fw_state);
		}
		p += c->config->array_size;
	}

	/* Scan all of the physical drives to find bad drives. */
	c->prev_bad_drives = c->bad_drives;
	c->bad_drives = 0;
	if (mfi_pd_get_list(c, &list, NULL) < 0)
		warn("mfi%d: Failed to get physical drive list", c->unit);
	else {
		for (i = 0; i < list->count; i++) {
			if (list->addr[i].scsi_dev_type != 0)
				continue;

			/* Skip drives we've already scanned above. */
			pd = c->pdrives[list->addr[i].device_id];
			if (pd != NULL && pd->generation == c->generation)
				continue;

			if (mfi_pd_get_info(c, list->addr[i].device_id, &info,
			    NULL) < 0) {
				warn("mfi%d: Failed to get info for drive %u",
				    c->unit, list->addr[i].device_id);
				continue;
			}
			if (info.fw_state == MFI_PD_STATE_UNCONFIGURED_BAD) {
				mfi_scan_drive(c, list->addr[i].device_id,
				    info.fw_state);
				c->bad_drives++;
			}
		}
		free(list);
	}

	/*
	 * If we have any missing drives, check to see if all of the drives
	 * that disappeared are missing drives.
	 */
	if (c->missing_drives) {
		count = 0;
		for (i = 0; i < MFI_MAX_PD_ID; i++) {
			pd = c->pdrives[i];
			if (pd == NULL)
				continue;
			if (pd->generation != c->generation)
				count++;
		}

		if (count <= c->missing_drives) {
			/*
			 * Ok, it looks like all of the drives that
			 * disappeared are known to be missing.
			 */
			for (i = 0; i < MFI_MAX_PD_ID; i++) {
				pd = c->pdrives[i];
				if (pd == NULL)
					continue;
				if (pd->generation != c->generation) {
					pd->missing = 1;
					pd->generation = c->generation;
				}
			}
		}
	}

	/* Throw away all the drives that disappeared. */
	for (i = 0; i < MFI_MAX_PD_ID; i++) {
		pd = c->pdrives[i];
		if (pd == NULL)
			continue;
		if (pd->generation != c->generation) {
			c->pdrives[i] = NULL;
			free(pd);
		}
	}
}

static void
mfi_scan_controller(struct mfi_controller *c)
{
	uint8_t status;
	int count;

	c->config_valid = 0;

	/* Start off with just the header. */
	if (c->config == NULL) {
		c->config_size = sizeof(struct mfi_config_data);
		c->config = malloc(sizeof(struct mfi_config_data));
	};

fetch:
	/* Try to fetch the RAID configuration for this controller. */
	for (count = 0; count < 5; count++) {
		if (mfi_dcmd_command(c, MFI_DCMD_CFG_READ, c->config,
		    c->config_size, NULL, 0, &status) < 0) {
			warn("mfi%d: Failed to get config", c->unit);
			return;
		}
		if (status != MFI_STAT_MEMORY_NOT_AVAILABLE)
			break;
		sleep(5);
	}
	if (status != MFI_STAT_OK) {
		warnx("mfi%d: Failed to get config with error status %x",
		    c->unit, status);
		return;
	}

	/* Is the size too small? */
	if (c->config_size < c->config->size) {
		c->config_size = c->config->size;
		c->config = realloc(c->config, c->config_size);
		if (c->config == NULL) {
			warn("mfi%d: Failed to grow config object", c->unit);
			return;
		}
		goto fetch;
	}

	/*
	 * Ok, now we have a config.  The config contains 3 arrays for us to
	 * process.  The first array contains MFI_ARRAY objects which define
	 * RAID arrays of physical drives.  The second array contains
	 * MFI_LD_CONFIG objects which define logical drives, or volumes,
	 * that are created by taking spans from backing MFI_ARRAYs.  Finally,
	 * the third array consists of MFI_SPARE objects describing spare
	 * disks.  We ignore the spares.  We care about the states of the
	 * volumes (degraded or not) and the state of the drives backing each
	 * of the volumes.  The MFI_LD_CONFIG objects already contain the
	 * state of each volume, but we need to query each of the physical
	 * drives to determine their state.
	 */

	/* Bump the overall generation count. */
	c->generation++;
	c->config_valid = 1;

	mfi_scan_volumes(c);
	mfi_scan_drives(c);
}

static void
mfi_scan_all(void)
{
	int i;

	for (i = 0; i < ncontrollers; i++)
		mfi_scan_controller(&controllers[i]);
}

static int
mfi_open(void)
{
	char path[MAXPATHLEN];
	int fd, unit;

	ncontrollers = 0;
	for (unit = 0; unit < MAX_UNIT; unit++) {
		snprintf(path, sizeof(path), "/dev/mfi%d", unit);
		fd = open(path, O_RDWR);
		if (fd < 0)
			continue;
		controllers[ncontrollers].fd = fd;
		controllers[ncontrollers].unit = unit;
		ncontrollers++;
	}
	if (ncontrollers == 0)
		return (ncontrollers);

	mfi_scan_all();

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

static struct mfi_ld_config *
mfi_find_ld_config(struct mfi_controller *c, uint8_t target_id)
{
	struct mfi_ld_config *ld;
	char *p;
	int i;

	p = (char *)&c->config[1] +
	    c->config->array_count * c->config->array_size;
	for (i = 0; i < c->config->log_drv_count; i++) {
		ld = (struct mfi_ld_config *)p;
		if (ld->properties.ld.v.target_id == target_id)
			return (ld);
		p += c->config->log_drv_size;
	}
	return (NULL);
}

static struct mfi_array *
mfi_find_array(struct mfi_controller *c, uint16_t array_ref)
{
	struct mfi_array *ar;
	char *p;
	int i;

	p = (char *)&c->config[1];
	for (i = 0; i < c->config->array_count; i++) {
		ar = (struct mfi_array *)p;
		if (ar->array_ref == array_ref)
			return (ar);
		p += c->config->array_size;
	}
	return (NULL);
}

static int
mfi_in_state(uint16_t state, struct mfi_controller *c,
    struct mfi_logical_drive *ld)
{
	struct mfi_physical_drive *pd;
	struct mfi_ld_config *ldc;
	struct mfi_array *ar;
	int i, instate, span;

	instate = 0;

	/* Find the config for this volume. */
	ldc = mfi_find_ld_config(c, ld->target_id);

	/* Walk each span for this volume. */
	for (span = 0; span < ldc->params.span_depth; span++) {
		ar = mfi_find_array(c, ldc->span[span].array_ref);

		/* Walk the array to find the backing drives. */
		for (i = 0; i < ar->num_drives; i++) {
			pd = c->pdrives[ar->pd[i].ref.v.device_id];
			if (pd == NULL)
				continue;
			if (pd->state == state) {
				instate++;
			}
		}
	}
	return (instate);
}

char *
mfi_show_state(uint16_t state, struct mfi_controller *c,
    struct mfi_logical_drive *ld)
{
	struct mfi_physical_drive *pd;
	struct mfi_ld_config *ldc;
	struct mfi_array *ar;
	int i, comma = 0, instate, span;
	char *str, *p;

	instate = mfi_in_state(state, c, ld);
	if (instate == 0)
		return (NULL);

	str = calloc(instate * 64, sizeof(char));
	if (str == NULL)
		return (NULL);

	p = str;
	*p++ = '(';

	/* Find the config for this volume. */
	ldc = mfi_find_ld_config(c, ld->target_id);

	/* Walk each span for this volume. */
	for (span = 0; span < ldc->params.span_depth; span++) {
		ar = mfi_find_array(c, ldc->span[span].array_ref);

		/* Walk the array to find the backing drives. */
		for (i = 0; i < ar->num_drives; i++) {
			pd = c->pdrives[ar->pd[i].ref.v.device_id];
			if (pd == NULL)
				continue;
			if (pd->state == state) {
				if (comma++)
					*p++ = ',';
				p += sprintf(p, "drive %u in ", pd->device_id);
				p += mfi_drive_location(p, pd);
			}
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
mfi_show_missing(struct mfi_controller *c, struct mfi_logical_drive *ld)
{
	struct mfi_physical_drive *pd;
	char *str, *p;
	int i, comma = 0, instate;

	if (c->missing_drives == 0 || ld->missing_drives == 0)
		return (NULL);

	instate = 0;
	for (i = 0; i < MFI_MAX_PD_ID; i++) {
		pd = c->pdrives[i];
		if (pd == NULL)
			continue;
		if (pd->missing)
			instate++;
	}
	if (instate == 0)
		return (NULL);

	str = calloc(instate * 64, sizeof(char));
	if (str == NULL)
		return (NULL);

	p = str;
	*p++ = '(';
	for (i = 0; i < MFI_MAX_PD_ID; i++) {
		pd = c->pdrives[i];
		if (pd == NULL)
			continue;
		if (pd->missing) {
			if (comma++)
				*p++ = ',';
			p += sprintf(p, "drive %u in ", pd->device_id);
			p += mfi_drive_location(p, pd);
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
mfi_show_bad(struct mfi_controller *c)
{
	struct mfi_physical_drive *pd;
	char *str, *p;
	int i, comma = 0, instate;

	if (c->bad_drives == 0)
		return (NULL);

	instate = 0;
	for (i = 0; i < MFI_MAX_PD_ID; i++) {
		pd = c->pdrives[i];
		if (pd == NULL)
			continue;
		if (pd->state == MFI_PD_STATE_UNCONFIGURED_BAD)
			instate++;
	}
	if (instate == 0)
		return (NULL);

	str = calloc(instate * 64, sizeof(char));
	if (str == NULL)
		return (NULL);

	p = str;
	*p++ = '(';
	for (i = 0; i < MFI_MAX_PD_ID; i++) {
		pd = c->pdrives[i];
		if (pd == NULL)
			continue;
		if (pd->state == MFI_PD_STATE_UNCONFIGURED_BAD) {
			if (comma++)
				*p++ = ',';
			p += sprintf(p, "drive %u in ", pd->device_id);
			p += mfi_drive_location(p, pd);
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
mfi_notify_failure(struct mfi_controller *c, struct mfi_logical_drive *ld)
{
	FILE *fp;
	int *sentcnt;
	char *bad, *failed, *missing, *rebuild;

	sentcnt = &ld->sentcnt;
	if (ld->state == ld->prev_state &&
	    c->bad_drives == c->prev_bad_drives &&
	    ((*sentcnt)++ % notifyminutes) != 0)
		return;
	*sentcnt = 1;
	c->sentcnt = 1;

	bad = mfi_show_bad(c);
	rebuild = mfi_show_state(MFI_PD_STATE_REBUILD, c, ld);
	failed = mfi_show_state(MFI_PD_STATE_FAILED, c, ld);
	missing = mfi_show_missing(c, ld);

	fp = mailer_open();
	mailer_write(fp, "Subject: [MFI ALERT] controller %d vol %s on %s\n\n",
	    c->unit, mfi_volume_name(c, ld->target_id), hostname);
	if (!VOLUME_DEGRADED(ld->state)) {
		mailer_write(fp,
	    "%s: controller %d volume %s is rebuilt and no longer has errors\n",
		    hostname, c->unit, mfi_volume_name(c, ld->target_id));
	} else {
		if (rebuild)
			mailer_write(fp,
		    "%s: recovering to %s on controller %d volume %s\n",
			    hostname, rebuild, c->unit,
			    mfi_volume_name(c, ld->target_id));
		if (failed)
			mailer_write(fp,
	    "%s: disk(s) on controller %d volume %s needs to be replaced: %s\n",
			    hostname, c->unit,
			    mfi_volume_name(c, ld->target_id), failed);
		if (missing)
			mailer_write(fp,
		    "%s: disk(s) on controller %d volume %s are missing: %s\n",
			    hostname, c->unit,
			    mfi_volume_name(c, ld->target_id), missing);
		else if (ld->missing_drives)
			mailer_write(fp,
		    "%s: %d disk(s) on controller %d volume %s are missing\n",
			    hostname, ld->missing_drives, c->unit,
			    mfi_volume_name(c, ld->target_id));
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

	if (bad)
		free(bad);
	if (failed)
		free(failed);
	if (rebuild)
		free(rebuild);
	if (missing)
		free(missing);

	mailer_close(fp);
}

static void
mfi_notify_bad(struct mfi_controller *c)
{
	FILE *fp;
	int *sentcnt;
	char *bad;

	sentcnt = &c->sentcnt;
	if (c->bad_drives == c->prev_bad_drives &&
	    ((*sentcnt)++ % notifyminutes) != 0)
		return;
	*sentcnt = 1;

	bad = mfi_show_bad(c);

	fp = mailer_open();
	mailer_write(fp, "Subject: [MFI ALERT] controller %d on %s\n\n",
	    c->unit, hostname);
	if (bad)
		mailer_write(fp, "%s: disk(s) on controller %d are bad: %s\n",
		    hostname, c->unit, bad);
	else if (c->bad_drives)
		mailer_write(fp, "%s: %d disk(s) on controller %d are bad\n",
		    hostname, c->bad_drives, c->unit);
	else
		mailer_write(fp,
		    "%s: controller %d no longer has any bad disks\n",
		    hostname, c->unit);

	if (bad)
		free(bad);

	mailer_close(fp);
}

static void
mfi_check_volumes(void)
{
	struct mfi_logical_drive *ld;
	struct mfi_controller *c;
	int i, j, notified;

	for (i = 0; i < ncontrollers; i++) {
		c = &controllers[i];
		if (!c->config_valid)
			continue;
		notified = 0;
		for (j = 0; j < MFI_MAX_LD_ID; j++) {
			ld = c->ldrives[j];
			if (ld == NULL)
				continue;

			if (VOLUME_DEGRADED(ld->state) ||
			    VOLUME_DEGRADED(ld->prev_state)) {
				mfi_notify_failure(c, ld);
				notified = 1;
			}
		}
		if (!notified &&
		    (c->bad_drives != 0 || c->prev_bad_drives != 0))
			mfi_notify_bad(c);
	}
}

static void
mfi_stop_patrol(void)
{
	struct mfi_controller *c;
	struct mfi_pr_status status;
	int i;

	for (i = 0; i < ncontrollers; i++) {
		c = &controllers[i];
		if (c->config_valid == 0)
			continue;
		if (mfi_dcmd_command(c, MFI_DCMD_PR_GET_STATUS, &status,
		    sizeof(status), NULL, 0, NULL) < 0)
			continue;
		if (status.state == MFI_PR_STATE_STOPPED)
			continue;
		if (mfi_dcmd_command(c, MFI_DCMD_PR_STOP, NULL, 0,
		    NULL, 0, NULL) < 0)
			warn("Failed to stop patrol reads");
	}
}

static void
mfi_disable_patrol(void)
{
	struct mfi_controller *c;
	struct mfi_pr_properties prop;
	int i;

	for (i = 0; i < ncontrollers; i++) {
		c = &controllers[i];
		if (c->config_valid == 0)
			continue;
		if (mfi_dcmd_command(c, MFI_DCMD_PR_GET_PROPERTIES, &prop,
		    sizeof(prop), NULL, 0, NULL) < 0)
			continue;
		if (prop.op_mode == MFI_PR_OPMODE_DISABLED)
			continue;
		prop.op_mode = MFI_PR_OPMODE_DISABLED;
		if (mfi_dcmd_command(c, MFI_DCMD_PR_SET_PROPERTIES, &prop,
		    sizeof(prop), NULL, 0, NULL) < 0)
			warn("Failed to disable patrol reads");
	}
}
		
static void
usage(void)
{
	fprintf(stderr, "usage: mfid [-ds] [-t minutes] [mailto]\n");
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

	mfi_open();

	if (ncontrollers == 0)
		return (0);

	mfi_stop_patrol();
	mfi_disable_patrol();

	for (;;) {
		mfi_scan_all();
		mfi_check_volumes();
		sleep(60);
	}
}
