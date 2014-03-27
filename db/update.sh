#!/bin/sh
#
# $FreeBSD$
#

user=$1
password=$2

cd $(dirname $(realpath $0))

../script/fbce_create.pl model FBCE DBIC::Schema FBCE::Schema \
    create=static \
    overwrite_modifications=1 \
    "dbi:Pg:dbname=fbce" "${user:-fbce}" "$password" \
    pg_enable_utf8=1
