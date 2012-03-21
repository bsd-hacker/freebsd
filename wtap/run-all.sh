#!/bin/sh

TEST_CASES="001 002 003 004 005"

for i in ${TEST_CASES}; do
	echo "=== Test ${i}"
	${i}/test.sh setup
	${i}/test.sh run
	${i}/test.sh teardown
done
