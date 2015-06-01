#!/bin/sh

#
# Copyright (c) 2015 EMC Corp.
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

# Memory leak detector: run vmstat -m & -z in a loop

[ $# -eq 1 ] && debug="-v debug=1"
export LANG=en_US.ISO8859-1
OIFS=$IFS
while true; do
	#          Type InUse MemUse
	vmstat -m | sed 1d | \
	    sed 's/\(.* \)\([0-9][0-9]*\)  *\(.*\)K .*/\1:\2:\3/' | \
	    while IFS=: read -r p1 p2 p3; do
		name=`echo $p1 | sed 's/^ *//;s/ *$//'`
		memuse=$((p3 * 1024))
		[ "$memuse" -ne 0 ] && echo "vmstat -m $name,$memuse"
	done

	# ITEM                   SIZE  LIMIT     USED
	IFS=OIFS
	vmstat -z | sed "1,2d;/^$/d" | while read l; do
		IFS=':,'
		set $l
		size=$2
		used=$4
		[ "$used" -ne 0 ] &&
		    echo "vmstat -z $1,$((size * used))"
	done
	sleep 10
done | awk $debug -F, '
{
# Pairs of "name, value" are passed to this awk script
	name=$1;
	size=$2;
	if (size > s[name]) {
		if (++n[name] > 60) {
			cmd="date '+%T'";
			cmd | getline t;
			close(cmd);
			printf "%s \"%s\" %'\''dK\r\n", t,
			    name, size / 1024;
			n[name] = 0;
		}
		s[name] = size;
		if (debug == 1 && n[name] > 1)
			printf "%s, size %d, count %d\r\n",
			    name, s[name], n[name]
	} else if (size < s[name] && n[name] > 0)
		n[name]--
}'
