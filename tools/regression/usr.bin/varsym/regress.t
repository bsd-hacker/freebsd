#!/bin/sh
# $FreeBSD: src/tools/regression/usr.bin/tr/regress.t,v 1.1 2008/01/13 08:33:20 keramida Exp $

cd `dirname $0`

m4 ../regress.m4 regress.sh | sh
