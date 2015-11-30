#!/bin/sh
#-
# Copyright (c) 2015 Dag-Erling Smørgrav
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

progname=$(basename $(realpath $0))
today=$(date +'%Y%m%d')

notice() {
	echo "$@" >&2
}

error() {
	echo "$@" >&2
	exit 1
}

[ $# -ne 3 ] || error "usage: ${progname} /path/old /path/new"

# Extract version information from old release
old="$1"
old_newvers="${old}/usr/src/sys/conf/newvers.sh"
old_param="${old}/usr/src/sys/sys/param.h"
[ -f "${old_newvers}" -a -f "${old_param}" ] || \
    error "missing or incomplete source tree for ${old}"
eval $(egrep '^(REVISION|BRANCH)=' "${old_newvers}" | tr '\n' ';')
old_revision="${REVISION}"
old_major="${REVISION%.*}"
old_minor="${REVISION#*.}"
old_branch="${BRANCH}"
unset REVISION BRANCH
old_version=$(awk '/^#define __FreeBSD_version/ { print $3 }' "${old_param}")
case $(file "${old}/sbin/init") in
*x86-64*)
	old_arch=amd64
	;;
*80386*)
	old_arch=i386
	;;
*)
	error "${old}: Unreconized architecture"
	;;
esac
notice $old_revision-$old_branch $old_version

# Extract version information from new release
new="$2"
new_newvers="${new}/usr/src/sys/conf/newvers.sh"
new_param="${new}/usr/src/sys/sys/param.h"
[ -f "${new_newvers}" -a -f "${new_param}" ] || \
    error "missing or incomplete source tree for ${new}"
eval $(egrep '^(REVISION|BRANCH)=' "${new_newvers}" | tr '\n' ';')
new_revision="${REVISION}"
new_major="${REVISION%.*}"
new_minor="${REVISION#*.}"
new_branch="${BRANCH}"
unset REVISION BRANCH
new_version=$(awk '/^#define __FreeBSD_version/ { print $3 }' "${new_param}")
case $(file "${new}/sbin/init") in
*x86-64*)
	new_arch=amd64
	;;
*80386*)
	new_arch=i386
	;;
*)
	error "${new}: Unreconized architecture"
	;;
esac
notice $new_revision-$new_branch $new_version

# Sanity check
if [ "${old_arch}" != "${new_arch}" ] ; then
    error "${old} (${old_arch}) and ${new} (${new_arch}) mismatch"
fi
if [ $((old_major + 1)) -ne $((new_major)) ] ; then
    error "expecting ${old_revision} and $((old_major + 1)).x or" \
	  "$((new_major - 1)).x and ${new_revision}, not" \
	  "${old_revision} and ${new_revision}"
fi

# Prepare
portname="compat${old_major}x"
portversion="${old_revision}.${old_version}.${today}"
pkgname="${portname}-${old_arch}-${portversion}"
tarname="${pkgname}.tar.xz"
mtree="mtree.${old_arch}"
echo '#mtree' >"${mtree}"
plist="pkg-plist.${old_arch}"
:>"${plist}"
distinfo="distinfo.${old_arch}"
:>"${distinfo}"

# Search old tree for libraries which do not exist in the new tree
(cd "${old}" && find -s lib* usr/lib* -type f -name 'lib*.so.*') | \
    egrep -v '/private/|/lib(lwres|ssh)\.so\.[0-9]+$' | \
    while read file ; do
	if [ ! -f "${new}/${file}" ] ; then
	    lib=$(basename "${file}")
	    dir=$(basename $(dirname "${file}"))
	    if [ -f "${pkgname}/${dir}/${lib}" ] ; then
		error "duplicate library: ${dir}/${lib}"
	    fi
	    notice "missing ${dir}/${lib}"
	    echo "${pkgname}/${dir}/${lib} uid=0 gid=0 mode=0444" \
		 "type=file content=${old}/${file}" >>"${mtree}"
	    echo "${dir}/${lib}" >>"${plist}"
	fi
    done
sort "${plist}" >"${plist}-" && mv "${plist}-" "${plist}"
sort "${mtree}" >"${mtree}-" && mv "${mtree}-" "${mtree}"

# Create tarball and distinfo
tar -Jcf "${tarname}" @"${mtree}" || exit 1
echo "SHA256 (${tarname}) = $(sha256 -q ${tarname})" >>"${distinfo}"
echo "SIZE (${tarname}) = $(stat -f%z ${tarname})" >>"${distinfo}"
