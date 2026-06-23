#ifndef CARAMEL_IMAGE_DECODERS_H
#define CARAMEL_IMAGE_DECODERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "image/image.h"

bool caramel_image_dimensions_ok(uint32_t width, uint32_t height);
bool caramel_decode_png(
	FILE *fp, struct caramel_image *img, char *err, size_t err_size);
bool caramel_decode_jpeg(
	FILE *fp, struct caramel_image *img, char *err, size_t err_size);
bool caramel_decode_webp(
	FILE *fp, struct caramel_image *img, char *err, size_t err_size);

#endif
