#!/bin/sh

REL=$1
REPO=$2
ARCH=$3
DISK=$4
HASH=$5

# Download release
ISO=FreeBSD-${REL}-${ARCH}-disc1.iso
fetch ${REPO}/${ISO}
if ! [ `sha256 -q ${ISO}` = "${HASH}" ]; then
	echo "SHA256 does not match!"
	exit 1
fi

# Extract release distribution bits
rm -rf /usr/dist/$ARCH
mkdir /usr/dist/$ARCH
tar -xf $ISO -C /usr/dist/$ARCH usr/freebsd-dist || true

# Partition EBS disk, create a filesystem, and mount it.
bsdlabel -w /dev/${DISK}
bsdlabel -B /dev/${DISK}
newfs -U /dev/${DISK}a
mkdir -p /mnt/image
mount /dev/${DISK}a /mnt/image

# Extract release from ISO image.  Skip the ports tree (it's probably going
# to be obsolete before the AMI is used).
for DIST in base doc games kernel lib32 src; do
	DISTFILE=/usr/dist/$ARCH/usr/freebsd-dist/$DIST.txz
	if [ -f $DISTFILE ]; then
		tar -xf $DISTFILE -C /mnt/image
	fi
done

# Install FreeBSD and EC2 configuration.
tar -cf- -C /home/ec2-user/ec2-bits boot etc | tar -xpof- -C /mnt/image
sh /home/ec2-user/ec2-bits/ec2-config.sh /mnt/image/etc
touch /mnt/image/firstboot

# Install FreeBSD EC2 rc.d scripts.  Note that these ports and their
# dependencies do not contain any architecture-specific binaries, so
# we don't need to worry about cross-building issues.
mount_unionfs /mnt/image/usr/local /usr/local
mount_nullfs /mnt/image/var/db/pkg /var/db/pkg
cd /usr/ports/sysutils/panicmail && make install clean BATCH=YES
cd /usr/ports/sysutils/ec2-scripts && make install clean BATCH=YES
cd /usr/ports/sysutils/firstboot-freebsd-update && make NO_IGNORE=YES install clean BATCH=YES
cd /usr/ports/sysutils/firstboot-pkgs && make NO_IGNORE=YES install clean BATCH=YES
umount /var/db/pkg
umount /usr/local

# Add EC2 bits
tar -cf- -C /home/ec2-user ec2-bits | tar -xpof- -C /mnt/image/root/

# Unmount filesystem
sync
umount /mnt/image
