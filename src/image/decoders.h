#ifndef MANJU_IMAGE_DECODERS_H
#define MANJU_IMAGE_DECODERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "image/image.h"

bool manju_image_dimensions_ok(uint32_t width, uint32_t height);
bool manju_decode_png(
	FILE *fp, struct manju_image *img, char *err, size_t err_size);
bool manju_decode_jpeg(
	FILE *fp, struct manju_image *img, char *err, size_t err_size);
bool manju_decode_webp(
	FILE *fp, struct manju_image *img, char *err, size_t err_size);

#endif
