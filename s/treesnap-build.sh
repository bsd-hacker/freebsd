#!/bin/sh -e

# No user-serviceable parts
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e treesnap-build.sh TREEREV DESCRIBES WORKDIR SNAPDIR
TREEREV="$1"
DESCRIBES="$2"
WORKDIR="$3"
SNAP="$4"

# Temporary directories and mount points
PORTSDIR=${WORKDIR}/ports
TMP=${WORKDIR}/tmp
JAILDIR=${WORKDIR}/jail

# Create mount points
mkdir ${PORTSDIR} ${TMP} ${JAILDIR}

# Create and mount memory disk for holding exported ports tree
PORTSMD=`mdconfig -a -t swap -s ${PORTSMDSIZE} -n`
newfs -f 512 -i 2048 -O 1 -n /dev/md${PORTSMD} >/dev/null
mount /dev/md${PORTSMD} ${PORTSDIR}

# Export ports tree
echo "`date`: Exporting \"${TREEREV}\" ports tree"
svn export -q --force ${REPO}/${TREEREV} ${PORTSDIR}
df -i ${PORTSDIR}

# Create snapshot
echo "`date`: Building snapshot tarballs"
sh -e s/treesnap-mktars-all.sh ${PORTSDIR} ${SNAP} ${SNAP}/INDEX ${TMP}

# Unmount the ports tree
while ! umount /dev/md${PORTSMD}; do
	sleep 1
done

# Perform index describes
for N in ${DESCRIBES}; do
	echo "`date`: Starting describe run for ${N}.x"
	if ! sh -e s/describes-run.sh /dev/md${PORTSMD} $WORLDTAR $JAILDIR	\
	    ${N}99999 ${SNAP}/DESCRIBE.${N} 2>${SNAP}/DESCRIBE.${N}.err; then
		echo "`date`: ${N}.x describes failed"
		cat ${SNAP}/DESCRIBE.${N}.err
		rm ${SNAP}/DESCRIBE.${N}
	else
		rm ${SNAP}/DESCRIBE.${N}.err
	fi
done

# Delete the ports tree disk
mdconfig -d -u ${PORTSMD}

# Clean up
rmdir ${PORTSDIR} ${TMP} ${JAILDIR}
