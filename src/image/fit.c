#include "image/fit.h"

void manju_cover_rect(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct manju_rect *out) {
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

void manju_contain_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct manju_placement *out) {
	out->src.x = 0;
	out->src.y = 0;
	out->src.w = src_w;
	out->src.h = src_h;

	uint64_t src_by_out = (uint64_t)src_w * out_h;
	uint64_t out_by_src = (uint64_t)out_w * src_h;
	uint32_t dst_w;
	uint32_t dst_h;
	if (src_by_out > out_by_src) {
		// Source is wider: bound by output width
		dst_w = out_w;
		dst_h = (uint32_t)((uint64_t)src_h * out_w / src_w);
	} else {
		// Source is taller: bound by output height
		dst_h = out_h;
		dst_w = (uint32_t)((uint64_t)src_w * out_h / src_h);
	}
	if (dst_w == 0) {
		dst_w = 1;
	}
	if (dst_h == 0) {
		dst_h = 1;
	}
	out->dst.w = dst_w;
	out->dst.h = dst_h;
	out->dst.x = (out_w - dst_w) / 2;
	out->dst.y = (out_h - dst_h) / 2;
}

void manju_center_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct manju_placement *out) {
	uint32_t vis_w = src_w < out_w ? src_w : out_w;
	uint32_t vis_h = src_h < out_h ? src_h : out_h;

	out->src.w = vis_w;
	out->src.h = vis_h;
	out->src.x = (src_w - vis_w) / 2;
	out->src.y = (src_h - vis_h) / 2;

	out->dst.w = vis_w;
	out->dst.h = vis_h;
	out->dst.x = (out_w - vis_w) / 2;
	out->dst.y = (out_h - vis_h) / 2;
}
