#!/bin/sh

SRC=$1
DEST=$2

# Copy filesystem
dd if=/dev/${SRC} of=/dev/${DEST} conv=sparse bs=1M

# Mount filesystem and install Marketplace customizations
mkdir -p /mnt/image
mount /dev/xbd7a /mnt/image
tar -cf- -C /home/ec2-user/ec2-bits/etc.marketplace . | tar -xpof- -C /mnt/image/etc
umount /dev/xbd7a
