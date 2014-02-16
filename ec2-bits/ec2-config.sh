#!/bin/sh

if [ $# -ne 1 ]; then
	echo "usage: ec2-config.sh /path/to/etc"
	exit 1
fi

ETCDIR=$1

# Disable ttys since they don't exist on EC2
sed -E -i '' 's/^([^#].*[[:space:]])on/\1off/' $ETCDIR/ttys
