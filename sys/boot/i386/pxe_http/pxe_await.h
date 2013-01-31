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

#ifndef PXE_AWAIT_H_INCLUDED
#define PXE_AWAIT_H_INCLUDED

/*
 * Implements await functions wrapper
 */

#include <stdint.h>

/* await callback function type */
typedef int (*pxe_await_func)(uint8_t function, uint16_t try_counter,
		uint32_t timeout,  void *data);

#define PXE_AWAIT_NEWPACKETS	0x00	/* some packets received, check it */
#define PXE_AWAIT_STARTTRY	0x01	/* start of new try */
#define PXE_AWAIT_FINISHTRY	0x02	/* end of current try */
#define PXE_AWAIT_END		0x03	/* ending of waiting */

/* values that may be returned by await function */
#define PXE_AWAIT_OK		0x00	/* ok, do what you want */
#define PXE_AWAIT_COMPLETED	0x01	/* wait ended succefully */
#define PXE_AWAIT_CONTINUE	0x02	/* continue waiting */
#define PXE_AWAIT_NEXTTRY	0x03	/* continue with next try */
#define PXE_AWAIT_BREAK		0x04	/* wait ended with failure */

#define TIME_DELTA_MS		1
#define TIME_DELTA		1000

/* universal waiting function */
int pxe_await(pxe_await_func func, uint16_t try_counter,
	uint32_t timeout, void *data);

#endif
