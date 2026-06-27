#ifndef MANJU_CONFIG_CONFIG_H
#define MANJU_CONFIG_CONFIG_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum manju_fit {
	MANJU_FIT_COVER,
	MANJU_FIT_CONTAIN,
	MANJU_FIT_CENTER,
	MANJU_FIT_TILE,
};

const char *manju_fit_name(enum manju_fit fit);

bool manju_fit_from_name(const char *name, enum manju_fit *out);

bool manju_config_parse_color(const char *s, uint32_t *out);

#define MANJU_CONFIG_MAX_OUTPUTS 16

struct manju_config_output {
	char name[64];
	char image[PATH_MAX];
	enum manju_fit fit;
	bool has_fit;
};

struct manju_config {
	char image[PATH_MAX];
	uint32_t color;
	enum manju_fit fit;
	struct manju_config_output outputs[MANJU_CONFIG_MAX_OUTPUTS];
	size_t output_count;
};

void manju_config_defaults(struct manju_config *cfg);
bool manju_config_load(struct manju_config *cfg, char *err, size_t err_size);
bool manju_config_parse(FILE *fp, const char *name, struct manju_config *cfg,
	char *err, size_t err_size);

bool manju_config_path(char *out, size_t out_size);

#endif
