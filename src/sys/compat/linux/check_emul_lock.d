#!/usr/sbin/dtrace -qs

/*
 * Check if the emul lock is correctly acquired/released:
 *  - no recursive locking
 *  - no unlocking of already unlocked one
 */

linuxulator*::emul_locked
/check[probeprov, arg0] > 0/
{
	printf("ERROR: recursive lock of emul_lock (%p),", arg0);
	printf("       or missing SDT probe in kernel. Stack trace follows:");
	stack();
}

linuxulator*::emul_locked
{
	++check[probeprov, arg0];
}

linuxulator*::emul_unlock
/check[probeprov, arg0] == 0/
{
	printf("ERROR: unlock attemt of unlocked emul_lock (%p),", arg0);
	printf("       missing SDT probe in kernel, or dtrace program started");
	printf("       while the emul_lock was already held (race condition).");
	printf("       Stack trace follows:");
	stack();
}

linuxulator*::emul_unlock
{
	--check[probeprov, arg0];
}

