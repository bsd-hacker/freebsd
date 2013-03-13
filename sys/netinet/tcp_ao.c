/*
 * Copyright (c) 2013 Juniper Networks
 * All rights reserved.
 *
 * Written by Andre Oppermann <andre@FreeBSD.org>
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

/*
 * TCP-AO protects a TCP session and individual segments with a keyed hash
 * mechanism.  It is specified in RFC5925 and RFC5926.  It is intended to
 * eventually replace TCP-MD5 RFC2385.
 *
 * The implementation consists of 4 parts:
 *  1. the hash implementation to sign and verify segments.
 *  2. changes to the tcp input path to validate incoming segments,
 *     and changes to the tcp output path to sign outgoing segments.
 *  3. key management in the kernel and the exposed userland API.
 *  4. test programs to verify the correct operation.
 *
 * TODO:
 *  all of the above.
 *
 * Discussion:
 *  the key management can be done in two ways: via the ipsec key interface
 *  or through the setsockopt() api.  Analyse which one is better to handle
 *  in the kernel and for userspace applications.
 *
 *  legacy tcp-md5 can be brought and integrated into the tcp-ao framework.
 */

