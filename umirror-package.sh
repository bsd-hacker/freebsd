#!/bin/sh -e

# $FreeBSD$

# Check parameters
STAGEDIR=$1
PRIVDIR=$2
PUBDIR=$3
ID=$4
if [ -z "${STAGEDIR}" -o -z "${PRIVDIR}" -o -z "${PUBDIR}" ]; then
	echo "usage: $0 <stagedir> <privdir> <pubdir> [<id>]"
	exit 1
fi

# Set an ID if we don't have one already
if [ -z "${ID}" ]; then
	ID=`date "+%s"`
fi

# Check that the files we're publishing have publishable permissions
if find "${PRIVDIR}" \! -perm -444 | grep -q .; then
	echo "Files to be published have bad permissions:"
	find "${PRIVDIR}" \! -perm -444
	echo
	echo "You should fix this before publishing them."
fi

# Create a tarball
tar -cf "${PRIVDIR}/dec-${ID}" -C "${STAGEDIR}" .

# Create an encryption key
dd if=/dev/urandom bs=1k count=1 2>/dev/null |
    tr -dc 'a-z' |
    head -c 32 > "${PRIVDIR}/key-${ID}"
echo >> "${PRIVDIR}/key-${ID}"

# Create an encrypted tarball
openssl enc -aes-256-cbc -pass "file:${PRIVDIR}/key-${ID}"	\
    < "${PRIVDIR}/dec-${ID}" > "${PRIVDIR}/tar-${ID}"

# Add line to flist
echo -n "${ID} " >> "${PRIVDIR}/flist"
sha256 -q "${PRIVDIR}/tar-${ID}" |
    tr '\n' ' ' >> "${PRIVDIR}/flist"
sha256 -q "${PRIVDIR}/dec-${ID}" >> "${PRIVDIR}/flist"

# Compute hash of flist
FHASH=`sha256 -q "${PRIVDIR}/flist"`

# Generate RSA signature
echo ${FHASH} |
    openssl rsautl -inkey "${PRIVDIR}/priv.ssl" -sign		\
	> "${PRIVDIR}/latest.ssl"

# Move encrypted tarball into place
mv "${PRIVDIR}/tar-${ID}" "${PUBDIR}/tar-${ID}"

# Delete unencrypted tarball
rm "${PRIVDIR}/dec-${ID}"

# Copy file list into place
cp "${PRIVDIR}/flist" "${PUBDIR}/flist-${FHASH}"

# Copy signature and public key into place
cp "${PRIVDIR}/latest.ssl" "${PUBDIR}/latest.ssl"
cp "${PRIVDIR}/pub.ssl" "${PUBDIR}/pub.ssl"

# Delete the contents of the staging directory
rm -r "${STAGEDIR}"
mkdir "${STAGEDIR}"
