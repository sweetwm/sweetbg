#include "image/decoders.h"

#include <stdlib.h>
#include <webp/decode.h>

#define BYTES_PER_PIXEL 4
#define MAX_WEBP_FILE (64UL * 1024 * 1024)

bool manju_decode_webp(
	FILE *fp, struct manju_image *img, char *err, size_t err_size) {
	// libwebp decodes from a memory buffer, so read the whole file in
	if (fseek(fp, 0, SEEK_END) != 0) {
		snprintf(err, err_size, "webp: cannot read");
		return false;
	}
	long size = ftell(fp);
	if (size <= 0 || (unsigned long)size > MAX_WEBP_FILE) {
		snprintf(err, err_size, "webp: file too large");
		return false;
	}
	if (fseek(fp, 0, SEEK_SET) != 0) {
		snprintf(err, err_size, "webp: cannot read");
		return false;
	}

	uint8_t *data = malloc((size_t)size);
	if (data == NULL) {
		snprintf(err, err_size, "webp: out of memory");
		return false;
	}
	if (fread(data, 1, (size_t)size, fp) != (size_t)size) {
		free(data);
		snprintf(err, err_size, "webp: short read");
		return false;
	}

	int width = 0;
	int height = 0;
	if (!WebPGetInfo(data, (size_t)size, &width, &height)) {
		free(data);
		snprintf(err, err_size, "webp: not a valid webp");
		return false;
	}
	if (!manju_image_dimensions_ok((uint32_t)width, (uint32_t)height)) {
		free(data);
		snprintf(err, err_size, "webp: image too large");
		return false;
	}

	size_t stride = (size_t)width * BYTES_PER_PIXEL;
	size_t out_size = stride * (size_t)height;
	img->pixels = malloc(out_size);
	if (img->pixels == NULL) {
		free(data);
		snprintf(err, err_size, "webp: out of memory");
		return false;
	}
	img->width = (uint32_t)width;
	img->height = (uint32_t)height;

	if (WebPDecodeBGRAInto(data, (size_t)size, img->pixels, out_size,
		    (int)stride) == NULL) {
		free(data);
		free(img->pixels);
		img->pixels = NULL;
		snprintf(err, err_size, "webp: decode failed");
		return false;
	}

	free(data);
	return true;
}
