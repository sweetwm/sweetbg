#ifndef SWEETBG_CLI_FLAGS_H
#define SWEETBG_CLI_FLAGS_H

#include <stdbool.h>

int sweetbg_cli_target_flag(
	int argc, char **argv, int *i, const char **output, bool *persist);

#endif
