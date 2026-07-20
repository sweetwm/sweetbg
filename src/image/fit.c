#include "image/fit.h"

void sweetbg_cover_rect(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct sweetbg_rect *out) {
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

void sweetbg_contain_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct sweetbg_placement *out) {
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

static void span_axis(uint32_t cover_pos, uint32_t cover_len,
	uint32_t slice_pos, uint32_t slice_len, uint32_t layout_len,
	uint32_t *out_pos, uint32_t *out_len) {
	uint32_t pos = cover_pos +
		       (uint32_t)((uint64_t)slice_pos * cover_len / layout_len);
	uint32_t len = (uint32_t)((uint64_t)slice_len * cover_len / layout_len);
	if (len == 0) {
		len = 1;
	}
	uint32_t limit = cover_pos + cover_len;
	if (pos >= limit) {
		pos = limit - 1;
	}
	if (pos + len > limit) {
		len = limit - pos;
	}
	*out_pos = pos;
	*out_len = len;
}

void sweetbg_span_rects(uint32_t src_w, uint32_t src_h, uint32_t layout_w,
	uint32_t layout_h, const struct sweetbg_rect *slice, uint32_t dst_w,
	uint32_t dst_h, struct sweetbg_placement *out) {
	out->dst = (struct sweetbg_rect){0, 0, dst_w, dst_h};

	if (layout_w == 0 || layout_h == 0 || slice->w == 0 || slice->h == 0) {
		sweetbg_cover_rect(src_w, src_h, dst_w, dst_h, &out->src);
		return;
	}

	struct sweetbg_rect cover;
	sweetbg_cover_rect(src_w, src_h, layout_w, layout_h, &cover);

	span_axis(cover.x, cover.w, slice->x, slice->w, layout_w, &out->src.x,
		&out->src.w);
	span_axis(cover.y, cover.h, slice->y, slice->h, layout_h, &out->src.y,
		&out->src.h);
}

void sweetbg_center_rects(uint32_t src_w, uint32_t src_h, uint32_t out_w,
	uint32_t out_h, struct sweetbg_placement *out) {
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
