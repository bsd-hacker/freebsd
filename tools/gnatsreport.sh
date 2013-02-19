#! /bin/sh
#
# Script to do weekly reminders of extant bug reports
#
# last modified by: linimon
#

PATH=${PATH}:/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin
export PATH

QUERYPR=/home/gnats/tools/query-pr-summary.cgi
TAGS=/home/gnats/tools/getalltags.short

SENDMAIL="/usr/sbin/sendmail -odi -fowner-bugmaster@FreeBSD.org -oem"

# set this to non-null when testing changes
#DEVELOPMENT="yes"

if [ -z "$DEVELOPMENT" ]; then
  # production:
  TO_BUGMASTER="bugmaster@FreeBSD.org"
  TO_FREEBSD_BUGS="freebsd-bugs@FreeBSD.org"
  TO_FREEBSD_DOC="freebsd-doc@FreeBSD.org"
  TO_FREEBSD_PORTS="freebsd-ports@FreeBSD.org"
else
  # development:
  TO_BUGMASTER="linimon@FreeBSD.org"
  TO_FREEBSD_BUGS="linimon@FreeBSD.org"
  TO_FREEBSD_DOC="linimon@FreeBSD.org"
  TO_FREEBSD_PORTS="linimon@FreeBSD.org"
fi

RESP=`query-pr --skip-closed | grep '^>Responsible:' | awk '{print $2}' | perl -pe 's/\@freebsd.org//i' | tr '[A-Z]' '[a-z]' | egrep -v 'gnats-|freebsd-(bugs|ports-bugs|doc|ports)' | sort -u`

# open confidential bugs report
(
  echo "From: FreeBSD bugmaster <bugmaster@freebsd.org>"
  echo "To: FreeBSD bugmaster <bugmaster@freebsd.org>"
  echo "Subject: open, unassigned, confidential bug PRs in limbo"
  echo ""
  ${QUERYPR} -q -C
) | ${SENDMAIL} ${TO_BUGMASTER}

# misfiled limbo bugs
(
  echo "From: FreeBSD bugmaster <bugmaster@freebsd.org>"
  echo "To: FreeBSD bugmaster <bugmaster@freebsd.org>"
  echo "Subject: open PRs (mis)filed to gnats-admin and in limbo"
  echo ""
  ${QUERYPR} -q -c -r gnats-admin
) | ${SENDMAIL} ${TO_BUGMASTER}

# complete bugs report
(
  echo "From: FreeBSD bugmaster <bugmaster@freebsd.org>"
  echo "To: FreeBSD bugs list <freebsd-bugs@freebsd.org>"
  echo "Subject: Current problem reports"
  echo ""
  echo "(Note: an HTML version of this report is available at"
  echo "http://www.freebsd.org/cgi/query-pr-summary.cgi .)"
  echo ""
  ${QUERYPR}
) | ${SENDMAIL} ${TO_FREEBSD_BUGS}

# unassigned ports report
(
  echo "From: FreeBSD bugmaster <bugmaster@freebsd.org>"
  echo "To: FreeBSD ports list <freebsd-ports@freebsd.org>"
  echo "Subject: Current unassigned ports problem reports"
  echo ""
  echo "(Note: an HTML version of this report is available at"
  echo "http://www.freebsd.org/cgi/query-pr-summary.cgi?category=ports .)"
  echo ""
  ${QUERYPR} -r freebsd-ports
) | ${SENDMAIL} ${TO_FREEBSD_PORTS}

# unassigned doc report
(
  echo "From: FreeBSD bugmaster <bugmaster@freebsd.org>"
  echo "To: FreeBSD doc list <freebsd-doc@freebsd.org>"
  echo "Subject: Current unassigned doc problem reports"
  echo ""
  echo "(Note: an HTML version of this report is available at"
  echo "http://www.freebsd.org/cgi/query-pr-summary.cgi?category=doc .)"
  echo ""
  ${QUERYPR} -r freebsd-doc
) | ${SENDMAIL} ${TO_FREEBSD_DOC}

# per user reports
for user in ${RESP}
do
  targ=`echo ${user} | grep @`
  if [ "${targ}" = "" ]; then
    targ=${user}@FreeBSD.org
  else
    targ=${user}
  fi
  if [ -z "$DEVELOPMENT" ]; then
    mail_to=${targ}
  else
    mail_to=${TO_BUGMASTER}
  fi
  (
    echo "From: FreeBSD bugmaster <bugmaster@freebsd.org>"
    echo "To: ${user}"
    echo "Subject: Current problem reports assigned to ${targ}"
    echo ""
    echo "Note: to view an individual PR, use:"
    echo "  http://www.freebsd.org/cgi/query-pr.cgi?pr=(number)."
    echo ""
    ${QUERYPR} -c -r ^${user}\$
  ) | ${SENDMAIL} ${mail_to}
done

# PRs with patches
(
  echo "From: FreeBSD bugmaster <bugmaster@FreeBSD.org>"
  echo "To: FreeBSD bugs list <freebsd-bugs@FreeBSD.org>"
  echo "Subject: Current problem reports containing patches"
  echo ""
  echo "(Note: an HTML version of this report is available at"
  echo "http://people.freebsd.org/~linimon/studies/prs/prs_for_tag_patch.html .)"
  echo ""
  ${QUERYPR} -q -t patch
) | ${SENDMAIL} ${TO_FREEBSD_BUGS}

# PRs sorted by tag
(
  tags=`${TAGS} | sort | uniq`
  echo "From: FreeBSD bugmaster <bugmaster@FreeBSD.org>"
  echo "To: FreeBSD bugs list <freebsd-bugs@FreeBSD.org>"
  echo "Subject: Current problem reports sorted by tag"
  echo ""
  echo "(Note: a better version of this report is available at"
  echo "http://people.freebsd.org/~linimon/studies/prs/pr_tag_index.html .)"
  echo ""
  for tag in $tags; do
    echo "Problem reports for tag '$tag':"
    ${QUERYPR} -q -T $tag
  done
) | ${SENDMAIL} ${TO_FREEBSD_BUGS}

