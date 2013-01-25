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

#include "pxe_ip.h"
#include "pxe_filter.h"

/* table with all filters */
static PXE_FILTER_ENTRY	filters_table[MAX_PXE_FILTERS];
static int 		all_filters = 0;	/* used count of filters */
static PXE_FILTER_ENTRY *filters_head = NULL;	/* head of filters list */
static PXE_FILTER_ENTRY *free_head = NULL;	/* head of free filters list */

/* pxe_filter_init() - init of filter module, filter lists
 * in/out:
 *	none
 */
void
pxe_filter_init()
{

	if (all_filters)	/* already was and inited and is used */
		return;
	
	printf("pxe_filter_init(): initing socket ip packet filters.\n");
	
        pxe_memset(filters_table, 0, sizeof(filters_table));

	/* init 2 linked list */
	int index = 0;
	
	for ( ; index < MAX_PXE_FILTERS; ++index) {

		filters_table[index - 1].next = &filters_table[index];
		filters_table[index].prev = &filters_table[index - 1];
	}

	free_head = filters_table;	
	filters_head = NULL;
}

/* filter_alloc() - allocates filter entry from free filters list
 * in:
 *	none
 * out:
 *	NULL	- if failed
 *	not NULL- pointer to filter entry if success
 */
PXE_FILTER_ENTRY *
filter_alloc()
{
	PXE_FILTER_ENTRY	*res = NULL;

#ifdef PXE_DEBUG_HELL
	printf("filter_alloc(): head = 0x%x, filters = %d.\n",
	    free_head, all_filters);
#endif

	if (free_head != NULL) {

		res = free_head;

		free_head = free_head->next;
		free_head->prev = NULL;
		++all_filters;
	}

#ifdef PXE_DEBUG_HELL
	printf("filter_alloc(): entry = 0x%x, head = 0x%x, filters = %d.\n",
	    res, free_head, all_filters);
#endif
	return (res);
}

/* filter_free() - releases filter entry
 * in:
 *	entry - filter entry to release
 * out:
 *	none
 */
void
filter_free(PXE_FILTER_ENTRY *entry)
{

	entry->next = free_head;
	entry->prev = NULL;
	
	if (free_head != NULL)
		free_head->prev = entry;

	free_head = entry;

#ifdef PXE_DEBUG_HELL
	printf("filter_free(): entry = 0x%x, head = 0x%x, filters = %d.\n",
	    entry, free_head, all_filters);
#endif

	--all_filters;
}

/* pxe_filter_add() - installs new filter
 * in:
 *	src_ip	- source IP address
 *	src_port- source port
 *	dst_ip	- destination IP address
 *	dst_port- destination port
 *	socket	- pointer to socket
 *	proto	- IP stack protocol (e.g. UDP)
 * out:
 *	NULL	- if failed
 *	not NULL- pointer to new installed entry
 */
PXE_FILTER_ENTRY *
pxe_filter_add(const PXE_IPADDR *src, uint16_t src_port, const PXE_IPADDR *dst,
	       uint16_t dst_port, void *socket, uint8_t proto)
{

	if (socket == NULL) {
#ifdef PXE_DEBUG		
		printf("pxe_filter_add(): NULL socket.\n");
#endif
		return (NULL);
	}

	if (free_head == NULL) {
#ifdef PXE_DEBUG
		printf("pxe_filter_add(): filter table is full (used = %d).\n",
		    all_filters);
#endif		    
		return (NULL);	/* there is no space for filters */
	}

	PXE_FILTER_ENTRY	*new_filter = filter_alloc();
	
	if (new_filter == NULL) {
		printf("pxe_filter_add(): cannot alloc filter entry.\n");
		return (NULL);
	}

	/* fillling data */
	new_filter->src.ip = (src != NULL) ? src->ip : 0;
	new_filter->dst.ip = dst->ip;
	new_filter->src_port = src_port;
	new_filter->dst_port = dst_port;

	/* default mask allows pckets specified  source and port
	 * to specified ip and port
	 */
	new_filter->src_mask = 0xffffffff;
	new_filter->dst_mask = 0xffffffff;
	new_filter->src_port_mask = 0xffff;
	new_filter->dst_port_mask = 0xffff;
	
	new_filter->socket = socket;
	new_filter->protocol = proto;

	/* updating list, may be rewrite all using some default list
	 *  implementations?
	 *  list is growing LIFO: last added filter become first
	 */
	new_filter->prev = NULL;	
	
	if (filters_head != NULL) {

		new_filter->next = filters_head;
		filters_head->prev = new_filter;

	} else  /* means, adding first filter */
		new_filter->next = NULL;

	filters_head = new_filter;	

	return (new_filter);
}

/* pxe_filter_mask() - fills filter masks 
 * in:
 *	filter		- filter, for which masks are changed
 *	src_ip_mask	- source IP address mask
 *	src_port_mask	- source port mask
 *	dst_ip_mask	- destination IP address mask
 *	dst_port_mask	- destination port mask
 * out:
 *	0 - failed
 *	1 - success
 */
int
pxe_filter_mask(PXE_FILTER_ENTRY *filter, uint32_t src_ip_mask,
	        uint16_t src_port_mask, uint32_t dst_ip_mask,
		uint16_t dst_port_mask)
{

	if (filter == NULL) {	/* sanity check */
#ifdef PXE_DEBUG
		printf("pxe_filter_mask(): NULL filter.\n");
#endif
		return (0);
	}
	
	
	filter->src_mask = src_ip_mask;
	filter->dst_mask = dst_ip_mask;
	filter->src_port_mask = src_port_mask;
	filter->dst_port_mask = dst_port_mask;
		
	return (1);
}

#ifdef PXE_MORE
/* pxe_filter_before() - adds new filter before provided 
 * in:
 *	filter	- pointer to filter to add before
 *	def	- pointer to definition of new filter
 * out:
 *	NULL	- if failed
 *	not NULL- pointer to newly added filter, if success
 */
PXE_FILTER_ENTRY *
pxe_filter_before(PXE_FILTER_ENTRY *filter, const PXE_FILTER_ENTRY* def)
{

	if ((def == NULL) ) {	/* sanity check */
#ifdef PXE_DEBUG
		printf("pxe_filter_before(): invalid filter.\n");
#endif
		return (NULL);
	}
	
	PXE_FILTER_ENTRY *res = NULL;
	
	if (filter == NULL) { /*  handle it as usual filter_add with masking */
	
		res = pxe_filter_add(	&def->src, def->src_port,
					&def->dst, def->dst_port,
					def->socket, def->protocol );
					
		if (res == NULL)
			return (NULL);
		
		if (!pxe_filter_mask(res, def->src_mask, def->src_port_mask,
				     def->dst_mask, def->dst_port_mask))
		{
			pxe_filter_remove(res);
			
			return (NULL);
		}
		
		return(res);
	}
	
	/* allocating new filter entry */
	res = filter_alloc();
	
	if (res == NULL)
		return (NULL);
	
	/* copy needded data*/
	pxe_memcpy(def, res, sizeof(PXE_FILTER_ENTRY));
	
	if (filter == filters_head) {	/* special case, must change head */

		filter->prev = res;
		res->next = filters_head;
		res->prev = NULL;
		filters_head =  res;

	} else { /* adding to list */
		
		res->prev = filter->prev;
		res->next = filter;
		
		if (res->prev) {	/* sanity check, must be always
					 * not NULL anyway (if not head)
					 */
			res->prev->next = res;
		}
	}
		
	return (res);
}
#endif /* PXE_MORE */

/* pxe_filter_remove() -removes filter from filter_table
 * in:
 *	filter - filter to remove
 * out:
 *	0 - failed
 *	1 - success
 */
int
pxe_filter_remove(PXE_FILTER_ENTRY *filter)
{
#ifdef PXE_DEBUG
    if (filter == NULL) {
	    printf("pxe_filter_remove(): NULL filter.\n");
	    return (0);
    }
    
    printf("pxe_filter_remove(): removing filter 0x%x.\n", filter);
    
#endif

    if (filter != filters_head) { /* non head filter */

	    PXE_FILTER_ENTRY	*prev = filter->prev;
	    PXE_FILTER_ENTRY	*next = filter->next;
	    
	    if (prev) 	/* it must be always non NULL*/
		    prev->next = next;
	    
	    if (next)  /* may be NULL for tail */
		    next->prev = prev;
    	    
    } else {	/* removing head filter */
    
	filters_head = filter->next;
	
	if (filters_head)
		filters_head->prev = NULL;
    }
    
    filter_free(filter);
    
    return (1);
}

/* pxe_filter_check() - returns pointer to socket, if parameters matches filter
 * in:
 *	src_ip	- source IP address
 *	src_port- source port
 *	dst_ip	- destination IP address
 *	dst_port- destination port
 *	proto	- IP stack protocol
 * out:
 *	NULL	- if no filter matches this parameters
 *	not NULL- pointer to socket structure
 */
void *
pxe_filter_check(const PXE_IPADDR *src, uint16_t src_port,
		 const PXE_IPADDR *dst, uint16_t dst_port, uint8_t proto)
{
	int			filter_index = 0;
	PXE_FILTER_ENTRY	*entry = filters_head;
	PXE_FILTER_ENTRY	*filter = NULL;	

	while (entry != NULL) {
	
		filter = entry;
		entry = entry->next;
		
		/* checking conditions */
		if (filter->protocol != proto)
			continue;
			
		if ( (filter->src.ip & filter->src_mask) !=
		     (src->ip & filter->src_mask) )
			continue;
			
		if ( (filter->src_port & filter->src_port_mask) != 
		     (src_port & filter->src_port_mask) )
			continue;
			
		if ( (filter->dst.ip & filter->dst_mask) !=
		     (dst->ip & filter->dst_mask) )
			continue;
			
		if ( (filter->dst_port & filter->dst_port_mask) !=
		     (dst_port & filter->dst_port_mask) )
			continue;
			
		/* filter triggered */

		/* sanity check */
		if (filter->socket == NULL)
			continue;
			
		return filter->socket;
	}
	
	return (NULL);
}

#ifdef PXE_MORE
/* pxe_filter_stats() - shows active filter stats
 * in/out:
 *	none
 */
void
pxe_filter_stats()
{
	PXE_FILTER_ENTRY 	*entry = filters_head;
	PXE_IPADDR		src;
	PXE_IPADDR		dst;

	printf("pxe_filter_stats(): %d/%d filters\n",
	    all_filters, MAX_PXE_FILTERS);

	while (entry != NULL) {
	
		printf("\t0x%x: %s/%x %u/%x ->",
		    entry->protocol,
		    inet_ntoa(entry->src.ip), ntohl(entry->src_mask),
		    entry->src_port, ntohs(entry->src_port_mask));
		
		printf("%s/%x %u/%x\tsocket: 0x%x\n",
		    inet_ntoa(entry->dst.ip), ntohl(entry->dst_mask),
		    entry->dst_port, ntohs(entry->dst_port_mask),
		    entry->socket);
		    
		entry = entry->next;
	}
}
#endif /* PXE_MORE */
