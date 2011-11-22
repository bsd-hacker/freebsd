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
src="$1"
dst="$2"

# Check src / dst datasets
if ! zfs list "$src" >/dev/null 2>&1 ; then
	error "'$src' is not a valid dataset"
fi
if ! zfs list "$dst" >/dev/null 2>&1 ; then
	error "'$dst' is not a valid dataset"
fi
if [ "$src" = "$dst" ] ; then
	error "source and destination must be different datasets"
fi
if [ "${src#$dst/}" != "$src" -o "${dst#$src/}" != "$dst" ] ; then
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
last=$(zfs list -t snapshot | cut -d' ' -f1 | grep "^$dst$sub@bak-" | sort | tail -1)
if [ -z "$last" ] ; then
	# First time
	info "It looks like this is the first backup from $src to $dst." \
	    "Continuing will DESTROY the contents of $dst, including any" \
	    "preexisting snapshots, and replace them with the contents of $src."
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
	zfs snapshot -r "$src@$this" || error "snapshot failed"
	zfs send $verbose -R "$src@$this" |
		zfs receive $verbose -F -d -u "$dst"
else
	# Subsequent times
	last="${last#*@}"
	zfs list "$src@$last" >/dev/null || error "failed to determine last backup"
	zfs snapshot -r "$src@$this" || error "snapshot failed"
	zfs send $verbose -R -I "$src@$last" "$src@$this" |
		zfs receive $verbose -F -d -u "$dst"
	zfs destroy -r "$src@$last"
fi
end=$(date +%s)
info "Backup from $src to $dst completed in $((end - beg)) seconds"
