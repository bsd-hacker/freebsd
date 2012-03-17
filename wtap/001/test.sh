#!/bin/sh

# This program requires:
# + wtap - to create/destroy the wtap instances
# + vis_map - to setup the visibility map between wtap instances
# + vimage - to configure/destroy vtap nodes

# The number of nodes to test

NBR_NODES=4

# The subnet prefix
IP_SUBNET="192.168.2."

cmd()
{
	echo "*** " $*
	$*
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

* does a ping test from each node to each other node.

It is expected that the initial creation and discovery phase
will take some time so the initial run will fail until discovery
is done.  A future extension to the test suite should be to
set lower/upper bounds on the discovery phase time.

EOL
}

setup()
{
	# Create wtap/vimage nodes
	for i in `seq 1 ${NBR_NODES}`; do
		wtap_if="`expr $i - 1`"
		echo "Setup: vimage $i - wtap$wtap_if"
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

	# What's this do?
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

run()
{
	# Test connectivity from each node to each other node
	for i in `seq 1 ${NBR_NODES}`; do
		for j in `seq 1 ${NBR_NODES}`; do
			if [ "$i" != "$j" ]; then
				# From vimage '$i' to vimage '$j'..
				echo "* Checking ${i} -> ${j}.."
				# Return after a single successful packet
				cmd jexec $i ping -q -t 5 -c 5 \
				    -o ${IP_SUBNET}${j}

				if [ "$?" = "0" ]; then
					echo "CHECK: ${i} -> ${j}: SUCCESS"
				else
					echo "CHECK: ${i} -> ${j}: FAILURE"
				fi
			fi
		done
	done
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

case $1 in
	'setup')
		setup
		exit 0
	;;
	'run')
		run
		exit 0
	;;
	'teardown')
		teardown
		exit 0
	;;
	'descr')
		descr
		exit 0
	;;
	*)
		echo "$0 {setup | run | teardown | descr}"
		exit 127
	;;
esac

exit 0

