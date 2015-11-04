#!/bin/sh

#
# Copyright (c) 2008-2009, 2012-13 Peter Holm <pho@FreeBSD.org>
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

# altbufferflushes.sh snapshots + disk full == know problem		20130617
# backingstore.sh
#		g_vfs_done():md6a[WRITE(offset=...)]error = 28		20111220
# backingstore2.sh
#		panic: 43 vncache entries remaining			20111220
# backingstore3.sh
#		g_vfs_done():md6a[WRITE(offset=...)]error = 28		20111230
# bio.sh	WiP							20150925
# core5.sh	May slow down if vnode cache is full			20150714
# crossmp4.sh	Known nullfs issue					20150523
# crossmp6.sh	Known lockd issue					20150625
# dfull.sh	umount stuck in "mount drain"				20111227
# ext2fs.sh	Deadlock						20120510
# ext2fs2.sh	panic							20140716
# fuse.sh	Memory corruption seen in log file kostik734.txt	20141114
# fuse2.sh	Deadlock seen						20121129
# fuse3.sh	Deadlock seen						20141120
# gbde.sh	panic: handle_written_inodeblock: Invalid link count...	20131128
# gjournal.sh	kmem_malloc(131072): kmem_map too small			20120626
# gjournal2.sh
# gjournal3.sh	panic: Journal overflow					20130729
# lockf5.sh	Page fault						20150622
# md2.sh	panic: ufs_dirbad: /mnt: bad dir ino ...: mangled entry	20150227
# maxmemdom.sh	Panic: vm_page_alloc: missing page			20151029
# maxproc.sh	WiP							20150329
# memguard.sh	Waiting for fix commit
# memguard2.sh	Waiting for fix commit
# memguard3.sh	Waiting for fix commit
# mkfifo.sh	Page fault in softdep_count_dependencies+0x27 seen	20150524
# mmap18.sh	panic: vm_fault_copy_entry: main object missing page	20141015
# mmap21.sh	rangelock issue?					20150326
# msdos5.sh	Panic: Freeing unused sector ...			20141118
# newfs4.sh	Deadlock seen						20150906
# nfs10.sh	Double fault						20151013
# pfl3.sh	panic: handle_written_inodeblock: live inodedep		20140812
# pmc.sh	NMI ... going to debugger				20111217
# snap5-1.sh	mksnap_ffs deadlock					20111218
# quota2.sh	panic: dqflush: stray dquot				20120221
# quota3.sh	panic: softdep_deallocate_dependencies: unrecovered ...	20111222
# quota6.sh	panic: softdep_deallocate_dependencies: unrecovered ...	20130206
# quota7.sh	panic: dqflush: stray dquot				20120221
# rw.sh		WiP							20150928
# shm_open.sh	panic: kmem_malloc(4096): kmem_map too small		20130504
# snap3.sh	mksnap_ffs stuck in snaprdb				20111226
# snap5.sh	mksnap_ffs stuck in getblk				20111224
# snap6.sh	panic: softdep_deallocate_dependencies: unrecovered ...	20130630
# snap8.sh	panic: softdep_deallocate_dependencies: unrecovered ...	20120630
# snap9.sh	panic: softdep_deallocate_dependencies: unrecovered ... 20150217
# suj3.sh	panic: Memory modified after free			20150721
# suj9.sh	page fault in softdep_count_dependencies+0x27		20141116
# suj11.sh	panic: ufsdirhash_newblk: bad offset			20120118
# suj13.sh	general protection fault in bufdaemon			20141130
# suj18.sh	panic: Bad tailq NEXT(0xc1e2a6088->tqh_last_s) != NULL	20120213
# suj30.sh	panic: flush_pagedep_deps: MKDIR_PARENT			20121020
# suj34.sh	Various hangs and panics (SUJ + NULLFS iisue)		20131210
# trim4.sh	Page fault in softdep_count_dependencies+0x27		20140608
# umountf3.sh	KDB: enter: watchdog timeout				20111217
# umountf7.sh	panic: handle_written_inodeblock: live inodedep ...	20131129
# unionfs.sh	insmntque: non-locked vp: xx is not exclusive locked...	20130909
# unionfs2.sh	insmntque: mp-safe fs and non-locked vp is not ...	20111219
# unionfs3.sh	insmntque: mp-safe fs and non-locked vp is not ...	20111216

# Test not to run for other reasons:

# fuzz.sh	A know issue
# statfs.sh	Not very interesting
# syscall.sh	OK, but runs for a very long time
# syscall2.sh	OK, but runs for a very long time
# syscall3.sh	OK, but syscall4.sh is better
# vunref.sh	No problems ever seen
# vunref2.sh	No problems ever seen

# Snapshots has been disabled on SU+J
# suj15.sh
# suj16.sh
# suj19.sh
# suj20.sh
# suj21.sh
# suj22.sh
# suj24.sh
# suj25.sh
# suj26.sh
# suj27.sh
# suj28.sh

# Exclude NFS loopback tests
# nfs2.sh	panic: wrong diroffset 					20140219
# nfs5.sh
# nfs6.sh
# nfs10.sh
# nfs11.sh	vmwait deadlock						20151004
# nfs13.sh
# nullfs8.sh

# End of list

# Suspects:
# ffs_syncvnode2.sh
#		Memory modified after free. ... used by inodedep	20111224

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

# Log files:
allfaillog=/tmp/stress2.all.fail.log	# Tests that failed
alllast=/tmp/stress2.all.last		# Last test run
alllist=/tmp/stress2.all.list		# -o list
alllog=/tmp/stress2.all.log		# Tests run
alloutput=/tmp/stress2.all.output	# Output from current test
allexcess=/tmp/stress2.all.excessive	# Tests with excessive runtime

args=`getopt acno $*`
[ $? -ne 0 ] && echo "Usage $0 [-a] [-c] [-n] [tests]" && exit 1
set -- $args
for i; do
	case "$i" in
	-a)	all=1		# Run all tests
		echo "Note: including known problem tests."
		shift
		;;
	-c)	rm -f $alllast	# Clear last know test
		shift
		;;
	-n)	noshuffle=1	# Do not shuffle the list of tests
		shift		# Resume test after last test
		;;
	-o)	once=1		# Only run once
		shift
		;;
	--)
		shift
		break
		;;
	esac
done

# Sanity checks
. ../default.cfg
minspace=$((1024 * 1024)) # in k
[ -d `dirname "$diskimage"` ] ||
    { echo "diskimage dir: $diskimage not found"; exit 1; }
[ `df -k $(dirname $diskimage) | tail -1 | awk '{print $4'}` -lt \
    $minspace ] &&
    echo "Warn: Not enough disk space on `dirname $diskimage` for \$diskimage"
[ ! -d $(dirname $RUNDIR) ] && echo "No such `dirname $RUNDIR`" &&
    exit 1
[ `df -k $(dirname $RUNDIR) | tail -1 | awk '{print $4'}` -lt \
    $minspace ] &&
    echo "Warn: Not enough disk space on `dirname $RUNDIR` for \$RUNDIR"
probe=`dirname $RUNDIR`/probe
su $testuser -c "touch $probe" > /dev/null 2>&1
[ -f `dirname $RUNDIR`/probe ] && rm $probe ||
    { echo "No write access to `dirname $RUNDIR`."; exit 1; }
[ `swapinfo | wc -l` -eq 1 ] &&
    echo "Consider adding a swap disk. Many tests rely on this."
grep -wq "$testuser" /etc/passwd ||
    { echo "\$testuser not found."; exit 1; }
[ -x ../testcases/run/run ] ||
    { echo "Please run \"cd stress2; make\" first." && exit 1; }
ping -c 2 -t 2 $BLASTHOST > /dev/null 2>&1 ||
    { echo "Can not ping \$BLASTHOST: $BLASTHOST"; }

rm -f $alllog $alllist
find `dirname $alllast` -maxdepth 1 -name $alllast -mtime +12h -delete
touch $alllast $alllog
chmod 640 $alllast $alllog
while true; do
	exclude=`sed -n '/^# Start of list/,/^# End of list/p' < $0 |
		cat - all.exclude 2>/dev/null |
		grep "\.sh" | awk '{print $2}'`
	list=`echo *.sh`
	[ $# -ne 0 ] && list=$*
	list=`echo $list | sed  "s/all\.sh//; s/cleanup\.sh//"`

	if [ -n "$noshuffle" -a $# -eq 0 ]; then
		last=`cat $alllast`
		if [ -n "$last" ]; then
			last=`basename $last`
			l=`echo "$list" | sed "s/.*$last//"`
			[ -z "$l" ] && l=$list	# start over
			list=$l
			echo "Resuming test at `echo "$list" | \
			    awk '{print $1}'`"
		fi
	fi
	[ -n "$noshuffle" ] ||
	    list=`echo $list | tr '\n' ' ' | ../tools/shuffle | \
	    tr ' ' '\n'`

	lst=""
	for i in $list; do
		[ -z "$all" ] && echo $exclude | grep -qw $i && continue
		lst="$lst $i"
	done
	[ -z "$lst" ] && exit
	[ -n "$once" ] && echo "$lst" > $alllist

	n1=0
	n2=`echo $lst | wc -w | sed 's/ //g'`
	for i in $lst; do
		n1=$((n1 + 1))
		echo $i > $alllast
		./cleanup.sh
		echo "`date '+%Y%m%d %T'` all: $i"
		echo "`date '+%Y%m%d %T'` all: $i" >> $alllog
		printf "`date '+%Y%m%d %T'` all ($n1/$n2): $i\r\n" > /dev/console
		logger "Starting test all: $i"
		sync;sync;sync
		start=`date '+%s'`
		./$i 2>&1 | tee $alloutput
		grep -qw FAIL $alloutput &&
		    echo "`date '+%Y%m%d %T'` $i" >> $allfaillog &&
		    logger "stress2 test $i failed"
		rm -f $alloutput
		[ $((`date '+%s'` - $start)) -gt 1980 ] &&
		    printf "*** Excessive run time: %s %d min\r\n" $i, \
		    $(((`date '+%s'` - $start) / 60)) | \
		    tee /dev/console >> $allexcess
		while pgrep -q swap; do
			echo "swap still running"
			sleep 2
		done
	done
	[ -n "$once" ] && break
done
