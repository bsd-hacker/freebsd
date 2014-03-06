#!/bin/sh
#-
# Copyright (c) 2013, 2014 The FreeBSD Foundation
# Copyright (c) 2012, 2013 Glen Barber
# All rights reserved.
#
# Portions of this software were developed by Glen Barber
# under sponsorship from the FreeBSD Foundation.
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
# Script to create the VM images available on ftp.FreeBSD.org.
#
# $FreeBSD$
#

PATH="/bin:/sbin:/usr/bin:/usr/sbin"
export PATH

usage () {
	echo "$(basename ${0}) -c /path/to/release.conf"
	exit 1
}

while getopts c: opt; do
	case ${opt} in
		c)
			VM_CONFIG="${OPTARG}"
			if [ ! -e "${VM_CONFIG}" ]; then
				echo -n "ERROR: Configuration file ${VM_CONFIG}"
				echo " does not exist."
				exit 1
			fi
			. ${VM_CONFIG}
			;;
		\?)
			usage
			;;
	esac
done

# Force use of configuration file
if [ "X${VM_CONFIG}" = "X" ]; then
	usage
fi

check_prereq () {
	rc=0
	if [ ! -x "/usr/local/bin/qemu-img" ]; then
		rc=1
	fi
	return ${rc}
}

VM_TARGET_ARCH=$(echo ${__CONFIG_NAME} | cut -f 2 -d -)

case ${VM_TARGET_ARCH} in
	# For now, only create amd64 and i386 vm images.
	i386|amd64)
		# Ok to create vm image
		;;
	*)
		exit 0
		;;
esac

case ${KERNEL} in
	GENERIC)
		;;
	*)
		exit 0
		;;
esac

check_prereq || exit 0

# Use the build environment to get the branch revision (i.e., 10.0) and branch
# (i.e., -CURRENT, -STABLE) for the vm image name.
VM_IMAGE_NAME=$(make -C ${CHROOTDIR}/usr/src/release -V REVISION -V BRANCH | tr '\n' '-')
# Prefix the image name with OS name, and suffix with the vm architecture.
VM_IMAGE_NAME="$(uname -s)-${VM_IMAGE_NAME}${VM_TARGET_ARCH}"

mkdir -p ${CHROOTDIR}/vmimage ${CHROOTDIR}/vmimage/mnt

# This should only ever happen if the script is being run again after failure.
if [ -e "${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.disk" ]; then
	rm -f "${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.disk"
fi

touch ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.disk
truncate -s 20G ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.disk
mddev=$(mdconfig -a -t vnode -f ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.disk)
gpart create -s gpt /dev/${mddev}
gpart add -t freebsd-boot -s 512k -l bootfs /dev/${mddev}
gpart bootcode -b /boot/pmbr -p /boot/gptboot -i 1 /dev/${mddev}
gpart add -t freebsd-swap -s 1G -l swapfs /dev/${mddev}
gpart add -t freebsd-ufs -l rootfs /dev/${mddev}
newfs /dev/${mddev}p3
mount /dev/${mddev}p3 ${CHROOTDIR}/vmimage/mnt

# Errors here are ok, 'set -e' is re-enabled below again.
set +e
mount -t devfs devfs ${CHROOTDIR}/dev
chroot ${CHROOTDIR} make -s -C /usr/src DESTDIR=/vmimage/mnt \
	installworld installkernel distribution
echo "# Custom /etc/fstab for FreeBSD VM images" \
	> ${CHROOTDIR}/vmimage/mnt/etc/fstab
echo "/dev/gpt/rootfs	/	ufs	rw	2	2" \
	>> ${CHROOTDIR}/vmimage/mnt/etc/fstab
echo "/dev/gpt/swapfs	none	swap	sw	0	0" \
	>> ${CHROOTDIR}/vmimage/mnt/etc/fstab
# Make sure we wait until the md(4) is unmounted before destroying it.
while ! umount /dev/${mddev}p3; do
	sleep 1
done
mdconfig -d -u ${mddev}
while ! umount ${CHROOTDIR}/dev; do
	sleep 1
done
set -e
diskformats="vmdk vpc qcow2"
for f in ${diskformats}; do
	_f=${f}
	case ${_f} in
		vpc)
			_f=vhd
			;;
		*)
			;;
	esac
	/usr/local/bin/qemu-img convert -O ${f} ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.disk \
		${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.${_f}
	xz ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.${_f}
done
cd ${CHROOTDIR}/vmimage
sha256 FreeBSD*.xz > CHECKSUM.SHA256
md5 FreeBSD*.xz > CHECKSUM.MD5

