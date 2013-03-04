#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e fileset-mkpatches.sh FSETDIR SNAP STAGE TMP
FSETDIR="$1"
SNAP="$2"
STAGE="$3"
TMP="$4"

# Report progress
echo "`date`: Building patches"

# Figure out which "OLD NEW" pairs we need
sort -k 1,1 -t '|' ${FSETDIR}/filedb |
    join -t '|' -o 1.3,2.2 - ${FSETDIR}/filedb.news |
    tr '|' ' ' > ${TMP}/patches

# Build the patches
while read X Y; do
	gunzip -c ${FSETDIR}/oldfiles/${X}.gz > ${TMP}/${X}
	gunzip -c ${SNAP}/${Y}.gz > ${TMP}/${Y}
	bsdiff ${TMP}/${X} ${TMP}/${Y} ${STAGE}/bp/${X}-${Y}
	rm ${TMP}/${X} ${TMP}/${Y}
done < ${TMP}/patches

# Clean up temporary file
rm ${TMP}/patches
