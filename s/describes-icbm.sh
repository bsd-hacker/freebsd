#!/bin/sh -e

# No user-serviceable parts          
if [ -z "$PORTSNAP_BUILD_CONF_READ" ]; then
	echo "Do not run $0 manually"
	exit 1
fi

# usage: sh -e describes-icbm.sh GOODREV BADREV ERRFILE
GOODREV="$1"
BADREV="$2"
ERRFILE="$3"

# The first potentially faulty commit is GOODREV+1
BADSTART=`expr "$GOODREV" + 1`

# Standard From/To/Subject lines
cat <<EOF
From: ${INDEXMAIL_FROM}
To: ${INDEXMAIL_TO}
Subject: INDEX build breakage
EOF

# CC people who might have broken the INDEX
jot - ${BADSTART} ${BADREV} |
    while read REV; do
	svn log -c ${REV} ${REPO} |
	    tail +2 |
	    head -1;
done |
    cut -f 2 -d '|' |
    sort -u |
    tr -d ' ' |
    lam -s 'CC: ' - -s '@freebsd.org'

# Blank line and build failure output
echo
cat ${ERRFILE}

# List potentially at-fault committers (again) and SVN history
echo
echo "Committers on the hook (CCed):"
jot - ${BADSTART} ${BADREV} |
    while read REV; do
	svn log -c ${REV} ${REPO} |
	    tail +2 |
	    head -1;
done |
    cut -f 2 -d '|' |
    sort -u |
    tr -d ' '
echo
echo "Latest SVN commits:"
svn log -r ${BADSTART}:${BADREV} ${REPO}

# Final message about when emails are sent
cat <<EOF

There may be different errors exposed by INDEX builds on other
branches, but no further emails will be sent until after the
INDEX next builds successfully on all branches.
EOF
