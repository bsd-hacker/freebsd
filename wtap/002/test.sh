#!/bin/sh

# This program requires:
# + wtap - to create/destroy the wtap instances
# + vis_map - to setup the visibility map between wtap instances
# + vimage - to configure/destroy vtap nodes

# The name of the test that will be printed in the begining
TEST_NAME="4 nodes in line-topology with HWMPROOT NORMAL"

# Global flags
FLAG_QUIET=0

# The number of nodes to test
NBR_NODES=4

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

This test establishes that the very basic 802.11s multi-hop
connectivity works.

It:

* creates four wtap instances
* creates four vimage instances
* creates one wlan vap for each wtap instance and places
  each vap in one of the four vimage instances
* sets up the visibility to the following:

  A <-> B <-> C <-> D

* configures A to be ROOT (NORMAL)
* After a grace period the forwarding information for each
  of B,C and D is checked to contain correct number of hops:

   ----NHOP 3 ---------
  / ---NHOP 2-------  |
  |/ -NHOP 1--     |  |
  ||/        |     |  |
   A <-----> B <-> C <-> D

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

	# Create each wlan subinterface, place into the correct vnet
	for i in `seq 0 ${n}`; do
		vnet="`expr ${i} + 1`"
		cmd ifconfig wlan${i} create wlandev wtap${i} wlanmode mesh
		cmd ifconfig wlan${i} meshid mymesh
		if [ ${i} = 0 ]; then
			cmd ifconfig wlan${i} hwmprootmode normal
		fi
		cmd wlandebug -i wlan${i} hwmp
		cmd ifconfig wlan${i} vnet ${vnet}
		cmd jexec ${vnet} ifconfig wlan${i} up

		cmd jexec ${vnet} ifconfig wlan${i} inet ${IP_SUBNET}${vnet}
	done
}

run()
{
	NBR_TESTS=0 NBR_FAIL=0

	# Wait for root to be discovered by all and then check if
	# it is present in all nodes forwarding information (FI)
	sleep_time=10
	info "Waiting ${sleep_time}s for network to settle"
	sleep ${sleep_time}

	# Check that the forwarding information in nodes 2,3,4
	# have correct number of hops to root (1)
	n="`expr ${NBR_NODES} - 1`"
	for i in `seq 1 ${n}`; do
		info "Checking forwarding information for ${i}.."
		NBR_TESTS="`expr ${NBR_TESTS} + 1`"
		j=`expr ${i} + 1`
		# Check number of hops to root
		FI=`jexec ${j} ifconfig wlan${i} list mesh | \
		    egrep "^00:98:9a:98:96:97.*" | awk '{print $3}'`
		if [ "${FI}" = "${i}" ]; then
			info "CHECK: ${i} -> ${j}: SUCCESS"
		else
			info "CHECK: ${i} -> ${j}: FAILURE"
			NBR_FAIL="`expr ${NBR_FAIL} + 1`"
		fi
	done
	if [ $NBR_FAIL = 0 ]; then
		echo "ALL TESTS PASSED"
	else
		echo "FAILED ${NBR_FAIL} of ${NBR_TESTS} TESTS"
	fi
}

teardown()
{
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

