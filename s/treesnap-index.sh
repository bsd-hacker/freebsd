#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e treesnap-index.sh SNAP INDEX tINDEX
SNAP="$1"
INDEX="$2"
tINDEX="$3"

# Let us see how much space we're using on this memory disk
df -i ${SNAP}

# Report what we're doing
echo "`date`: Indexing snapshot metadata"

# Grab the INDEX file out
cp ${SNAP}/INDEX $INDEX

# Hash and compress the metadata
for F in `cd ${SNAP} && ls INDEX DESCRIBE.*`; do
	H=`sha256 -q ${SNAP}/${F}`
	mv ${SNAP}/${F} ${SNAP}/${H}
	gzip -9fn ${SNAP}/${H}
	echo "${F}|${H}"
done > $tINDEX
