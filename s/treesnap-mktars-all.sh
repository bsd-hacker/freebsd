#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e treesnap-mktars-all.sh DIR SNAP INDEX TMP
DIR="$1"
SNAP="$2"
INDEX="$3"
TMP="$4"

# Temporary staging trees
TMP2=`mktemp -d "${TMP}/tmp.XXXXXX"`
TARTREE=`mktemp -d "${TMP}/tartree.XXXXXX"`

# Create subdirectories under TARTREE
( cd "${DIR}" && find . -maxdepth 1 -type d -depth 1 ) |
    ( cd "${TARTREE}" && xargs mkdir )

# Build snapshot and index
( cd "${DIR}" && find . -maxdepth 2 \( -depth 2 -or \! -type d \) ) |
    cut -f 2- -d / |
    xargs -P ${JNUM} -n 200 sh -e s/treesnap-mktars.sh	\
	"${DIR}" "${TARTREE}" "${SNAP}" "${TMP2}" |
    xargs cat |
    sort -k 1,1 -t '|' > "${INDEX}"

# Clean up temporary directories
rm -r ${TMP2} ${TARTREE}
