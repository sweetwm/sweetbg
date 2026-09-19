#include "image/decode_scale.h"

static bool target_fits(uint32_t source_width, uint32_t source_height,
	const struct sweetbg_decode_target *target) {
	if (target->width == 0 || target->height == 0) {
		return false;
	}
	struct sweetbg_placement place;
	switch (target->fit) {
	case SWEETBG_FIT_COVER:
		place.dst = (struct sweetbg_rect){
			0, 0, target->width, target->height};
		sweetbg_cover_rect(source_width, source_height, target->width,
			target->height, &place.src);
		break;
	case SWEETBG_FIT_CONTAIN:
		sweetbg_contain_rects(source_width, source_height,
			target->width, target->height, &place);
		break;
	case SWEETBG_FIT_SPAN:
		sweetbg_span_rects(source_width, source_height,
			target->layout_width, target->layout_height,
			&target->slice, target->width, target->height, &place);
		break;
	case SWEETBG_FIT_CENTER:
	case SWEETBG_FIT_TILE:
	default:
		return false;
	}
	return place.src.w >= place.dst.w && place.src.h >= place.dst.h;
}

bool sweetbg_decode_targets_fit(uint32_t source_width, uint32_t source_height,
	const struct sweetbg_image_load_options *options) {
	if (options == NULL || options->targets == NULL ||
		options->target_count == 0) {
		return false;
	}
	for (size_t i = 0; i < options->target_count; i++) {
		if (!target_fits(source_width, source_height,
			    &options->targets[i])) {
			return false;
		}
	}
	return true;
}
