#!/usr/sbin/dtrace -s

/* see virtio_blk(4) */

fbt::vtblk_strategy:entry /execname == "dd"/
{
	stack(30);
	exit(0);
}
