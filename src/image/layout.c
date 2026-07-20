#include "image/layout.h"

#define LAYOUT_MAX_EXTENT 1000000

static bool usable(const struct sweetbg_layout_output *o) {
	return o->w > 0 && o->h > 0 && o->w <= LAYOUT_MAX_EXTENT &&
	       o->h <= LAYOUT_MAX_EXTENT;
}

bool sweetbg_layout_slice(const struct sweetbg_layout_output *outputs,
	size_t count, size_t index, uint32_t *layout_w, uint32_t *layout_h,
	struct sweetbg_rect *slice) {
	if (outputs == NULL || index >= count || !usable(&outputs[index])) {
		return false;
	}

	int64_t min_x = 0;
	int64_t min_y = 0;
	int64_t max_x = 0;
	int64_t max_y = 0;
	bool seen = false;

	for (size_t i = 0; i < count; i++) {
		const struct sweetbg_layout_output *o = &outputs[i];
		if (!usable(o)) {
			continue;
		}
		int64_t x0 = o->x;
		int64_t y0 = o->y;
		int64_t x1 = x0 + o->w;
		int64_t y1 = y0 + o->h;
		if (!seen) {
			min_x = x0;
			min_y = y0;
			max_x = x1;
			max_y = y1;
			seen = true;
			continue;
		}
		if (x0 < min_x) {
			min_x = x0;
		}
		if (y0 < min_y) {
			min_y = y0;
		}
		if (x1 > max_x) {
			max_x = x1;
		}
		if (y1 > max_y) {
			max_y = y1;
		}
	}

	if (!seen) {
		return false;
	}

	int64_t span_w = max_x - min_x;
	int64_t span_h = max_y - min_y;
	if (span_w <= 0 || span_h <= 0 || span_w > LAYOUT_MAX_EXTENT ||
		span_h > LAYOUT_MAX_EXTENT) {
		return false;
	}

	*layout_w = (uint32_t)span_w;
	*layout_h = (uint32_t)span_h;
	slice->x = (uint32_t)(outputs[index].x - min_x);
	slice->y = (uint32_t)(outputs[index].y - min_y);
	slice->w = outputs[index].w;
	slice->h = outputs[index].h;
	return true;
}
