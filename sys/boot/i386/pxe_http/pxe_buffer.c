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
 
#include <stand.h>

#include "pxe_buffer.h"
#include "pxe_mem.h"

#ifdef PXE_POOL_SLOTS
/* statically allocated space, used for buffer allocating */
static	uint8_t send_pool[PXE_DEFAULT_SEND_BUFSIZE * PXE_POOL_SLOTS];
static	uint8_t recv_pool[PXE_DEFAULT_RECV_BUFSIZE * PXE_POOL_SLOTS];
/* pool slot usage 0 - unused, 1 - used */
static	uint8_t	send_pool_slots[PXE_POOL_SLOTS];
static	uint8_t	recv_pool_slots[PXE_POOL_SLOTS];

/* pxe_buffer_init() - initializes slots for statically allocated buffers
 * in/ out:
 *	none
 */
void
pxe_buffer_init()
{

	int slot = 0;
	
	for ( ; slot < PXE_POOL_SLOTS; ++slot) {
		send_pool_slots[slot] = 0;
		recv_pool_slots[slot] = 0;
	}
}
#endif

/* pxe_buffer_write() - write data to buffer, if possible
 * in:
 *	buf	- pointer to buffer structure
 *	from	- pointer to data to write
 *	size	- size of data buffer
 * out:
 *	actual count of written bytes
 */
uint16_t
pxe_buffer_write(PXE_BUFFER *buf, const void *from, uint16_t size)
{

	if (buf == NULL) {
		printf("pxe_buffer_write(): NULL buffer\n");
		return (0);
	}
	
	uint16_t	to_write = (size < buf->bufleft) ? size : buf->bufleft;

	if (buf->data == NULL) {
		printf("pxe_buffer_write(): NULL buffer data\n");
		return (0);
	}
	
#ifdef PXE_DEBUG_HELL
	printf("pxe_buffer_write(): fstart %d, fend %d, bufleft %d (of %d),"
	       " to_write %d (%d)\n", buf->fstart, buf->fend, buf->bufleft,
	       buf->bufsize, to_write, size);
#endif			

	if (to_write == 0)	/* no space left*/
		return (0);
		
	/* check if possible to place without cycling */
	if (buf->fstart < buf->fend) { /* possible to place without cycling */
	
		pxe_memcpy(from, buf->data + buf->fstart, to_write);
		buf->fstart += to_write;
			
	} else {/* may be need to place, using two memcpy operations */
		
		/* right part of buffer */
		uint16_t part1 = buf->bufsize - buf->fstart;
		/* left part of buffer */
		uint16_t part2 = to_write - part1;

		if (part1)
			pxe_memcpy(from, buf->data + buf->fstart,
			    (part1 < to_write) ? part1 : to_write);

		if (part1 >= to_write) {
			buf->fstart += to_write;
		} else {
			pxe_memcpy(from + part1, buf->data, part2);
			buf->fstart = part2;
		}
	}

	buf->bufleft -= to_write;

#ifdef PXE_DEBUG
	printf("pxe_buffer_write(): bufleft %d (-%d)\n", buf->bufleft, to_write);
#endif	
	return (to_write);
}

/* pxe_buffer_read() - reades data from buffer, if possible
 * in:
 *	buf	- pointer to buffer structure
 *	to	- pointer to data to read to,
 *		  if NULL - data is read but not placed anywhere
 *	size	- size of data buffer
 * out:
 *	actual count of read bytes
 */
uint16_t
pxe_buffer_read(PXE_BUFFER *buf, void *to, uint16_t size)
{

	if (buf == NULL) {
		printf("pxe_buffer_read(): NULL buffer\n");
		return (0);
	}
	
	if (buf->data == NULL) {
		printf("pxe_buffer_read(): NULL buffer data\n");
		return (0);
	}
	
	uint16_t	usage = buf->bufsize - buf->bufleft;
	uint16_t	to_read = (size <= usage) ? size : usage;

	if (to_read == 0)	/* nothing to read */
		return (0);
	
	uint16_t fstart = buf->fstart;
	uint16_t fend = buf->fend;
	uint16_t bufsize = buf->bufsize;
	
	if (fstart <= fend) { /* two cases handling: |*s...e**|, |***se***| */

		/* right part of buffer */
		uint16_t part1 = bufsize - fend;
		/* left part of buffer */
		uint16_t part2 = to_read - part1;

		if (part1 && (to != NULL) )
			pxe_memcpy(buf->data + fend, to,
			    (part1 < to_read) ? part1 : to_read);
		
		if (part1 >= to_read)
			buf->fend += to_read;
		else {
			if (to != NULL)
				pxe_memcpy(buf->data, to + part1, part2);
			
			buf->fend = part2;			
		}
		
	} else  { /* third case: |..e**s...| */

		if (to != NULL)	
			pxe_memcpy(buf->data + buf->fend, to, to_read);
		
		buf->fend += to_read;	
	} 

	buf->bufleft += to_read;

#ifdef PXE_DEBUG_HELL		
	printf("pxe_buffer_read(): bufleft %d (+%d), fstart %d, fend %d\n",
	    buf->bufleft, to_read, buf->fstart, buf->fend
	);
#endif	
	return (to_read);
}

/* pxe_buffer_space() - returns free space in buffer
 * in:
 *	buffer - pointer to buffer structure
 * out:
 *	count in bytes of free space in buffer
 */
uint16_t
pxe_buffer_space(PXE_BUFFER *buffer)
{

	if (buffer == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_buffer_space(): NULL buffer\n");
#endif
		return (0);
	}
	
	return (buffer->bufleft);
}

#ifdef PXE_POOL_SLOTS
int
alloc_free_slot(uint8_t *slots, int slot_count)
{
	int slot = 0;
	
	for ( ; slot < slot_count; ++slot)
		if (slots[slot] == 0) {
			slots[slot] = 1;
			return (slot);
		}

	return (-1);
}

/* pxe_buffer_memalloc() - allocates memory for buffer
 * in:
 *	buffer	- pointer to buffer structure
 *	size	- bytes to allocate
 * out:
 *	0	- failed
 *	1	- success
 */
int
pxe_buffer_memalloc(PXE_BUFFER *buffer, uint16_t size)
{

	if (buffer == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_buffer_memalloc(): NULL buffer\n");
#endif
		return (0);
	}

	if (buffer->data != NULL) {
		/* buffer has same size */
		if (buffer->bufsize == size)
			return (1);
		
		/* theoretically we never must get here, cause of
		 * current method of allocating buffers for sockets.
		 */
		printf("pxe_buffer_memalloc(): Unhandled alloc case.\n");
		return (0);
	}
	
	int	slot = -1;
	uint8_t	*data = NULL;
	
	switch (size) {
	case PXE_DEFAULT_RECV_BUFSIZE:
		slot = alloc_free_slot(recv_pool_slots, PXE_POOL_SLOTS);
		data = recv_pool + slot * size;
		break;
		
	case PXE_DEFAULT_SEND_BUFSIZE:
		slot = alloc_free_slot(send_pool_slots, PXE_POOL_SLOTS);
		data = send_pool + slot * size;
		break;
		
	default:
		printf("pxe_buffer_memalloc(): unsupported size (%u bytes).\n",
		    size);
		break;
	}

	if (slot == -1)	/* failed to find free slot */
		return (0);
		
	buffer->bufsize = size;
	buffer->bufleft = size;
        buffer->fstart = 0;
        buffer->fend = size;
	buffer->data = data;

#ifdef PXE_DEBUG_HELL
	printf("pxe_buffer_memalloc(): buffer 0x%x, data 0x%x, bufleft %u.\n",
	    buffer, buffer->data, buffer->bufleft
	);
#endif
	return (1);
}

/* pxe_buffer_memfree() - release memory used by buffer
 * in:
 *	buffer	- pointer to buffer structure
 * out:
 *	none
 */
void
pxe_buffer_memfree(PXE_BUFFER *buffer)
{

	if (buffer == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_buffer_memfree(): NULL buffer\n");
#endif
		return;
	}
	
	if (buffer->data == NULL) { /* already released */
		printf("pxe_buffer_memfree(): already released.\n");
		return;
	}

#ifdef PXE_DEBUG_HELL
	printf("pxe_buffer_memfree(): buffer 0x%x, data 0x%x, bufleft: %d.\n",
	    buffer, buffer->data, buffer->bufleft
	);
#endif
	int	slot = -1;
	uint8_t *slots = NULL;
	
	switch (buffer->bufsize) {
	case PXE_DEFAULT_RECV_BUFSIZE:
		slot = (buffer->data - (void *)recv_pool) / 
			PXE_DEFAULT_RECV_BUFSIZE;
			
		slots = recv_pool_slots;
		break;
		
	case PXE_DEFAULT_SEND_BUFSIZE:
		slot = (buffer->data - (void *)send_pool) /
			PXE_DEFAULT_SEND_BUFSIZE;
			
		slots = send_pool_slots;
		break;
	default:
		printf("pxe_buffer_memfree(): unsupported size (%u bytes).\n",
		    buffer->bufsize);
		break;
	}

	if (slots && (slot > -1) && (slot < PXE_POOL_SLOTS)) {
		slots[slot] = 0;
	}

	buffer->data = NULL;
}
#endif /* #ifdef PXE_POOL_SLOTS */

/* pxe_buffer_memalloc() - allocates memory for buffer
 * in:
 *	buffer	- pointer to buffer structure
 *	size	- bytes to allocate
 * out:
 *	0	- failed
 *	1	- success
 */
int
pxe_buffer_memalloc(PXE_BUFFER *buffer, uint16_t size)
{

	if (buffer == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_buffer_memalloc(): NULL buffer\n");
#endif
		return (0);
	}
	
	if (buffer->data == NULL) { /* alloc if not already allocated */
		buffer->data = pxe_alloc(size);
	
		if (buffer->data == NULL)
			return (0);
	} else {
		printf("pxe_buffer_memalloc(): already %u bytes, asked %u.\n",
		    buffer->bufsize, size);
	}

	buffer->bufsize = size;
	buffer->bufleft = size;
        buffer->fstart = 0;
        buffer->fend = size;

#ifdef PXE_DEBUG_HELL
	printf("pxe_buffer_memalloc(): buffer 0x%x, data 0x%x, bufleft %u.\n",
	    buffer, buffer->data, buffer->bufleft
	);
#endif
	return (1);
}

/* pxe_buffer_memfree() - release memory used by buffer
 * in:
 *	buffer	- pointer to buffer structure
 * out:
 *	none
 */
void
pxe_buffer_memfree(PXE_BUFFER *buffer)
{

	if (buffer == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_buffer_memfree(): NULL buffer\n");
#endif
		return;
	}
	
	if (buffer->data == NULL) { /* already released */
		printf("pxe_buffer_memfree(): already released.\n");
		return;
	}

#ifdef PXE_DEBUG_HELL
	printf("pxe_buffer_memfree(): buffer 0x%x, data 0x%x, bufleft: %d.\n",
	    buffer, buffer->data, buffer->bufleft
	);
#endif	
	pxe_free(buffer->data);
	buffer->data = NULL;
}
