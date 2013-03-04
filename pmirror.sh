#!/bin/sh -e

#-
# Copyright 2005 Colin Percival
# All rights reserved
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted providing that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
# IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# $FreeBSD$

# READ THIS BEFORE USING THIS CODE
# --------------------------------
#
# On average, portsnap requires 2-5MB/month of bandwidth to keep a
# single machine up to date.  If several machines are sharing an
# HTTP proxy, a significant fraction of this can be cached.
#
# In contrast, using this code to keep a portsnap *mirror* up to
# date requires roughly 1GB of disk space and 5GB/month of bandwidth.
# This is because of the "graceful failure" mechanisms built into
# portsnap -- it can usually take advantage of pregenerated patches,
# but a mirror needs to have lots of larger files just in case they
# are needed.
#
# This means that, in terms of bandwidth, running a portsnap mirror
# is completely and utterly pointless unless you expect more than
# 1000 portsnap-running systems to be using the mirror.  In fact,
# it's worse than pointless, since it would consume bandwidth and
# increase the load on existing mirrors (since the mirroring would
# require more work than serving those <1000 machines from the
# existing mirrors).
#
# For reference, the number of systems running portsnap at the end
# of 2005 is roughly 4500.
#
# In short: Even if you already run FreeBSD CVSup, WWW, and FTP
# mirrors, you shouldn't necessarily start running a portsnap mirror
# as well.  Please talk to me (cperciva@FreeBSD.org) before you
# start chewing up bandwidth.

# Usage:
# lockf -s -t 0 lockfile	\
#	sh -e pmirror.sh portsnap-master.freebsd.org /path/to/www

if [ $# -ne 2 ]; then
	echo "Usage: pmirror.sh portsnap-master.freebsd.org /path/to/www"
	exit 1
fi

WRKDIR=`mktemp -d -t pmirror` || exit 1
chown :`id -ng` ${WRKDIR}
cd ${WRKDIR}

SERVER=$1
PUBDIR=$2
PHTTPGET="/usr/libexec/phttpget ${SERVER}"

export HTTP_USER_AGENT="pmirror/0.9"

# If ${PUBDIR}/pub.ssl does not exist, assume we have an empty
# mirror directory and set things up.
if ! [ -f ${PUBDIR}/pub.ssl ]; then
	mkdir -p ${PUBDIR} ${PUBDIR}/bp ${PUBDIR}/f	\
	    ${PUBDIR}/s ${PUBDIR}/t ${PUBDIR}/tp
	touch ${PUBDIR}/latest.ssl
	echo 'User-agent: *' > ${PUBDIR}/robots.txt
	echo 'Disallow: /' >> ${PUBDIR}/robots.txt
fi

${PHTTPGET} pub.ssl snapshot.ssl latest.ssl 2>&1 |
	grep -v "200 OK" || true
[ -f pub.ssl -a -f snapshot.ssl -a -f latest.ssl ]

if cmp -s latest.ssl ${PUBDIR}/latest.ssl; then
	cd /tmp/
	rm -r ${WRKDIR}
	exit 0
fi

echo "`date`: Fetching binary files list"
rm -f bl.gz bl bp.wanted bp.present
fetch -q http://${SERVER}/bl.gz
[ -f bl.gz ] || exit 1
gunzip -c bl.gz > bl

echo "`date`: Constructing list of binary patches wanted"
LASTSNAP=`cut -f 2 -d '|' bl | grep -E '^[0-9]+$' | sort -urn | head -1`
awk -F \| -v cutoff=`expr ${LASTSNAP} - 86400`		\
	'{ if ($2 > cutoff) { print } }' bl |
	join -t '|' bl - |
	awk -F \| '{ if ($4 > $2) { print $3 "-" $5 } }' |
	sort | grep -E '^[0-9a-f]{64}-[0-9a-f]{64}$' > bp.wanted
( cd ${PUBDIR}/bp/ && ls ) |
	grep -E '^[0-9a-f]{64}-[0-9a-f]{64}$' > bp.present || true
echo "`date`: Fetching needed binary patches"
comm -13 bp.present bp.wanted | lam -s 'bp/' - |
	( cd ${PUBDIR}/bp/ && xargs ${PHTTPGET} ) 2>&1 |
	grep -v "200 OK" || true
echo "`date`: Removing unneeded binary patches"
comm -23 bp.present bp.wanted | ( cd ${PUBDIR}/bp/ && xargs rm )

echo "`date`: Fetching metadata files list"
rm -f tl.gz tl
fetch -q http://${SERVER}/tl.gz
[ -f tl.gz ] || exit 1
gunzip -c tl.gz > tl

echo "`date`: Constructing list of files wanted"
awk -F \| -v cutoff=`expr ${LASTSNAP} - 86400`		\
	'{ if ($2 > cutoff) { print $3 ".gz" } }' bl |
	grep -E '^[0-9a-f]{64}\.gz$' > f.wanted || true
awk -F \| -v cutoff=`expr ${LASTSNAP} - 691200`		\
	'{ if ($2 > cutoff) { print $3 ".gz" } }' tl |
	grep -E '^[0-9a-f]{64}\.gz$' >> f.wanted || true
sort f.wanted > f.wanted.tmp
mv f.wanted.tmp f.wanted
( cd ${PUBDIR}/f/ && ls ) |
	grep -E '^[0-9a-f]{64}\.gz$' > f.present || true
echo "`date`: Fetching needed files"
comm -13 f.present f.wanted | lam -s 'f/' - |
	( cd ${PUBDIR}/f/ && xargs ${PHTTPGET} ) 2>&1 |
	grep -v "200 OK" || true
echo "`date`: Removing corrupt files"
comm -13 f.present f.wanted | tr -d '.gz' | while read F; do
	if [ -f ${PUBDIR}/f/${F}.gz ] &&
	    ! [ `gunzip < ${PUBDIR}/f/${F}.gz` | sha256` = $F ]; then
		echo "Deleting f/$F.gz"
		rm ${PUBDIR}/f/${F}.gz
	fi
done
echo "`date`: Removing unneeded files"
comm -23 f.present f.wanted | ( cd ${PUBDIR}/f/ && xargs rm )

echo "`date`: Fetching extra files list"
rm -f el.gz el
fetch -q http://${SERVER}/el.gz
[ -f el.gz ] || exit 1
gunzip -c el.gz > el

echo "`date`: Constructing list of snapshots wanted"
grep -E '^s/' el | cut -f 2 -d '/' |
	sort | grep -E '^[0-9a-f]{64}\.tgz$' > s.wanted || true
( cd ${PUBDIR}/s/ && ls ) |
	grep -E '^[0-9a-f]{64}\.tgz$' > s.present || true
echo "`date`: Fetching needed snapshots"
comm -13 s.present s.wanted | lam -s 's/' - |
	( cd ${PUBDIR}/s/ && xargs ${PHTTPGET} ) 2>&1 |
	grep -v "200 OK" || true
echo "`date`: Removing unneeded snapshots"
comm -23 s.present s.wanted | ( cd ${PUBDIR}/s/ && xargs rm )

echo "`date`: Constructing list of tags wanted"
grep -E '^t/' el | cut -f 2 -d '/' |
	sort | grep -E '^[0-9a-f]{64}$' > t.wanted || true
( cd ${PUBDIR}/t/ && ls ) |
	grep -E '^[0-9a-f]{64}$' > t.present || true
echo "`date`: Fetching needed tags"
comm -13 t.present t.wanted | lam -s 't/' - |
	( cd ${PUBDIR}/t/ && xargs ${PHTTPGET} ) 2>&1 |
	grep -v "200 OK" || true

# Don't bother deleting old tag files.  They don't take up any
# significant space, and keeping them is useful for statistical
# purposes.
# echo "`date`: Removing unneeded tags"
# comm -23 t.present t.wanted | ( cd ${PUBDIR}/t/ && xargs rm )

echo "`date`: Constructing list of metadata patches wanted"
awk -F \| -v cutoff=`expr ${LASTSNAP} - 86400`		\
	'{ if ($2 > cutoff) { print } }' tl |
	join -t '|' tl - |
	awk -F \| '{ if ($4 > $2) { print $3 "-" $5 ".gz" } }' |
	sort | grep -E '^[0-9a-f]{64}-[0-9a-f]{64}\.gz$' > tp.wanted || true
awk -F \| -v cutoff=`expr ${LASTSNAP} - 86400`		\
	'{ if ($2 > cutoff) { print } }' tl |
	join -t '|' tl - |
	fgrep "|${LASTSNAP}|" |
	awk -F \| '{ if ($4 > $2) { print $3 "-" $5 ".gz" } }' |
	sort | grep -E '^[0-9a-f]{64}-[0-9a-f]{64}\.gz$' > tp.needed || true
( cd ${PUBDIR}/tp/ && ls ) |
	grep -E '^[0-9a-f]{64}-[0-9a-f]{64}\.gz$' > tp.present || true

echo "`date`: Generating needed metadata patches"
# This generates lines of the form RECENTHASH|OLDHASH|NEWHASH,
# where RECENTHASH is the most recent metadata file of the same
# type which existed prior to this mirroring run.
# This list is also sorted starting with the most recent OLDHASH.
#
# If there are no existing metadata files of the relevant type
# then the metadata patches won't be created.  Sorry.  They'll
# all be created the next time.

sort -k 3 -t '|' tl > tl.sorted

cut -f 1 -d '.' f.present |
	join -2 3 -t '|' - tl.sorted |
	sort -k 3 -t '|' |
	perl -e '
		while (<>) {
			@_ = split /\|/;
			$l{$_[1]} = $_[0]
		};
		for $f (sort(keys %l)) {
			print "$f|$l{$f}\n"
		}' > metadata.latest

comm -13 tp.present tp.needed |
	cut -f 1 -d '.' |
	tr '-' '|' |
	join -o 1.1,1.2,2.1,2.2 -1 3 -t '|' tl.sorted - |
	sort |
	join -o 1.2,2.2,2.3,2.4 -t '|' metadata.latest - |
	sort -rn -k 2 -t '|' |
	cut -f 1,3,4 -d '|' |
while read LINE; do
	X=`echo ${LINE} | cut -f 2 -d '|'`
	Y=`echo ${LINE} | cut -f 3 -d '|'`
	M=`echo ${LINE} | cut -f 1 -d '|'`

	if [ ! -f "${PUBDIR}/tp/${X}-${M}.gz" ] ||
	    [ ! -f "${PUBDIR}/tp/${M}-${Y}.gz" ]; then
		gunzip -c < ${PUBDIR}/f/${X}.gz | sort > ${X}
		gunzip -c < ${PUBDIR}/f/${Y}.gz | sort > ${Y}
		perl -e '
			open F, $ARGV[0];
			open G, $ARGV[1];
			$s = <F>;
			$t = <G>;
			do {
				if ($s eq $t) {
					$s = <F>;
					$t = <G>;
				} elsif ((! $t) || ($s && ($s lt $t))) {
					@s = split /\|/, $s;
					print "-$s[0]\n";
					$s = <F>;
				} else {
					print "+$t";
					$t = <G>;
				}
			} while ($s || $t)' ${X} ${Y} |
			sort -k 1.2,1 -t '|' > ${X}-${Y}
		rm ${X} ${Y}
	else
		gunzip -c "${PUBDIR}/tp/${X}-${M}.gz" | sort -r |
			sort -s -k 1.2,1 -t '|' > ${X}-${M}
		gunzip -c "${PUBDIR}/tp/${M}-${Y}.gz" | sort -r |
			sort -s -k 1.2,1 -t '|' > ${M}-${Y}
		perl -e '
			open F, $ARGV[0];
			open G, $ARGV[1];
			$s = <F>;
			$t = <G>;
			while ($s || $t) {
				chomp $s;
				chomp $t;

				if (! $t) {
					print "$s\n";
					$s = <F>;
					next;
				};
				if (! $s) {
					print "$t\n";
					$t = <G>;
					next;
				};

				@s = split //, $s, 2;
				@s2 = split /\|/, $s[1];
				@t = split //, $t, 2;
				@t2 = split /\|/, $t[1];

				if ($s2[0] lt $t2[0]) {
					print "$s\n";
					$s = <F>;
					next;
				};
				if ($s2[0] gt $t2[0]) {
					print "$t\n";
					$t = <G>;
					next;
				};

				if ($s[0] eq "-") {
					print "$s\n";
				} else {
					$t = <G>;
				};
				$s = <F>;
			}' ${X}-${M} ${M}-${Y}		\
			> ${X}-${Y}
		rm ${X}-${M} ${M}-${Y}
	fi

	gzip -9n ${X}-${Y}
	mv ${X}-${Y}.gz ${PUBDIR}/tp/
done

echo "`date`: Removing unneeded metadata patches"
comm -23 tp.present tp.wanted | ( cd ${PUBDIR}/tp/ && xargs rm )

echo "`date`: Publishing file lists and signatures"
mv bl.gz el.gz tl.gz ${PUBDIR}
mv latest.ssl pub.ssl snapshot.ssl ${PUBDIR}

echo "`date`: Removing temporary files"
rm bl el tl
rm tl.sorted metadata.latest
rm bp.wanted bp.present
rm f.wanted f.present
rm s.present s.wanted
rm t.present t.wanted
rm tp.present tp.wanted tp.needed

# Remove temporary directory
cd /tmp/
rmdir ${WRKDIR}
