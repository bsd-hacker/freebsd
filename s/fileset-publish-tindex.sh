#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e fileset-publish-tindex.sh FSETDIR SNAPDATE tINDEX STAGE
FSETDIR="$1"
SNAPDATE="$2"
tINDEX="$3"
STAGE="$4"

# Figure out the tINDEX hash
SNAPSHOTHASH=`sha256 -q ${tINDEX}`

# Copy the tINDEX file into place in the staging directory
cp ${tINDEX} ${STAGE}/t/${SNAPSHOTHASH}

# Add to our database
echo "${SNAPDATE}|t/${SNAPSHOTHASH}" >> ${FSETDIR}/extradb
