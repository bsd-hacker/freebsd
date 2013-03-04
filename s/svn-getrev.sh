#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e svn-getrev.sh TARGET
TARGET=$1

# Get the latest revision #
svn info ${REPO}/${TARGET} |
    grep '^Last Changed Rev:' |
    cut -f 2 -d : |
    cut -f 2 -d ' ' |
    grep -E '^[0-9]+$'
