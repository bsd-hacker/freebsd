#!/bin/sh
#
# $Id$
#

set -e

aclocal
autoheader
automake --add-missing --copy --foreign
autoconf
