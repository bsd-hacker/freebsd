#!/bin/sh

cd $(dirname $(realpath $0))

../script/fbce_create.pl model \
    FBCE DBIC::Schema FBCE::Schema \
    create=static \
    "dbi:Pg:dbname=fbce" fbce
