#!/bin/sh

# testsystem.sh <scratch directory> <dists directory> <output iso>

BSDINSTALL=`pwd`/..

mkdir $2

# Kernel package
cd /usr/src
mkdir $1
make installkernel DESTDIR=$1
cd $1
tar cvzf $2/kernel.tgz .
chflags -R noschg .
rm -rf $1

# World
cd /usr/src
mkdir $1
make installworld distribution DESTDIR=$1

# Install BSDinstall as though it were part of world
mkdir $1/usr/libexec/bsdinstall
cd $BSDINSTALL
make install DESTDIR=$1

cd $1
tar cvzf $2/world.tgz .
# Keep world around

# Make system
cd /usr/src
make installkernel DESTDIR=$1
mkdir $1/usr/bsdinstall-dist
cp $2/kernel.tgz $2/world.tgz $1/usr/bsdinstall-dist

# Things for the CD environment
ln -s /tmp/bsdinstall_etc/resolv.conf $1/etc/resolv.conf
echo kernel_options=\"-C\" > $1/boot/loader.conf
echo sendmail_enable=\"NONE\" > $1/etc/rc.conf
echo hostid_enable=\"NO\" >> $1/etc/rc.conf
touch $1/etc/fstab
cd $BSDINSTALL
cp release/rc.local $1/etc

sh /usr/src/release/i386/mkisoimages.sh -b FreeBSD_Install $3 $1
