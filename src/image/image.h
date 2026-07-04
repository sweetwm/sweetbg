#ifndef SWEETBG_IMAGE_IMAGE_H
#define SWEETBG_IMAGE_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/config.h"

struct sweetbg_image {
	uint32_t width;
	uint32_t height;
	uint8_t *pixels;
};

bool sweetbg_image_load(struct sweetbg_image *img, const char *path, char *err,
	size_t err_size);

// Free decoded pixels. Idempotent
void sweetbg_image_free(struct sweetbg_image *img);

bool sweetbg_image_render(const struct sweetbg_image *src, enum sweetbg_fit fit,
	uint32_t out_w, uint32_t out_h, uint32_t color, uint8_t *dst);

#endif
