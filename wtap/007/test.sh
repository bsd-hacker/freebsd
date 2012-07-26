#!/bin/sh

# This program requires:
# + wtap - to create/destroy the wtap instances
# + vis_map - to setup the visibility map between wtap instances
# + vimage - to configure/destroy vtap nodes

# The name of the test that will be printed in the begining
TEST_NBR="007"
TEST_NAME="test (intermediate) PREP reply for proxy entry"

# Return value from this test, 0 success failure otherwise
TEST_RESULT=127

# The maximum number of nodes to test
# not counting the mesh gate.
# This will iterate from 0 to MAX_NBR_NODES
MAX_NBR_NODES=5

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

This test establishes that the very basic 802.11s PROXY
connectivity works.

It:

* creates MAX_NBR_NODES + 1 wtap instances
* creates MAX_NBR_NODES + 2 vimage instances
* creates one wlan vap for each wtap instance and places
  each vap in one of the three vimage instances
* creates one epair and places each end in one
  of two vimage instances
* creates one bridge and places it with a wtap in
  one vimage instance
* sets up the visibility to the following:

             -------------MESH----------------------
  proxyA <--|--> MG(A) <-> MAX_NBR_NODES <-> MP(Z)  |
             ---------------------------------------

* does a ping test from end-to-end.

It is expected that the initial creation and discovery phase
will take some time so the initial run will fail until discovery
is done.  A future extension to the test suite should be to
set lower/upper bounds on the discovery phase time.

EOL
}

intr_setup()
{
	NBR_WTAPS=$1
	info "Internal setup called $NBR_WTAPS"
	# Initialize output file
	info "TEST: ${TEST_NAME}"
	info `date`

	# Create wtap/vimage nodes
	cmd vimage -c 1		# proxyA
	info "Setup: vimage 2 - wtap0"
	cmd vimage -c 2		# MG(A)
	cmd wtap c 0
	info "Setup: vimage 3 - wtap1"
	cmd vimage -c 3		# MP(Z)
	cmd wtap c 1
	if [ $NBR_WTAPS != 0 ]; then
		for i in `seq 1 $NBR_WTAPS`; do
			wtap_if="`expr $i + 1`"
			vimage="`expr $i + 3`"
			info "Setup: vimage $vimage - wtap$wtap_if"
			cmd vimage -c $vimage
			cmd wtap c $wtap_if
		done
	fi

	# Set visibility for each node to see the
	# next node.
	n="`expr $NBR_WTAPS + 2 - 1`"
	for l in `seq 0 ${n}`; do
		k="`expr ${l} + 1`"
		cmd vis_map a $k $l
		cmd vis_map a $l $k
	done

	# Makes the visibility map plugin deliver packets to resp. dest.
	cmd vis_map o

	# Create and setup PROXY node with corresponding bridge
	cmd ifconfig epair0 create
	cmd ifconfig epair0a vnet 1
	cmd ifconfig epair0b vnet 2
	cmd ifconfig bridge0 create
	# Disables bridge filtering
	cmd sysctl net.link.bridge.pfil_member=0
	cmd sysctl net.link.bridge.pfil_bridge=0
	cmd ifconfig bridge0 vnet 2

	# Create each wlan subinterface, place into the correct vnet
	# MP(A)
	cmd ifconfig wlan0 create wlandev wtap0 wlanmode mesh
	cmd ifconfig wlan0 meshid mymesh
	cmd wlandebug -i wlan0 hwmp
	cmd ifconfig wlan0 vnet 2
	# MP(Z)
	cmd ifconfig wlan1 create wlandev wtap1 wlanmode mesh
	cmd ifconfig wlan1 meshid mymesh
	cmd wlandebug -i wlan1 hwmp
	cmd ifconfig wlan1 vnet 3
	# MPs inbetween
	if [ $NBR_WTAPS != 0 ]; then
		for l in `seq 1 $NBR_WTAPS`; do
			wtap_if="`expr ${l} + 1`"
			vimage="`expr ${l} + 3`"
			cmd ifconfig wlan${wtap_if} create wlandev wtap${wtap_if} wlanmode mesh
			cmd ifconfig wlan${wtap_if} meshid mymesh
			cmd wlandebug -i wlan${wtap_if} hwmp
			cmd ifconfig wlan${wtap_if} vnet ${vimage}
			cmd jexec ${vimage} ifconfig wlan${wtap_if} inet ${IP_SUBNET}${vimage}
		done
	fi

	# Bring all interfaces up.
	# NB: Bridge need to be brought up before the bridged interfaces
	cmd jexec 1 ifconfig epair0a inet 192.168.2.1
	cmd jexec 2 ifconfig bridge0 addm epair0b addm wlan0 up
	cmd jexec 2 ifconfig epair0b up
	cmd jexec 2 ifconfig wlan0 up
	cmd jexec 2 ifconfig wlan0 inet ${IP_SUBNET}2
	cmd jexec 3 ifconfig wlan1 up
	cmd jexec 3 ifconfig wlan1 inet ${IP_SUBNET}3

	sleep 5
}

intr_run()
{
	NBR_WTAPS=$1
	MP_Z_VIMAGE="3"
	MP_Z_WTAP="1"
	PROXY_A_VIMAGE="1"
	PROXY_A_WTAP="0"
	INTR_TEST_FAIL="0"
	info "internal run called with $NBR_WTAPS inbetween wtaps"

	LAST_VIMAGE="`expr $NBR_WTAPS + 3`"
	for k in `seq 3 ${LAST_VIMAGE}`; do
		# From vimage '$i' to vimage '$j'..
		info "Checking MP(${k}) -> proxyA.."
		# Return after a single successful packet
		cmd jexec ${k} ping -q -t 5 -c 5 \
			-o ${IP_SUBNET}${PROXY_A_VIMAGE}

		if [ "$?" = "0" ]; then
			info "CHECK: MP(${k}) -> proxyA: SUCCESS"
		else
			info "CHECK: MP(${k}) -> proxyA: FAILURE"
			INTR_TEST_FAIL="127"
		fi
	done
	return $INTR_TEST_FAIL
}

intr_teardown()
{
	NBR_WTAPS=$1
	info "Internal teardown called $NBR_WTAPS"
	cmd vis_map c
	# Unlink all links
	# XXX: this is a limitation of the current plugin,
	# no way to reset vis_map without unload wtap.
	n="`expr $NBR_WTAPS + 2 - 1`"
	for l in `seq 0 ${n}`; do
		k="`expr ${l} + 1`"
		cmd vis_map d $k $l
		cmd vis_map d $l $k
	done

	# We need to destroy the bridge first otherwise
	# we panic the system.
	cmd jexec 2 ifconfig bridge0 destroy
	# Bring epair back to host view, we bring both back
	# otherwise a panic occurs, ie one is not enough.
	cmd ifconfig epair0a -vnet 1
	cmd ifconfig epair0b -vnet 2
	cmd ifconfig epair0a destroy

	# Destroy wtap/vimage nodes
	cmd vimage -d 1		# proxyA
	info "Teardown: vimage 2 - wtap0"
	cmd cat /dev/wtap0 > itr${NBR_WTAPS}_wtap0.debug
	cmd jexec 2 ifconfig wlan0 destroy
	cmd vimage -d 2		# MG(A)
	cmd wtap d 0
	info "Teardown: vimage 3 - wtap1"
	cmd cat /dev/wtap1 > itr${NBR_WTAPS}_wtap1.debug
	cmd jexec 3 ifconfig wlan1 destroy
	cmd vimage -d 3		# MP(Z)
	cmd wtap d 1
	if [ $NBR_WTAPS != 0 ]; then
		for l in `seq 1 $1`; do
			wtap_if="`expr $l + 1`"
			vimage="`expr $l + 3`"
			info "Teardown: vimage $vimage - wtap$wtap_if"
			cmd cat /dev/wtap${wtap_if} > itr${NBR_WTAPS}_wtap${wtap_if}.debug
			cmd jexec ${vimage} ifconfig wlan${wtap_if} destroy
			cmd vimage -d $vimage
			cmd wtap d $wtap_if
		done
	fi
}

run()
{
	NBR_TESTS=0 NBR_FAIL=0 RUN_STATUS=0

	for i in `seq 0 ${MAX_NBR_NODES}`; do
		NBR_TESTS="`expr ${NBR_TESTS} + 1`"
		intr_setup ${i}
		intr_run ${i}
		if [ $? = 0 ]; then
			info "Iteration ${i} PASSED"
		else
			info "Iteration ${i} FAILED"
			NBR_FAIL="`expr ${NBR_FAIL} + 1`"
		fi
		intr_teardown ${i}
	done

	if [ $NBR_FAIL = 0 ]; then
		info "ALL TESTS PASSED"
		TEST_RESULT=0
	else
		info "FAILED ${NBR_FAIL} of ${NBR_TESTS} TESTS"
	fi
}

teardown()
{
	exit ${TEST_RESULT}
}

EXEC_RUN=0
EXEC_TEARDOWN=0
while [ "$#" -gt "0" ]
do
	case $1 in
		'all')
			EXEC_RUN=1
			EXEC_TEARDOWN=1
		;;
		'setup')
			info "Does nothing. This is an iterative test."
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

if [ $EXEC_RUN = 1 ]; then
	run
fi
if [ $EXEC_TEARDOWN = 1 ]; then
	teardown
fi

exit 0

