#ifndef SWEETBG_IMAGE_LAYOUT_H
#define SWEETBG_IMAGE_LAYOUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "image/fit.h"

struct sweetbg_layout_output {
	int32_t x;
	int32_t y;
	uint32_t w;
	uint32_t h;
};

bool sweetbg_layout_slice(const struct sweetbg_layout_output *outputs,
	size_t count, size_t index, uint32_t *layout_w, uint32_t *layout_h,
	struct sweetbg_rect *slice);

#endif
