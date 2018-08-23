#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e fileset-findnew.sh FSETDIR INDEX tINDEX TMP
FSETDIR="$1"
INDEX="$2"
tINDEX="$3"
TMP="$4"

# Report progress
echo "`date`: Identifying new files"

# Find new port tarballs
sort -k 3,3 -t '|' ${FSETDIR}/filedb > ${TMP}/filedb.sorted
sort -k 2,2 -t '|' ${INDEX} |
    join -1 3 -2 2 -t '|' -v 2 -o 2.1,2.2 ${TMP}/filedb.sorted - |
    sort -k 1,1 -t '|' > ${FSETDIR}/filedb.news

# Report new files
echo "New files:"
cut -f 1 -d '|' < ${FSETDIR}/filedb.news |
    lam -s '    ' -

# Find new metadata files
sort -k 3,3 -t '|' ${FSETDIR}/metadb > ${TMP}/metadb.sorted
sort -k 2,2 -t '|' ${tINDEX} |
    join -1 3 -2 2 -t '|' -v 2 -o 2.1,2.2 ${TMP}/metadb.sorted - |
    sort -k 1,1 -t '|' > ${FSETDIR}/metadb.news

# Clean up temporary files
rm ${TMP}/filedb.sorted ${TMP}/metadb.sorted
