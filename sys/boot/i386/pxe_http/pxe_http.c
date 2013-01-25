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
#include "pxe_http.h"
#include "pxe_ip.h"
#include "pxe_tcp.h"

/* #ifndef FNAME_SIZE
#define FNAME_SIZE	128
#endif
*/
/* for testing purposes, used by pxe_fetch() */
#ifdef PXE_MORE
static char http_data[PXE_MAX_HTTP_HDRLEN];
#endif

/* extern char rootpath[FNAME_SIZE]; */

/* parse_size_t() - converts zero ended string to size_t value
 * in:
 *	str	- string to parse
 *	result	- where store result
 * out:
 *	NULL	- failed, ignore result value
 *	not NULL- pointer to next character after last parsed
 */
char *
parse_size_t(char *str, size_t *result)
{
	char *p = str;
	
	while ( (*p != '\0') && (!isdigit(*p)) ) {
		++p;
	}
	
	if (!isdigit(*p)) 	/* nothing to parse */
		return (NULL);
	
	size_t accum = 0;
	
	while ( (*p) && (isdigit(*p))) {
		accum *= 10;
		accum += (*p - '0');
		++p;
	}
	
	*result = accum;
	
	return (p);
}

/* http_reply_parse() - parses http reply, gets status code and length
 * in:
 *	data		- pointer to reply data
 *	count		- max bytes to process [now ignored]
 *	parse_data	- where to store result of parsing
 * out:
 *	0	- failed, ignore parse_data
 *	1	- parsed successfully
 */
int
http_reply_parse(char *data, int count, PXE_HTTP_PARSE_DATA *parse_data)
{
	if (strncmp(data, "HTTP/1.1", 8) != 0) /* wrong header */
		return (0);
	
	size_t result = 0;
	
	char *found = parse_size_t(data + 8, &result);
	parse_data->code = (uint16_t) result;
	
	if (found == NULL)
		return (0);	/* failed to parse response code */

	found = strstr(data, "Content-Length:");

	parse_data->size = PXE_HTTP_SIZE_UNKNOWN;
	
	if (found != NULL) /* parsing message body size */
		found = parse_size_t(found + strlen("Content-Length:"),
			    &parse_data->size);
	
	return (1);
}

/* http_get_header() - gets from socket data related to http header
 * in:
 *	socket		- socket descriptor
 *	data		- pointer to working buffer
 *	maxsize		- buffer size
 * 	found_result	- if not NULL, there stored pointer to end of header
 *	count_result	- if not NULL, received count stored
 * out:
 *	-1	- failed
 *	>=0	- success
 */
int
http_get_header(int socket, char *data, size_t maxsize,
		char **found_result, size_t *count_result)
{
	int		result = -1;
	size_t		count = 0;
	char		*found = NULL;
	char		ch = '\0';
	
	while (count < maxsize - 1) {
		result = pxe_recv(socket, &data[count], maxsize - 1 - count);
		
		if (result == -1) {	/* failed to recv */
#ifdef PXE_HTTP_DEBUG_HELL
			printf("http_get_header(): pxe_recv() failed\n");
#endif
			break;
		}
		
		if (result == 0)	/* nothing received yet */
			continue;

		/* make string ended with '\0' */
		ch = data[count + result];
		data[count + result] = '\0';
		
		/* searching end of reply */
		found = strstr(&data[count], "\r\n\r\n");

		/* restore char replaced by zero */
		data[count + result] = ch;
		
		count += result;
		
		if (found != NULL)
			break;
	}

	if (found_result)
		*found_result = found;
	
	if (count_result)
		*count_result = count;
	
	return (result);
}

#ifdef PXE_HTTPFS_CACHING
/* http_get_header2() - gets from socket data related to http header
 *			byte by byte.
 * in:
 *	socket		- socket descriptor
 *	data		- pointer to working buffer
 *	maxsize		- buffer size
 * 	found_result	- if not NULL, there stored pointer to end of header
 *	count_result	- if not NULL, received count stored
 * out:
 *	-1	- failed
 *	>=0	- success
 */
int
http_get_header2(int socket, char *data, size_t maxsize,
		char **found_result, size_t *count_result)
{
	int		result = -1;
	size_t		count = 0;
	char		*found = NULL;
	char		ch = '\0';
	
	while (count < maxsize - 1) {
		result = pxe_recv(socket, &data[count], 1);
		
		if (result == -1) {	/* failed to recv */
#ifdef PXE_HTTP_DEBUG_HELL
			printf("http_get_header2(): pxe_recv() failed\n");
#endif
			break;
		}
		
		if (result == 0)	/* nothing received yet */
			continue;

		count += 1;

		if (count < 4)	/* wait at least 4 bytes */
			continue;
		
		/* make string ended with '\0' */
		ch = data[count];
		data[count] = '\0';
		
		/* searching end of reply */
		found = strstr(&data[count - 4], "\r\n\r\n");

#ifdef PXE_HTTP_DEBUG_HELL
		if (found != NULL)
			printf("%s", data);
#endif			
		/* restore char replaced by zero */
		data[count] = ch;
		
		if (found != NULL)
			break;
	}

	if (found_result)
		*found_result = found;
	
	if (count_result)
		*count_result = count;
	
	return (result);
}

/* http_await() - await callback function for filling buffer
 * in:
 *      function        - await function
 *      try_number      - current number of try
 *      timeout         - current timeout from start of try
 *      data            - pointer to PXE_DNS_WAIT_DATA
 * out:
 *      PXE_AWAIT_ constants
 */
int
http_await(uint8_t function, uint16_t try_number, uint32_t timeout, void *data)
{
	PXE_HTTP_WAIT_DATA	*wait_data = (PXE_HTTP_WAIT_DATA *)data;
	uint16_t		space = 0;
	
	switch(function) {
     
		case PXE_AWAIT_NEWPACKETS:
			space = pxe_buffer_space(wait_data->buf);
			
			/* check, have we got enough? */
			if (wait_data->start_size - space >=
			    wait_data->wait_size)
				return (PXE_AWAIT_COMPLETED);

			/* check, is socket still working? */
			if (pxe_sock_state(wait_data->socket) !=
			    PXE_SOCKET_ESTABLISHED)
				return (PXE_AWAIT_BREAK);

			return (PXE_AWAIT_CONTINUE);
		default:
			break;
	}

	return (PXE_AWAIT_OK);
}
	
#endif /* PXE_HTTPFS_CACHING */

#ifdef PXE_MORE
/* pxe_fetch() - testing function, if size = from = 0, retrieve full file,
 *		otherwise partial 
 * in:
 *	server_name	- web server name
 *	filename	- path to file to fetch
 *	from		- offset in file (if supported by server)
 * 	size		- size of part to receive
 * out:
 *	0	- failed
 *	1	- success
 */
int
pxe_fetch(char *server_name, char *filename, off_t from, size_t size)
{
	const PXE_IPADDR *server = NULL;
	
	printf("pxe_fetch(): fetching http://%s:80/%s (%llu+%lu)\n",
	    server_name, filename, from, size);
	
	server = pxe_gethostbyname(server_name);
	
	if (server == NULL) {
		printf("pxe_fetch(): cannot resolve server name.\n");
		return (0);
	} else
		printf("pxe_fetch(): resolved as: %s\n", inet_ntoa(server->ip));
	
	int socket = pxe_socket();
	
	int result = pxe_connect(socket, server, 80, PXE_TCP_PROTOCOL);
	
	if (result == -1) {
		printf("pxe_fetch(): failed to connect.\n");
		pxe_close(socket);		
		return (0);
	}

	if ( (from == 0) && (size == 0) )
		snprintf(http_data, PXE_MAX_HTTP_HDRLEN,
		    "GET /%s HTTP/1.1\r\nHost: %s\r\nConnection: Close\r\n"
		    "User-Agent: pxe_http/0\r\n\r\n", filename, server_name);
	else
		snprintf(http_data, PXE_MAX_HTTP_HDRLEN,
		    "GET /%s HTTP/1.1\r\nHost: %s\r\nRange: bytes=%llu-%llu\r\n"
		    "Connection: Close\r\nUser-Agent: pxe_http/0\r\n\r\n",
		    filename, server_name, from, from + size - 1);

	size_t len = strlen(http_data);

	if (len != pxe_send(socket, http_data, len)) {
		printf("pxe_fetch(): failed to send request.\n");
		pxe_close(socket);
		return (0);
	}

	if (pxe_flush(socket) == -1) {
		printf("pxe_fetch(): failed to push request.\n");
		pxe_close(socket);
		return (0);
	}

	size_t	count = 0;
	char	*found = NULL;

	/* retrieve header */	
	result = http_get_header(socket, http_data, PXE_MAX_HTTP_HDRLEN - 1,
		    &found, &count);

	if (found == NULL) {	/* haven't found end of header */
		pxe_close(socket);
		return (0);
	}
	
	/* parse header */
	PXE_HTTP_PARSE_DATA	parse_data;
	pxe_memset(&parse_data, 0, sizeof(PXE_HTTP_PARSE_DATA));
	
	if (!http_reply_parse(http_data, count, &parse_data)) {
		pxe_close(socket);
		return (0);
	}

	printf("pxe_fetch(): response %u, length = %lu\n",
	    parse_data.code, parse_data.size);
	    
	delay(2000000);
	
	if ( (parse_data.code < 200) ||
	     (parse_data.code >= 300) )
	{
		printf("pxe_fetch(): failed to fetch.\n");
		pxe_close(socket);
		return (0);
	}

	http_data[count] = '\0';
	
	/* update counter, substruct header size */
	count -= (found - http_data) + 4;
			
	/* process body data */
	printf("%s", found + 4);
	
	while (1) {
		result = pxe_recv(socket, http_data, PXE_MAX_HTTP_HDRLEN - 1);
		
		if (result == -1)
			break;
		
		if (result == 0)
			continue;
		
		http_data[result] = '\0';
		
		printf("%s", http_data);
		
		count += result;
	}
	
	pxe_close(socket);
	printf("\npxe_fetch(): %lu of %lu byte(s) received.\n",
	    count, parse_data.size);
	
	return (1);
}
#endif

/* pxe_get() - gets portion of file
 * in:
 *	hh	- descriptor of file to read data from
 *	size	- size of part to read starting at from hh->offset
 * out:
 *	-1	- failed
 *	>=0	- actual bytes read
 */
int
pxe_get(PXE_HTTP_HANDLE *hh, size_t size, void *buffer)
{
	size_t size_to_get = (size > 0) ? size : hh->size;
	
#ifdef PXE_HTTP_DEBUG_HELL
	printf("pxe_get(): %s:%s:%llu+%lu(%lu:%lu) to 0x%x\n",
	    inet_ntoa(hh->addr.ip), hh->filename, hh->offset,
	    size_to_get, size, hh->size, hh->buf);
#endif

	if (hh->socket == -1) {
		printf("pxe_get(): invalid socket.\n");
		return (-1);
	}
	
	if (pxe_sock_state(hh->socket) != PXE_SOCKET_ESTABLISHED) {
		/* means connection was closed, due e.g. for Apache
		 * - MaxKeepAliveRequests exceeds for that connection
		 * - Waited between read attempts more than KeepAliveTimeout
		 *  or
		 * some other problem
		 * need to reestablish connection
		 */
		 
		 /* close socket gracefully */
		 pxe_close(hh->socket);
		 hh->socket = -1;
		 
		 if (!pxe_exists(hh)) {
#ifdef PXE_HTTP_DEBUG
			printf("pxe_get(): connection breaked.\n");
#endif
			return (-1);
		 }
		 /* reestablished, continue normal work */
	}

	if ( (size_to_get < PXE_HTTP_SIZE_UNKNOWN) &&
	     (size_to_get > 0))
	{
		snprintf(hh->buf, hh->bufsize, "GET %s%s HTTP/1.1\r\nHost: %s\r\n"
		    "Range: bytes=%llu-%llu\r\nConnection: keep-alive\r\n"
		    "Keep-Alive: 300\r\nUser-Agent: pxe_http/0\r\n\r\n",
		    rootpath, hh->filename, hh->servername, hh->offset,
		    hh->offset + size - 1);
	} else {
		/* asked zero bytes, or size of file is unknown */
		printf("pxe_get(): internal error\n");
		return (-1);
	}
    
	size_t len = strlen(hh->buf);

#ifdef PXE_HTTPFS_CACHING
	PXE_HTTP_WAIT_DATA	wait_data;	
	wait_data.buf = pxe_sock_recv_buffer(hh->socket);
	/* assuming recv_buffer always is not NULL */
	wait_data.start_size = pxe_buffer_space(wait_data.buf);
	wait_data.wait_size = (uint16_t)size;
	wait_data.socket = hh->socket;
#endif
	if (len != pxe_send(hh->socket, hh->buf, len)) {
		printf("pxe_get(): failed to send request.\n");
		return (-1);
	}

	if (pxe_flush(hh->socket) == -1) {
		printf("pxe_get(): failed to push request.\n");
		return (-1);
	}

	size_t	count = 0;
	char	*found = NULL;

	/* retrieve header */	
#ifndef PXE_HTTPFS_CACHING
	int result = http_get_header(hh->socket, hh->buf, hh->bufsize - 1,
		    &found, &count);
#else
	int result = http_get_header2(hh->socket, hh->buf, hh->bufsize - 1,
		    &found, &count);
#endif
	
	if (found == NULL) {	/* haven't found end of header */
		printf("pxe_get(): cannot find reply header.\n");
		return (-1);
	}
	
	/* parse header */
	PXE_HTTP_PARSE_DATA	parse_data;
	pxe_memset(&parse_data, 0, sizeof(PXE_HTTP_PARSE_DATA));
	
	if (!http_reply_parse(hh->buf, count, &parse_data)) {
		printf("pxe_get(): cannot parse reply header.\n");
		return (-1);
	}

	if ( (parse_data.code < 200) ||
	     (parse_data.code >= 300) )
	{
		printf("pxe_get(): failed to get (status: %u).\n",
		    parse_data.code);
		    
		return (-1);
	}

	/* update counter, substruct header size */
	count -= (found - hh->buf) + 4;

	/* process body data */
	if (count > size_to_get) { /* sanity check, never must be  */
#ifdef PXE_HTTP_DEBUG
		printf("pxe_get(): warning:1, count: %lu, size_to_get: %lu\n",
		    count, size);
#endif		
		count = size_to_get;
	}

#ifndef PXE_HTTPFS_CACHING
	pxe_memcpy((char *)found + 4, buffer, count);
	
	while (count < size_to_get) {
	
		result = pxe_recv(hh->socket, buffer + count,
			    size_to_get - count);
		
		if (result == -1)
			break;
		
		if (result == 0)
			continue;
		
		count += result;
	}
	
#ifdef PXE_HTTP_DEBUG_HELL
	printf("\npxe_get(): %lu of %lu byte(s) received.\n", count, size);
#endif

	if (count > size_to_get) { /* sanity check, never must be */
#ifdef PXE_HTTP_DEBUG
		printf("pxe_get(): warning:2, count: %lu, size_to_get: %lu\n",
		    count, size);
#endif
		count = size_to_get;
	}
#else
	/* waiting buffer space become filled by our data,
	 * main difference with normal processing, that we don't need read
	 * received data here. Main trick is just receive it, and store
	 * in buffer, httpfs code will read it when needed.
	 */
	
	if (wait_data.start_size - pxe_buffer_space(wait_data.buf) < size)
		/* receiving data, maximum wait 1 minute */
		pxe_await(http_await, 1, 60000, &wait_data);
	
	count = wait_data.start_size -
		pxe_buffer_space(wait_data.buf);

	hh->cache_size += count;

#ifdef PXE_HTTP_DEBUG
	printf("%lu read, cache: %lu \n", count, hh->cache_size);
#endif
	
#endif /* PXE_HTTPFS_CACHING */
	return (count);
}

#ifdef PXE_MORE
/* pxe_get_close() - gets portion of file, closing socket after getting
 * in:
 *	hh	- descriptor of file to read data from
 *	size	- size of part to read starting at from offset
 * out:
 *	-1	- failed
 *	>=0	- actual bytes read
 */
int
pxe_get_close(PXE_HTTP_HANDLE *hh, size_t size, void *buffer)
{
	size_t size_to_get = (size > 0) ? size : hh->size;
	
#ifdef PXE_DEBUG
	printf("pxe_get_close(): %s:%s:%llu+%lu(%lu:%lu) to 0x%x\n",
	    inet_ntoa(hh->addr.ip), hh->filename, hh->offset,
	    size_to_get, size, hh->size, hh->buf);
#endif
	int socket = pxe_socket();

	int result = pxe_connect(socket, &hh->addr, 80, PXE_TCP_PROTOCOL);
	
	if (result == -1) {
#ifdef PXE_DEBUG
		printf("pxe_get_close(): failed to connect.\n");
#endif
		pxe_close(socket);
		return (-1);
	}

	if ( (size_to_get < PXE_HTTP_SIZE_UNKNOWN) &&
	     (size_to_get > 0))
	{
		snprintf(hh->buf, hh->bufsize, "GET %s%s HTTP/1.1\r\nHost: %s\r\n"
		    "Range: bytes=%llu-%llu\r\nConnection: Close\r\n"
		    "User-Agent: pxe_http/0\r\n\r\n",
		    rootpath, hh->filename, hh->servername,
		    hh->offset, hh->offset + size - 1);
	} else {
		snprintf(hh->buf, hh->bufsize,
		    "GET %s%s HTTP/1.1\r\nHost: %s\r\nConnection: Close\r\n"
		    "User-Agent: pxe_http/0\r\n\r\n",
		    rootpath, hh->filename, hh->servername);
	}

	size_t len = strlen(hh->buf);

	if (len != pxe_send(socket, hh->buf, len)) {
		printf("pxe_get_close(): failed to send request.\n");
		pxe_close(socket);
		return (-1);
	}

	if (pxe_flush(socket) == -1) {
		printf("pxe_get_close(): failed to push request.\n");
		pxe_close(socket);
		return (-1);
	}

	size_t	count = 0;
	char	*found = NULL;

	/* retrieve header */	
	result = http_get_header(socket, hh->buf, hh->bufsize - 1,
		    &found, &count);

	if (found == NULL) {	/* haven't found end of header */
		printf("pxe_get_close(): cannot find reply header\n");
		pxe_close(socket);
		return (-1);
	}
	
	/* parse header */
	PXE_HTTP_PARSE_DATA	parse_data;
	pxe_memset(&parse_data, 0, sizeof(PXE_HTTP_PARSE_DATA));
	
	if (!http_reply_parse(hh->buf, count, &parse_data)) {
		printf("pxe_get_close(): cannot parse reply header\n");
		pxe_close(socket);
		return (-1);
	}

	if ( (parse_data.code < 200) ||
	     (parse_data.code >= 300) )
	{
		printf("pxe_get_close(): failed to get (status: %u).\n",
		    parse_data.code);
		    
		pxe_close(socket);
		return (-1);
	}

	/* update counter, substruct header size */
	count -= (found - hh->buf) + 4;

	/* process body data */
	if (count > size_to_get) { /* sanity check, never must be  */
#ifdef PXE_HTTP_DEBUG
		printf("pxe_get_close(): warning:1, count: %lu, "
		       "size_to_get: %lu\n", count, size);
#endif		
		count = size_to_get;
	}
	
	pxe_memcpy((char *)found + 4, buffer, count);
	
	while (count < size_to_get) {
		result = pxe_recv(socket, buffer + count, size_to_get - count);
		
		if (result == -1)
			break;
		
		if (result == 0)
			continue;
		
		count += result;
	}
	
	pxe_close(socket);

#ifdef PXE_DEBUG
	printf("\npxe_get_close(): %lu of %lu byte(s) received.\n",
	    count, size);
#endif

	if (count > size_to_get) { /* sanity check, never must be */
#ifdef PXE_HTTP_DEBUG
		printf("pxe_get_close(): warning:2, count: %lu, "
		       "size_to_get: %lu\n",  count, size);
#endif		       
		count = size_to_get;
	}
	
	return (count);
}
#endif

/* pxe_exists() - checks if file exists and gets it's size
 * in:
 *	hh	- descriptor of file to read data from
 * out:
 *	0	- failed, not exists
 *	1	- ok, file exists
 */
int
pxe_exists(PXE_HTTP_HANDLE *hh)
{
	int socket = pxe_socket();
	
	int result = pxe_connect(socket, &hh->addr, 80, PXE_TCP_PROTOCOL);
	
	if (result == -1) {
		pxe_close(socket);
		return (0);
	}

	snprintf(hh->buf, hh->bufsize,
	    "HEAD %s%s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\n"
	    "User-Agent: pxe_http/0\r\n\r\n", rootpath, hh->filename,
	    hh->servername);

	size_t len = strlen(hh->buf);

	if (len != pxe_send(socket, hh->buf, len)) {
		printf("pxe_exists(): failed to send request.\n");
		pxe_close(socket);
		return (0);
	}

	if (pxe_flush(socket) == -1) {
		printf("pxe_exists(): failed to push request.\n");
		pxe_close(socket);
		return (0);
	}

	size_t	count = 0;
	char	*found = NULL;

	/* retrieve header */	
	result = http_get_header(socket, hh->buf, hh->bufsize, &found, &count);

	if (found == NULL) {	/* haven't found end of header */
		pxe_close(socket);
		return (0);
	}
	
	/* parse header */
	PXE_HTTP_PARSE_DATA	parse_data;
	pxe_memset(&parse_data, 0, sizeof(PXE_HTTP_PARSE_DATA));
	
	if (!http_reply_parse(hh->buf, count, &parse_data)) {
		pxe_close(socket);
		return (0);
	}

	if ( (parse_data.code < 200) ||
	     (parse_data.code >= 300) )
	{
#ifdef PXE_HTTP_DEBUG
		printf("pxe_exists(): failed to get header (status: %u).\n",
		    parse_data.code);
#endif
		pxe_close(socket);
		return (0);
	}
	
	hh->socket = socket;
	hh->size = parse_data.size;

#ifdef PXE_HTTP_DEBUG
	printf("pxe_exists(): size = %lu bytes\n", hh->size);
#endif
	return (1);
}
