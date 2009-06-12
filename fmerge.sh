#!/bin/sh
#-
# Copyright (c) 2009 Dag-Erling Coïdan Smørgrav
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
# $FreeBSD$
#

error() {
	echo "$@" 1>&2
	exit 1
}

say_and_do() {
	echo "$@"
	"$@"
}

eval $(svn info | awk '
/^URL/ { print "svnurl=" $2 }
/^Repository Root/ { print "svnroot=" $3 }
')
svnpath=${svnurl##${svnroot}/}

case $svnpath in
stable/[0-9]/*)
	subdir=${svnpath##stable/[0-9]/}
	branch=${svnpath%%/${subdir}}
	;;
*)
	error "I don't know where $svnpath is"
	;;
esac

[ $# -gt 0 ] || error "You have to give me a revision number"

for rev ; do
	# I wish sh had something approaching a kleene star.
	case $rev in
	[0-9]*)
		rev=-c${rev}
		;;
	r[0-9]*)
		rev=-c${rev##r}
		;;
	[0-9]*-[0-9]*)
		reva=${rev%%-[0-9]*}
		revb=${rev##[0-9]*-}
		rev=-r${reva}-${revb}
		;;
	*)
		error "I can't figure this revision number out"
		;;
	esac
	say_and_do svn merge ${rev} ${svnroot}/head/${subdir} .
done
