/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * Copyright (c) 2006 Marcel Moolenaar
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stand.h>
#include <string.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/linker.h>

#include <efi.h>
#include <efilib.h>

#include "bootstrap.h"
#include "libi386.h"

#include "x86_efi_copy.h"

/*
 * Return a 'boothowto' value corresponding to the kernel arguments in
 * (kargs) and any relevant environment variables.
 */
static struct 
{
	const char	*ev;
	int		mask;
} howto_names[] = {
	{ "boot_askname",	RB_ASKNAME},
	{ "boot_cdrom",		RB_CDROM},
	{ "boot_ddb",		RB_KDB},
	{ "boot_dfltroot",	RB_DFLTROOT},
	{ "boot_gdb",		RB_GDB},
	{ "boot_multicons",	RB_MULTIPLE},
	{ "boot_mute",		RB_MUTE},
	{ "boot_pause",		RB_PAUSE},
	{ "boot_serial",	RB_SERIAL},
	{ "boot_single",	RB_SINGLE},
	{ "boot_verbose",	RB_VERBOSE},
	{ NULL,	0}
};

static const char howto_switches[] = "aCdrgDmphsv";
static int howto_masks[] = {
	RB_ASKNAME, RB_CDROM, RB_KDB, RB_DFLTROOT, RB_GDB, RB_MULTIPLE,
	RB_MUTE, RB_PAUSE, RB_SERIAL, RB_SINGLE, RB_VERBOSE
};

int
bi_getboothowto(char *kargs)
{
	const char *sw;
	char *opts;
	int howto, i;

	howto = 0;

	/* Get the boot options from the environment first. */
	for (i = 0; howto_names[i].ev != NULL; i++) {
		if (getenv(howto_names[i].ev) != NULL)
			howto |= howto_names[i].mask;
	}

	/* Parse kargs */
	if (kargs == NULL)
		return (howto);

	opts = strchr(kargs, '-');
	while (opts != NULL) {
		while (*(++opts) != '\0') {
			sw = strchr(howto_switches, *opts);
			if (sw == NULL)
				break;
			howto |= howto_masks[sw - howto_switches];
		}
		opts = strchr(opts, '-');
	}

	return (howto);
}

/*
 * Copy the environment into the load area starting at (addr).
 * Each variable is formatted as <name>=<value>, with a single nul
 * separating each variable, and a double nul terminating the environment.
 */
vm_offset_t
bi_copyenv(vm_offset_t start)
{
	struct env_var *ep;
	vm_offset_t addr, last;
	size_t len;

	addr = last = start;

	/* Traverse the environment. */
	for (ep = environ; ep != NULL; ep = ep->ev_next) {
		len = strlen(ep->ev_name);
		if (x86_efi_copyin(ep->ev_name, addr, len) != len)
			break;
		addr += len;
		if (x86_efi_copyin("=", addr, 1) != 1)
			break;
		addr++;
		if (ep->ev_value != NULL) {
			len = strlen(ep->ev_value);
			if (x86_efi_copyin(ep->ev_value, addr, len) != len)
				break;
			addr += len;
		}
		if (x86_efi_copyin("", addr, 1) != 1)
			break;
		last = ++addr;
	}

	if (x86_efi_copyin("", last++, 1) != 1)
		last = start;
	return(last);
}
