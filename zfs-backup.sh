#!/bin/sh
#-
# Copyright (c) 2011 Dag-Erling Smørgrav
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer
#    in this position and unchanged.
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
# $Id$
#

PATH=/usr/bin:/usr/sbin:/bin:/sbin

error() {
	echo "$@" >/dev/stderr
	exit 1
}

info() {
	echo "$@" | fmt -s -w $((${COLUMNS:-80} - 6))
}

# Lock
if [ -z "$SYNC_LOCKED" ] ; then
	exec lockf -k -s -t0 "$0" env SYNC_LOCKED=$$ "$0" "$@"
	exit 1
fi

if [ $# -ne 2 ] ; then
	error "usage: $0 src dst"
fi

fqsrc="$1"
case $fqsrc in
*:*)
	src="${fqsrc#*:}"
	srczfs="ssh ${fqsrc%%:*} zfs"
	;;
*)
	src="${fqsrc}"
	srczfs="zfs"
	;;
esac

fqdst="$2"
case $fqdst in
*:*)
	dst="${fqdst#*:}"
	dstzfs="ssh ${fqdst%%:*} zfs"
	;;
*)
	dst="${fqdst}"
	dstzfs="zfs"
	;;
esac

# Check src / dst datasets
if ! $srczfs list "$src" >/dev/null 2>&1 ; then
	error "'$fqsrc' is not a valid dataset"
fi
if ! $dstzfs list "$dst" >/dev/null 2>&1 ; then
	error "'$fqdst' is not a valid dataset"
fi
if [ "$fqsrc" = "$fqdst" ] ; then
	error "source and destination must be different datasets"
fi
if [ "${fqsrc#$fqdst/}" != "$fqsrc" -o "${fqdst#$fqsrc/}" != "$fqdst" ] ; then
	error "source and destination must be non-overlapping datasets"
fi
case src in
*/*)
	sub="/${src#*/}"
	;;
esac

if tty >/dev/null ; then
	verbose="-v"
fi

now=$(date +%Y%m%d-%H%M%S)
beg=$(date +%s)
this="bak-$now"
last=$($dstzfs list -t snapshot | cut -d' ' -f1 | grep "^$dst$sub@bak-" | sort | tail -1)
if [ -z "$last" ] ; then
	# First time
	info "It looks like this is the first backup from $fqsrc to $fqdst." \
	    "Continuing will DESTROY the contents of $fqdst, including any" \
	    "preexisting snapshots, and replace them with the contents of $fqsrc."
	while :; do
		echo -n "Are you sure you want to proceed? (yes/no) "
		read answer
		case $answer in
		[Yy][Ee][Ss])
			break
			;;
		[Nn][Oo])
			exit 1
			;;
		esac
	done
	$srczfs snapshot -r "$src@$this" || error "snapshot failed"
	$srczfs send $verbose -R "$src@$this" |
		$dstzfs receive $verbose -F -d -u "$dst"
else
	# Subsequent times
	last="${last#*@}"
	$srczfs list "$src@$last" >/dev/null || error "failed to determine last backup"
	$srczfs snapshot -r "$src@$this" || error "snapshot failed"
	$srczfs send $verbose -R -I "$src@$last" "$src@$this" |
		$dstzfs receive $verbose -F -d -u "$dst"
	$srczfs destroy -r "$src@$last"
fi
end=$(date +%s)
#info "Backup from $fqsrc to $fqdst completed in $((end - beg)) seconds"
