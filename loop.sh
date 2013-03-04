#!/bin/sh
set -e

# Load configuration
. build.conf

# Loop until we fail or the looping is stopped administratively
while ! [ -f failed ] && ! [ -f adminlock ]; do
	# Figure out if this is the first build of a new day
	touch -t `date "+%Y%m%d0000"` ${STATEDIR}/midnight
	if ! [ ${STATEDIR}/lastsnap -nt ${STATEDIR}/midnight ]; then
		BUILDTYPE=snap
		touch ${STATEDIR}/lastsnap
	else
		BUILDTYPE=update
	fi
	rm ${STATEDIR}/midnight

	# Send an email
	(
		echo "From: ${BUILDMAIL_FROM}"
		for ADDR in ${BUILDMAIL_TO}; do
			echo "To: ${ADDR}"
		done
		echo "Subject: `hostname` Portsnap build.sh ${BUILDTYPE}"
		echo

		# Do the build
		if ! sh -e build.sh ${BUILDTYPE} 2>&1; then
			touch failed;
		else
			# Upload if the build succeeded
			sh upload.sh 2>&1 || true
		fi

		# Once a day, clean up portsnap-master
		if [ ${BUILDTYPE} = "snap" ]; then
			echo "`date`: Cleaning bits on portsnap-master"
			ssh -i ${UPLOAD_KEY} ${UPLOAD_ACCT}	\
			    sh portsnap-clean.sh 2>&1
		fi
	) | sendmail -t
done

# Send a warning if builds stop running
echo "Subject: `hostname` Portsnap builds no longer running!" |
    sendmail ${BUILDMAIL_TO}
