#!/bin/sh
#
# $FreeBSD$
#

# TODO: add support for ports/ and doc/ tree

usage() {
	echo "Usage:"
	echo "${0} -b <branch>"
	exit 1
}

get_build_rev() {
	if [ -e "${svnfile}" -a ! -f "${svnfile}" ]; then
		echo "SVN file is not a regular file."
		echo "Renaming file."
		mv ${svnfile} ${svnfile}.bak.${today}
	fi
	svnrev=$(svn info ${svnhost}/${branch} | \
		awk -F ': ' '/^Last Changed Rev/ {print $2}')
	svnrev=$(echo ${svnrev} | tr -d '[a-z]')
	echo ${svnrev} > ${svnfile}
}

get_build_date() {
	if [ -e "${datefile}" -a ! -f "${datefile}" ]; then
		echo "SVN file is not a regular file."
		echo "Renaming file."
		mv ${svnfile} ${svnfile}.bak.${today}
	fi
	echo ${today} > ${datefile}
}

main() {
	export PATH="/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin"
	export TZ='UTC'
	export __BUILDCONFDIR="$(dirname $(realpath ${0}))"
	svnhost="svn://svn.FreeBSD.org/base"
	svnfile="${__BUILDCONFDIR}/svnrev_src"
	datefile="${__BUILDCONFDIR}/builddate"
	today="$(date +%Y%m%d)"

	while getopts "b:" opt; do
		case ${opt} in
			b)
				branch="${OPTARG}"
				;;
			*)
				;;
		esac
	done

	if [ -z "${branch}" ]; then
		echo "Branch not specified."
		usage
	fi

	get_build_rev
	get_build_date
}

main "$@"
