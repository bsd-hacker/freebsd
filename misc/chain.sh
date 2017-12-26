#!/bin/sh

# Unkillable process in "vm map (user)" seen.
# https://people.freebsd.org/~pho/stress/log/kostik1070.txt

# Test scenario by Peter Jeremy <peterj@>

if [ ! -f /usr/local/include/libmill.h -o \
    ! -x /usr/local/lib/libmill.so ]; then
	echo "ports/devel/libmill needed."
	exit 0
fi

. ../default.cfg

cat > /tmp/chain.c <<EOF
#include <stdio.h>
#include <stdlib.h>
#include <libmill.h>

coroutine void f(chan left, chan right) {
    chs(left, int, 1 + chr(right, int));
}

int
main(int argc __unused, char **argv)
{
	int i, n = argv[1] ? atoi(argv[1]) : 10000;
	chan leftmost = chmake(int, 0);
	chan left = NULL;
	chan right = leftmost;

	for (i = 0; i < n; i++) {
		left = right;
		right = chmake(int, 0);
		go(f(left, right));
	}
	chs(right, int, 0);
	i = chr(leftmost, int);
	printf("result = %d\n", i);
	return(0);
}
EOF

mycc -o /tmp/chain -I /usr/local/include -L /usr/local/lib -Wall -Wextra \
	 -O2 -g /tmp/chain.c -lmill || exit 1
/tmp/chain 1000000
rm -f /tmp/chain /tmp/chain.c
exit 0
