/*-
 * Copyright (c) 2015-2016 Landon Fuller <landonf@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/endian.h>

#ifdef _KERNEL

#include <sys/param.h>
#include <sys/ctype.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#else /* !_KERNEL */

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#endif /* _KERNEL */

#include "bhnd_nvram_private.h"

#include "bhnd_nvram_datavar.h"

#include "bhnd_nvram_data_bcmreg.h"	/* for BCM_NVRAM_MAGIC */

/**
 * Broadcom "Board Text" data class.
 *
 * This format is used to provide external NVRAM data for some
 * fullmac WiFi devices, and as an input format when programming
 * NVRAM/SPROM/OTP.
 */

struct bhnd_nvram_btxt {
	struct bhnd_nvram_data	 nv;	/**< common instance state */
	struct bhnd_nvram_io	*data;	/**< memory-backed board text data */
	size_t			 count;	/**< variable count */
};

BHND_NVRAM_DATA_CLASS_DEFN(btxt, "Broadcom Board Text",
    sizeof(struct bhnd_nvram_btxt))

/** Minimal identification header */
union bhnd_nvram_btxt_ident {
	uint32_t	bcm_magic;
	char		btxt[8];
};

static size_t	bhnd_nvram_btxt_io_offset(struct bhnd_nvram_btxt *btxt,
					  void *cookiep);

static int	bhnd_nvram_btxt_entry_len(struct bhnd_nvram_io *io,
		    size_t offset, size_t *line_len, size_t *env_len);
static int	bhnd_nvram_btxt_seek_next(struct bhnd_nvram_io *io,
		    size_t *offset);
static int	bhnd_nvram_btxt_seek_eol(struct bhnd_nvram_io *io,
		    size_t *offset);

static int
bhnd_nvram_btxt_probe(struct bhnd_nvram_io *io)
{
	union bhnd_nvram_btxt_ident	ident;
	char				c;
	int				error;

	/* Look at the initial header for something that looks like 
	 * an ASCII board text file */
	if ((error = bhnd_nvram_io_read(io, 0x0, &ident, sizeof(ident))))
		return (error);

	/* The BCM NVRAM format uses a 'FLSH' little endian magic value, which
	 * shouldn't be interpreted as BTXT */
	if (le32toh(ident.bcm_magic) == BCM_NVRAM_MAGIC)
		return (ENXIO);

	/* Don't match on non-ASCII/non-printable data */
	for (size_t i = 0; i < nitems(ident.btxt); i++) {
		c = ident.btxt[i];
		if (!bhnd_nv_isprint(c))
			return (ENXIO);
	}

	/* The first character should either be a valid key char (alpha),
	 * whitespace, or the start of a comment ('#') */
	c = ident.btxt[0];
	if (!bhnd_nv_isspace(c) && !bhnd_nv_isalpha(c) && c != '#')
		return (ENXIO);

	/* We assert a low priority, given that we've only scanned an
	 * initial few bytes of the file. */
	return (BHND_NVRAM_DATA_PROBE_MAYBE);
}

/**
 * Initialize @p btxt with the provided board text data mapped by @p src.
 * 
 * @param btxt A newly allocated data instance.
 */
static int
bhnd_nvram_btxt_init(struct bhnd_nvram_btxt *btxt, struct bhnd_nvram_io *src)
{
	const void		*ptr;
	const char		*name, *value;
	size_t			 name_len, value_len;
	size_t			 line_len, env_len;
	size_t			 io_offset, io_size, str_size;
	int			 error;

	BHND_NV_ASSERT(btxt->data == NULL, ("btxt data already allocated"));
	
	if ((btxt->data = bhnd_nvram_iobuf_copy(src)) == NULL)
		return (ENOMEM);

	io_size = bhnd_nvram_io_getsize(btxt->data);
	io_offset = 0;

	/* Fetch a pointer mapping the entirity of the board text data */
	error = bhnd_nvram_io_read_ptr(btxt->data, 0x0, &ptr, io_size, NULL);
	if (error)
		return (error);

	/* Determine the actual size, minus any terminating NUL. We
	 * parse NUL-terminated C strings, but do not include NUL termination
	 * in our internal or serialized representations */
	str_size = strnlen(ptr, io_size);

	/* If the terminating NUL is not found at the end of the buffer,
	 * this is BCM-RAW or other NUL-delimited NVRAM format. */
	if (str_size < io_size && str_size + 1 < io_size)
		return (EINVAL);

	/* Adjust buffer size to account for NUL termination (if any) */
	io_size = str_size;
	if ((error = bhnd_nvram_io_setsize(btxt->data, io_size)))
		return (error);

	/* Process the buffer */
	btxt->count = 0;
	while (io_offset < io_size) {
		const void	*envp;

		/* Seek to the next key=value entry */
		if ((error = bhnd_nvram_btxt_seek_next(btxt->data, &io_offset)))
			return (error);

		/* Determine the entry and line length */
		error = bhnd_nvram_btxt_entry_len(btxt->data, io_offset,
		    &line_len, &env_len);
		if (error)
			return (error);
	
		/* EOF? */
		if (env_len == 0) {
			BHND_NV_ASSERT(io_offset == io_size,
		           ("zero-length record returned from "
			    "bhnd_nvram_btxt_seek_next()"));
			break;
		}

		/* Fetch a pointer to the line start */
		error = bhnd_nvram_io_read_ptr(btxt->data, io_offset, &envp,
		    env_len, NULL);
		if (error)
			return (error);

		/* Parse the key=value string */
		error = bhnd_nvram_parse_env(envp, env_len, '=', &name,
		    &name_len, &value, &value_len);
		if (error) {
			return (error);
		}

		/* Insert a '\0' character, replacing the '=' delimiter and
		 * allowing us to vend references directly to the variable
		 * name */
		error = bhnd_nvram_io_write(btxt->data, io_offset+name_len,
		    &(char){'\0'}, 1);
		if (error)
			return (error);

		/* Add to variable count */
		btxt->count++;

		/* Advance past EOL */
		io_offset += line_len;
	}

	return (0);
}

static int
bhnd_nvram_btxt_new(struct bhnd_nvram_data *nv, struct bhnd_nvram_io *io)
{
	struct bhnd_nvram_btxt	*btxt;
	int			 error;

	/* Allocate and initialize the BTXT data instance */
	btxt = (struct bhnd_nvram_btxt *)nv;

	/* Parse the BTXT input data and initialize our backing
	 * data representation */
	if ((error = bhnd_nvram_btxt_init(btxt, io))) {
		bhnd_nvram_btxt_free(nv);
		return (error);
	}

	return (0);
}

static void
bhnd_nvram_btxt_free(struct bhnd_nvram_data *nv)
{
	struct bhnd_nvram_btxt *btxt = (struct bhnd_nvram_btxt *)nv;
	if (btxt->data != NULL)
		bhnd_nvram_io_free(btxt->data);
}

size_t
bhnd_nvram_btxt_count(struct bhnd_nvram_data *nv)
{
	struct bhnd_nvram_btxt *btxt = (struct bhnd_nvram_btxt *)nv;
	return (btxt->count);
}

static int
bhnd_nvram_btxt_size(struct bhnd_nvram_data *nv, size_t *size)
{
	struct bhnd_nvram_btxt *btxt = (struct bhnd_nvram_btxt *)nv;

	/* The serialized form will be identical in length
	 * to our backing buffer representation */
	*size = bhnd_nvram_io_getsize(btxt->data);
	return (0);
}

static int
bhnd_nvram_btxt_serialize(struct bhnd_nvram_data *nv, void *buf, size_t *len)
{
	struct bhnd_nvram_btxt	*btxt;
	size_t			 limit;
	int			 error;

	btxt = (struct bhnd_nvram_btxt *)nv;

	limit = *len;

	/* Provide actual output size */
	if ((error = bhnd_nvram_data_size(nv, len)))
		return (error);

	if (buf == NULL) {
		return (0);
	} else if (limit < *len) {
		return (ENOMEM);
	}	

	/* Copy our internal representation to the output buffer */
	if ((error = bhnd_nvram_io_read(btxt->data, 0x0, buf, *len)))
		return (error);

	/* Restore the original key=value format, rewriting all '\0'
	 * key\0value delimiters back to '=' */
	for (char *p = buf; (size_t)(p - (char *)buf) < *len; p++) {
		if (*p == '\0')
			*p = '=';
	}

	return (0);
}

static uint32_t
bhnd_nvram_btxt_caps(struct bhnd_nvram_data *nv)
{
	return (BHND_NVRAM_DATA_CAP_READ_PTR|BHND_NVRAM_DATA_CAP_DEVPATHS);
}

static void *
bhnd_nvram_btxt_find(struct bhnd_nvram_data *nv, const char *name)
{
	return (bhnd_nvram_data_generic_find(nv, name));
}

static const char *
bhnd_nvram_btxt_next(struct bhnd_nvram_data *nv, void **cookiep)
{
	struct bhnd_nvram_btxt	*btxt;
	const void		*nptr;
	size_t			 io_offset, io_size;
	int			 error;

	btxt = (struct bhnd_nvram_btxt *)nv;

	io_size = bhnd_nvram_io_getsize(btxt->data);
	io_offset = bhnd_nvram_btxt_io_offset(btxt, *cookiep);

	/* Already at EOF? */
	if (io_offset == io_size)
		return (NULL);

	/* Seek to the next entry (if any) */
	if ((error = bhnd_nvram_btxt_seek_eol(btxt->data, &io_offset))) {
		BHND_NV_LOG("unexpected error in seek_eol(): %d\n", error);
		return (NULL);
	}

	if ((error = bhnd_nvram_btxt_seek_next(btxt->data, &io_offset))) {
		BHND_NV_LOG("unexpected error in seek_next(): %d\n", error);
		return (NULL);
	}

	/* Provide the new cookie for this offset */
	if (io_offset > UINTPTR_MAX) {
		BHND_NV_LOG("io_offset > UINPTR_MAX!\n");
		return (NULL);
	}

	*cookiep = (void *)(uintptr_t)io_offset;

	/* Hit EOF? */
	if (io_offset == io_size)
		return (NULL);

	/* Fetch the name pointer; it must be at least 1 byte long */
	error = bhnd_nvram_io_read_ptr(btxt->data, io_offset, &nptr, 1, NULL);
	if (error) {
		BHND_NV_LOG("unexpected error in read_ptr(): %d\n", error);
		return (NULL);
	}

	/* Return the name pointer */
	return (nptr);
}

static int
bhnd_nvram_btxt_getvar(struct bhnd_nvram_data *nv, void *cookiep, void *buf,
    size_t *len, bhnd_nvram_type type)
{
	return (bhnd_nvram_data_generic_rp_getvar(nv, cookiep, buf, len, type));
}

const void *
bhnd_nvram_btxt_getvar_ptr(struct bhnd_nvram_data *nv, void *cookiep,
    size_t *len, bhnd_nvram_type *type)
{
	struct bhnd_nvram_btxt	*btxt;
	const void		*eptr;
	const char		*vptr;
	size_t			 io_offset, io_size;
	size_t			 line_len, env_len;
	int			 error;
	
	btxt = (struct bhnd_nvram_btxt *)nv;
	
	io_size = bhnd_nvram_io_getsize(btxt->data);
	io_offset = bhnd_nvram_btxt_io_offset(btxt, cookiep);

	/* At EOF? */
	if (io_offset == io_size)
		return (NULL);

	/* Determine the entry length */
	error = bhnd_nvram_btxt_entry_len(btxt->data, io_offset, &line_len,
	    &env_len);
	if (error) {
		BHND_NV_LOG("unexpected error in entry_len(): %d\n", error);
		return (NULL);
	}

	/* Fetch the entry's value pointer and length */
	error = bhnd_nvram_io_read_ptr(btxt->data, io_offset, &eptr, env_len,
	    NULL);
	if (error) {
		BHND_NV_LOG("unexpected error in read_ptr(): %d\n", error);
		return (NULL);
	}

	error = bhnd_nvram_parse_env(eptr, env_len, '\0', NULL, NULL, &vptr,
	    len);
	if (error) {
		BHND_NV_LOG("unexpected error in parse_env(): %d\n", error);
		return (NULL);
	}

	/* Type is always CSTR */
	*type = BHND_NVRAM_TYPE_STRING;

	return (vptr);
}

static const char *
bhnd_nvram_btxt_getvar_name(struct bhnd_nvram_data *nv, void *cookiep)
{
	struct bhnd_nvram_btxt	*btxt;
	const void		*ptr;
	size_t			 io_offset, io_size;
	int			 error;
	
	btxt = (struct bhnd_nvram_btxt *)nv;
	
	io_size = bhnd_nvram_io_getsize(btxt->data);
	io_offset = bhnd_nvram_btxt_io_offset(btxt, cookiep);

	/* At EOF? */
	if (io_offset == io_size)
		BHND_NV_PANIC("invalid cookiep: %p", cookiep);

	/* Variable name is found directly at the given offset; trailing
	 * NUL means we can assume that it's at least 1 byte long */
	error = bhnd_nvram_io_read_ptr(btxt->data, io_offset, &ptr, 1, NULL);
	if (error)
		BHND_NV_PANIC("unexpected error in read_ptr(): %d\n", error);

	return (ptr);
}

/* Convert cookie back to an I/O offset */
static size_t
bhnd_nvram_btxt_io_offset(struct bhnd_nvram_btxt *btxt, void *cookiep)
{
	size_t		io_size;
	uintptr_t	cval;

	io_size = bhnd_nvram_io_getsize(btxt->data);
	cval = (uintptr_t)cookiep;

	BHND_NV_ASSERT(cval < SIZE_MAX, ("cookie > SIZE_MAX)"));
	BHND_NV_ASSERT(cval <= io_size, ("cookie > io_size)"));

	return ((size_t)cval);
}

/* Determine the entry length and env 'key=value' string length of the entry
 * at @p offset */
static int
bhnd_nvram_btxt_entry_len(struct bhnd_nvram_io *io, size_t offset,
    size_t *line_len, size_t *env_len)
{
	const uint8_t	*baseptr, *p;
	const void	*rbuf;
	size_t		 nbytes;
	int		 error;

	/* Fetch read buffer */
	if ((error = bhnd_nvram_io_read_ptr(io, offset, &rbuf, 0, &nbytes)))
		return (error);

	/* Find record termination (EOL, or '#') */
	p = rbuf;
	baseptr = rbuf;
	while ((size_t)(p - baseptr) < nbytes) {
		if (*p == '#' || *p == '\n' || *p == '\r')
			break;

		p++;
	}

	/* Got line length, now trim any trailing whitespace to determine
	 * actual env length */
	*line_len = p - baseptr;
	*env_len = *line_len;

	for (size_t i = 0; i < *line_len; i++) {
		char c = baseptr[*line_len - i - 1];
		if (!bhnd_nv_isspace(c))
			break;

		*env_len -= 1;
	}

	return (0);
}

/* Seek past the next line ending (\r, \r\n, or \n) */
static int
bhnd_nvram_btxt_seek_eol(struct bhnd_nvram_io *io, size_t *offset)
{
	const uint8_t	*baseptr, *p;
	const void	*rbuf;
	size_t		 nbytes;
	int		 error;

	/* Fetch read buffer */
	if ((error = bhnd_nvram_io_read_ptr(io, *offset, &rbuf, 0, &nbytes)))
		return (error);

	baseptr = rbuf;
	p = rbuf;
	while ((size_t)(p - baseptr) < nbytes) {
		char c = *p;

		/* Advance to next char. The next position may be EOF, in which
		 * case a read will be invalid */
		p++;

		if (c == '\r') {
			/* CR, check for optional LF */
			if ((size_t)(p - baseptr) < nbytes) {
				if (*p == '\n')
					p++;
			}

			break;
		} else if (c == '\n') {
			break;
		}
	}

	/* Hit newline or EOF */
	*offset += (p - baseptr);
	return (0);
}

/* Seek to the next valid non-comment line (or EOF) */
static int
bhnd_nvram_btxt_seek_next(struct bhnd_nvram_io *io, size_t *offset)
{
	const uint8_t	*baseptr, *p;
	const void	*rbuf;
	size_t		 nbytes;
	int		 error;

	/* Fetch read buffer */
	if ((error = bhnd_nvram_io_read_ptr(io, *offset, &rbuf, 0, &nbytes)))
		return (error);

	/* Skip leading whitespace and comments */
	baseptr = rbuf;
	p = rbuf;
	while ((size_t)(p - baseptr) < nbytes) {
		char c = *p;

		/* Skip whitespace */
		if (bhnd_nv_isspace(c)) {
			p++;
			continue;
		}

		/* Skip entire comment line */
		if (c == '#') {
			size_t line_off = *offset + (p - baseptr);
	
			if ((error = bhnd_nvram_btxt_seek_eol(io, &line_off)))
				return (error);

			p = baseptr + (line_off - *offset);
			continue;
		}

		/* Non-whitespace, non-comment */
		break;
	}

	*offset += (p - baseptr);
	return (0);
}
