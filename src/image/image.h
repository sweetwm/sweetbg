#ifndef MANJU_IMAGE_IMAGE_H
#define MANJU_IMAGE_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "config/config.h"

struct manju_image {
	uint32_t width;
	uint32_t height;
	uint8_t *pixels;
};

bool manju_image_load(
	struct manju_image *img, const char *path, char *err, size_t err_size);

// Free decoded pixels. Idempotent
void manju_image_free(struct manju_image *img);

bool manju_image_render(const struct manju_image *src, enum manju_fit fit,
	uint32_t out_w, uint32_t out_h, uint32_t color, uint8_t *dst);

#endif
