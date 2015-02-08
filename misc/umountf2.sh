#!/bin/sh

#
# Copyright (c) 2008 Peter Holm <pho@FreeBSD.org>
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

# Test scenario by kris@freebsd.org

# Test problems with "umount -f and fsx. Results in a "KDB: enter: watchdog timeout"

# http://people.freebsd.org/~pho/stress/log/kostik745.txt
# Fixed by r275743

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

fsxc=`find -x /usr/src -name fsx.c | tail -1`
[ -z "$fsxc" ] && fsxc=`find -x / -name fsx.c | tail -1`
[ -z "$fsxc" ] && exit

cc -o /tmp/fsx $fsxc

D=$diskimage
dede $D 1m 1k || exit 1

mount | grep "$mntpoint" | grep md${mdstart}$part > /dev/null && umount $mntpoint
mdconfig -l | grep md${mdstart} > /dev/null &&  mdconfig -d -u $mdstart

mdconfig -a -t vnode -f $D -u $mdstart
bsdlabel -w md$mdstart auto
newfs md${mdstart}$part > /dev/null 2>&1
mount /dev/md${mdstart}$part $mntpoint
sleep 5
for i in `jot 100`; do
	/tmp/fsx -S $i -q ${mntpoint}/xxx$i > /dev/null &
done
sleep 30
umount -f $mntpoint &
for i in `jot 10`; do
	sleep 30
	pgrep -q fsx || break
done
pgrep -q fsx && echo FAIL && pkill fsx
sleep 5
wait
mdconfig -d -u $mdstart
rm -f $D /tmp/fsx
