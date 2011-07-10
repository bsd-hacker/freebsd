#include <err.h>
#include <regex.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
usage(void)
{
	printf("Usage: %s pattern string\n", getprogname());
	exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
	regex_t pattern;
	regmatch_t pmatch;
	ssize_t len;
	int cflags = 0, ret;
	int eflags = REG_STARTEND;

	if (argc != 3)
		usage();

	ret = regcomp(&pattern, argv[1], cflags);
	if (ret != 0)
		errx(1, NULL);

	len = strlen(argv[2]);
	pmatch.rm_so = 0;
	pmatch.rm_eo = len;
	putchar('(');
	for (bool first = true;;) {
		ret = regexec(&pattern, argv[2], 1, &pmatch, eflags);
		if (ret == REG_NOMATCH)
			break;
		if (!first)
			putchar(',');
		printf("(%lu,%lu)", (unsigned long)pmatch.rm_so,
			(unsigned long)pmatch.rm_eo);
		if (pmatch.rm_eo == len)
			break;
		pmatch.rm_so = pmatch.rm_eo;
		pmatch.rm_eo = len;
		first = false;
	}
	printf(")\n");
	regfree(&pattern);
}
