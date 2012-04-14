#!/bin/sh

# This program requires:
# + wtap - to create/destroy the wtap instances
# + vis_map - to setup the visibility map between wtap instances
# + vimage - to configure/destroy vtap nodes

# The name of the test that will be printed in the begining
TEST_NAME="Net80211s test script"

# Return value from this test, 0 success failure otherwise
TEST_RESULT=127

# Global flags
FLAG_QUITE="0"
CLEAR_LOGS=0

RUN_ALL_TESTS=0
RUN_TEST_NBR=0

cmd()
{
	echo "*** " $*
	$*
}

info()
{
	echo "*** " $*
}

descr()
{
	cat <<EOL

This script runs either one specific or all tests.

EOL
}

setup()
{
	info "SETTING UP TEST ENVIROMENT: ${TEST_NAME}"
}

run()
{
	NBR_TESTS=0 NBR_FAIL=0

	ALL_TEST=`find * -type d | egrep '^[0-9]*$'`
	info "Running tests: ${ALL_TEST}"

	for i in ${ALL_TEST}; do
		NBR_TESTS="`expr ${NBR_TESTS} + 1`"
		info "Running test: ${i}"
		cd ${i}
		if [ ${CLEAR_LOGS} = 1 ]; then
			rm out.log
		fi
		if [ ${FLAG_QUITE} = 1 ]; then
			./test.sh setup run teardown >> out.log
		else
			./test.sh setup run teardown
		fi
		if [ "$?" != "0" ]; then
			info "TEST ${i} FAILED !!!"
			NBR_FAIL="`expr ${NBR_FAIL} + 1`"
		fi
		cd ..
	done

 	if [ $NBR_FAIL = 0 ]; then
 		info "TESTS PASSED"
 		TEST_RESULT=0
 	else
		info "FAILED ${NBR_FAIL} of ${NBR_TESTS} TESTS"
 	fi
}

run_one()
{
	NBR_FAIL=0

	cd ${RUN_TEST_NBR}
	if [ ${CLEAR_LOGS} = 1 ]; then
		rm out
	fi
	if [ ${FLAG_QUITE} = 1 ]; then
		./test.sh all >> out
	else
		./test.sh all
	fi

	if [ "$?" = "0" ]; then
		info "TEST PASSED"
	else
		info "TEST FAILED"
		NBR_FAIL="`expr ${NBR_FAIL} + 1`"
	fi
	cd ..
}

teardown()
{
	exit ${TEST_RESULT}
}

while [ "$#" -gt "0" ]
do
	case $1 in
		-c)
			CLEAR_LOGS=1
		;;
		-q)
			FLAG_QUITE=1
		;;
		'all')
			RUN_ALL_TESTS=1
		;;
		'one')
			RUN_TEST_NBR=$2
			shift
		;;
		'descr')
			descr
			exit 0
		;;
                *)
			echo "$0 {all | one test_nbr | descr [-q -c]}"
			exit 127
		;;
        esac
        shift
done

if [ ${RUN_ALL_TESTS} -eq 1 ]; then
	setup
	run
	teardown
elif [ ${RUN_TEST_NBR} -gt 0 ]; then
	setup
	run_one
	teardown
else
	echo "$0 {all | one test_nbr | descr [-q -c]}"
	exit 127
fi

exit 0