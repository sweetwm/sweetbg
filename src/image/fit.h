#ifndef SWEETBG_IMAGE_FIT_H
#define SWEETBG_IMAGE_FIT_H

#include <stdint.h>

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

void sweetbg_cover_rect(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct sweetbg_rect *out);

void sweetbg_contain_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct sweetbg_placement *out);

void sweetbg_center_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct sweetbg_placement *out);

void sweetbg_span_rects(uint32_t src_w, uint32_t src_h, uint32_t layout_w,
	uint32_t layout_h, const struct sweetbg_rect *slice, uint32_t dst_w,
	uint32_t dst_h, struct sweetbg_placement *out);

#endif
