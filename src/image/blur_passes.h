#ifndef SWEETBG_IMAGE_BLUR_PASSES_H
#define SWEETBG_IMAGE_BLUR_PASSES_H

#include <stdbool.h>
#include <stdint.h>

enum { SWEETBG_BLUR_CHANNELS = 3 };

bool sweetbg_blur_passes(
	uint16_t *pixels, int32_t width, int32_t height, double sigma);

#endif
