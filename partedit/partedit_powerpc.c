#include <string.h>

#include "partedit.h"

int
is_scheme_bootable(const char *part_type) {
	if (strcmp(part_type, "APM") == 0)
		return (1);
	return (0);
}

size_t
partcode_size(const char *part_type) {
	if (strcmp(part_type, "APM") == 0)
		return (800*1024);
	return (0);
}

const char *
bootcode_path(const char *part_type) {
	return (NULL);
}
	
const char *
partcode_path(const char *part_type) {
	if (strcmp(part_type, "APM") == 0)
		return ("/boot/boot1.hfs");
	return (NULL);
}

