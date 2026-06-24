#ifndef CARAMEL_IMAGE_FIT_H
#define CARAMEL_IMAGE_FIT_H

#include <stdint.h>

struct caramel_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct caramel_placement {
	struct caramel_rect src; // region of the source image to sample
	struct caramel_rect dst; // region of the output to fill with it
};

void caramel_cover_rect(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct caramel_rect *out);

void caramel_contain_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct caramel_placement *out);

void caramel_center_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct caramel_placement *out);

#endif
