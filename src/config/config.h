#ifndef SWEETBG_CONFIG_CONFIG_H
#define SWEETBG_CONFIG_CONFIG_H

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

enum sweetbg_fit {
	SWEETBG_FIT_COVER,
	SWEETBG_FIT_CONTAIN,
	SWEETBG_FIT_CENTER,
	SWEETBG_FIT_TILE,
	SWEETBG_FIT_SPAN,
};

bool sweetbg_fit_is_global_only(enum sweetbg_fit fit);

const char *sweetbg_fit_name(enum sweetbg_fit fit);

bool sweetbg_fit_from_name(const char *name, enum sweetbg_fit *out);

bool sweetbg_config_parse_color(const char *s, uint32_t *out);

#define SWEETBG_CONFIG_MAX_OUTPUTS 16

struct sweetbg_config_output {
	char name[64];
	char image[PATH_MAX];
	enum sweetbg_fit fit;
	bool has_image;
	bool has_fit;
};

struct sweetbg_config {
	char image[PATH_MAX];
	uint32_t color;
	bool color_auto;
	enum sweetbg_fit fit;
	struct sweetbg_config_output outputs[SWEETBG_CONFIG_MAX_OUTPUTS];
	size_t output_count;
};

void sweetbg_config_defaults(struct sweetbg_config *cfg);
bool sweetbg_config_load(
	struct sweetbg_config *cfg, char *err, size_t err_size);
bool sweetbg_config_parse(FILE *fp, const char *name,
	struct sweetbg_config *cfg, char *err, size_t err_size);

bool sweetbg_config_path(char *out, size_t out_size);

#endif
