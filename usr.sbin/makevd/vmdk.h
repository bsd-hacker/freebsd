/*-
 * Copyright (c) 2011
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
#ifndef _VMDK_H
#define _VMDK_H

#include <sys/endian.h>
#include <stdint.h>

typedef uint64_t SectorType;
typedef uint32_t uint32;
typedef uint16_t uint16;
typedef uint8_t uint8;
typedef uint8_t Bool;

/* All of the fields are in LE byte order. */
struct SparseExtentHeader {
	uint32	     magicNumber;
#define	SEH_MAGICNUMBER		(0x564d444b)	/* "VMDK" */
	uint32	     version;
#define	SEH_VERSION_DEFAULT	(2)
	uint32	     flags;
	SectorType   capacity;
	SectorType   grainSize;
	SectorType   descriptorOffset;
	SectorType   descriptorSize;
	uint32	     numGTEsPerGT;
	SectorType   rgdOffset;
	SectorType   gdOffset;
	SectorType   overHead;
	Bool	     uncleanShutdown;
	char	     singleEndLineChar;
	char	     nonEndLineChar;
	char	     doubleEndLineChar1;
	char	     doubleEndLineChar2;
	uint16	     compressAlgorithm;
	uint8	     pad[433];
} __attribute__((__packed__));

#define	VMDK_SEH_HOSTEDSPARSE_INIT	{			\
	.magicNumber = htole32(SEH_MAGICNUMBER),		\
	.version = htole32(SEH_VERSION_DEFAULT),		\
	.flags = htole32(1),					\
	.capacity = htole64(0),					\
	.grainSize = htole64(16),				\
	.numGTEsPerGT = htole32(512),				\
	.rgdOffset = htole64(0),				\
	.gdOffset = htole64(0),					\
	.uncleanShutdown = 0,					\
	.singleEndLineChar = '\n',				\
	.nonEndLineChar = ' ',					\
	.doubleEndLineChar1 = '\r',				\
	.doubleEndLineChar2 = '\n',				\
	 }

#endif	/* _VMDK_H */
