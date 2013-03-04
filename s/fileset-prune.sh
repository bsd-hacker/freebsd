#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e fileset-prune.sh SNAPDATE FSETDIR TMP
SNAPDATE="$1"
FSETDIR="$2"
TMP="$3"

# Report progress
echo "`date`: Removing old files and database entries"

# Sort list of files so we can use comm(1) on it
sort ${FSETDIR}/filedb > ${TMP}/filedb.sorted

# Find lines corresponding to files we don't want any more
awk -F \| -v cutoff=`expr ${SNAPDATE} - ${MAXAGE_DATA}`			\
    '{ if ($2 < cutoff) { print } }' ${TMP}/filedb.sorted > ${TMP}/filedb.olds

# Delete old files
cut -f 3 -d '|' ${TMP}/filedb.olds |
    grep -E '^[0-9a-f]{64}$' |
    lam -s "${FSETDIR}/oldfiles/" - -s '.gz' |
    xargs rm -f

# Construct a new old-files list
comm -23 ${TMP}/filedb.sorted ${TMP}/filedb.olds |
    sort -k 1,1 -t '|' > ${FSETDIR}/filedb.tmp
mv ${FSETDIR}/filedb.tmp ${FSETDIR}/filedb

# Remove old lines from metadb and extradb
echo "`date`: Removing old metadata and extra database entries"
awk -F \| -v cutoff=`expr ${SNAPDATE} - ${MAXAGE_META}`			\
    '{ if ($2 >= cutoff) { print } }' ${FSETDIR}/metadb > ${FSETDIR}/metadb.tmp
mv ${FSETDIR}/metadb.tmp ${FSETDIR}/metadb
awk -F \| -v cutoff=`expr ${SNAPDATE} - ${MAXAGE_EXTRA}`			\
    '{ if ($1 >= cutoff) { print } }' ${FSETDIR}/extradb > ${FSETDIR}/extradb.tmp
mv ${FSETDIR}/extradb.tmp ${FSETDIR}/extradb

# Remove temporary files
rm ${TMP}/filedb.sorted ${TMP}/filedb.olds
