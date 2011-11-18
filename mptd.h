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

#ifndef __MPTD_H__
#define	__MPTD_H__

#include <sys/cdefs.h>

#include <dev/mpt/mpilib/mpi_type.h>
#include <dev/mpt/mpilib/mpi.h>
#include <dev/mpt/mpilib/mpi_cnfg.h>
#include <dev/mpt/mpilib/mpi_raid.h>

#define	IOC_STATUS_SUCCESS(status)					\
	(((status) & MPI_IOCSTATUS_MASK) == MPI_IOCSTATUS_SUCCESS)

struct mpt_query_disk {
	char	devname[SPECNAMELEN + 1];
};

void	*mpt_read_config_page(int fd, U8 PageType, U8 PageNumber,
    U32 PageAddress);
int	mpt_write_config_page(int fd, void *buf);
const char *mpt_ioc_status(U16 IOCStatus);
#if 0
int	mpt_raid_action(int fd, U8 Action, U8 VolumeBus, U8 VolumeID,
    U8 PhysDiskNum, U32 ActionDataWord, void *buf, int len,
    RAID_VOL0_STATUS *VolumeStatus, U32 *ActionData, int datalen,
    U16 *IOCStatus, U16 *ActionStatus, int write);
#endif
int	mpt_query_disk(int fd, int unit, U8 VolumeBus, U8 VolumeID,
    struct mpt_query_disk *qd);
const char *mpt_volume_name(int fd, int unit, U8 VolumeBus, U8 VolumeID);

static __inline void *
mpt_read_ioc_page(int fd, U8 PageNumber)
{

	return (mpt_read_config_page(fd, MPI_CONFIG_PAGETYPE_IOC, PageNumber,
	    0));
}

static __inline U32
mpt_vol_pageaddr(U8 VolumeBus, U8 VolumeID)
{

	return (VolumeBus << 8 | VolumeID);
}

static __inline CONFIG_PAGE_RAID_VOL_0 *
mpt_vol_info(int fd, U8 VolumeBus, U8 VolumeID)
{

	return (mpt_read_config_page(fd, MPI_CONFIG_PAGETYPE_RAID_VOLUME, 0,
	    mpt_vol_pageaddr(VolumeBus, VolumeID)));
}

static __inline CONFIG_PAGE_RAID_PHYS_DISK_0 *
mpt_pd_info(int fd, U8 PhysDiskNum)
{

	return (mpt_read_config_page(fd, MPI_CONFIG_PAGETYPE_RAID_PHYSDISK, 0,
	    PhysDiskNum));
}

#endif /* !__MPTD_H__ */
