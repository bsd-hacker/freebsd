#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e fileset-mirrorlists.sh FSETDIR STAGE
FSETDIR="$1"
STAGE="$2"

# Publish file lists
echo "`date`: Copying file lists into staging area"
gzip -c < ${FSETDIR}/filedb > ${STAGE}/bl.gz
gzip -c < ${FSETDIR}/metadb > ${STAGE}/tl.gz
cut -f 2 -d '|' ${FSETDIR}/extradb |
    sort -u |
    gzip -c > ${STAGE}/el.gz
