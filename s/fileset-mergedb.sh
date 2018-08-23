#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e fileset-mergedb.sh FSETDIR SNAPDATE INDEX tINDEX TMP
FSETDIR="$1"
SNAPDATE="$2"
INDEX="$3"
tINDEX="$4"
TMP="$5"

# Report progress
echo "`date`: Updating databases"

# The filedb and metadb files contain lines of the form
# FILENAME|TIMESTAMP|HASH
# where TIMESTAMP is the *most recent* SNAPDATE for which the data
# associated with FILENAME is HASH.gz (and has SHA256 hash HASH).
# We use this to tell us which binary patches to generate, which
# also tells us which old data files to keep around; the mirrors
# use this to prune the data they publihsh and figure out which
# metadata patches they should generate.

# Update filedb using INDEX
sed -e "s/|/|${SNAPDATE}|/" ${INDEX} |
    sort -k 3,3 -t '|' > ${TMP}/new.dated
sort -k 3,3 -t '|' ${FSETDIR}/filedb |
    join -1 3 -2 3 -t '|' -v 1 -o 1.1,1.2,1.3 - ${TMP}/new.dated |
    sort -k 1,1 -t '|' - ${TMP}/new.dated > ${FSETDIR}/filedb.tmp
mv ${FSETDIR}/filedb.tmp ${FSETDIR}/filedb

# Update metadb using tINDEX
sed -e "s/|/|${SNAPDATE}|/" ${tINDEX} |
    sort -k 3,3 -t '|' > ${TMP}/new.dated
sort -k 3,3 -t '|' ${FSETDIR}/metadb |
    join -1 3 -2 3 -t '|' -v 1 -o 1.1,1.2,1.3 - ${TMP}/new.dated |
    sort -k 1,1 -t '|' - ${TMP}/new.dated > ${FSETDIR}/metadb.tmp
mv ${FSETDIR}/metadb.tmp ${FSETDIR}/metadb

# Clean up
rm ${TMP}/new.dated
