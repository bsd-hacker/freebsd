#!/bin/sh -e
set -e

# usage: sh -e releasesnap.sh TREE DESCRIBES TARBALL
# e.g., sh -e releasesnap.sh tags/RELEASE_9_1_0 "7 8 9" portsnap.tgz
TREE="$1"
DESCRIBES="$2"
TARBALL="$3"

# Sanity-check input
if [ -z "${TREE}" ] || [ -z "${DESCRIBES}" ] || [ -z "${TARBALL}" ]; then
	echo "usage: sh -e releasesnap.sh TREE DESCRIBES TARBALL"
	exit 1
fi

# Load configuration
. build.conf

# Get the latest revision # on the tree
NEWREV=`sh -e s/svn-getrev.sh ${TREE}`

# Create a memory disk for holding everything which will end up in
# /var/db/portsnap.  Note that for normal (head) builds we mount the disk
# on ${SNAPDIR}; we can't do that here because we want everything to be on
# a single filesystem so that hardlinks work.
SNAPMD=`mdconfig -a -t swap -s ${SNAPMDSIZE} -n`
newfs -O 1 -n /dev/md${SNAPMD} >/dev/null

# Mount the memory disk
WORKDIR=${STATEDIR}/work
mkdir ${WORKDIR}
mount -onoatime,async /dev/md${SNAPMD} ${WORKDIR}

# Build a snapshot
SNAPDIR=${STATEDIR}/work/files
mkdir ${SNAPDIR}
sh -e s/treesnap-build.sh ${TREE}@${NEWREV} "${DESCRIBES}" ${WORKDIR} ${SNAPDIR}

# Replace tarballs with "aliased" tarballs
if ! [ -z ${ALIASFILE} ]; then
	sh -e s/alias-all.sh ${SNAPDIR} ${STATEDIR}/fileset/oldfiles \
	    ${ALIASFILE} ${WORKDIR}
fi

# Make sure we have the required describe files
sh -e s/describes-err.sh ${SNAPDIR} "${DESCRIBES}"

# Collect metadata
sh -e s/treesnap-index.sh ${SNAPDIR} ${WORKDIR}/INDEX ${WORKDIR}/tINDEX

# Hard-link compressed index file and remove the uncompressed file
ln ${SNAPDIR}/`sha256 -q ${WORKDIR}/INDEX`.gz ${WORKDIR}/INDEX.gz
rm ${WORKDIR}/INDEX

# Create tag file
echo "portsnap|`date "+%s"`|`sha256 -q ${WORKDIR}/tINDEX`" > ${WORKDIR}/tag

# Create tarball
tar -czf ${TARBALL} -C ${WORKDIR} tag tINDEX INDEX.gz files

# Unmount and delete the snapshot disk
while ! umount /dev/md${SNAPMD}; do
	sleep 1
done
mdconfig -d -u ${SNAPMD}

# Delete temporary directories
rmdir ${WORKDIR}

# Report sucess
echo "`date`: Finished"
