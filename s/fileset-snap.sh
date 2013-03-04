#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e fileset-snap.sh SNAPDATE FSETDIR SNAP INDEX tINDEX STAGE WORKDIR
SNAPDATE="$1"
FSETDIR="$2"
SNAP="$3"
INDEX="$4"
tINDEX="$5"
STAGE="$6"
WORKDIR="$7"

# Create temporary directory
TMPDIR=${WORKDIR}/tmp
mkdir ${TMPDIR}

# Prune old files from the file set
sh -e s/fileset-prune.sh ${SNAPDATE} ${FSETDIR} ${TMP}

# Figure out which files in this snapshot are new
sh -e s/fileset-findnew.sh ${FSETDIR} ${INDEX} ${tINDEX} ${TMP}

# Build binary patches and place them into the staging directory
sh -e s/fileset-mkpatches.sh ${FSETDIR} ${SNAP} ${STAGE} ${TMP}

# Make copies of old files so we can use them for future patch-building
sh -e s/fileset-addfiles.sh ${FSETDIR} ${SNAP}

# Copy new files into the staging directory
sh -e s/fileset-publish.sh ${FSETDIR} ${SNAP} ${STAGE}

# Publish the tINDEX and record it as an extra file
sh -e s/fileset-publish-tindex.sh ${FSETDIR} ${SNAPDATE} ${tINDEX} ${STAGE}

# Update databases of old files with new files
sh -e s/fileset-mergedb.sh ${FSETDIR} ${SNAPDATE} ${INDEX} ${tINDEX} ${TMP}

# Clean up new-files lists
sh -e s/fileset-findnew-cleanup.sh ${FSETDIR}

# Clean up temporary directory
rmdir ${TMPDIR}
