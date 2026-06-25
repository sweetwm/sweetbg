#ifndef CARAMEL_CONFIG_CONFIG_WRITE_H
#define CARAMEL_CONFIG_CONFIG_WRITE_H

#include <stdbool.h>
#include <stddef.h>

bool caramel_config_patch_image(const char *input, const char *output_name,
	const char *image_path, char **out, char *err, size_t err_size);

bool caramel_config_persist_image(const char *output_name,
	const char *image_path, char *err, size_t err_size);

bool caramel_config_patch_setting(const char *input, const char *key,
	const char *value, char **out, char *err, size_t err_size);

bool caramel_config_patch_output_setting(const char *input,
	const char *output_name, const char *key, const char *value, char **out,
	char *err, size_t err_size);

bool caramel_config_persist_setting(
	const char *key, const char *value, char *err, size_t err_size);

bool caramel_config_persist_output_setting(const char *output_name,
	const char *key, const char *value, char *err, size_t err_size);

#endif
