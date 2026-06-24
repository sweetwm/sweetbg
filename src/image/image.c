#include "image/image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image/decoders.h"
#include "image/fit.h"

#define MAX_DIMENSION 16384u
#define MAX_PIXELS (1u << 26)
#define BYTES_PER_PIXEL 4

bool caramel_image_dimensions_ok(uint32_t width, uint32_t height) {
	if (width == 0 || height == 0 || width > MAX_DIMENSION ||
		height > MAX_DIMENSION) {
		return false;
	}
	return (uint64_t)width * height <= MAX_PIXELS;
}

static bool is_png(const uint8_t *sig, size_t n) {
	static const uint8_t png[8] = {
		0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
	return n >= 8 && memcmp(sig, png, 8) == 0;
}

static bool is_jpeg(const uint8_t *sig, size_t n) {
	return n >= 3 && sig[0] == 0xff && sig[1] == 0xd8 && sig[2] == 0xff;
}

// WebP is a RIFF container: "RIFF" then 4 size bytes then "WEBP"
static bool is_webp(const uint8_t *sig, size_t n) {
	return n >= 12 && memcmp(sig, "RIFF", 4) == 0 &&
	       memcmp(sig + 8, "WEBP", 4) == 0;
}

bool caramel_image_load(struct caramel_image *img, const char *path, char *err,
	size_t err_size) {
	img->width = 0;
	img->height = 0;
	img->pixels = NULL;

	FILE *fp = fopen(path, "rb");
	if (fp == NULL) {
		snprintf(err, err_size, "cannot open image");
		return false;
	}

	uint8_t sig[12];
	size_t got = fread(sig, 1, sizeof(sig), fp);
	if (fseek(fp, 0, SEEK_SET) != 0) {
		snprintf(err, err_size, "cannot read image");
		fclose(fp);
		return false;
	}

	bool ok;
	if (is_png(sig, got)) {
		ok = caramel_decode_png(fp, img, err, err_size);
	} else if (is_jpeg(sig, got)) {
		ok = caramel_decode_jpeg(fp, img, err, err_size);
	} else if (is_webp(sig, got)) {
		ok = caramel_decode_webp(fp, img, err, err_size);
	} else {
		snprintf(err, err_size, "unsupported image format");
		ok = false;
	}

	fclose(fp);
	return ok;
}

void caramel_image_free(struct caramel_image *img) {
	free(img->pixels);
	img->pixels = NULL;
	img->width = 0;
	img->height = 0;
}

static void sample_box(const struct caramel_image *src,
	const struct caramel_rect *crop, uint32_t out_w, uint32_t out_h,
	uint32_t ox, uint32_t oy, uint8_t *out) {
	uint32_t sx0 = crop->x + (uint32_t)((uint64_t)ox * crop->w / out_w);
	uint32_t sx1 =
		crop->x + (uint32_t)((uint64_t)(ox + 1) * crop->w / out_w);
	uint32_t sy0 = crop->y + (uint32_t)((uint64_t)oy * crop->h / out_h);
	uint32_t sy1 =
		crop->y + (uint32_t)((uint64_t)(oy + 1) * crop->h / out_h);

	if (sx1 <= sx0) {
		sx1 = sx0 + 1;
	}
	if (sy1 <= sy0) {
		sy1 = sy0 + 1;
	}
	if (sx1 > crop->x + crop->w) {
		sx1 = crop->x + crop->w;
	}
	if (sy1 > crop->y + crop->h) {
		sy1 = crop->y + crop->h;
	}

	uint64_t b = 0;
	uint64_t g = 0;
	uint64_t r = 0;
	for (uint32_t sy = sy0; sy < sy1; sy++) {
		const uint8_t *row =
			src->pixels +
			((uint64_t)sy * src->width + sx0) * BYTES_PER_PIXEL;
		for (uint32_t sx = sx0; sx < sx1; sx++) {
			b += row[0];
			g += row[1];
			r += row[2];
			row += BYTES_PER_PIXEL;
		}
	}

	uint64_t count = (uint64_t)(sx1 - sx0) * (sy1 - sy0);
	out[0] = (uint8_t)(b / count);
	out[1] = (uint8_t)(g / count);
	out[2] = (uint8_t)(r / count);
	out[3] = 0;
}

static void fill_color(
	uint8_t *dst, uint32_t out_w, uint32_t out_h, uint32_t color) {
	uint32_t word = color & 0xffffffu;
	uint32_t *px = (uint32_t *)dst;
	uint64_t count = (uint64_t)out_w * out_h;
	for (uint64_t i = 0; i < count; i++) {
		px[i] = word;
	}
}

static void blit_placement(const struct caramel_image *src,
	const struct caramel_placement *place, uint32_t out_w, uint8_t *dst) {
	for (uint32_t ly = 0; ly < place->dst.h; ly++) {
		uint32_t oy = place->dst.y + ly;
		uint8_t *out_row = dst + (uint64_t)oy * out_w * BYTES_PER_PIXEL;
		for (uint32_t lx = 0; lx < place->dst.w; lx++) {
			uint32_t ox = place->dst.x + lx;
			sample_box(src, &place->src, place->dst.w, place->dst.h,
				lx, ly,
				out_row + (uint64_t)ox * BYTES_PER_PIXEL);
		}
	}
}

static void render_tile(const struct caramel_image *src, uint32_t out_w,
	uint32_t out_h, uint8_t *dst) {
	for (uint32_t oy = 0; oy < out_h; oy++) {
		uint32_t sy = oy % src->height;
		const uint8_t *src_row = src->pixels + (uint64_t)sy *
							       src->width *
							       BYTES_PER_PIXEL;
		uint8_t *out_row = dst + (uint64_t)oy * out_w * BYTES_PER_PIXEL;
		for (uint32_t ox = 0; ox < out_w; ox++) {
			uint32_t sx = ox % src->width;
			memcpy(out_row + (uint64_t)ox * BYTES_PER_PIXEL,
				src_row + (uint64_t)sx * BYTES_PER_PIXEL,
				BYTES_PER_PIXEL);
		}
	}
}

bool caramel_image_render(const struct caramel_image *src, enum caramel_fit fit,
	uint32_t out_w, uint32_t out_h, uint32_t color, uint8_t *dst) {
	if (src->pixels == NULL || src->width == 0 || src->height == 0 ||
		out_w == 0 || out_h == 0) {
		return false;
	}

	struct caramel_placement place;
	switch (fit) {
	case CARAMEL_FIT_TILE:
		render_tile(src, out_w, out_h, dst);
		return true;
	case CARAMEL_FIT_CONTAIN:
		fill_color(dst, out_w, out_h, color);
		caramel_contain_rects(
			src->width, src->height, out_w, out_h, &place);
		break;
	case CARAMEL_FIT_CENTER:
		fill_color(dst, out_w, out_h, color);
		caramel_center_rects(
			src->width, src->height, out_w, out_h, &place);
		break;
	case CARAMEL_FIT_COVER:
	default:
		place.dst = (struct caramel_rect){0, 0, out_w, out_h};
		caramel_cover_rect(
			src->width, src->height, out_w, out_h, &place.src);
		break;
	}

	blit_placement(src, &place, out_w, dst);
	return true;
}
