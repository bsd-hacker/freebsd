#!/bin/sh
# $FreeBSD$

cd `dirname $0`

m4 ../../usr.bin/regress.m4 regress.sh | sh
