#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e describes-run.sh PORTSDISK WORLDTAR JAILDIR OSVERSION DESCFILE
PORTSDISK="$1"
WORLDTAR="$2"
JAILDIR="$3"
OSVERSION="$4"
DESCFILE="$5"

# helper function
findruleset () {
        jot 0 |
            while read N; do
                if ! devfs rule -s ${N} show | grep -q .; then
                        echo ${N}
                        break
                fi
        done
}

# Create memory disks and format filesystems
JAILMD=`mdconfig -a -t swap -s ${JAILMDSIZE} -n`
TMPMD=`mdconfig -a -t swap -s ${TMPMDSIZE} -n`
newfs -O 1 -n /dev/md${JAILMD} >/dev/null
newfs -O 1 -n /dev/md${TMPMD} >/dev/null

# Mount filesystems under jail mount point
mount -o noatime,nosuid /dev/md${JAILMD} ${JAILDIR}
mkdir ${JAILDIR}/tmp
mount -o noatime,nosuid,noexec /dev/md${TMPMD} ${JAILDIR}/tmp
chmod 1777 ${JAILDIR}/tmp
mkdir ${JAILDIR}/usr ${JAILDIR}/usr/ports
mount -o noatime,nosuid,noexec,ro ${PORTSDISK} ${JAILDIR}/usr/ports

# Attach device filesystem with null, fd/*, and std*
mkdir ${JAILDIR}/dev
mount -t devfs devfs ${JAILDIR}/dev
RULESET=`findruleset`
devfs rule -s ${RULESET} add hide
devfs rule -s ${RULESET} add path null unhide
devfs rule -s ${RULESET} add path fd unhide
devfs rule -s ${RULESET} add path 'fd/*' unhide
devfs rule -s ${RULESET} add path 'std*' unhide
devfs -m ${JAILDIR}/dev ruleset ${RULESET}
devfs -m ${JAILDIR}/dev rule applyset

# Extract world tarball
tar -xf ${WORLDTAR} -C ${JAILDIR}

# Protect against naughtiness
mount -u -o noatime,nosuid,ro /dev/md${JAILMD}

# Build the describes output
if env - PATH=${PATH} jail -c path=${JAILDIR} host.hostname=localhost	\
    exec.jail_user=nobody exec.system_jail_user command=/bin/sh -e	\
    > ${DESCFILE} <<- EOF
	export __MAKE_CONF=/nonexistant
	export OSVERSION=${OSVERSION}
	export PORTOBJFORMAT=elf
	export INDEX_TMPDIR=/tmp
	export WRKDIRPREFIX=/tmp
	export BUILDING_INDEX=1
	export LOCALBASE=/removeme/usr/local
	export ECHO_MSG="echo >/dev/null"
	cd /usr/ports && make describe -j ${JNUM} 1>&2
	cd /tmp && cat INDEX* | sed -e 's/  */ /g' -e 's/|  */|/g'      \
	    -e 's/  *|/|/g' -e 's./removeme..g' |                       \
	    sed -E -e ':x' -e 's|/[^/]+/\.\.||' -e 'tx' |               \
	    sort -k 1,1 -t '|' | tr -d '\r'
EOF
then
	R=0
else
	R=1
fi

# Clean up
while ! umount ${JAILDIR}/dev; do
	sleep 1
done
while ! umount ${JAILDIR}/tmp; do
	sleep 1
done
while ! umount ${JAILDIR}/usr/ports; do
	sleep 1
done
while ! umount ${JAILDIR}; do
	sleep 1
done
mdconfig -d -u ${JAILMD}
mdconfig -d -u ${TMPMD}
devfs rule -s ${RULESET} delset

# Test if make_index works
if [ ${R} = 0 ] && ! /usr/libexec/make_index ${DESCFILE} >/dev/null; then
	R=1
fi

# Return success/failure
exit ${R}
