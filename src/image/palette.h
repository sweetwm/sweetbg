#ifndef SWEETBG_IMAGE_PALETTE_H
#define SWEETBG_IMAGE_PALETTE_H

#include <stddef.h>
#include <stdint.h>

#include "image/image.h"

void sweetbg_palette_extract(const struct sweetbg_image *img, uint32_t *colors,
	size_t max, size_t *count);

#endif
