#ifndef CARAMEL_CONFIG_CONFIG_H
#define CARAMEL_CONFIG_CONFIG_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum caramel_fit {
	CARAMEL_FIT_COVER,
	CARAMEL_FIT_CONTAIN,
	CARAMEL_FIT_CENTER,
	CARAMEL_FIT_TILE,
};

const char *caramel_fit_name(enum caramel_fit fit);

bool caramel_fit_from_name(const char *name, enum caramel_fit *out);

bool caramel_config_parse_color(const char *s, uint32_t *out);

#define CARAMEL_CONFIG_MAX_OUTPUTS 16

struct caramel_config_output {
	char name[64];
	char image[PATH_MAX];
};

struct caramel_config {
	char image[PATH_MAX];
	uint32_t color;
	enum caramel_fit fit;
	struct caramel_config_output outputs[CARAMEL_CONFIG_MAX_OUTPUTS];
	size_t output_count;
};

void caramel_config_defaults(struct caramel_config *cfg);
bool caramel_config_load(
	struct caramel_config *cfg, char *err, size_t err_size);
bool caramel_config_parse(FILE *fp, const char *name,
	struct caramel_config *cfg, char *err, size_t err_size);

bool caramel_config_path(char *out, size_t out_size);

#endif
