#!/bin/sh

# Test scenario from D17599 "Fix for double free when deleting entries from
# epoch managed lists"
# by Hans Petter Selasky <hselasky@freebsd.org>

# Page fault in nd6_dad_timer+0x6b seen:
# https://people.freebsd.org/~pho/stress/log/ifconfig2.txt

# $FreeBSD$

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
if=`ifconfig | grep -w mtu | grep -v RUNNING | sed 's/:.*//' | head -1`
[ -z "$if" ] &&
    if=`ifconfig | \
    awk  '/^[a-z].*: / {gsub(":", ""); ifn = $1}; /no car/{print ifn; exit}'`

[ -z "$if" ] && exit 0
echo "Using $if for test."
ifconfig $if | grep -q RUNNING && running=1

sync=/tmp/`basename $0`.sync
rm -f $sync
for i in `jot 5`; do
	(
		while [ ! -f $sync ]; do
			sleep .1
		done
		while [ -f $sync ]; do
			ifconfig $if.$i create
			ifconfig $if.$i inet 224.0.0.$i
			ifconfig $if.$i destroy
		done
	) > /dev/null 2>&1 &
done
touch $sync
sleep 120
rm -f $sync
wait
[ $running ] || ifconfig $if down

exit 0
