#include <string.h>

#include "partedit.h"

int
is_scheme_bootable(const char *part_type) {
	if (strcmp(part_type, "VTOC8") == 0)
		return (1);
	return (0);
}

size_t
bootpart_size(const char *part_type) {
	/* No standalone boot partition */

	return (0);
}

const char *
bootcode_path(const char *part_type) {
	return (NULL);
}
	
const char *
partcode_path(const char *part_type) {
	if (strcmp(part_type, "VTOC8") == 0)
		return ("/boot/boot1");
	return (NULL);
}

