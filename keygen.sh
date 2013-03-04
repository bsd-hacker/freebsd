#!/bin/sh -e
set -e

# usage: sh -e keygen.sh

# Load configuration
. build.conf

# Create temporary working space
mkdir ${STATEDIR}/work

# Create key
openssl genrsa -F4 2048 > ${STATEDIR}/work/priv.ssl
openssl rsa -in ${STATEDIR}/work/priv.ssl -pubout > ${STATEDIR}/work/pub.ssl

# Compute key hash
KEYHASH=`sha256 -q ${STATEDIR}/work/pub.ssl`

# Move files into their permanent location
mv ${STATEDIR}/work/priv.ssl ${STATEDIR}/keys/priv-${KEYHASH}.ssl
mv ${STATEDIR}/work/pub.ssl ${STATEDIR}/keys/pub-${KEYHASH}.ssl

# Announce the public key hash
cat <<- EOF

	A key has been generated with hash:
	  ${KEYHASH}

EOF
xargs -s 80 <<- EOF
	Add this to the SIGNKEYS list in build.conf to start signing builds
	with it; and set it as the KEYPRINT value in /etc/portsnap.conf on
	systems which need to use these updates.
EOF

# Remove our temporary working directory
rmdir ${STATEDIR}/work
