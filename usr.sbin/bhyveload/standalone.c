/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: user/syuu/bhyve_standalone_guest/usr.sbin/bhyveload/bhyveload.c 253922 2013-08-04 01:22:26Z syuu $
 */

/*-
 * Copyright (c) 2011 Google, Inc.
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
 * $FreeBSD: user/syuu/bhyve_standalone_guest/usr.sbin/bhyveload/bhyveload.c 253922 2013-08-04 01:22:26Z syuu $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: user/syuu/bhyve_standalone_guest/usr.sbin/bhyveload/bhyveload.c 253922 2013-08-04 01:22:26Z syuu $");

#include <sys/stat.h>
#include <sys/param.h>

#include <machine/specialreg.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "userboot.h"

#define MSR_EFER        0xc0000080
#define CR4_PAE         0x00000020
#define CR4_PSE         0x00000010
#define CR0_PG          0x80000000
#define	CR0_PE		0x00000001	/* Protected mode Enable */
#define	CR0_NE		0x00000020	/* Numeric Error enable (EX16 vs IRQ13) */

#define PG_V	0x001
#define PG_RW	0x002
#define PG_U	0x004
#define PG_PS	0x080

typedef u_int64_t p4_entry_t;
typedef u_int64_t p3_entry_t;
typedef u_int64_t p2_entry_t;

#define	GUEST_NULL_SEL		0
#define	GUEST_CODE_SEL		1
#define	GUEST_DATA_SEL		2
#define	GUEST_GDTR_LIMIT	(3 * 8 - 1)

int stand_load(struct loader_callbacks *cb, char *image, uint64_t addr);

static void
setup_stand_gdt(uint64_t *gdtr)
{
	gdtr[GUEST_NULL_SEL] = 0;
	gdtr[GUEST_CODE_SEL] = 0x0020980000000000;
	gdtr[GUEST_DATA_SEL] = 0x0000900000000000;
}

int
stand_load(struct loader_callbacks *cb, char *image, uint64_t addr)
{
	int i;
	int fd;
	struct stat sb;
	char *buf;
	uint32_t		stack[1024];
	p4_entry_t		PT4[512];
	p3_entry_t		PT3[512];
	p2_entry_t		PT2[512];
	uint64_t		gdtr[3];

	if ((fd = open(image, O_RDONLY)) < 0) {
		perror("open");
		return (1);
	}
	if (fstat(fd, &sb)) {
		perror("fstat");
		return (1);
	}
	buf = alloca(sb.st_size);
	if (read(fd, buf, sb.st_size) != sb.st_size) {
		perror("read");
		return (1);
	}
	if (close(fd) < 0) {
		perror("close");
		return (1);
	}
	if (cb->copyin(NULL, buf, addr, sb.st_size)) {
		perror("copyin");
		return (1);
	}

	bzero(PT4, PAGE_SIZE);
	bzero(PT3, PAGE_SIZE);
	bzero(PT2, PAGE_SIZE);

	/*
	 * Build a scratch stack at physical 0x1000, page tables:
	 *	PT4 at 0x2000,
	 *	PT3 at 0x3000,
	 *	PT2 at 0x4000,
	 *      gdtr at 0x5000,
	 */

	/*
	 * This is kinda brutal, but every single 1GB VM memory segment
	 * points to the same first 1GB of physical memory.  But it is
	 * more than adequate.
	 */
	for (i = 0; i < 512; i++) {
		/* Each slot of the level 4 pages points to the same level 3 page */
		PT4[i] = (p4_entry_t) 0x3000;
		PT4[i] |= PG_V | PG_RW | PG_U;

		/* Each slot of the level 3 pages points to the same level 2 page */
		PT3[i] = (p3_entry_t) 0x4000;
		PT3[i] |= PG_V | PG_RW | PG_U;

		/* The level 2 page slots are mapped with 2MB pages for 1GB. */
		PT2[i] = i * (2 * 1024 * 1024);
		PT2[i] |= PG_V | PG_RW | PG_PS | PG_U;
	}

#ifdef DEBUG
	printf("Start @ %#llx ...\n", addr);
#endif

	cb->copyin(NULL, stack, 0x1000, sizeof(stack));
	cb->copyin(NULL, PT4, 0x2000, sizeof(PT4));
	cb->copyin(NULL, PT3, 0x3000, sizeof(PT3));
	cb->copyin(NULL, PT2, 0x4000, sizeof(PT2));
	cb->setreg(NULL, 4, 0x1000);

	cb->setmsr(NULL, MSR_EFER, EFER_LMA | EFER_LME);
	cb->setcr(NULL, 4, CR4_PAE | CR4_VMXE);
	cb->setcr(NULL, 3, 0x2000);
	cb->setcr(NULL, 0, CR0_PG | CR0_PE | CR0_NE);

	setup_stand_gdt(gdtr);
	cb->copyin(NULL, gdtr, 0x5000, sizeof(gdtr));
        cb->setgdt(NULL, 0x5000, sizeof(gdtr));

	cb->exec(NULL, addr);
	return (0);
}

