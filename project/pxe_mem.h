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
			 
#ifndef PXE_MEM_H_INCLUDED
#define PXE_MEM_H_INCLUDED

/*
 * Contains wrappers to memory routines, needed for allocating
 * and releasing memory blocks.
 */
 
#include <stddef.h>

/* pxe_alloc() - allocates memory block of requested size
 * in:
 *	bsize - size in bytes of requeseted memory block
 * out:
 *	NULL     - failed to allocate
 *	not NULL - pointer to allocated memory
 */
void	*pxe_alloc(size_t bsize);

/* pxe_free() - frees previously allocated memory block
 * in:
 *	pmem -  pointer to memory block. Attempt to free unallocated block
 *               may cause undefined behaviour.
 * out:
 *	none
 */
void 	pxe_free(void *pmem);

/* pxe_init_mem() - inits internal structures for memory routines
 * in:
 *	chunk   -   non NULL pointer to memory address, from which starts pool,
 *                   used to allocate memory blocks
 *	size    -   size of provided memory pool
 * out:
 *	positive - all is ok
 *	0        - failed to init structures.
 */
int	pxe_init_mem(void *pool, size_t size);

/* pxe_memset() - set specified memory block with given value
 * in:
 *	mblock  -   non NULL pointer to memory block
 *	toset   -   value to set in every byte in memory block
 *	size    -   size of memory block
 * out:
 *	none
 */
void 	pxe_memset(void *mblock, int toset, size_t size);

/* pxe_memcpy() - copy memory block
 * in:
 *	from    -   non NULL pointer to source memory block
 *	to      -   non NULL pointer to destination memory block
 *	size    -   size of memory block
 * out:
 *	none
 */
void	pxe_memcpy(const void *from, void *to, size_t size);

#endif // PXE_MEM_H_INCLUDED
