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

check_prereq () {
	rc=0
	case ${VM_TARGET_ARCH} in
		i386|amd64)
			# Ok to create vm image
			;;
		*)
			rc=1
			;;
	esac
	case ${KERNEL} in
		GENERIC)
			;;
		*)
			rc=1
			;;
	esac

	return ${rc}
}

create_etc() {
	chroot ${CHROOTDIR} /usr/bin/newaliases
	echo "# Custom /etc/fstab for FreeBSD VM images" \
		> ${CHROOTDIR}/vmimage/mnt/etc/fstab
	echo "/dev/gpt/rootfs	/	ufs	rw	2	2" \
		>> ${CHROOTDIR}/vmimage/mnt/etc/fstab
	echo "/dev/gpt/swapfs	none	swap	sw	0	0" \
		>> ${CHROOTDIR}/vmimage/mnt/etc/fstab
	sync
	while ! umount ${CHROOTDIR}/vmimage/mnt; do
		sleep 1
	done
}

create_vmimage_qemu() {
	diskformats="qcow2"
	if [ ! -x /usr/local/bin/qemu-img ]; then
		echo "qemu-img not found, skipping qcow2 format."
		return 0
	fi

	if [ ! -x /usr/bin/mkimg ]; then
		# No mkimg(1) here, so use qemu-img for them all.
		diskformats="${diskformats} vpc vmdk"
	fi

	if [ -e "${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk" ]; then
		rm -f "${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk"
	fi
	mkdir -p ${CHROOTDIR}/vmimage ${CHROOTDIR}/vmimage/mnt
	touch ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk
	truncate -s 20G ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk
	mddev=$(mdconfig -a -t vnode -f ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk)

	gpart create -s gpt /dev/${mddev}
	gpart add -t freebsd-boot -a 1m -s 512k -l bootfs /dev/${mddev}
	gpart add -t freebsd-swap -a 1m -s 1G -l swapfs /dev/${mddev}
	gpart add -t freebsd-ufs -a 1m -l rootfs /dev/${mddev}
	gpart bootcode -b ${CHROOTDIR}/boot/pmbr \
		-p ${CHROOTDIR}/boot/gptboot -i 1 /dev/${mddev}
	newfs -j -L rootfs /dev/${mddev}p3
	mount /dev/${mddev}p3 ${CHROOTDIR}/vmimage/mnt

	# Errors here are ok, 'set -e' is re-enabled below again.
	set +e
	mount -t devfs devfs ${CHROOTDIR}/dev
	chroot ${CHROOTDIR} make -s -C /usr/src DESTDIR=/vmimage/mnt \
		installworld installkernel distribution
	set -e
	while ! umount ${CHROOTDIR}/vmimage/mnt/dev; do
		sleep 1
	done
	create_etc

	for f in ${diskformats}; do
		_f=${f}
		case ${_f} in
			vpc)
				_f=vhd
				;;
			*)
				;;
		esac
		/usr/local/bin/qemu-img convert \
			-O ${f} ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk \
			${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.${_f}
		xz ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.${_f}
	done
	mdconfig -d -u ${mddev}
	mv ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk \
		${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.raw
	xz ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.raw
	return 0
}

create_vmimage_mkimg() {
	diskformats="vmdk vhdf"

	if [ ! -x /usr/bin/mkimg ]; then
		return 0
	fi

	if [ ! -d ${CHROOTDIR}/R/ftp ]; then
		echo "Error: Cannot find the ftp/*.txz files."
		exit 1
	fi

	dists="base kernel games"
	if [ -e ${CHROOTDIR}/R/ftp/lib32.txz ]; then
		dists="${dists} lib32"
	fi

	if [ -e "${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk" ]; then
		rm -f "${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk"
	fi
	mkdir -p ${CHROOTDIR}/vmimage ${CHROOTDIR}/vmimage/mnt
	touch ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk
	truncate -s 20G ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk
	mddev=$(mdconfig -a -t vnode -f ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk)

	newfs -j -L rootfs /dev/${mddev}
	mount /dev/${mddev} ${CHROOTDIR}/vmimage/mnt

	for d in ${dists}; do
		tar -xf ${CHROOTDIR}/R/ftp/${d}.txz -C ${CHROOTDIR}/vmimage/mnt
	done
	create_etc
	mdconfig -d -u ${mddev}

	for f in ${diskformats}; do
		case ${f} in
			vhdf)
				_f=vhd
				;;
			*)
				_f=${f}
				;;
		esac
		/usr/bin/mkimg -f ${f} \
			-s gpt -b ${CHROOTDIR}/boot/pmbr \
			-p freebsd-boot/bootfs:=${CHROOTDIR}/boot/gptboot \
			-p freebsd-swap/swapfs::1G \
			-p freebsd-ufs/rootfs:=${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk \
			-o ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.${_f}
		xz ${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.${_f}
	done
	mdconfig -d -u ${mddev}
	rm -f "${CHROOTDIR}/vmimage/${VM_IMAGE_NAME}.rawdisk"
	return 0
}

main() {
	while getopts c: opt; do
		case ${opt} in
			c)
				VM_CONFIG="${OPTARG}"
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
	if [ ! -e "${VM_CONFIG}" ]; then
		echo "ERROR: Configuration file not found"
		exit 1
	fi
	. ${VM_CONFIG}

	VM_TARGET_ARCH=$(echo ${__CONFIG_NAME} | cut -f 2 -d -)

	# Use the build environment to get the branch revision (i.e., 10.0)
	# and branch (i.e., -CURRENT, -STABLE) for the vm image name.
	VM_IMAGE_REVISION=$(make -C ${CHROOTDIR}/usr/src/release -V REVISION)
	VM_IMAGE_BRANCH=$(make -C ${CHROOTDIR}/usr/src/release -V BRANCH)
	VM_IMAGE_NAME="${VM_IMAGE_REVISION}-${VM_IMAGE_BRANCH}"
	VM_IMAGE_NAME="$(uname -s)-${VM_IMAGE_NAME}-${VM_TARGET_ARCH}"

	check_prereq || exit 0

	create_vmimage_mkimg
	create_vmimage_qemu

	cd ${CHROOTDIR}/vmimage
	sha256 FreeBSD*.xz > CHECKSUM.SHA256
	md5 FreeBSD*.xz > CHECKSUM.MD5

}

main "$@"
