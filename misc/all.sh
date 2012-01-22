#!/bin/sh

#
# Copyright (c) 2008-2009, 2012 Peter Holm <pho@FreeBSD.org>
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

# Run all the scripts in stress2/misc, except these known problems:

# Start of list	Known problems						Seen

# backingstore.sh
#		g_vfs_done():md6a[WRITE(offset=...)]error = 28		20111220
# backingstore2.sh
#		panic: 43 vncache entries remaining			20111220
# backingstore3.sh
#		g_vfs_done():md6a[WRITE(offset=...)]error = 28		20111230
# datamove.sh	Deadlock (ufs)						20111216
# datamove2.sh	Deadlock (ufs)						20111220
# datamove3.sh	Deadlock (ufs)						20111221
# dfull.sh	umount stuck in "mount drain"				20111227
# fts.sh	Deadlock seen, possibly due to low v_free_count		20120105
# mkfifo.sh	umount stuck in suspfs					20111224
# mkfifo2c.sh	panic: ufsdirhash_newblk: bad offset			20111225
# newfs.sh	Memory modified after free. ... used by inodedep	20111217
# newfs2.sh	umount stuck in ufs					20111226
# pmc.sh	NMI ... going to debugger				20111217
# snap5-1.sh	mksnap_ffs deadlock					20111218
# quota3.sh	panic: softdep_deallocate_dependencies: unrecovered ...	20111222
# quota6.sh	panic: softdep_deallocate_dependencies: unrecovered ...	20111219
# snap3.sh	mksnap_ffs stuck in snaprdb				20111226
# snap5.sh	mksnap_ffs stuck in getblk				20111224
# suj11.sh	panic: ufsdirhash_newblk: bad offset			20120118
# suj23.sh	panic: Bad link elm 0xc9d00e00 next->prev != elm	20111216
# tmpfs6.sh	watchdogd fired. Test stuck in pgrbwt			20111219
# trim3.sh	watchdog timeout					20111225
# umountf3.sh	KDB: enter: watchdog timeout				20111217
# unionfs.sh	insmntque: mp-safe fs and non-locked vp is not ...	20111217
# unionfs2.sh	insmntque: mp-safe fs and non-locked vp is not ...	20111219
# unionfs3.sh	insmntque: mp-safe fs and non-locked vp is not ...	20111216

# Test not to run for other reasons:

# fuzz.sh	A know issue
# syscall.sh	OK, but runs for a very long time
# syscall2.sh	OK, but runs for a very long time
# vunref.sh	No problems ever seen
# vunref2.sh	No problems ever seen

# End of list

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

args=`getopt acn $*`
[ $? -ne 0 ] && echo "Usage $0 [-a] [-c] [-n] [tests]" && exit 1
set -- $args
for i; do
	case "$i" in
	-a)	all=1		# Run all tests
		shift
		;;
	-c)	rm -f .all.last	# Clear last know test
		shift
		;;
	-n)	noshuffle=1	# Do not shuffle the list of tests
		shift
		;;
	--)
		shift
		break
		;;
	esac
done

> .all.log
find . -maxdepth 1 -name .all.last -mtime +12h -delete
touch .all.last
chmod 555 .all.last .all.log
while true; do
	exclude=`sed -n '/^# Start of list/,/^# End of list/p' < $0 | \
		grep "\.sh" | awk '{print $2}'`
	list=`ls *.sh | egrep -v "all\.sh|cleanup\.sh"`
	[ $# -ne 0 ] && list=$*

	if [ -n "$noshuffle" -a $# -eq 0 ]; then
		last=`cat .all.last`
		if [ -n "$last" ]; then
			list=`echo "$list" | sed "1,/$last/d"`
			echo "Resuming test at `echo "$list" | head -1`"
		fi
	fi
	[ -n "$noshuffle" ] ||
		list=`echo $list | tr ' ' '\n' | random -w | tr '\n' ' '`

	lst=""
	for i in $list; do
		[ -z "$all" ] && echo $exclude | grep -q $i && continue
		lst="$lst $i"
	done
	[ -z "$lst" ] && exit

	for i in $lst; do
		echo $i > .all.last
		./cleanup.sh
		echo "`date '+%Y%m%d %T'` all: $i" | tee /dev/tty >> .all.log
		logger "Starting test all: $i"
		sync;sync;sync
		./$i
	done
done
