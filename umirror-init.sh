#!/bin/sh

# $FreeBSD$

# Check parameters
PRIVDIR=$1
PUBDIR=$2
if [ -z "${PRIVDIR}" -o -z "${PUBDIR}" ]; then
	echo "usage: $0 <privdir> <pubdir>"
	exit 1
fi

# Generate needed directories
if ! mkdir -p "${PRIVDIR}"; then
	echo "Error creating private directory: ${PRIVDIR}"
	exit 1
fi
chmod 700 ${PRIVDIR}
if ! mkdir -p "${PUBDIR}"; then
	echo "Error creating public directory: ${PUBDIR}"
	exit 1
fi

# Generate private key
if ! openssl genrsa -F4 4096 > "${PRIVDIR}/priv.ssl"; then
	echo "Error generating private key"
	exit 1
fi

# Create file list
if ! touch "${PRIVDIR}/flist"; then
	echo "Error creating file list"
	exit 1
fi

# Generate public key
if ! openssl rsa -in "${PRIVDIR}/priv.ssl" -pubout		\
    2>/dev/null >"${PRIVDIR}/pub.ssl"; then
	echo "Error computing public key"
	exit 1
fi

# Print public keyprint
echo -n "Public key hash: "
sha256 -q "${PRIVDIR}/pub.ssl"
