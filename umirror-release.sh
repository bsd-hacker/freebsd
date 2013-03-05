#!/bin/sh -e

# $FreeBSD$

# Check parameters
PRIVDIR=$1
PUBDIR=$2
if [ -z "${PRIVDIR}" -o -z "${PUBDIR}" ]; then
	echo "usage: $0 <privdir> <pubdir>"
	exit 1
fi

# Move keys into the public directory
while read ID X Y; do
	if [ -f "${PRIVDIR}/key-${ID}" ]; then
		mv "${PRIVDIR}/key-${ID}" "${PUBDIR}/key-${ID}"
	fi
done < "${PRIVDIR}/flist"
