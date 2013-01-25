/*-
 * Copyright (c) 2007 Alexey Tarasov
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

#ifndef PXE_BUFFER_H_INCLUDED
#define PXE_BUFFER_H_INCLUDED

/*
 * Implements cyclic buffer routines
 */
 
#include <stdint.h>

/* slot count in buffer pool, define this for statical buffer allocation */
/* #define PXE_POOL_SLOTS 2 */

/* buffer size choosed by default for sending/recieving */
#define PXE_DEFAULT_RECV_BUFSIZE	16384
#define PXE_DEFAULT_SEND_BUFSIZE        4096

/* pxe_buffer - buffer related information */
typedef struct pxe_buffer {

	void        *data;      /* pointer to memory block, used for buffer */

	uint16_t    fstart;	/* start of free space part in buffer */
	uint16_t    fend;	/* end of free space part in buffer */
	
	uint16_t    bufsize;    /* size of memory block */
	uint16_t    bufleft;    /* left buffer space */

} PXE_BUFFER;

/* allocates memory for buffer */
int	 pxe_buffer_memalloc(PXE_BUFFER *buffer, uint16_t size);

/* releases buffer memory */
void	 pxe_buffer_memfree(PXE_BUFFER *buffer);

/* writes data to buffer */
uint16_t pxe_buffer_write(PXE_BUFFER *buffer, const void* data, uint16_t size);

/* returns free space size in buffer */
uint16_t pxe_buffer_space(PXE_BUFFER *buffer);

/* performs initialization of buffer pool */
#ifdef PXE_POOL_SLOTS
void pxe_buffer_init();
#endif

#endif // PXE_BUFFER_H_INCLUDED
