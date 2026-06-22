#include "image/fit.h"

void caramel_cover_rect(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct caramel_rect *out) {
	uint64_t src_by_out = (uint64_t)src_w * out_h;
	uint64_t out_by_src = (uint64_t)out_w * src_h;

	if (src_by_out > out_by_src) {
		uint32_t crop_w = (uint32_t)((uint64_t)src_h * out_w / out_h);
		if (crop_w > src_w) {
			crop_w = src_w;
		}
		out->w = crop_w;
		out->h = src_h;
		out->x = (src_w - crop_w) / 2;
		out->y = 0;
	} else {
		uint32_t crop_h = (uint32_t)((uint64_t)src_w * out_h / out_w);
		if (crop_h > src_h) {
			crop_h = src_h;
		}
		out->w = src_w;
		out->h = crop_h;
		out->x = 0;
		out->y = (src_h - crop_h) / 2;
	}
}
