#ifndef SWEETBG_IMAGE_DECODE_SCALE_H
#define SWEETBG_IMAGE_DECODE_SCALE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/config.h"
#include "image/fit.h"

struct sweetbg_decode_target {
	enum sweetbg_fit fit;
	uint32_t width;
	uint32_t height;
	uint32_t layout_width;
	uint32_t layout_height;
	struct sweetbg_rect slice;
};

struct sweetbg_image_load_options {
	const struct sweetbg_decode_target *targets;
	size_t target_count;
};

bool sweetbg_decode_targets_fit(uint32_t source_width, uint32_t source_height,
	const struct sweetbg_image_load_options *options);

#endif
