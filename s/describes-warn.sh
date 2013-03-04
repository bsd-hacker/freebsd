#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e describes-warn.sh SNAP NEWREV DESCDIR DESCRIBES
SNAP="$1"
NEWREV="$2"
DESCDIR="$3"
DESCRIBES="$4"

# Haven't found any broken describes yet
BROKEN=0

# Check if any describes broke
for N in ${DESCRIBES}; do
	# Did this one break?
	if [ -f ${SNAP}/DESCRIBE.${N}.err ]; then
		# Yep, remember this
		BROKEN=1

		# Launch an ICBM if this is new breakage
		if [ -f ${DESCDIR}/indexok ]; then
			sh -e s/describes-icbm.sh `cat ${DESCDIR}/indexok` \
			    ${NEWREV} ${SNAP}/DESCRIBE.${N}.err |
			    sendmail -t
			rm ${DESCDIR}/indexok
		fi

		# Delete the error messages
		rm ${SNAP}/DESCRIBE.${N}.err
	fi
done

# If the tree is non-broken but was previously broken, send an email
if [ ${BROKEN} = 0 ] && ! [ -f ${DESCDIR}/indexok ]; then
	sendmail -t <<- EOF
		From: ${INDEXMAIL_FROM}
		To: ${INDEXMAIL_TO}
		Subject: INDEX build fixed

		EOF
fi

# If the tree is non-broken, record the new index-ok-at SVN #
if [ ${BROKEN} = 0 ]; then
	echo ${NEWREV} > ${DESCDIR}/indexok
fi
