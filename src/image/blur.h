#ifndef SWEETBG_IMAGE_BLUR_H
#define SWEETBG_IMAGE_BLUR_H

#include <stdbool.h>
#include <stdint.h>

// blur a composed XRGB8888 buffer in place; sigma is in buffer pixels
bool sweetbg_blur(
	uint8_t *pixels, uint32_t width, uint32_t height, double sigma);

#endif
