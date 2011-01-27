#!/bin/sh

#
# Copyright (c) 2011 Peter Holm <pho@FreeBSD.org>
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
# $FreeBSD: projects/stress2/misc/sendfile.sh 199142 2009-11-10 16:47:48Z pho $
#

# Deadlock scenario based on kern/154228, fixed in r217880.

. ../default.cfg

size="2g"
m=$((mdstart + 1))
mount | grep /media    | grep -q /dev/md && umount -f /media
mount | grep $mntpoint | grep -q /dev/md && umount -f $mntpoint
mdconfig -l | grep -q md$mdstart &&  mdconfig -d -u $mdstart
mdconfig -l | grep -q md$m       &&  mdconfig -d -u $m
mdconfig -a -t swap -s $size -u $mdstart || exit 1

gjournal load
gjournal label -s $((200 * 1024 * 1024)) md$mdstart
sleep .5
newfs -J /dev/md$mdstart.journal > /dev/null
mount -o async /dev/md$mdstart.journal $mntpoint

here=`pwd`
cd $mntpoint
truncate -s 1g image
mdconfig -a -t vnode -f image -u $m
bsdlabel -w md$m auto
newfs md${m}$part > /dev/null
mount /dev/md${m}$part /media
# dd will suspend in wdrain
dd if=/dev/zero of=/media/zero bs=1M 2>&1 | egrep -v "records|transferred"
while mount | grep /media | grep -q /dev/md; do
	umount /media || sleep 1
done
mdconfig -d -u $m
cd $here

gjournal sync
while mount | grep $mntpoint | grep -q /dev/md; do
	umount $mntpoint || sleep 1
done
gjournal stop md$mdstart
gjournal unload
mdconfig -d -u $mdstart
