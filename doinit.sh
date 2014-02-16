#!/bin/sh

# Create a memory disk so we don't waste time writing to EBS
mdconfig -a -t swap -s 25G -u 0
newfs /dev/md0
mkdir -p /mnt/md
mount /dev/md0 /mnt/md
for D in ports dist; do
	rm -rf /usr/$D
	mkdir /mnt/md/$D
	ln -s /mnt/md/$D /usr/$D
done
rm -rf /var/db/portsnap
mkdir /mnt/md/portsnapdb
ln -s /mnt/md/portsnapdb /var/db/portsnap

# Fetch and extract a ports tree
/usr/bin/time -h portsnap fetch extract

# If we don't have pkg installed yet, install it now.
if ! [ -f /usr/local/sbin/pkg ]; then
	cd /usr/ports/ports-mgmt/pkg && make install clean BATCH=YES
	echo 'WITH_PKGNG=yes' >> /etc/make.conf
fi
