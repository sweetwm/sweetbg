#ifndef CARAMEL_IMAGE_IMAGE_H
#define CARAMEL_IMAGE_IMAGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// A decoded image in XRGB8888 (one 0x00RRGGBB word per pixel). `pixels` owns
// width * height * 4 bytes
struct caramel_image {
	uint32_t width;
	uint32_t height;
	uint8_t *pixels;
};

bool caramel_image_load(struct caramel_image *img, const char *path, char *err,
	size_t err_size);

// Free decoded pixels. Idempotent
void caramel_image_free(struct caramel_image *img);

bool caramel_image_render_cover(const struct caramel_image *src, uint32_t out_w,
	uint32_t out_h, uint8_t *dst);

#endif
