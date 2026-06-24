#ifndef CARAMEL_IMAGE_IMAGE_H
#define CARAMEL_IMAGE_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/config.h"

struct caramel_image {
	uint32_t width;
	uint32_t height;
	uint8_t *pixels;
};

bool caramel_image_load(struct caramel_image *img, const char *path, char *err,
	size_t err_size);

// Free decoded pixels. Idempotent
void caramel_image_free(struct caramel_image *img);

bool caramel_image_render(const struct caramel_image *src, enum caramel_fit fit,
	uint32_t out_w, uint32_t out_h, uint32_t color, uint8_t *dst);

#endif
