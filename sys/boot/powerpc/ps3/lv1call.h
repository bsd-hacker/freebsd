/*-
 * Copyright (C) 2010 Nathan Whitehorn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _PS3_LV1CALL_H
#define _PS3_LV1CALL_H

#include <machine/pte.h>

int lv1_get_physmem(uint64_t *maxmem);
int lv1_setup_address_space(uint64_t *as_id, uint64_t *ptsize);
int lv1_insert_pte(u_int ptegidx, struct lpte *pte, int lockflags);
int lv1_panic(int reboot);

#define L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_MODE_SET	0x0100
#define L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_SYNC		0x0101
#define  L1GPU_DISPLAY_SYNC_HSYNC			1
#define  L1GPU_DISPLAY_SYNC_VSYNC			2
#define L1GPU_CONTEXT_ATTRIBUTE_DISPLAY_FLIP		0x0102

int lv1_gpu_open(int);
int lv1_gpu_context_attribute(int context, int op, int, int, int, int);
int lv1_gpu_memory_allocate(int size, int, int, int, int, uint64_t *handle,
	uint64_t *paddr);
int lv1_gpu_context_allocate(uint64_t handle, int, uint64_t *context);

#endif

