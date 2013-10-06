#include <sys/types.h>
#include <sys/module.h>
#include <sys/syscall.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char **argv)
{
	int modid, syscall_num;
	struct module_stat stat;

	stat.version = sizeof(stat);
	if ((modid = modfind("sys/foo_syscall")) == -1)
		err(1, "modfind");
	if (modstat(modid, &stat) != 0)
		err(1, "modstat");
	syscall_num = stat.data.intval;

	return syscall(syscall_num, argc, argv);
}
