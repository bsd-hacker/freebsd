#!/bin/sh

# This program requires:
# + wtap - to create/destroy the wtap instances
# + vis_map - to setup the visibility map between wtap instances
# + vimage - to configure/destroy vtap nodes

# The name of the test that will be printed in the begining
TEST_NAME="2 nodes and 2 PROXY nodes"

# Global flags
FLAG_QUIET=0

# The number of nodes to test
NBR_NODES=2

# The subnet prefix
IP_SUBNET="192.168.2."

cmd()
{
	if [ $FLAG_QUIET = 1 ]; then
		echo "*** " $* >> output
		$* >> output
	else
		echo "*** " $*
		$*
	fi
}

info()
{
	if [ $FLAG_QUIET = 1 ]; then
		echo "*** " $* >> output
	else
		echo "*** " $*
	fi
}

descr()
{
	cat <<EOL

This test establishes that the very basic 802.11s PROXY
connectivity works.

It:

* creates two wtap instances
* creates three vimage instances
* creates one wlan vap for each wtap instance and places
  each vap in one of the three vimage instances
* creates two epair and places each end in one
  of four vimage instances
* creates two bridge and places it with a wtap in
  one of the two vimage instances
* sets up the visibility to the following:

             -----MESH------
  proxyA <--|--> A <-> B <--|--> proxyB
             ---------------

* does a ping test from each node to each other node.

It is expected that the initial creation and discovery phase
will take some time so the initial run will fail until discovery
is done.  A future extension to the test suite should be to
set lower/upper bounds on the discovery phase time.

EOL
}

setup()
{
	# Initialize output file
	echo "" > output
	echo "TEST: ${TEST_NAME}"

	# Create wtap/vimage nodes
	for i in `seq 1 ${NBR_NODES}`; do
		wtap_if="`expr $i - 1`"
		info "Setup: vimage $i - wtap$wtap_if"
		cmd vimage -c $i
		cmd wtap c $wtap_if
	done
	# Need one more vimage for the PROXY node
	cmd vimage -c 3
	cmd vimage -c 4

	# Set visibility for each node to see the
	# next node.
	n="`expr ${NBR_NODES} - 1`"
	for i in `seq 0 ${n}`; do
		j="`expr ${i} + 1`"
		cmd vis_map a $i $j
		cmd vis_map a $j $i
	done

	# Makes the visibility map plugin deliver packets to resp. dest.
	cmd vis_map o

	# Create and setup PROXY A/B node with corresponding bridge
	# NB: both epair must be created before moving them outside
	# host view, otherwise they will receive same MAC address.
	cmd ifconfig epair0 create
	cmd ifconfig epair1 create
	cmd ifconfig epair0a vnet 1
	cmd ifconfig epair0b vnet 2
	cmd ifconfig epair1a vnet 4
	cmd ifconfig epair1b vnet 3
	cmd ifconfig bridge0 create
	cmd ifconfig bridge1 create
	cmd ifconfig bridge0 vnet 2
	cmd ifconfig bridge1 vnet 3

	# Disables bridge filtering
	cmd sysctl net.link.bridge.pfil_member=0
	cmd sysctl net.link.bridge.pfil_bridge=0

	# Create each wlan subinterface, place into the correct vnet
	for i in `seq 0 ${n}`; do
		vnet="`expr ${i} + 2`"
		cmd ifconfig wlan${i} create wlandev wtap${i} wlanmode mesh
		cmd ifconfig wlan${i} meshid mymesh
		cmd wlandebug -i wlan${i} hwmp+input+output+mesh
		cmd ifconfig wlan${i} vnet ${vnet}
	done

	# Bring all interfaces up.
	# NB: Bridge need to be brought up before the bridged interfaces
	cmd jexec 1 ifconfig epair0a inet 192.168.2.1
	cmd jexec 2 ifconfig bridge0 addm epair0b addm wlan0 up
	cmd jexec 2 ifconfig epair0b up
	cmd jexec 2 ifconfig wlan0 up
	cmd jexec 2 ifconfig wlan0 inet ${IP_SUBNET}2
	cmd jexec 3 ifconfig bridge1 addm epair1b addm wlan1 up
	cmd jexec 3 ifconfig epair1b up
	cmd jexec 3 ifconfig wlan1 up
	cmd jexec 3 ifconfig wlan1 inet ${IP_SUBNET}3
	cmd jexec 4 ifconfig epair1a inet 192.168.2.4
}

run()
{
	NBR_TESTS=0 NBR_FAIL=0

	# Number of all nodes in this test
	ALL_NODES=`expr ${NBR_NODES} + 2`
	# Test connectivity from each node to each other node
	for i in `seq 1 ${ALL_NODES}`; do
		for j in `seq 1 ${ALL_NODES}`; do
			if [ "$i" != "$j" ]; then
				# From vimage '$i' to vimage '$j'..
				info "Checking ${i} -> ${j}.."
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
	if [ $NBR_FAIL = 0 ]; then
		echo "ALL TESTS PASSED"
	else
		echo "FAILED ${NBR_FAIL} of ${NBR_TESTS} TESTS"
	fi
}

teardown()
{
	cmd vis_map c
	cmd jexec 2 ifconfig bridge0 destroy
	cmd jexec 3 ifconfig bridge1 destroy
	# Bring epair back to host view, we bring both back
	# otherwise a panic occurs, ie one is not enough.
	cmd ifconfig epair0a -vnet 1
	cmd ifconfig epair0b -vnet 2
	cmd ifconfig epair0a destroy
	cmd ifconfig epair1a -vnet 4
	cmd ifconfig epair1b -vnet 3
	cmd ifconfig epair1a destroy
	n="`expr ${NBR_NODES} - 1`"
	for i in `seq 0 ${n}`; do
		vnet="`expr ${i} + 2`"
		cmd jexec ${vnet} ifconfig wlan${i} destroy
	done
	for i in `seq 1 ${NBR_NODES}`; do
		wtap_if="`expr $i - 1`"
		cmd wtap d ${wtap_if}
		cmd vimage -d ${i}
	done
	cmd vimage -d 3
	cmd vimage -d 4
}

EXEC_SETUP=0
EXEC_RUN=0
EXEC_TEARDOWN=0
while [ "$#" -gt "0" ]
do
	case $1 in
		-q)
			FLAG_QUIET=1
		;;
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
			echo "$0 {all | setup | run | teardown | descr [-q]}"
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

