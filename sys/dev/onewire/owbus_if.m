#-
# Copyright (c) 2011 Andrew Thompson <thompsa@FreeBSD.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#

#include <sys/bus.h>
#include <dev/onewire/onewirevar.h>

INTERFACE owbus;

#
# Lock the onewire bus
#
METHOD void lock_bus {
	device_t busdev;
};

#
# Unlock the onewire bus
#
METHOD void unlock_bus {
	device_t busdev;
};

#
# 
#
METHOD int reset {
	device_t busdev;
};

METHOD int bit {
	device_t busdev;
	int value;
};

METHOD int read_byte {
	device_t busdev;
};

METHOD void write_byte {
	device_t busdev;
	int value;
};

METHOD void read_block {
	device_t busdev;
	void *buf;
	int len;
};

METHOD void write_block {
	device_t busdev;
	const void *buf;
	int len;
};

METHOD int triplet {
	device_t busdev;
	int dir;
};

METHOD void matchrom {
	device_t busdev;
	uint64_t rom;
};

METHOD int search {
	device_t busdev;
	uint64_t *buf;
	int size;
	uint64_t startrom;
};
