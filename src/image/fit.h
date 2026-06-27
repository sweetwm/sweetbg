#ifndef MANJU_IMAGE_FIT_H
#define MANJU_IMAGE_FIT_H

#include <stdint.h>

struct manju_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct manju_placement {
	struct manju_rect src; // region of the source image to sample
	struct manju_rect dst; // region of the output to fill with it
};

void manju_cover_rect(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct manju_rect *out);

void manju_contain_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct manju_placement *out);

void manju_center_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct manju_placement *out);

#endif
