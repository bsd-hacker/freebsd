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

#include <stand.h>

#include <efi.h>
#include <efilib.h>

#include <machine/efi.h>

static EFI_GUID gop_guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;

void
efi_find_framebuffer(struct efi_header *efihdr)
{
	EFI_GRAPHICS_OUTPUT			*gop;
	EFI_STATUS				status;
	EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE	*mode;
	EFI_GRAPHICS_OUTPUT_MODE_INFORMATION	*info;

	status = BS->LocateProtocol(&gop_guid, NULL, (VOID **)&gop);
	if (EFI_ERROR(status)) {
		efihdr->fb.fb_present = 0;
		return;
	}

	mode = gop->Mode;
	info = gop->Mode->Info;

	efihdr->fb.fb_present = 1;
	efihdr->fb.fb_addr = mode->FrameBufferBase;
	efihdr->fb.fb_size = mode->FrameBufferSize;
	efihdr->fb.fb_height = info->VerticalResolution;
	efihdr->fb.fb_width = info->HorizontalResolution;
	efihdr->fb.fb_stride = info->PixelsPerScanLine;

	switch (info->PixelFormat) {
	case PixelRedGreenBlueReserved8BitPerColor:
		efihdr->fb.fb_mask_red = 0x000000ff;
		efihdr->fb.fb_mask_green = 0x0000ff00;
		efihdr->fb.fb_mask_blue = 0x00ff0000;
		efihdr->fb.fb_mask_reserved = 0xff000000;
		break;
	case PixelBlueGreenRedReserved8BitPerColor:
		efihdr->fb.fb_mask_red = 0x00ff0000;
		efihdr->fb.fb_mask_green = 0x0000ff00;
		efihdr->fb.fb_mask_blue = 0x000000ff;
		efihdr->fb.fb_mask_reserved = 0xff000000;
		break;
	case PixelBitMask:
		efihdr->fb.fb_mask_red = info->PixelInformation.RedMask;
		efihdr->fb.fb_mask_green = info->PixelInformation.GreenMask;
		efihdr->fb.fb_mask_blue = info->PixelInformation.BlueMask;
		efihdr->fb.fb_mask_reserved =
		    info->PixelInformation.ReservedMask;
		break;
	default:
		efihdr->fb.fb_present = 0;
	}
}
