/*-
 * Copyright (c) 2011,2012,2013
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
 */
#ifndef _VDI_H
#define _VDI_H

#include <sys/endian.h>
#include <stdint.h>

#define VDI_IMAGE_FILE_INFO0	"<<< Sun VM VirtualBox Disk Image >>>\n"
#define VDI_IMAGE_FILE_INFO	"<<< Oracle VM VirtualBox Disk Image >>>\n"
#define	VDI_IMAGE_SIGNATURE	(0xbeda107f)
#define	VDI_IMAGE_BLOCK_SIZE	(1024*1024)

#define VDI_IMAGE_VERSION_MAJOR	(0x0001)
#define VDI_IMAGE_VERSION_MINOR	(0x0001)
#define VDI_IMAGE_VERSION	((VDI_IMAGE_VERSION_MAJOR << 16) | VDI_IMAGE_VERSION_MINOR)

struct VDIPREHEADER {
	char		szFileInfo[64];	/* comment field */
	uint32_t	u32Signature;	/* VDI_IMAGE_SIGNATURE */
	uint32_t	u32Version;	/* VDI_IMAGE_VERSION */
} __attribute__((__packed__));

#define VDI_IMAGE_COMMENT_SIZE	256

struct VDIDISKGEOMETRY {
	uint32_t	cCylinders;	/* Cylinders */
	uint32_t	cHeads;		/* Heads */
	uint32_t	cSectors;	/* Sectors per track */
	uint32_t	cbSector;	/* Sector size in bytes */
} __attribute__((__packed__));

struct VDIHEADER0 {
	uint32_t	u32Type;	/* VDI_IMAGE_TYPE_* */
	uint32_t	fFlags;		/* VDI_IMAGE_FLAGS_* */
	char		szComment[VDI_IMAGE_COMMENT_SIZE];
	struct VDIDISKGEOMETRY	LegacyGeometry;
	uint64_t	cbDisk;		/* Disk size in bytes */
	uint32_t	cbBlock;	/* VDI_IMAGE_BLOCK_SIZE */
	uint32_t	cBlocks;	/* Number of blocks */
	uint32_t	cBlocksAllocated;	/* Number of allocated blocks */
	uint8_t		uuidCreate[16];
	uint8_t		uuidModify[16];
	uint8_t		uuidLinkage[16];
} __attribute__((__packed__));

struct VDIHEADER1 {
	uint32_t	cbHeader;	/* size of this structure */
	uint32_t	u32Type;	/* VDI_IMAGE_TYPE_* */
	uint32_t	fFlags;		/* VDI_IMAGE_FLAGS_* */
	char		szComment[VDI_IMAGE_COMMENT_SIZE];
	uint32_t	offBlocks;	/* Offset of Blocks array from the
					 * beginning of the image file.
					 * Sector-aligned.
					 */
	uint32_t	offData;	/* Offset of image data from the
					 * beginning of the image file.
					 * Sector-aligned.
					 */
	struct VDIDISKGEOMETRY	LegacyGeometry;
	uint32_t	u32Dummy;
	uint64_t	cbDisk;		/* Disk size in bytes */
	uint32_t	cbBlock;	/* VDI_IMAGE_BLOCK_SIZE */
	uint32_t	cBlocks;	/* Number of blocks */
	uint32_t	cBlocksAllocated;	/* Number of allocated blocks */
	uint8_t		uuidCreate[16];
	uint8_t		uuidModify[16];
	uint8_t		uuidLinkage[16];
	uint8_t		uuidParentModify[16];
} __attribute__((__packed__));

struct VDIHEADER1PLUS {
	uint32_t	cbHeader;	/* size of this structure */
	uint32_t	u32Type;	/* VDI_IMAGE_TYPE_* */
	uint32_t	fFlags;		/* VDI_IMAGE_FLAGS_* */
	char		szComment[VDI_IMAGE_COMMENT_SIZE];
	uint32_t	offBlocks;	/* Offset of Blocks array from the
					 * beginning of the image file.
					 * Sector-aligned.
					 */
	uint32_t	offData;	/* Offset of image data from the
					 * beginning of the image file.
					 * Sector-aligned.
					 */
	struct VDIDISKGEOMETRY	LegacyGeometry;
	uint32_t	u32Dummy;
	uint64_t	cbDisk;		/* Disk size in bytes */
	uint32_t	cbBlock;	/* VDI_IMAGE_BLOCK_SIZE */
	uint32_t	cbBlockExtra;	/* Prepended information before every
					 * data block.  A power of 2.
					 * May be 0.  Sector-aligned.
					 */
	uint32_t	cBlocks;	/* Number of blocks */
	uint32_t	cBlocksAllocated;	/* Number of allocated blocks */
	uint8_t		uuidCreate[16];
	uint8_t		uuidModify[16];
	uint8_t		uuidLinkage[16];
	uint8_t		uuidParentModify[16];
	struct VDIDISKGEOMETRY	LCHSGeometry;
} __attribute__((__packed__));

struct VDIHEADER {
	unsigned	uVersion;
	union {
		struct VDIHEADER0	v0;
		struct VDIHEADER1	v1;
		struct VDIHEADER1PLUS	v1plus;
	} u;
};

enum VDIIMAGETYPE {
	VDI_IMAGE_TYPE_NORMAL = 1,	/* Dynamically growing base image */
	VDI_IMAGE_TYPE_FIXED,		/* Preallocated fixed size base image */
	VDI_IMAGE_TYPE_UNDO,		/* NORMAL with undo/commit support */
	VDI_IMAGE_TYPE_DIFF,		/* NORMAL with differencing support */
	VDI_IMAGE_FIRST = VDI_IMAGE_TYPE_NORMAL,
	VDI_IMAGE_LAST = VDI_IMAGE_TYPE_DIFF,
};

#define VDI_VH1_PREHEADER_INIT {				\
	.szFileInfo = VDI_IMAGE_FILE_INFO,			\
	.u32Signature =  htole32(VDI_IMAGE_SIGNATURE),		\
	.u32Version = htole32(VDI_IMAGE_VERSION),		\
	}

#define	VDI_VH1_NORMAL_INIT	{				\
	.u32Type = htole32(VDI_IMAGE_TYPE_NORMAL),		\
	.fFlags = 0,						\
	.szComment = "",					\
	.u32Dummy = 0,						\
	.cbBlock = VDI_IMAGE_BLOCK_SIZE,			\
	}

#endif	/* _VDI_H */
