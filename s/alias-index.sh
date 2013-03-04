#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e alias-index.sh SNAP OLDFILES ALIASFILE WORKDIR
SNAP="$1"
OLDFILES="$2"
ALIASFILE="$3"
WORKDIR="$4"

# Report progress
echo "`date`: Reverting files to aliased versions"

# Revert index
sort -k2 -t '|' ${SNAP}/INDEX |
    join -1 2 -t '|' -o 1.1,1.2,2.2 - ${ALIASFILE} > ${WORKDIR}/INDEX.remap
cut -f 1,2 -d '|' ${WORKDIR}/INDEX.remap |
    sort > ${WORKDIR}/INDEX.remap.delete
cut -f 1,3 -d '|' ${WORKDIR}/INDEX.remap |
    sort > ${WORKDIR}/INDEX.remap.add
sort ${SNAP}/INDEX |
    comm -23 - ${WORKDIR}/INDEX.remap.delete |
    sort -k 1,1 -t '|' - ${WORKDIR}/INDEX.remap.add > ${SNAP}/INDEX.new
mv ${SNAP}/INDEX.new ${SNAP}/INDEX

# Clean up
rm ${WORKDIR}/INDEX.remap
rm ${WORKDIR}/INDEX.remap.add
rm ${WORKDIR}/INDEX.remap.delete
