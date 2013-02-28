/*-
 * Copyright (c) 2004, 2006 Marcel Moolenaar
 * All rights reserved.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>

#include <efi.h>
#include <efilib.h>

#include <machine/efi.h>
#include <machine/metadata.h>

#include "bootstrap.h"
#include "framebuffer.h"

static UINTN mapkey;

uint64_t
ldr_alloc(vm_offset_t va)
{

	return (0);
}

int
ldr_bootinfo(struct preloaded_file *kfp)
{
	EFI_MEMORY_DESCRIPTOR *mm;
	EFI_PHYSICAL_ADDRESS addr;
	EFI_STATUS status;
	size_t efisz;
	UINTN mmsz, pages, sz;
	UINT32 mmver;
	struct efi_header *efihdr;

        efisz = (sizeof(struct efi_header) + 0xf) & ~0xf;

	/*
	 * Allocate enough pages to hold the bootinfo block and the memory
	 * map EFI will return to us. The memory map has an unknown size,
	 * so we have to determine that first. Note that the AllocatePages
	 * call can itself modify the memory map, so we have to take that
	 * into account as well. The changes to the memory map are caused
	 * by splitting a range of free memory into two (AFAICT), so that
	 * one is marked as being loader data.
	 */
	sz = 0;
	BS->GetMemoryMap(&sz, NULL, &mapkey, &mmsz, &mmver);
	sz += mmsz;
	sz = (sz + 0xf) & ~0xf;
	pages = EFI_SIZE_TO_PAGES(sz + efisz);
	status = BS->AllocatePages(AllocateAnyPages, EfiLoaderData, pages,
	    &addr);
	if (EFI_ERROR(status)) {
		printf("%s: AllocatePages() returned 0x%lx\n", __func__,
		    (long)status);
		return (ENOMEM);
	}

	/*
	 * Read the memory map and stash it after bootinfo. Align the
	 * memory map on a 16-byte boundary (the bootinfo block is page
	 * aligned).
	 */
	efihdr = (struct efi_header *)addr;
	mm = (void *)((uint8_t *)efihdr + efisz);
	sz = (EFI_PAGE_SIZE * pages) - efisz;
	status = BS->GetMemoryMap(&sz, mm, &mapkey, &mmsz, &mmver);
	if (EFI_ERROR(status)) {
		printf("%s: GetMemoryMap() returned 0x%lx\n", __func__,
		    (long)status);
		return (EINVAL);
	}

	efihdr->memory_size = sz;
	efihdr->descriptor_size = mmsz;
	efihdr->descriptor_version = mmver;

	efi_find_framebuffer(efihdr);

	file_addmetadata(kfp, MODINFOMD_EFI, efisz + sz, efihdr);

	return (0);
}

int
ldr_enter(const char *kernel)
{
	EFI_STATUS status;

	status = BS->ExitBootServices(IH, mapkey);
	if (EFI_ERROR(status)) {
		printf("%s: ExitBootServices() returned 0x%lx\n", __func__,
		    (long)status);
		return (EINVAL);
	}

	return (0);
}
