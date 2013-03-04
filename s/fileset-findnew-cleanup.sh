#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e fileset-findnew-cleanup.sh FSETDIR
FSETDIR="$1"

# Clean up new-files lists generated be fileset-findnew.sh
rm ${FSETDIR}/filedb.news ${FSETDIR}/metadb.news
