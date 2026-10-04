#ifndef SWEETBG_IMAGE_FIT_H
#define SWEETBG_IMAGE_FIT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/config.h"

struct sweetbg_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct sweetbg_placement {
	struct sweetbg_rect src; // region of the source image to sample
	struct sweetbg_rect dst; // region of the output to fill with it
};

// one output a decode is sized for, layout and slice are used by span only
struct sweetbg_decode_target {
	enum sweetbg_fit fit;
	uint32_t width;
	uint32_t height;
	uint32_t layout_width;
	uint32_t layout_height;
	struct sweetbg_rect slice;
};

void sweetbg_cover_rect(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct sweetbg_rect *out);

void sweetbg_contain_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct sweetbg_placement *out);

void sweetbg_center_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct sweetbg_placement *out);

void sweetbg_span_rects(uint32_t src_w, uint32_t src_h, uint32_t layout_w,
	uint32_t layout_h, const struct sweetbg_rect *slice, uint32_t dst_w,
	uint32_t dst_h, struct sweetbg_placement *out);

void sweetbg_fit_placement(enum sweetbg_fit fit, uint32_t src_w, uint32_t src_h,
	uint32_t out_w, uint32_t out_h, struct sweetbg_placement *out);

bool sweetbg_decode_targets_fit(uint32_t src_w, uint32_t src_h,
	const struct sweetbg_decode_target *targets, size_t count);

#endif
