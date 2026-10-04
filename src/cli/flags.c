#include "cli/flags.h"

#include <stdio.h>
#include <string.h>

int sweetbg_cli_target_flag(
	int argc, char **argv, int *i, const char **output, bool *persist) {
	const char *a = argv[*i];
	if (strcmp(a, "-p") == 0 || strcmp(a, "--persist") == 0) {
		*persist = true;
		return 1;
	}
	const char *name;
	if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
		if (*i + 1 >= argc) {
			fprintf(stderr, "sweetbg: --output needs a name\n");
			return -1;
		}
		name = argv[++*i];
	} else if (strncmp(a, "--output=", 9) == 0) {
		name = a + 9;
	} else {
		return 0;
	}
	// output names travel in 64-byte IPC and config fields
	if (name[0] == '\0' || strlen(name) > 63) {
		fprintf(stderr, "sweetbg: invalid --output name\n");
		return -1;
	}
	*output = name;
	return 1;
}
