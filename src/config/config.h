#ifndef CARAMEL_CONFIG_CONFIG_H
#define CARAMEL_CONFIG_CONFIG_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum caramel_fit {
	CARAMEL_FIT_COVER,
};

struct caramel_config {
	char image[PATH_MAX];
	uint32_t color;
	enum caramel_fit fit;
};

void caramel_config_defaults(struct caramel_config *cfg);
bool caramel_config_load(
	struct caramel_config *cfg, char *err, size_t err_size);
bool caramel_config_parse(FILE *fp, const char *name,
	struct caramel_config *cfg, char *err, size_t err_size);

#endif
