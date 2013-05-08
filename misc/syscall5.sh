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


# syscalls with knows issues:
broken="
swapcontext
rfork
_umtx_op
"

n=$syscall
n=$((n - 1))

rm -f ./syscall5.log
start=`date '+%s'`
while [ $n -gt 0 ]; do
	ps -lUnobody | grep syscall4 | awk '{print $2}' | xargs kill
	name=`grep -w $n /usr/include/sys/syscall.h | awk '{print $2}' | 
		sed 's/SYS_//'`
	echo "`date '+%T'` syscall $n ($name)" | 
		tee /dev/tty >> ./syscall5.log
	sync; sleep 1
	echo "$broken" | grep -qw "$name" || 
		./syscall4.sh $n || break
	n=$((n - 1))
	[ $# -eq 0 -a `date '+%s'` -gt $((start + 1800)) ] && break
done
rm -f ./syscall5.log
