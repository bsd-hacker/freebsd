#!/bin/sh -e
set -e

# usage: sh -e build.sh MODE
MODE=$1

# Sanity-check input
if ! [ "${MODE}" = "update" ] && ! [ "${MODE}" = "snap" ]; then
	echo "usage: sh -e build.sh (update|snap)"
	exit 1
fi

# Load configuration
. build.conf

# Create working direcories
WORKDIR=${STATEDIR}/work
SNAPDIR=${WORKDIR}/snap
TMPDIR=${WORKDIR}/tmp
SIGDIR=${WORKDIR}/sigs
mkdir ${WORKDIR} ${SNAPDIR} ${TMPDIR} ${SIGDIR}

# Record when we're starting
SNAPDATE=`date "+%s"`

# Get the latest revision # on the tree
if ! NEWREV=`sh -e s/svn-getrev.sh head`; then
	echo "Waiting 5 minutes for svn server to return"
	sleep 300
	NEWREV=`sh -e s/svn-getrev.sh head`
fi

# Create a memory disk for holding the snapshot files.
SNAPMD=`mdconfig -a -t swap -s ${SNAPMDSIZE} -n`
newfs -O 1 -n /dev/md${SNAPMD} >/dev/null
mount -onoatime,async /dev/md${SNAPMD} ${SNAPDIR}

# Build a snapshot
sh -e s/treesnap-build.sh head@${NEWREV} "${DESCRIBES_BUILD}"	\
    ${TMPDIR} ${SNAPDIR}

# Replace tarballs with "aliased" tarballs
if ! [ -z ${ALIASFILE} ]; then
	if [ ${MODE} = "snap" ]; then
		sh -e s/alias-all.sh ${SNAPDIR} ${STATEDIR}/fileset/oldfiles \
		    ${ALIASFILE} ${WORKDIR}
	else
		sh -e s/alias-index.sh ${SNAPDIR} ${STATEDIR}/fileset/oldfiles \
		    ${ALIASFILE} ${WORKDIR}
	fi
fi

# Send emails if INDEX was broken or fixed
sh -e s/describes-warn.sh ${SNAPDIR} ${NEWREV} ${STATEDIR}/describes	\
    "${DESCRIBES_BUILD}"

# Use old DESCRIBE files if the latest ones didn't build
sh -e s/describes-fallback.sh ${SNAPDIR} ${STATEDIR}/describes		\
    "${DESCRIBES_PUBLISH}"

# Collect metadata
sh -e s/treesnap-index.sh ${SNAPDIR} ${WORKDIR}/INDEX ${WORKDIR}/tINDEX

# Add these files to our (overlapping) set of snapshots
sh -e s/fileset-snap.sh ${SNAPDATE} ${STATEDIR}/fileset ${SNAPDIR}	\
    ${WORKDIR}/INDEX ${WORKDIR}/tINDEX ${STATEDIR}/stage ${TMPDIR}

# Sign the tree
sh -e s/treesnap-sign.sh ${SNAPDATE} ${WORKDIR}/tINDEX ${SIGDIR}

# Publish signatures as the "latest" build
sh -e s/treesnap-publishsigs.sh ${SIGDIR} ${STATEDIR}/stage latest

# Build a snapshot tarball if necessary
if [ ${MODE} = "snap" ]; then
	echo "`date`: Building snapshot tarball"
	SNAPSHOTHASH=`sha256 -q ${WORKDIR}/tINDEX`
	tar -czf ${STATEDIR}/stage/s/${SNAPSHOTHASH}.tgz -C ${WORKDIR} snap
	echo "${SNAPDATE}|s/${SNAPSHOTHASH}.tgz"	\
	    >> ${STATEDIR}/fileset/extradb
	sh -e s/treesnap-publishsigs.sh ${SIGDIR} ${STATEDIR}/stage snapshot
fi

# Delete signatures
rm ${SIGDIR}/*.ssl

# Unmount and delete the snapshot disk
while ! umount /dev/md${SNAPMD}; do
	sleep 1
done
mdconfig -d -u ${SNAPMD}

# Delete indexes
rm ${WORKDIR}/INDEX ${WORKDIR}/tINDEX

# Delete temporary directories
rmdir ${SIGDIR} ${TMPDIR} ${SNAPDIR} ${WORKDIR}

# Publish file lists for mirroring
sh -e s/fileset-mirrorlists.sh ${STATEDIR}/fileset ${STATEDIR}/stage

# Report sucess 
echo "`date`: Finished"

