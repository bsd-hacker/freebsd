#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e fileset-publish.sh FSETDIR SNAP STAGE
FSETDIR="$1"
SNAP="$2"
STAGE="$3"

# Report progress
echo "`date`: Copying files into staging area"

# Copy new files
cut -f 2 -d '|' ${FSETDIR}/filedb.news ${FSETDIR}/metadb.news |
    lam -s "${SNAP}/" - -s '.gz' |
    xargs -J % cp % ${STAGE}/f/
