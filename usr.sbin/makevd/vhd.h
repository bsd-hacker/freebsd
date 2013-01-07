/*-
 * Copyright (c) 2011-2013
 *	Hiroki Sato <hrs@FreeBSD.org>  All rights reserved.
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
 * $FreeBSD$
 *
 */
#ifndef _VHD_H
#define _VHD_H

#include <sys/endian.h>
#include <stdint.h>

/* All of the fields are in BE byte order. */
struct HardDiskFooter {
	uint64_t	Cookie;
#define	HDF_COOKIE	(0x636f6e6563746978)	/* "conectix" */
	uint32_t	Features;
#define	HDF_FEATURES_TEMP	(0x00000001)
#define	HDF_FEATURES_RES	(0x00000002)
	uint32_t	FileFormatVersion;
#define	HDF_FILEFORMATVERSION_DEFAULT	(0x00010000)
	uint64_t       	DataOffset;
#define	HDF_DATAOFFSET_FIXEDHDD	(0xFFFFFFFF)
	uint32_t 	TimeStamp;
	uint32_t	CreatorApplication;
#define	HDF_CREATORAPP_VPC	(0x76707320)	/* "vpc " */
#define	HDF_CREATORAPP_VS	(0x76732020)	/* "vs  " */
	uint32_t	CreatorVersion;
#define	HDF_CREATORVERSION_VS2004	(0x00010000)
#define	HDF_CREATORVERSION_VPC2004	(0x00050000)
	uint32_t	CreatorHostOS;
#define	HDF_CREATORHOSTOS_WIN	(0x5769326b)	/* "Wi2k" */
#define	HDF_CREATORHOSTOS_MAC	(0x4d616320)	/* "Mac " */
	uint64_t       	OriginalSize;
	uint64_t       	CurrentSize;
	struct {
		uint16_t	cylinder;
		uint8_t		heads;
		uint8_t		sectcyl;
	} DiskGeometry;
	uint32_t	DiskType;
#define	HDF_DISKTYPE_FIXEDHDD	(2)
#define	HDF_DISKTYPE_DYNAMICHDD	(3)
#define	HDF_DISKTYPE_DIFFHDD	(4)
	uint32_t	Checksum;
	uint8_t		UniqueId[16];
	uint8_t		SavedState;
	char		Reserved[427];
} __attribute__((__packed__));

#define	VHD_HDF_FIXEDHDD_INIT	{					\
	.Cookie = htobe64(HDF_COOKIE),					\
	.Features = htobe32(HDF_FEATURES_RES),				\
	.FileFormatVersion = htobe32(HDF_FILEFORMATVERSION_DEFAULT),	\
	.DataOffset = htobe32(HDF_DATAOFFSET_FIXEDHDD),			\
	.CreatorApplication = htobe32(HDF_CREATORAPP_VPC),		\
	.CreatorVersion = htobe32(HDF_CREATORVERSION_VPC2004),		\
	.CreatorHostOS = htobe32(HDF_CREATORHOSTOS_WIN),		\
	.DiskType = htobe32(HDF_DISKTYPE_FIXEDHDD),			\
	.SavedState = 0,						\
	}

struct DynamicDiskHeader {
	uint64_t	Cookie;
#define DDH_COOKIE	(0x6378737061727365)	/* "cxsparse" */
	uint64_t	DataOffset;
#define	DDH_DATAOFFSET	(0xffffffff)
	uint64_t	TableOffset;
	uint32_t	HeaderVersion;
#define	DDH_HEADERVERSION	(0x00010000)
	uint32_t	MaxTableEntries;
	uint32_t	BlockSize;
#define	DDH_BLOCKSIZE_DEFAULT	(0x00200000)
	uint32_t	Checksum;
	uint8_t		ParentUniqueID[16];
	uint32_t	ParentTimeStamp;
	uint32_t	Reserved1;
	uint8_t		ParentUnicodeName[512];
	uint8_t		ParentLocatorEntry1[24];
	uint8_t		ParentLocatorEntry2[24];
	uint8_t		ParentLocatorEntry3[24];
	uint8_t		ParentLocatorEntry4[24];
	uint8_t		ParentLocatorEntry5[24];
	uint8_t		ParentLocatorEntry6[24];
	uint8_t		ParentLocatorEntry7[24];
	uint8_t		ParentLocatorEntry8[24];
	uint8_t		Reserved2[256];
} __attribute__((__packed__));

#endif	/* _VHD_H */
