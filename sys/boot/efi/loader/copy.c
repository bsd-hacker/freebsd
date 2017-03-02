/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Benno Rice under sponsorship from
 * the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <stand.h>
#include <bootstrap.h>

#include <efi.h>
#include <efilib.h>

#include "loader_efi.h"

#if defined(__i386__) || defined(__amd64__)

#define KERNEL_PHYSICAL_BASE (2*1024*1024)

static void
efi_verify_staging_size(unsigned long *nr_pages)
{
	UINTN sz;
	EFI_MEMORY_DESCRIPTOR *map, *p;
	EFI_PHYSICAL_ADDRESS start, end;
	UINTN key, dsz;
	UINT32 dver;
	EFI_STATUS status;
	int i, ndesc;
	unsigned long available_pages;

	sz = 0;
	status = BS->GetMemoryMap(&sz, 0, &key, &dsz, &dver);
	if (status != EFI_BUFFER_TOO_SMALL) {
		printf("Can't determine memory map size\n");
		return;
	}

	map = malloc(sz);
	status = BS->GetMemoryMap(&sz, map, &key, &dsz, &dver);
	if (EFI_ERROR(status)) {
		printf("Can't read memory map\n");
		goto out;
	}

	ndesc = sz / dsz;

	for (i = 0, p = map; i < ndesc;
	     i++, p = NextMemoryDescriptor(p, dsz)) {
		start = p->PhysicalStart;
		end = start + p->NumberOfPages * EFI_PAGE_SIZE;

		if (KERNEL_PHYSICAL_BASE < start ||
		    KERNEL_PHYSICAL_BASE >= end)
			continue;

		if (p->Type != EfiConventionalMemory)
			continue;

		available_pages = p->NumberOfPages -
			((KERNEL_PHYSICAL_BASE - start) >> EFI_PAGE_SHIFT);

		if (*nr_pages > available_pages) {
			printf("staging area size is reduced: %ld -> %ld!\n",
			    *nr_pages, available_pages);
			*nr_pages = available_pages;
		}

		break;
	}

out:
	free(map);
}
#endif

#ifndef EFI_STAGING_SIZE
#define	EFI_STAGING_SIZE	64
#endif

EFI_PHYSICAL_ADDRESS	staging, staging_end;
int			stage_offset_set = 0;
ssize_t			stage_offset;

int
efi_copy_init(void)
{
	EFI_STATUS	status;

	unsigned long nr_pages;

	nr_pages = EFI_SIZE_TO_PAGES((EFI_STAGING_SIZE) * 1024 * 1024);

#if defined(__i386__) || defined(__amd64__)
	/* We'll decrease nr_pages, if it's too big. */
	efi_verify_staging_size(&nr_pages);

	/*
	 * The staging area must reside in the the first 1GB physical
	 * memory: see elf64_exec() in
	 * boot/efi/loader/arch/amd64/elf64_freebsd.c.
	 */
	staging = 1024*1024*1024;
	status = BS->AllocatePages(AllocateMaxAddress, EfiLoaderData,
	    nr_pages, &staging);
#else
	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData,
	    nr_pages, &staging);
#endif
	if (EFI_ERROR(status)) {
		printf("failed to allocate staging area: %lu\n",
		    EFI_ERROR_CODE(status));
		return (status);
	}
	staging_end = staging + nr_pages * EFI_PAGE_SIZE;

#if defined(__aarch64__) || defined(__arm__)
	/*
	 * Round the kernel load address to a 2MiB value. This is needed
	 * because the kernel builds a page table based on where it has
	 * been loaded in physical address space. As the kernel will use
	 * either a 1MiB or 2MiB page for this we need to make sure it
	 * is correctly aligned for both cases.
	 */
	staging = roundup2(staging, 2 * 1024 * 1024);
#endif

	return (0);
}

void *
efi_translate(vm_offset_t ptr)
{

	return ((void *)(ptr + stage_offset));
}

ssize_t
efi_copyin(const void *src, vm_offset_t dest, const size_t len)
{

	if (!stage_offset_set) {
		stage_offset = (vm_offset_t)staging - dest;
		stage_offset_set = 1;
	}

	/* XXX: Callers do not check for failure. */
	if (dest + stage_offset + len > staging_end) {
		errno = ENOMEM;
		return (-1);
	}
	bcopy(src, (void *)(dest + stage_offset), len);
	return (len);
}

ssize_t
efi_copyout(const vm_offset_t src, void *dest, const size_t len)
{

	/* XXX: Callers do not check for failure. */
	if (src + stage_offset + len > staging_end) {
		errno = ENOMEM;
		return (-1);
	}
	bcopy((void *)(src + stage_offset), dest, len);
	return (len);
}


ssize_t
efi_readin(const int fd, vm_offset_t dest, const size_t len)
{

	if (dest + stage_offset + len > staging_end) {
		errno = ENOMEM;
		return (-1);
	}
	return (read(fd, (void *)(dest + stage_offset), len));
}

void
efi_copy_finish(void)
{
	uint64_t	*src, *dst, *last;

	src = (uint64_t *)staging;
	dst = (uint64_t *)(staging - stage_offset);
	last = (uint64_t *)staging_end;

	while (src < last)
		*dst++ = *src++;
}
