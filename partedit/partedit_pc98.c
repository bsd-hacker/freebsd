#include <string.h>

#include "partedit.h"

const char *
default_scheme(void) {
	return ("PC98");
}

int
is_scheme_bootable(const char *part_type) {
	if (strcmp(part_type, "PC98") == 0)
		return (1);
	if (strcmp(part_type, "BSD") == 0)
		return (1);

	return (0);
}

size_t
bootpart_size(const char *part_type) {
	/* No boot partition */
	return (0);
}

const char *
bootcode_path(const char *part_type) {
	if (strcmp(part_type, "BSD") == 0)
		return ("/boot/boot");

	return (NULL);
}
	
const char *
partcode_path(const char *part_type) {
	/* No partcode */
	return (NULL);
}

