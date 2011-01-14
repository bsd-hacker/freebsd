#include <string.h>

#include "partedit.h"

const char *
default_scheme(void) {
	return ("GPT");
}

int
is_scheme_bootable(const char *part_type) {
	if (strcmp(part_type, "BSD") == 0)
		return (1);
	if (strcmp(part_type, "GPT") == 0)
		return (1);
	if (strcmp(part_type, "MBR") == 0)
		return (1);

	return (0);
}

size_t
bootpart_size(const char *part_type) {
	if (strcmp(part_type, "GPT") == 0)
		return (64*1024);

	/* No partcode except for GPT */
	return (0);
}

const char *
bootcode_path(const char *part_type) {
	if (strcmp(part_type, "GPT") == 0)
		return ("/boot/pmbr");
	if (strcmp(part_type, "MBR") == 0)
		return ("/boot/mbr");
	if (strcmp(part_type, "BSD") == 0)
		return ("/boot/boot");

	return (NULL);
}
	
const char *
partcode_path(const char *part_type) {
	if (strcmp(part_type, "GPT") == 0)
		return ("/boot/gptboot");
	
	/* No partcode except for GPT */
	return (NULL);
}

