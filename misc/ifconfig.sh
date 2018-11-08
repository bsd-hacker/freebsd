#!/bin/sh

# Test scenario from D17599 "Fix for double free when deleting entries from
# epoch managed lists"
# by Hans Petter Selasky <hselasky@freebsd.org>

# "panic: starting DAD on non-tentative address 0xfffff8010c311000" seen.
# https://people.freebsd.org/~pho/stress/log/epoch.txt

# Fatal trap 9: general protection fault while in kernel mode
# https://people.freebsd.org/~pho/stress/log/epoch-2.txt

# $FreeBSD$

[ `id -u ` -ne 0 ] && echo "Must be root!" && exit 1
if=`ifconfig | grep -w mtu | grep -v RUNNING | sed 's/:.*//' | head -1`
[ -z "$if" ] &&
    if=`ifconfig | \
    awk  '/^[a-z].*: / {gsub(":", ""); ifn = $1}; /no car/{print ifn; exit}'`

[ -z "$if" ] && exit 0
echo "Using $if for test."
ifconfig $if | grep -q RUNNING && running=1

start=`date +%s`
while [ $((`date +%s` - start)) -lt 300 ]; do
	for i in `jot 255`; do
		(ifconfig $if.$i create
		ifconfig $if.$i inet 224.0.0.$i
		ifconfig $if.$i destroy) > /dev/null 2>&1 &
	done
	wait
done
[ $running ] || ifconfig $if down

exit 0
