#!/usr/sbin/dtrace -qs

/*-
 * Copyright (c) 2008 Alexander Leidinger <netchild@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Check if the emul lock is correctly acquired/released:
 *  - no recursive locking
 *  - no unlocking of already unlocked one
 *
 * Print stacktrace if the emul_lock is longer locked than about 10sec or more.
 */

linuxulator*::emul_locked
/check[probeprov, arg0] > 0/
{
	printf("ERROR: recursive lock of emul_lock (%p),", arg0);
	printf("       or missing SDT probe in kernel. Stack trace follows:");
	stack();
}

linuxulator*::emul_locked
{
	++check[probeprov, arg0];

	ts = timestamp;
	spec = speculation();
	printf("Stacktrace of last lock operation of the emul_lock:\n");
	stack();
}

linuxulator*::emul_unlock
/check[probeprov, arg0] == 0/
{
	printf("ERROR: unlock attemt of unlocked emul_lock (%p),", arg0);
	printf("       missing SDT probe in kernel, or dtrace program started");
	printf("       while the emul_lock was already held (race condition).");
	printf("       Stack trace follows:");
	stack();
}

linuxulator*::emul_unlock
{
	discard(spec);
	spec = 0;
	--check[probeprov, arg0];
}

tick-10s
/spec != 0 && timestamp - ts >= 9999999000/
{
	commit(spec);
	spec = 0;
}
