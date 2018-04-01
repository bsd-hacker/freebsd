#!/bin/sh

# Extract and install stress2, a tool to find kernel errors.

# The default installation directory is /tmp/work
# Please note that stress2 was never meant to be a validation tool.

# $FreeBSD$

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1

set -e
echo -n "Enter non-root test user name: "
read testuser
id $testuser > /dev/null 2>&1 ||
    { echo "user \"$testuser\" not found."; exit 1; }
[ $testuser ] || exit 1
work=${work:-/tmp/work}
mkdir -p $work
cd $work
echo "Extracting stress2 to $work"
[ -d stress2 ] && rm -rf stress2
svnlite checkout -q svn://svn.freebsd.org/base/user/pho/stress2
cd stress2
echo "testuser=$testuser" > `hostname`
make > /dev/null
echo "Tests to run are in $work/stress2/misc.
To run all tests, type ./all.sh -on
To run for example all tmpfs tests, type ./all.sh -on \`grep -l tmpfs *.sh\`
To run fdatasync.sh for one hour, type ./all.sh -m 60 fdatasync.sh"
