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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/* Report error conditions. */

linuxulator*:emul:proc_exit:child_clear_tid_error,
linuxulator*:emul:proc_exit:futex_failed,
linuxulator*:emul:linux_schedtail:copyout_error
{
	printf("ERROR: %s in %s:%s:%s\n", probename, probeprov, probemod, probefunc);
	stack();
	/* ustack(); */	/* needs to be enabled when PID tracing is available in FreeBSD dtrace */
}

linuxulator*:util:linux_driver_get_name_dev:nullcall,
linuxulator*:util:linux_driver_get_major_minor:nullcall
{
	printf("WARNING: %s:%s:%s:%s in application %s, maybe an application error?\n", probename, probeprov, probemod, probefunc, execname);
	stack();
	/* ustack(); */ /* needs to be enabled when PID tracing is available in FreeBSD dtrace */
}

linuxulator*:util:linux_driver_get_major_minor:notfound
{
	printf("WARNING: Application %s failed to find %s in %s:%s:%s, this may or may not be a problem.\n", execname, arg0, probename, probeprov, probemod);
	stack();
	/* ustack(); */ /* needs to be enabled when PID tracing is available in FreeBSD dtrace */
}

