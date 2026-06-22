#ifndef CARAMEL_IMAGE_FIT_H
#define CARAMEL_IMAGE_FIT_H

#include <stdint.h>

// A rectangle in source-image pixels
struct caramel_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

void caramel_cover_rect(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct caramel_rect *out);

#endif
