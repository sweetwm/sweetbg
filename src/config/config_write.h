#ifndef SWEETBG_CONFIG_CONFIG_WRITE_H
#define SWEETBG_CONFIG_CONFIG_WRITE_H

#include <stdbool.h>
#include <stddef.h>

bool sweetbg_config_patch(const char *input, const char *output_name,
	const char *key, const char *value, char **out, char *err,
	size_t err_size);

// apply sweetbg_config_patch to the user's config file atomically
bool sweetbg_config_persist(const char *output_name, const char *key,
	const char *value, char *err, size_t err_size);

#endif
