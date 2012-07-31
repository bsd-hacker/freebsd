#!/bin/sh

# This program requires:
# + wtap - to create/destroy the wtap instances
# + vis_map - to setup the visibility map between wtap instances
# + vimage - to configure/destroy vtap nodes

# The name of the test that will be printed in the begining
TEST_NBR="009"
TEST_NAME="test mesh peer code w/ ACL policy"

# Return value from this test, 0 success failure otherwise
TEST_RESULT=127

# The number of nodes to test
NBR_NODES=2

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

This test establishes that the very basic 802.11s mesh peer
connectivity works.

It:

* creates two wtap instances
* creates two vimage instances
* creates one wlan vap for each wtap instance and places
  each vap in one of the vimage instances
* sets up the visibility to the following:

  A <-> B

* node A filters out B with ACL deny policy
* node B should not flood with PEER OPEN
* node A changed ACL policy to open
* does a ping test from each node to each other node.

EOL
}

setup()
{
	# Initialize output file
	info "TEST: ${TEST_NAME}"
	info `date`

	# load mac ACL policy
	cmd kldload wlan_acl

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
		if [ ${i} = 0 ]; then
			cmd ifconfig wlan${i} mac:add 00:98:9a:98:96:98
			cmd ifconfig wlan${i} mac:deny
		fi
		cmd ifconfig wlan${i} meshid mymesh
		cmd wlandebug -i wlan${i} mesh
		cmd ifconfig wlan${i} vnet ${vnet}
		cmd jexec ${vnet} ifconfig wlan${i} up
		cmd jexec ${vnet} ifconfig wlan${i} inet ${IP_SUBNET}${vnet}
	done
}

run()
{
	NBR_TESTS=1 NBR_FAIL=0

	# chicken egg problem, this is default value
	cmd sleep 5 #net.wlan.mesh.backofftimeout=5
	n="`expr ${NBR_NODES} - 1`"
	for i in `seq 0 ${n}`; do
		vnet="`expr ${i} + 1`"
		cmd cat /dev/wtap${i} > wtap${i}.debug
	done
	TMP=`cat wtap${i}.debug | grep "OPEN SENT -> HOLDING" | wc -l`

	NBR_HOLDING="2" #net.wlan.mesh.maxholding=2

	if [ $TMP -gt "$NBR_HOLDING" ]; then
		info "node B is flooding!"
		NBR_FAIL="`expr ${NBR_FAIL} + 1`"
	fi

	# Now remove ACL policy and see if we can ping
	cmd jexec 1 ifconfig wlan0 mac:open

	# Test connectivity from each node to each other node
	for i in `seq 1 ${NBR_NODES}`; do
		for j in `seq 1 ${NBR_NODES}`; do
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
		j="`expr ${i} + 1`"
		cmd vis_map d $i $j
		cmd vis_map d $j $i
	done
	n="`expr ${NBR_NODES} - 1`"
	for i in `seq 0 ${n}`; do
		vnet="`expr ${i} + 1`"
		cmd cat /dev/wtap${i} >> wtap${i}.debug
		cmd jexec ${vnet} ifconfig wlan${i} destroy
	done
	for i in `seq 1 ${NBR_NODES}`; do
		wtap_if="`expr $i - 1`"
		cmd wtap d ${wtap_if}
		cmd vimage -d ${i}
	done

	# unload mac ACL policy
	cmd kldunload wlan_acl

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

