#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e describes-err.sh SNAP DESCRIBES
SNAP="$1"
DESCRIBES="$2"

# Check that the required describes files exist
for N in $DESCRIBES; do
	if ! [ -f ${SNAP}/DESCRIBE.${N} ]; then
		echo "DESCRIBE.${N} does not exist!"
		exit 1
	fi
done
