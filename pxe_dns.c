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
#include "pxe_dns.h"
#include "pxe_ip.h"
#include "pxe_sock.h"

static 	PXE_DNS_WAIT_DATA	static_wait_data;

/* write_question_for()-  writes labels for provided domain name 
 * in:
 *	question - where to write
 *	name	 - domain name
 * out:
 *	NULL	- if failed
 *	non NULL- ponits to the next byte after labeled name.
 */
char *
write_question_for(uint8_t *question, const char *name) 
{
	size_t	len = strlen(name);

	if (len > 255)	/* oversize */
		return (NULL);

	size_t	ind = len;
	uint8_t	symbol_count = 0;
	uint8_t		*res = question + len;
	const char	*np = name + len - 1;
	question[len + 1] = 0; /* label end of question */	
	
	/* placing from the end, replacing dots with symbol counter */
	for ( ; ind != 0; --ind) {

		*res = *np;
		
		if ( *res == '.') {
			
			if (symbol_count == 0) { /* example..of.error */
				return (NULL);
			}
			
			*res = symbol_count;
			symbol_count = 0;
			
		} else {
			++symbol_count;
		}
		
		--res;
		--np;
	}
	
	*res = symbol_count; /* length for first label */
		
	/* +1 for first length, +1 for \0 */
	return (question + len + 2);
}

/* create_dns_packet()-  creates DNS request packet 
 * in:
 *	wd	- pointer to wait data structure
 *	id	 - request id to distinguish requests
 * out:
 *	0	- failed
 *	>0	- size of created packet
 */
int
create_dns_packet(PXE_DNS_WAIT_DATA *wd, uint16_t id, uint16_t query_type)
{
	char	*name = wd->name;
	void	*data = wd->data;
	int	max_size = wd->size;
	
	PXE_DNS_REQUEST_HDR	*request = (PXE_DNS_REQUEST_HDR *)data;
	pxe_memset(request, 0, sizeof(PXE_DNS_REQUEST_HDR));
	
	uint8_t	*question =
		    (uint8_t *)(data + sizeof(PXE_DNS_REQUEST_HDR));
	
	/* header set with zeroes, so fill only needed values */	
	request->id = htons(id);
	request->flags = htons(PXE_DNS_DEFAULT_FLAGS);
	request->qdcount = htons(1);
	
	question = write_question_for(question, name);
	
	if (question == NULL)	/* failed to write question section */
		return (0);	/* may be, name size is to big */
		
	PXE_DNS_REQUEST_FOOT *foot = (PXE_DNS_REQUEST_FOOT *)question;

	/* finishing creating of packet */ 
	foot->qtype = htons(query_type);
	foot->qclass = htons(PXE_DNS_CLASS_IN);
	 
	question += sizeof(PXE_DNS_REQUEST_FOOT);
	
	/* return total size of packet */
	return (((void *)question) - data);
}

/* skip_name() - gets name from answers
 * in:
 *	org - pointer to packet data start
 *	off - offset to name or part of name to get
 *	to_place - where to place name. NULL, if not interesting for us
 * out:
 *	bytes, read from offset (name definition length)
 */
int
skip_name(uint8_t *org, uint16_t off, uint8_t *to_place)
{
	uint8_t	label[64];
	label[0] = 0;
	
	int res =0 ;
	uint8_t *data = org + off;
	
	while (*data != 0) {
	
		if (*data < 64) { /* just a label */
			pxe_memcpy(data + 1, label, *data);
			label[*data] = 0;

			if (to_place != 0) {
				/* updating to_place, add dot, if there is
				 * part of name in buffer */
				if (to_place[0] != 0) 
					strcat((char *)to_place, ".");
					
				strcat((char *)to_place, (const char*)label);
			}
				
			res += (1 + *data);
			data += (1 + *data);
			
		} else {/* compression  enabled, this is part of pointer */
			uint16_t off = (((*data) & 0x3f) << 8) + *(data + 1);
			skip_name(org, off, to_place);
			
			res += 1;
			break;
		}
	}
	
	res += 1;	/* ending zero skip */
	return (res);
}

/* parse_dns_reply() - parses reply from DNS server
 * in:
 *	wd - pointer to waiting data structure
 * out:
 *	0 	- parsing failed, or packet has no information about our domain
 *	1	- success, wait_data->result contains ip
 */
int
parse_dns_reply(PXE_DNS_WAIT_DATA *wd)
{
	uint8_t *data = wd->data;
	int	size = wd->size;
	char	*name = wd->name;
	uint16_t id = wd->id;
	uint8_t *cname = wd->cname;
	
	cname[0] = 0;
	
	if (size < sizeof(PXE_DNS_REQUEST_HDR) + 8) {
		/* too small packet to be with data */
#ifdef PXE_DEBUG
		printf("parse_dns_reply(): too small packet.\n");
#endif
		return (0);
	}
	
	PXE_DNS_REQUEST_HDR	*hdr = (PXE_DNS_REQUEST_HDR *)data;
	uint8_t			*answer = data + sizeof(PXE_DNS_REQUEST_HDR);
	
	if ( hdr->id != htons(id)) { /* wrong id */
#ifdef PXE_DEBUG
		printf("parse_dns_reply(): wrong id %d, expected %d.\n",
		    ntohs(hdr->id), id);
#endif
		return (0);
	}
	
	uint16_t flags = ntohs(hdr->flags);
	
	if ( (flags & 0xf800) != 0x8000) { /* QR != 1 */
#ifdef PXE_DEBUG
		printf("parse_dns_reply(): got request. Ignoring it.\n");
#endif	
		return (0);
	}

#ifdef PXE_DEBUG
	printf("parse_dns_reply(): query/answer/ns/additional = %d/%d/%d/%d\n",
	    ntohs(hdr->qdcount), ntohs(hdr->ancount),
	    ntohs(hdr->nscount), ntohs(hdr->arcount));
#endif
	/* getting server return code */
	int rcode = (flags & 0x000f);
	
	switch(rcode) {
	case 0: /* good */
		break;
	case 1:
	        printf("parse_dns_reply(): server said format error.\n");
		return (0);
		break;
	case 2:
	        printf("parse_dns_reply(): server failed.\n");
		return (0);
		break;
	case 3:
	        printf("parse_dns_reply(): name error, domain not exists?\n");
		return (0);
		break;	
	case 4:
	        printf("parse_dns_reply(): operation not implemented.\n");
		return (0);
		break;		
	case 5:
	        printf("parse_dns_reply(): access refused.\n");
		return (0);
		break;		
	default:
	        printf("parse_dns_reply(): unknown rcode = %d.\n", rcode);
		return (0);
		break;
	}
	
	/* server reported success */
	
	if (hdr->ancount == 0) { /* there is no answers */
	        printf("parse_dns_reply(): there are no answers in DNS reply.\n");
		return (0);	
	}

	uint8_t	 aname[256];	/* storage for domain names in answers */
	
	switch (ntohs(hdr->qdcount)) {
	case 0:	/* best case, nothing must be skipped to get answer data */
		break;
	case 1:

		aname[0] = 0;
		answer += skip_name(data, answer - data, aname);
#ifdef PXE_DEBUG
		printf("question: %s\n", aname);
#endif		
		/* answer points qclass/qtypr, skipping it */
		answer += sizeof(PXE_DNS_REQUEST_FOOT);
		break;
	
	default: /* error */
		printf("parse_dns_reply(): me sent only one query, "
		       "but server says %d.\n", ntohs(hdr->qdcount));
		return (0);
	}
	
	
	/* parsing answers, authorative section and additional section,
	 * hoping to find A resource record
	 */
	uint16_t index = ntohs(hdr->ancount) + ntohs(hdr->nscount) +
			 ntohs(hdr->arcount);
	
	while (index) {

		aname[0] = 0;		
		answer += skip_name(data, answer - data, aname);

#ifdef PXE_DEBUG
		printf("answer: %s", aname);	
#endif		
		PXE_DNS_REQUEST_FOOT	*ans_foot =
					    (PXE_DNS_REQUEST_FOOT *)answer;
	
		if (ntohs(ans_foot->qclass) != PXE_DNS_CLASS_IN) {
			printf("parse_dns_reply(): IN expected, got 0x%x.\n",
			    ntohs(ans_foot->qclass));
			    
			return (0);
		}
	
		answer += sizeof(PXE_DNS_REQUEST_FOOT);	

		PXE_DNS_REQUEST_FOOT2	*ans_foot2 =
					    (PXE_DNS_REQUEST_FOOT2 *)answer;

		answer += sizeof(PXE_DNS_REQUEST_FOOT2);
		
		uint16_t qtype = ntohs(ans_foot->qtype);
		uint16_t rdlength = ntohs(ans_foot2->rdlength);
			
		if (qtype == PXE_DNS_QUERY_A) {
			/* successfully got A record */
			
/* A for our address */	if ( (!strcmp(aname, name)) ||	
/* A for our CNAME */	     ((cname[0]) && (!strcmp(aname, cname))) ) 
			{
				/* sanity check */
				if (rdlength != 4) {
					/* wrong length of ip4 adrress length*/
					return (0);
				}
		
				/* answer points to rdata = ip4 */	

				wd->result.octet[0] = answer[0];
				wd->result.octet[1] = answer[1];
				wd->result.octet[2] = answer[2];
				wd->result.octet[3] = answer[3];
#ifdef PXE_DEBUG
				printf(" = %s\n", inet_ntoa(wd->result.ip));
#endif				
				return (1);
			}

#ifdef PXE_DEBUG
			printf("parse_dns_reply(): A resource record '%s' "
			       "is strange. Ignoring it.\n", aname);
#endif
		}

		if (qtype == PXE_DNS_QUERY_CNAME) {

			cname[0] = 0;
			skip_name(data, answer - data, cname);			
#ifdef PXE_DEBUG
			printf(" is alias to %s\n", (char *)cname);
#endif
			
		} else {
			printf("parse_dns_reply(): A or CNAME expected, "
			       "but got 0x%x, rdlength: %d.\n", qtype, rdlength);
		}
		
		answer += rdlength;
		--index;
	}

	/* have not found anything good */
	return (0);	
}

/* converts provided string representation of of address to uint32_t
 * in:
 *	str - string to convert
 * out:
 *	0   	- failed
 *	not 0 	- ip4 addr
 */
uint32_t
pxe_convert_ipstr(char* str)
{
        PXE_IPADDR      ip;
        ip.ip = 0;
		
        int     octet_index = 0;
        int     accum = 0;
        int     ch_index = 0;
	
        for ( ; ch_index < strlen(str); ++ch_index) {

		if (str[ch_index] == '.') {
                        ip.octet[octet_index] = accum;
                        accum = 0;
                        ++octet_index;

	                if (octet_index == 4)
	                        break;
	
	                continue;
	        }
	
	        if (!isdigit(str[ch_index]))
	                return (0);
    
                accum *= 10;
                accum += (str[ch_index] - 0x30);
        }

	if (octet_index < 4)
    		ip.octet[octet_index] = accum;
    
        return ip.ip;
}

/* dns_request() - creates and sends request
 * in:
 *	wait_data - DNS waiting data
 * out:
 *	0 - failed
 *	1 - success
 */
int
dns_request(PXE_DNS_WAIT_DATA *wait_data)
{
	int socket = pxe_socket();
	
	if (socket == -1) {
		printf("dns_request(): failed to create socket.\n");
		return (0);
	}

	uint16_t size = create_dns_packet(wait_data, wait_data->id,
			    PXE_DNS_QUERY_A);
	
	if (size == 0) {
		printf("dns_request(): failed to create request.\n");
		pxe_close(socket);
		return (0);	    
	}

	if (size != pxe_sendto(socket, pxe_get_ip(PXE_IP_NAMESERVER), 53,
			wait_data->data, size))
	{
		printf("dns_request(): failed to send DNS request.\n");
		pxe_close(socket);
		return (0);
	}
	
	wait_data->socket = socket;
	
	return (1);
}

/* dns_await() - await callback function for DNS requests/replies
 * in:
 *	function	- await function
 *	try_number	- current number of try
 *	timeout		- current timeout from start of try
 *	data		- pointer to PXE_DNS_WAIT_DATA
 * out:
 *	PXE_AWAIT_ constants
 */
int
dns_await(uint8_t function, uint16_t try_number, uint32_t timeout, void *data)
{
	PXE_DNS_WAIT_DATA	*wait_data = (PXE_DNS_WAIT_DATA *)data;
	int			size = -1;

	switch(function) {

	case PXE_AWAIT_STARTTRY:
		if (!dns_request(wait_data)) {
			return (PXE_AWAIT_NEXTTRY);
		}
		break;
		
	case PXE_AWAIT_FINISHTRY:
		if (wait_data->socket != -1)
			pxe_close(wait_data->socket);
			
		wait_data->id += 1;
		break;

	case PXE_AWAIT_NEWPACKETS:
		size = pxe_recv(wait_data->socket, wait_data->data,
			    wait_data->size, PXE_SOCK_NONBLOCKING);

		if (size > 0) {
#ifdef PXE_DEBUG
			printf("dns_await(): Received DNS reply (%d bytes).\n",
			    size);
#endif			    
			parse_dns_reply(wait_data);
			
			if (wait_data->result.ip != 0) {
			        return (PXE_AWAIT_COMPLETED);
			}
				
			if (wait_data->cname[0] != 0) {
				/* failed to get A, but found CNAME,
				 * need to send other request 
				 * with CNAME as name to resolve
				 */
				strcpy(wait_data->name, wait_data->cname);
					
				size = create_dns_packet(wait_data,
					    wait_data->id,
					    PXE_DNS_QUERY_A);
	
				if (size == 0) {
					printf("dns_await(): failed to create request.\n");
					return (PXE_AWAIT_NEXTTRY); /* next try */
				}

				if (size != pxe_send(wait_data->socket,
						wait_data->data, size))
				{
					printf("dns_await(): failed to send DNS request.\n");
					return (PXE_AWAIT_NEXTTRY);
				}			
			}

		}
		return (PXE_AWAIT_CONTINUE);
		break;
	
	case PXE_AWAIT_END:
	default:
		break;
	}
	
	return (PXE_AWAIT_OK);
}

/* pxe_gethostbyname() - returns ip4 address by domain name
 * in:
 *	name - domain name to resolve
 * out:
 *	NULL 	- if failed
 *	ip addr - if success
 */
const PXE_IPADDR *
pxe_gethostbyname(char *name)
{
	/* sanity check */
	if (name == NULL)
		return (0);
	
	uint32_t res = pxe_convert_ipstr(name);

	if (res != 0) {
		static_wait_data.result.ip = res;
		return (&static_wait_data.result);
	}
		
	/* 512 bytes is limit for packet, sent via UDP */
	uint8_t	dns_pack[PXE_DNS_MAX_PACKET_SIZE];
	uint8_t	cname[256];
	char	tname[256];

	size_t len = strlen(name);
	
	if (len < 256)
		strcpy(tname, name);
	else {
		strncpy(name, tname, 255);
		tname[255] = 0;
	}
	
	pxe_memset(dns_pack, 0, sizeof(dns_pack));
	
	static_wait_data.socket = -1;
	static_wait_data.id = 1;
	static_wait_data.data = dns_pack;
	static_wait_data.cname = cname;
	static_wait_data.name = tname;
	static_wait_data.size = PXE_DNS_MAX_PACKET_SIZE;
	static_wait_data.result.ip = 0;

	if (!pxe_await(dns_await, 4, 20000, &static_wait_data))
		return (NULL);
	
	return (&static_wait_data.result);
}
