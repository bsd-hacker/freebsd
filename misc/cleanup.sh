#!/bin/sh

#
# Copyright (c) 2008-2013 Peter Holm <pho@FreeBSD.org>
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

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

MOUNTS=31
mount | grep -q "on $mntpoint " && umount -f $mntpoint
rm -rf ${mntpoint}/stressX*
rm -f /tmp/.snap/stress2* /var/.snap/stress2*
rm -rf /tmp/stressX.control $RUNDIR /tmp/misc.name
[ -d `dirname "$diskimage"` ] || mkdir -p `dirname "$diskimage"`
mkdir -p $RUNDIR
chmod 0777 $RUNDIR

for i in `jot $MOUNTS 0 | sort -nr` ""; do
	while mount | grep -q "on ${mntpoint}$i "; do
		fstat ${mntpoint}$i | sed 1d | awk '{print $3}' | xargs kill
		umount -f ${mntpoint}$i > /dev/null 2>&1
	done
done
# Delete the test mount points /mnt0 .. /mnt31
for i in `jot $MOUNTS 0`; do
	if ! mount | grep -q "on ${mntpoint}$i "; then
		[ -d ${mntpoint}$i ] && find ${mntpoint}$i -delete 2>/dev/null
		rm -rf ${mntpoint}$i > /dev/null 2>&1
	fi
done
find $mntpoint/* -delete 2>/dev/null
m=$mdstart
for i in `jot $MOUNTS`; do
	[ -c /dev/md$m ] &&  mdconfig -d -u $m
	m=$((m + 1))
done

# Delete $testuser's ipcs
ipcs | awk "\$5 ~/$testuser/ && \$6 ~/$testuser/ {print \"-\" \$1,\$2}" | \
    xargs -t ipcrm

