#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e fileset-addfiles.sh FSETDIR SNAP
FSETDIR="$1"
SNAP="$2"

# Report progress
echo "`date`: Copying files into /oldfiles/"

# Copy data files -- we don't need metadata files since we
# don't generate patches for those here
cut -f 2 -d '|' ${FSETDIR}/filedb.news |
    lam -s "${SNAP}/" - -s '.gz' |
    xargs -J % cp % ${FSETDIR}/oldfiles/
