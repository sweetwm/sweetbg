#ifndef SWEETBG_QUERY_JSON_H
#define SWEETBG_QUERY_JSON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct sweetbg_query_json_output {
	const char *name;
	int32_t width;
	int32_t height;
	int32_t scale;
	bool configured;
	const char *image;
	bool image_override;
	bool blank;
	const char *fit;
	bool fit_override;
	// Dominant colours (0x00RRGGBB) of the image this output shows
	const uint32_t *colors;
	size_t color_count;
};

struct sweetbg_query_json_state {
	const char *default_image;
	uint32_t color;
	bool color_auto;
	const char *default_fit;
	const uint32_t *default_colors;
	size_t default_color_count;
	const struct sweetbg_query_json_output *outputs;
	size_t output_count;
};

bool sweetbg_query_json_write(const struct sweetbg_query_json_state *state,
	char *out, size_t out_size);

#endif
