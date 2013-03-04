#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e treesnap-publishsigs.sh SIGDIR STAGE NAME
SIGDIR="$1"
STAGE="$2"
NAME="$3"

# Copy signatures and keys
for KEY in ${SIGNKEYS}; do
	cp ${SIGDIR}/${KEY}.ssl ${STAGE}/${NAME}-${KEY}.ssl
	cp ${STATEDIR}/keys/pub-${KEY}.ssl ${STAGE}/pub-${KEY}.ssl
done

# Copy backwards-compatible signature
cp ${SIGDIR}/${SIGNKEYS_PRIMARY}.ssl ${STAGE}/${NAME}.ssl
cp ${STATEDIR}/keys/pub-${SIGNKEYS_PRIMARY}.ssl ${STAGE}/pub.ssl
