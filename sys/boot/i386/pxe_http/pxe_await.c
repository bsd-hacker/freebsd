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

#include "pxe_await.h"
#include "pxe_core.h"

/* pxe_await() - awaits for packets
 * in:
 * 	await_func	- callback function
 *	trys		- how much trys to make
 *	timeout		- timeout of waiting in ms
 *	data		- additional data to give to callback function
 * out:
 *	0		- failed
 *	1		- success
 */
int
pxe_await(pxe_await_func await_func, uint16_t trys, uint32_t timeout, void *data)
{
	int		result = 0;
	uint32_t	time_elapsed = 0;
	uint16_t	try_counter = 0;
	
	while (try_counter < trys) {
	
		/* notify about start of try */
		result = await_func(PXE_AWAIT_STARTTRY, try_counter, 0, data);
		
		switch (result) {
		case PXE_AWAIT_NEXTTRY:
			++try_counter;
			time_elapsed = 0;	/* skip this try */
			continue;
			
		case PXE_AWAIT_BREAK:	/* return failure */
			return (0);
			break;
			
		default:		/* other codes are good for us */
			break;
		}
		
		while (time_elapsed < timeout) {
#ifdef PXE_DEBUG
			twiddle(1);
#endif
        	        if (pxe_core_recv_packets()) {
				/* means some packet was received */
                	        
				result = await_func(PXE_AWAIT_NEWPACKETS,
					    try_counter, time_elapsed, data); 

				if (result == PXE_AWAIT_COMPLETED) {
					await_func(PXE_AWAIT_FINISHTRY,
					    try_counter, time_elapsed, data);
					    
					return (1);
				}
				
				if (result == PXE_AWAIT_NEXTTRY)
					break;
				
				/* aborted waiting */
				if (result == PXE_AWAIT_BREAK)
					break;
					
				/* continue */		
			}
		
	        	delay(TIME_DELTA);
	    		time_elapsed += TIME_DELTA_MS;
		}

		/* notify about end of try */
		result = await_func(PXE_AWAIT_FINISHTRY, try_counter,
			    time_elapsed, data);
		
		if (result == PXE_AWAIT_BREAK)	/* failure */
			return (0);

		++try_counter;
		time_elapsed = 0;
	}
	
	/* notify about end of await, result is not interesting */
	await_func(PXE_AWAIT_END, try_counter, time_elapsed, data);
	
	/* if waiting of packet successful,
	 * control returned higher (PXE_AWAIT_COMPLETED)
	 */
	return (0);
}
