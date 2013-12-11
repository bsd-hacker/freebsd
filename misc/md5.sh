#!/bin/sh

#
# Copyright (c) 2013 EMC Corp.
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

# "dd: /dev/md5: Input/output error" seen.
# kib@: kern_physio() detects EOF due to incorrect calculation
# of bio bio_resid after the bio_length was clipped by the 'excess' code
# in g_io_check()
# Test scenario by Stefan Hegnauer <stefan hegnauer gmx ch>
# Fixed in r259200

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

dd if=/dev/zero of=$diskimage bs=1k count=5k 2>&1 |
    egrep -v "records|transferred"
mdconfig -f $diskimage -u md$mdstart
newfs -U /dev/md$mdstart > /dev/null
(
	dd if=/dev/md$mdstart of=/dev/null 2>&1 || echo FAIL
) | egrep -v "records|transferred"
mdconfig -d -u $mdstart
rm -f $diskimage
