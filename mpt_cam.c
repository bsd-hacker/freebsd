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

#include <sys/param.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <camlib.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_pass.h>

#include "mptd.h"

static int xptfd;

static int
xpt_open(void)
{

	if (xptfd == 0)
		xptfd = open(XPT_DEVICE, O_RDWR);
	return (xptfd);
}

int
mpt_query_disk(int fd, int unit, U8 VolumeBus, U8 VolumeID,
    struct mpt_query_disk *qd)
{
	struct bus_match_pattern *b;
	struct periph_match_pattern *p;
	struct periph_match_result *r;
	union ccb ccb;
	size_t bufsize;
	int i;

	/* mpt(4) only handles devices on bus 0. */
	if (VolumeBus != 0)
		return (ENXIO);

	if (xpt_open() < 0)
		return (ENXIO);

	bzero(&ccb, sizeof(ccb));

	ccb.ccb_h.func_code = XPT_DEV_MATCH;

	bufsize = sizeof(struct dev_match_result) * 5;
	ccb.cdm.num_matches = 0;
	ccb.cdm.match_buf_len = bufsize;
	ccb.cdm.matches = calloc(1, bufsize);

	bufsize = sizeof(struct dev_match_pattern) * 2;
	ccb.cdm.num_patterns = 2;
	ccb.cdm.pattern_buf_len = bufsize;
	ccb.cdm.patterns = calloc(1, bufsize);

	/* Match mptX bus. */
	ccb.cdm.patterns[0].type = DEV_MATCH_BUS;
	b = &ccb.cdm.patterns[0].pattern.bus_pattern;
	snprintf(b->dev_name, sizeof(b->dev_name), "mpt");
	b->unit_number = unit;
	b->flags = BUS_MATCH_NAME | BUS_MATCH_UNIT;

	/* Look for a "da" device at the specified target and lun. */
	ccb.cdm.patterns[1].type = DEV_MATCH_PERIPH;
	p = &ccb.cdm.patterns[1].pattern.periph_pattern;
	snprintf(p->periph_name, sizeof(p->periph_name), "da");
	p->target_id = VolumeID;
	p->flags = PERIPH_MATCH_NAME | PERIPH_MATCH_TARGET;

	if (ioctl(xptfd, CAMIOCOMMAND, &ccb) < 0) {
		i = errno;
		free(ccb.cdm.matches);
		free(ccb.cdm.patterns);
		return (i);
	}
	free(ccb.cdm.patterns);

	if (((ccb.ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) ||
	    (ccb.cdm.status != CAM_DEV_MATCH_LAST)) {
		warnx("mpt_query_disk got CAM error %#x, CDM error %d\n",
		    ccb.ccb_h.status, ccb.cdm.status);
		free(ccb.cdm.matches);
		return (EIO);
	}

	/*
	 * We should have exactly 2 matches, 1 for the bus and 1 for
	 * the peripheral.
	 */
	if (ccb.cdm.num_matches != 2) {
		warnx("mpt_query_disk got %d matches, expected 2",
		    ccb.cdm.num_matches);
		free(ccb.cdm.matches);
		return (EIO);
	}
	if (ccb.cdm.matches[0].type != DEV_MATCH_BUS ||
	    ccb.cdm.matches[1].type != DEV_MATCH_PERIPH) {
		warnx("mpt_query_disk got wrong CAM matches");
		free(ccb.cdm.matches);
		return (EIO);
	}

	/* Copy out the data. */
	r = &ccb.cdm.matches[1].result.periph_result;
	snprintf(qd->devname, sizeof(qd->devname), "%s%d", r->periph_name,
	    r->unit_number);
	free(ccb.cdm.matches);

	return (0);
}
