#include "image/image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image/decoders.h"
#include "image/fit.h"

#define MAX_DIMENSION 16384u
#define MAX_PIXELS (1u << 26)
#define BYTES_PER_PIXEL 4

bool sweetbg_image_dimensions_ok(uint32_t width, uint32_t height) {
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

bool sweetbg_image_load(struct sweetbg_image *img, const char *path,
	const struct sweetbg_image_load_options *options, char *err,
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
		ok = sweetbg_decode_png(fp, img, err, err_size);
	} else if (is_jpeg(sig, got)) {
		ok = sweetbg_decode_jpeg(fp, img, options, err, err_size);
	} else if (is_webp(sig, got)) {
		ok = sweetbg_decode_webp(fp, img, err, err_size);
	} else {
		snprintf(err, err_size, "unsupported image format");
		ok = false;
	}

	fclose(fp);
	return ok;
}

void sweetbg_image_free(struct sweetbg_image *img) {
	free(img->pixels);
	img->pixels = NULL;
	img->width = 0;
	img->height = 0;
}

struct span {
	uint32_t start;
	uint32_t end;
};

// Source range each output column or row averages; always at least one pixel
static void fill_spans(
	uint32_t start, uint32_t len, uint32_t out, struct span *spans) {
	for (uint32_t o = 0; o < out; o++) {
		uint32_t a = start + (uint32_t)((uint64_t)o * len / out);
		uint32_t b = start + (uint32_t)((uint64_t)(o + 1) * len / out);
		if (b <= a) {
			b = a + 1;
		}
		if (b > start + len) {
			b = start + len;
		}
		spans[o] = (struct span){a, b};
	}
}

static void average_box(const struct sweetbg_image *src, struct span col,
	struct span row, uint8_t *out) {
	uint64_t b = 0;
	uint64_t g = 0;
	uint64_t r = 0;
	for (uint32_t sy = row.start; sy < row.end; sy++) {
		const uint8_t *pixel =
			src->pixels + ((uint64_t)sy * src->width + col.start) *
					      BYTES_PER_PIXEL;
		for (uint32_t sx = col.start; sx < col.end; sx++) {
			b += pixel[0];
			g += pixel[1];
			r += pixel[2];
			pixel += BYTES_PER_PIXEL;
		}
	}

	uint64_t count =
		(uint64_t)(col.end - col.start) * (row.end - row.start);
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

static bool blit_placement(const struct sweetbg_image *src,
	const struct sweetbg_placement *place, uint32_t out_w, uint8_t *dst) {
	const struct sweetbg_rect *source = &place->src;
	const struct sweetbg_rect *dest = &place->dst;
	if (source->w == 0 || source->h == 0 || dest->w == 0 || dest->h == 0) {
		return false;
	}
	if (source->w == dest->w && source->h == dest->h) {
		// The X byte is ignored by XRGB8888, so whole rows can be
		// copied
		for (uint32_t y = 0; y < dest->h; y++) {
			memcpy(dst + ((uint64_t)(dest->y + y) * out_w +
					     dest->x) *
						BYTES_PER_PIXEL,
				src->pixels + ((uint64_t)(source->y + y) *
							      src->width +
						      source->x) *
						      BYTES_PER_PIXEL,
				(size_t)dest->w * BYTES_PER_PIXEL);
		}
		return true;
	}

	struct span *cols = malloc((size_t)dest->w * sizeof(*cols));
	struct span *rows = malloc((size_t)dest->h * sizeof(*rows));
	if (cols == NULL || rows == NULL) {
		free(cols);
		free(rows);
		return false;
	}
	fill_spans(source->x, source->w, dest->w, cols);
	fill_spans(source->y, source->h, dest->h, rows);
	for (uint32_t ly = 0; ly < dest->h; ly++) {
		uint8_t *out =
			dst + ((uint64_t)(dest->y + ly) * out_w + dest->x) *
				      BYTES_PER_PIXEL;
		for (uint32_t lx = 0; lx < dest->w; lx++) {
			average_box(src, cols[lx], rows[ly], out);
			out += BYTES_PER_PIXEL;
		}
	}
	free(cols);
	free(rows);
	return true;
}

static void render_tile(const struct sweetbg_image *src, uint32_t out_w,
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

bool sweetbg_image_render_placement(const struct sweetbg_image *src,
	const struct sweetbg_placement *place, uint32_t out_w, uint32_t out_h,
	uint8_t *dst) {
	if (src->pixels == NULL || src->width == 0 || src->height == 0 ||
		out_w == 0 || out_h == 0) {
		return false;
	}
	if (place->src.w == 0 || place->src.h == 0 || place->dst.w == 0 ||
		place->dst.h == 0) {
		return false;
	}
	if ((uint64_t)place->src.x + place->src.w > src->width ||
		(uint64_t)place->src.y + place->src.h > src->height) {
		return false;
	}
	if ((uint64_t)place->dst.x + place->dst.w > out_w ||
		(uint64_t)place->dst.y + place->dst.h > out_h) {
		return false;
	}
	return blit_placement(src, place, out_w, dst);
}

bool sweetbg_image_render(const struct sweetbg_image *src, enum sweetbg_fit fit,
	uint32_t out_w, uint32_t out_h, uint32_t color, uint8_t *dst) {
	if (src->pixels == NULL || src->width == 0 || src->height == 0 ||
		out_w == 0 || out_h == 0) {
		return false;
	}

	struct sweetbg_placement place;
	switch (fit) {
	case SWEETBG_FIT_TILE:
		render_tile(src, out_w, out_h, dst);
		return true;
	case SWEETBG_FIT_CONTAIN:
		fill_color(dst, out_w, out_h, color);
		sweetbg_contain_rects(
			src->width, src->height, out_w, out_h, &place);
		break;
	case SWEETBG_FIT_CENTER:
		fill_color(dst, out_w, out_h, color);
		sweetbg_center_rects(
			src->width, src->height, out_w, out_h, &place);
		break;
	case SWEETBG_FIT_COVER:
	default:
		place.dst = (struct sweetbg_rect){0, 0, out_w, out_h};
		sweetbg_cover_rect(
			src->width, src->height, out_w, out_h, &place.src);
		break;
	}

	return blit_placement(src, &place, out_w, dst);
}
