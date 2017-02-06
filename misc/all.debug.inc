# $FreeBSD$

# Example debug include file for misc/all.sh

pre_debug() {
	if [ ! $debug_first ] ; then
		debug_first=1
		debug_statfs=`vmstat -m | grep statfs | awk '{print $2}'`
	fi
}

post_debug() {
	local debug_old

	debug_old=$debug_statfs
	debug_statfs=`vmstat -m | grep statfs | awk '{print $2}'`
	[ $((debug_statfs - debug_old)) -gt 1 ] &&
	    printf "statfs leak: $i  %d %d\r\n" \
	        $((debug_statfs - debug_old)) $debug_statfs > $console
}

all_debug=1
