/*-
 * Copyright (c) 2011 Andrew Thompson <thompsa@FreeBSD.org>
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
 * $FreeBSD$
 *
 */

#ifndef	__ONEWIREVAR_H__
#define	__ONEWIREVAR_H__

#include <sys/param.h>

#define ONEWIRE_MAXDEVS		16
#define ONEWIRE_SCANTIME	30

struct onewire_attach_args {
	void		*oa_onewire;
	uint64_t	oa_rom;
};

struct onewire_matchfam {
	int		om_type;
	const char	*om_desc;
};

void		onewire_lock(device_t);
void		onewire_unlock(device_t);

int		onewire_reset(device_t);
int		onewire_bit(device_t, int);
int		onewire_read_byte(device_t);
void		onewire_write_byte(device_t, int);
void		onewire_read_block(device_t, void *, int);
void		onewire_write_block(device_t, const void *, int);
int		onewire_triplet(device_t, int);
void		onewire_matchrom(device_t, uint64_t);
int		onewire_search(device_t, uint64_t *, int, uint64_t);

int		onewire_crc(const void *buf, int len);
uint16_t	onewire_crc16(const void *buf, int len);
const char *	onewire_famname(int type);
const struct onewire_matchfam *onewire_matchbyfam(struct onewire_attach_args *,
                        const struct onewire_matchfam *, int);

extern driver_t		owbus_driver;
extern devclass_t	owbus_devclass;
extern driver_t		owbb_driver;
extern devclass_t	owbb_devclass;

#endif	/* __ONEWIREVAR_H__ */
