#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e treesnap-sign.sh SNAPDATE tINDEX SIGDIR
SNAPDATE="$1"
tINDEX="$2"
SIGDIR="$3"

# Figure out the tINDEX hash
SNAPSHOTHASH=`sha256 -q ${tINDEX}`

# Generate signatures
for KEY in ${SIGNKEYS}; do
	echo "portsnap|${SNAPDATE}|${SNAPSHOTHASH}" |	\
	    openssl rsautl -inkey ${STATEDIR}/keys/priv-${KEY}.ssl	\
	    -sign > ${SIGDIR}/${KEY}.ssl
done
