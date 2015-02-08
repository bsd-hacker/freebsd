#!/bin/sh

#
# Copyright (c) 2013 Peter Holm
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

# Call syscall4.sh with syscall number as argument.
# Only run the last ~16 syscalls, if no argument is specified.

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

. ../default.cfg

syscall=`grep SYS_MAXSYSCALL /usr/include/sys/syscall.h | awk '{print $NF}'`
syscall=$((syscall - 1))

args=`getopt ars:t: $*`
[ $? -ne 0 ] && echo "Usage $0 [-a] [-r] [-s number] [-t seconds]" && exit 1
set -- $args
last=/tmp/syscall5.last
log=/tmp/syscall5.log
for i; do
	case "$i" in
	-a)	all=1		# Test all syscalls
		shift
		;;
	-r)	[ -h $last ] &&
			syscall=`ls -l $last | awk '{print $NF}'`
			syscall=$((syscall - 1))
		shift
		;;
	-s)	syscall=$2
		shift; shift
		;;
	-t)	sleeptime=$2
		export sleeptime=$((sleeptime / 10))	# used in syscall4.sh
		shift; shift
		;;
	--)
		shift
		break
		;;
	esac
done

# syscalls with known issues:
broken="
swapcontext
pdfork
rfork
pselect
"

rm -f $log
n=$syscall
start=`date '+%s'`
while [ $n -gt 0 ]; do
	ps -lUnobody | grep syscall4 | awk '{print $2}' | xargs kill
	ln -fs $n $last
	name=`grep -w "$n$" /usr/include/sys/syscall.h | awk '{print $2}' |
		sed 's/SYS_//'`
	[ -z "$name" ] && name="unknown"
	if [ -x ../tools/exclude_syscall.sh ]; then
		name=`../tools/exclude_syscall.sh $n`
		[ $name = "Excluded" ] &&
		    { n=$((n - 1)); continue; }
	fi
	echo "`date '+%T'` syscall $n ($name)"
	echo "`date '+%T'` syscall $n ($name)"  >> $log
	printf "`date '+%T'` syscall $n ($name)\r\n" > /dev/console
	sync; sleep 1
	echo "$broken" | grep -qw "$name" ||
		./syscall4.sh $n || break
	n=$((n - 1))
	[ -z "$all" -a `date '+%s'` -gt $((start + 1800)) ] && break
done
rm -f $log $last
