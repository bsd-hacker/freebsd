#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e describes-fallback.sh SNAP DESCDIR DESCRIBES
SNAP="$1"
DESCDIR="$2"
DESCRIBES="$3"

# For each DESCRIBE file...
for N in ${DESCRIBES}; do
	# If a DESCRIBE failed...
	if ! [ -f ${SNAP}/DESCRIBE.${N} ]; then
		# ... use an old version ...
		cp ${DESCDIR}/DESCRIBE.${N} ${SNAP}/DESCRIBE.${N}
	else
		# ... otherwise, store what we have for future reference.
		cp ${SNAP}/DESCRIBE.${N} ${DESCDIR}/DESCRIBE.${N}
	fi
done
