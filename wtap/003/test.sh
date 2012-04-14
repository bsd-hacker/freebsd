#!/bin/sh

# This program requires:
# + wtap - to create/destroy the wtap instances
# + vis_map - to setup the visibility map between wtap instances
# + vimage - to configure/destroy vtap nodes

# The name of the test that will be printed in the begining
TEST_NBR="003"
TEST_NAME="3 nodes in a mesh topology"

# Return value from this test, 0 success failure otherwise
TEST_RESULT=127

# The number of nodes to test
NBR_NODES=3

# The subnet prefix
IP_SUBNET="192.168.2."

cmd()
{
	echo "***${TEST_NBR}*** " $*
	$*
}

info()
{
	echo "***${TEST_NBR}*** " $*
}

descr()
{
	cat <<EOL

This test establishes that the very basic 802.11s multi-hop
connectivity works.

It:

* creates three wtap instances
* creates three vimage instances
* creates one wlan vap for each wtap instance and places
  each vap in one of the four vimage instances
* sets up the visibility to the following:

  A --- B
   \   /
     C

The test:

1) does a ping test from each node to each other node.
2) changes the visibility to the following:

  A --- B
       /
     C

3) does a ping test from each node to each other node.

Note:

It is expected that the initial creation and discovery phase
will take some time so the initial run will fail until discovery
is done.  A future extension to the test suite should be to
set lower/upper bounds on the discovery phase time.

EOL
}

setup()
{
	# Initialize output file
	info "TEST: ${TEST_NAME}"
	info `date`

	# Create wtap/vimage nodes
	for i in `seq 1 ${NBR_NODES}`; do
		wtap_if="`expr $i - 1`"
		info "Setup: vimage $i - wtap$wtap_if"
		cmd vimage -c $i
		cmd wtap c $wtap_if
	done

	# Set visibility for each node to all other nodes.
	n="`expr ${NBR_NODES} - 1`"
	for i in `seq 0 ${n}`; do
		for j in `seq 0 ${n}`; do
			if [ $j != $i ]; then
				cmd vis_map a $i $j
			fi
		done
	done

	# Mark the visibility map plugin to start delivering packets.
	cmd vis_map o

	# Create each wlan subinterface, place into the correct vnet
	for i in `seq 0 ${n}`; do
		vnet="`expr ${i} + 1`"
		cmd ifconfig wlan${i} create wlandev wtap${i} wlanmode mesh
		cmd ifconfig wlan${i} meshid mymesh
		cmd wlandebug -i wlan${i} hwmp
		cmd ifconfig wlan${i} vnet ${vnet}
		cmd jexec ${vnet} ifconfig wlan${i} up

		cmd jexec ${vnet} ifconfig wlan${i} inet ${IP_SUBNET}${vnet}
	done
}

# Test statistics
NBR_TESTS=0
NBR_FAIL=0

ping_all()
{
	# Test connectivity from each node to each other node
	for i in `seq 1 ${NBR_NODES}`; do
		for j in `seq 1 ${NBR_NODES}`; do
			if [ "$i" != "$j" ]; then
				# From vimage '$i' to vimage '$j'..
				info "* Checking ${i} -> ${j}.."
				NBR_TESTS="`expr ${NBR_TESTS} + 1`"
				# Return after a single successful packet
				cmd jexec $i ping -q -t 5 -c 5 \
				    -o ${IP_SUBNET}${j}

				if [ "$?" = "0" ]; then
					info "CHECK: ${i} -> ${j}: SUCCESS"
				else
					info "CHECK: ${i} -> ${j}: FAILURE"
					NBR_FAIL="`expr ${NBR_FAIL} + 1`"
				fi
			fi
		done
	done
}

run()
{
	ping_all

	info "Removing link between 1 2"
	cmd vis_map d 0 1
	cmd vis_map d 1 0

	# NB: Because we still have not implemented lifetime decrementation
	# we have to wait at least IEEE80211_INACT_RUN.
	# XXX: Make IEEE80211_INACT_* SYSCTLs?
	sleeptime=5
	info "Sleeping > ${sleeptime}s to timeout disconnected neighbor nodes."
	sleep ${sleeptime}

	ping_all

	if [ $NBR_FAIL = 0 ]; then
		info "ALL TESTS PASSED"
		TEST_RESULT=0
	else
		info "FAILED ${NBR_FAIL} of ${NBR_TESTS} TESTS"
	fi
}

teardown()
{
	cmd vis_map c
	# Unlink all links
	# XXX: this is a limitation of the current plugin,
	# no way to reset vis_map without unload wtap.
	n="`expr ${NBR_NODES} - 1`"
	for i in `seq 0 ${n}`; do
		for j in `seq 0 ${n}`; do
			if [ $j != $i ]; then
				cmd vis_map d $i $j
			fi
		done
	done
	n="`expr ${NBR_NODES} - 1`"
	for i in `seq 0 ${n}`; do
		vnet="`expr ${i} + 1`"
		cmd jexec ${vnet} ifconfig wlan${i} destroy
	done
	for i in `seq 1 ${NBR_NODES}`; do
		wtap_if="`expr $i - 1`"
		cmd wtap d ${wtap_if}
		cmd vimage -d ${i}
	done
	exit ${TEST_RESULT}
}

EXEC_SETUP=0
EXEC_RUN=0
EXEC_TEARDOWN=0
while [ "$#" -gt "0" ]
do
	case $1 in
		'all')
			EXEC_SETUP=1
			EXEC_RUN=1
			EXEC_TEARDOWN=1
		;;
		'setup')
			EXEC_SETUP=1
		;;
		'run')
			EXEC_RUN=1
		;;
		'teardown')
			EXEC_TEARDOWN=1
		;;
		'descr')
			descr
			exit 0
		;;
                *)
			echo "$0 {all | setup | run | teardown | descr}"
			exit 127
		;;
        esac
        shift
done

if [ $EXEC_SETUP = 1 ]; then
	setup
fi
if [ $EXEC_RUN = 1 ]; then
	run
fi
if [ $EXEC_TEARDOWN = 1 ]; then
	teardown
fi

exit 0

