#include <string.h>

#include "partedit.h"

const char *
default_scheme(void) {
	/*
	 * Our loader can parse GPT, so pick that as the default for lack of
	 * a better idea.
	 */

	return ("GPT");
}

int
is_scheme_bootable(const char *part_type) {
	/*
	 * We don't know anything about this platform, so don't irritate the
	 * user by claiming the chosen partition scheme isn't bootable.
	 */

	return (1);
}

/* No clue => no boot partition, bootcode, or partcode */

size_t
bootpart_size(const char *part_type) {
	return (0);
}

const char *
bootcode_path(const char *part_type) {
	return (NULL);
}
	
const char *
partcode_path(const char *part_type) {
	return (NULL);
}

