#!/bin/sh
#-
# Copyright (c) 2013 Glen Barber
# Copyright (c) 2011 Nathan Whitehorn
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
# release.sh: check out source trees, and build release components with
#  totally clean, fresh trees.
# Based on release/generate-release.sh written by Nathan Whitehorn
#
# $FreeBSD$
#
set -e # Everything must succeed
PATH="/sbin:/bin:/usr/sbin:/usr/bin"; export PATH
LC_ALL=C; export LC_ALL
for X in /usr/bin/svnlite /usr/local/bin/svn; do
	[ -x $X ] && break
done
: ${SVN_CMD:=$X}
: ${SVN_ARGS:=}
: ${SH:=/bin/sh}
: ${SYSCTL:=/sbin/sysctl}
: ${MKDIR:=/bin/mkdir -p}
: ${MAKE:=/usr/bin/make}
: ${CP:=/bin/cp}
: ${MOUNT:=/sbin/mount}
: ${UMOUNT:=/sbin/umount}
: ${UNAME:=/usr/bin/uname}
: ${ID:=/usr/bin/id}
if [ $($ID -u) -ne 0 ]; then
	echo 1>&2 "$0: Needs to be run as root."
	exit 1
fi
# The directory within which the release will be built.
: ${CHROOTDIR:=/scratch}
: ${CHROOT_CMD:=chroot $CHROOTDIR}
: ${DESTDIR=:/R/}
: ${MAKEOBJDIRPREFIX:=/usr/obj}

# The default svn checkout server, and svn branches for src/, doc/,
# and ports/.
: ${SVNROOT:=svn://svn.FreeBSD.org}
: ${SRCBRANCH:=base/head@rHEAD}
: ${DOCBRANCH:=doc/head@rHEAD}
: ${PORTBRANCH:=ports/head@rHEAD}

# Sometimes one needs to checkout src with --force svn option.
# If custom kernel configs copied to src tree before checkout, e.g.
: ${SRC_FORCE_CHECKOUT=}

# HTTP Proxy option.  Note that http:// must be used for subversion to use
# a proxy.
if [ -n "$HTTP_PROXY" ]; then
	_host=${HTTP_PROXY:%:*}
	_port=${HTTP_PROXY:#*:}
	case $_port in
	//*)	_host=$HTTP_PROXY; _port="" ;;
	esac
	SVN_ARGS="$SVN_ARGS \
	    --config-option=servers:global:http-proxy-host=$_host \
	    --config-option=servers:global:http-proxy-port=$_port \
	"
fi
# The number of make(1) jobs, defaults to the number of CPUs available for
# buildworld, and half of number of CPUs available for buildkernel.
: ${NCPU:=$($SYSCTL -n hw.ncpu)}
: ${WNCPU:=$(($NCPU + 0))}		# for buildworld
: ${KNCPU:=$((($NCPU + 1) / 2))}	# for buildkernel
: ${WORLD_FLAGS=-j$WNCPU}		# can be unset
: ${KERNEL_FLAGS=-j$KNCPU}		# can be unset
: ${MAKE_FLAGS=-s}			# can be unset

while getopts c: opt; do
	case $opt in
	c)
		# Source the specified configuration file for overrides
		. "$OPTARG"
		;;
	\?)
		echo 1>&2 "Usage: $0 [-c release.conf]"
		exit 1
		;;
	esac
done
shift $(($OPTIND - 1))

# Construct release make args.  The value will be normalized to true or false.
RMAKE_ARGS_LIST="NODOC NOPKG NOPORTS NODVD"
RMAKE_ARGS=
for A in $RMAKE_ARGS_LIST; do
	case $(eval echo \${$A:-no}) in
	[Nn][Oo])
		eval $A=false
	;;
	*)
		eval $A=true
		RMAKE_ARGS="$RMAKE_ARGS $A=true"
	;;
	esac
done
# Sanity check.
# XXX: This false:true case must be supported.
case ${NODOC}:${NOPORTS} in
false:true)
	echo 1>&2 "$0: NODOC is required when NOPORTS is defined."
	exit 1
;;
esac

# The aggregated build-time flags based upon variables defined within
# this file, unless overridden by release.conf.  In most cases, these
# will not need to be changed.
CONF_FILES="__MAKE_CONF=/dev/null SRCCONF=/dev/null"
CROSS_FLAGS=
if [ -n "$TARGET" ]; then
	CROSS_FLAGS="$CROSS_FLAGS TARGET=$TARGET"
fi
if [ -n "$TARGET_ARCH" ]; then
	CROSS_FLAGS="$CROSS_FLAGS TARGET_ARCH=$TARGET_ARCH"
fi
: ${KERNCONF:=${KERNEL:-${KERNELS_BASE}}}

DOCMAKE_ARGS=" \
    $CONF_FILES \
    BATCH=yes \
    DISABLE_VULNERABILITIES=yes \
    OPTIONS_UNSET_FORCE+=X11 \
    OPTIONS_UNSET_FORCE+=SVN \
    OPTIONS_UNSET_FORCE+=IGOR \
    OPTIONS_UNSET_FORCE+=FOP \
"
CHROOT_MAKEFLAGS="-C ${CHROOTDIR}/usr/src -DDB_FROM_SRC"
CHROOT_WMAKEFLAGS="$CHROOT_MAKEFLAGS $MAKE_FLAGS $WORLD_FLAGS $CONF_FILES"
CHROOT_IMAKEFLAGS="$CHROOT_MAKEFLAGS $MAKE_FLAGS $CONF_FILES"
CHROOT_DMAKEFLAGS="$CHROOT_MAKEFLAGS $MAKE_FLAGS $CONF_FILES"
RELEASE_WMAKEFLAGS="-C /usr/src $MAKE_FLAGS $WORLD_FLAGS $CROSS_FLAGS"
RELEASE_KMAKEFLAGS="-C /usr/src $MAKE_FLAGS $KERNEL_FLAGS $CROSS_FLAGS"
RELEASE_RMAKEFLAGS="-C /usr/src/release $MAKE_FLAGS $CROSS_FLAGS $RMAKE_ARGS"
# KERNCONF can be empty to leave the default value to Makefile.
for K in $KERNCONF; do
	RELEASE_KMAKEFLAGS="$RELEASE_KMAKEFLAGS KERNCONF+=$K"
	RELEASE_RMAKEFLAGS="$RELEASE_RMAKEFLAGS KERNCONF+=$K"
done
SETENV="env -i PATH=$PATH LC_ALL=$LC_ALL"
SETENV_CHROOT="$SETENV MAKEOBJDIRPREFIX=$MAKEOBJDIRPREFIX"

# Force src checkout if configured.
if [ -n "$SRC_FORCE_CHECKOUT" ]; then
	SVN_ARGS="$SVN_ARGS --force"
fi
if [ ! -d "$CHROOTDIR" ]; then
	echo 1>&2 "$0: $CHROOTDIR not found."
	exit 1
fi

# Check out trees.
$MKDIR ${CHROOTDIR}/usr
$SETENV LC_ALL=en_US.UTF-8 $SVN_CMD co $SVN_ARGS \
    ${SVNROOT}/${SRCBRANCH} ${CHROOTDIR}/usr/src
if ! $NODOC; then
	$SETENV LC_ALL=en_US.UTF-8 $SVN_CMD co $SVN_ARGS \
	    ${SVNROOT}/${DOCBRANCH} ${CHROOTDIR}/usr/doc
fi
if ! $NOPORTS; then
	$SETENV LC_ALL=en_US.UTF-8 $SVN_CMD co $SVN_ARGS \
	    ${SVNROOT}/${PORTBRANCH} ${CHROOTDIR}/usr/ports
fi
# Fetch distfiles for ports-mgmt/pkg and textproc/docproj port if necessary.
if ! $NODOC && ! $NOPORTS; then
	# LOCALBASE=/var/empty is required to disable automatic detection of
	# PERL_VERSION.  It depends on existence of ${LOCALBASE}/bin/perl5.
	# CLEAN_FETCH_ENV disables ${LOCALBASE}/sbin/pkg dependency.
	$MKDIR ${CHROOTDIR}/usr/ports/distfiles
	for P in ports-mgmt/pkg textproc/docproj; do
		$SETENV PORTSDIR=${CHROOTDIR}/usr/ports \
		    HTTP_PROXY=$HTTP_PROXY \
		    $MAKE -C ${CHROOTDIR}/usr/ports/$P \
			LOCALBASE=/var/empty \
			CLEAN_FETCH_ENV=yes \
			MASTER_SITE_FREEBSD=yes \
			$DOCMAKE_ARGS fetch-recursive
	done
fi

# Build a clean environment by using $CHROOTDIR/usr/src and install it into
# $CHROOTDIR.  Note that this is always a native build.
$SETENV_CHROOT $MAKE $CHROOT_WMAKEFLAGS buildworld
$SETENV_CHROOT $MAKE $CHROOT_IMAKEFLAGS installworld DESTDIR=$CHROOTDIR
$SETENV_CHROOT $MAKE $CHROOT_DMAKEFLAGS distribution DESTDIR=$CHROOTDIR
$MOUNT -t devfs devfs ${CHROOTDIR}/dev
trap "$UMOUNT ${CHROOTDIR}/dev" EXIT # Clean up devfs mount on exit

# If MAKE_CONF and/or SRC_CONF are set copy them to the chroot.
# If not, create empty one.
$CP ${MAKE_CONF:-/dev/null} ${CHROOTDIR}/etc/make.conf
$CP ${SRC_CONF:-/dev/null} ${CHROOTDIR}/etc/src.conf

# Run ldconfig(8) in the chroot directory so /var/run/ld-elf*.so.hints
# is created.  This is needed by ports-mgmt/pkg.
$CHROOT_CMD $SETENV $SH /etc/rc.d/ldconfig forcestart

# Build docproj port if necessary.
if ! $NODOC && ! $NOPORTS; then
	## Trick the ports 'run-autotools-fixup' target to do the right thing.
	$CHROOT_CMD $SETENV PATH=${PATH}:/usr/local/bin \
		$MAKE -C /usr/ports/textproc/docproj \
		OSVERSION=$($SYSCTL -n kern.osreldate) \
		$DOCMAKE_ARGS install
fi
# Build a release in $CHROOTDIR and install it into $DESTDIR.
# This can be a cross build.
$CHROOT_CMD $SETENV $MAKE $RELEASE_WMAKEFLAGS buildworld
$CHROOT_CMD $SETENV $MAKE $RELEASE_KMAKEFLAGS buildkernel
$CHROOT_CMD $SETENV $MAKE $RELEASE_RMAKEFLAGS release
$CHROOT_CMD $SETENV $MAKE $RELEASE_RMAKEFLAGS install DESTDIR=$DESTDIR
