#!/bin/sh -e
set -e

# usage: sh -e setup.sh

# Load configuration
. build.conf

# Make the state directory
mkdir ${STATEDIR}
chmod 700 ${STATEDIR}

# Describes state
mkdir ${STATEDIR}/describes

# Instruct the user about unbuilt DESCRIBE files
UNBUILT=""
for N in ${DESCRIBES_PUBLISH}; do
	NEED=1
	for M in ${DESCRIBES_BUILD}; do
		if [ $N = $M ]; then
			NEED=0
		fi
	done
	if [ $NEED = 1 ]; then
		UNBUILT="${UNBUILT} DESCRIBE.${N}"
	fi
done
xargs -s 80 <<- EOF
	The files ${UNBUILT} are set to be published but are not in the
	list to be built; please create them in the directory
	${STATEDIR}/describes.  (These are probably DESCRIBE files for
	old STABLE branches which no longer supported.)
EOF
echo

# Fileset state
mkdir ${STATEDIR}/fileset ${STATEDIR}/fileset/oldfiles
touch ${STATEDIR}/fileset/filedb
touch ${STATEDIR}/fileset/metadb
touch ${STATEDIR}/fileset/extradb

# Instruct the user about the need for a world tarball
xargs -s 80 <<- EOF
	If you haven\\'t already done so, please create a .tar file in
	${WORLDTAR} containing the portion of the FreeBSD world needed
	for \\'make describe\\' to run.
EOF

# Create a directory for keys
mkdir ${STATEDIR}/keys

# Instruct the user about creating keys
echo
xargs -s 80 <<- EOF
	Before you can perform Portsnap builds, you need to run keygen.sh
	to create a signing key.
EOF
# Create a staging area for files waiting to be uploaded
mkdir ${STATEDIR}/stage
mkdir ${STATEDIR}/stage/f
mkdir ${STATEDIR}/stage/bp
mkdir ${STATEDIR}/stage/t
mkdir ${STATEDIR}/stage/s

