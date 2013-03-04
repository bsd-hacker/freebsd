#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e treesnap-mktars.sh DIR TARTREE SNAP TMP ...
DIR="$1"
TARTREE="$2"
SNAP="$3"
TMP="$4"

# Create temporary files
OUTFILE=`mktemp "${TMP}/index.XXXXXX"`
F_OBJ=`mktemp "${TMP}/f_obj.XXXXXX"`
F_HASH=`mktemp "${TMP}/f_hash.XXXXXX"`

# Skip past fixed command-line args to list of objects in tree
shift 4

# Create tarballs
for OBJ in "$@"; do
	if [ -d ${DIR}/${OBJ} ]; then
		( cd ${DIR}/${OBJ} && find . -type f ) |
		    sort |
		    ${TAR} -cf ${TARTREE}/${OBJ} -T- -C ${DIR}/${OBJ} -s '|./||'
		echo "${OBJ}/"
	else
		${TAR} -cf ${TARTREE}/${OBJ} -C ${DIR} ${OBJ}
		echo "${OBJ}"
	fi
done > ${F_OBJ}

# Compute SHA256 hashes
( cd ${TARTREE} && sha256 -r "$@" ) > ${F_HASH}

# Compress all the files
( cd ${TARTREE} && gzip -9n "$@" )

# Move files into place
while read HASH OBJ; do
	mv ${TARTREE}/${OBJ}.gz ${SNAP}/${HASH}.gz
done < ${F_HASH}

# Produce the partial index we need
cut -f 1 -d ' ' ${F_HASH} |
    lam ${F_OBJ} -s '|' - > ${OUTFILE}

# Report back the name of our partial index
echo ${OUTFILE}
