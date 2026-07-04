#ifndef SWEETBG_IMAGE_DECODERS_H
#define SWEETBG_IMAGE_DECODERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "image/image.h"

bool sweetbg_image_dimensions_ok(uint32_t width, uint32_t height);
bool sweetbg_decode_png(
	FILE *fp, struct sweetbg_image *img, char *err, size_t err_size);
bool sweetbg_decode_jpeg(
	FILE *fp, struct sweetbg_image *img, char *err, size_t err_size);
bool sweetbg_decode_webp(
	FILE *fp, struct sweetbg_image *img, char *err, size_t err_size);

#endif
