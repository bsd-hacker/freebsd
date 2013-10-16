#include <sys/types.h>
#include <sys/module.h>
#include <sys/syscall.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <api.h>

int
main(int argc, char **argv)
{
	int modid, syscall_num;
	struct module_stat stat;
	int what;

	stat.version = sizeof(stat);
	if ((modid = modfind("sys/foo_syscall")) == -1)
		err(1, "modfind");
	if (modstat(modid, &stat) != 0)
		err(1, "modstat");
	syscall_num = stat.data.intval;

	if (argc < 2)
		errx(1, "argument required");

	if (strcmp(argv[1], "add") == 0)
		what = ADD;
	else if (strcmp(argv[1], "delete") == 0)
		what = DELETE;
	else
		errx(1, "add or delete");

	return syscall(syscall_num, what, argv);
}
