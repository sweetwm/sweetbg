#include "image/blur.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>

#include "image/blur_passes.h"

enum { CHANNELS = SWEETBG_BLUR_CHANNELS, MAX_DIMENSION = 32768 };

// Average directly into 16 bits so downsampling retains channel fractions
static uint16_t *reduce(const uint8_t *pixels, uint32_t width, uint32_t height,
	uint32_t factor, uint32_t rw, uint32_t rh) {
	size_t count = (size_t)rw * rh * CHANNELS;
	if (count > SIZE_MAX / sizeof(uint16_t)) {
		return NULL;
	}
	uint16_t *wide = malloc(count * sizeof(*wide));
	if (wide == NULL) {
		return NULL;
	}
	for (uint32_t y = 0; y < rh; y++) {
		uint32_t top = y * factor;
		uint32_t bottom = height - top < factor ? height : top + factor;
		for (uint32_t x = 0; x < rw; x++) {
			uint32_t left = x * factor;
			uint32_t right =
				width - left < factor ? width : left + factor;
			uint64_t sum[CHANNELS] = {0};
			for (uint32_t sy = top; sy < bottom; sy++) {
				const uint8_t *p =
					pixels +
					((size_t)sy * width + left) * 4;
				for (uint32_t sx = left; sx < right;
					sx++, p += 4) {
					for (int c = 0; c < CHANNELS; c++) {
						sum[c] += p[c];
					}
				}
			}
			uint64_t samples =
				(uint64_t)(bottom - top) * (right - left);
			for (int c = 0; c < CHANNELS; c++) {
				wide[((size_t)y * rw + x) * CHANNELS +
					(size_t)c] =
					(uint16_t)((sum[c] << 8) / samples);
			}
		}
	}
	return wide;
}

struct tap {
	uint32_t from;
	uint32_t to;
	uint32_t weight;
};

static struct tap tap_at(uint32_t pixel, uint32_t factor, uint32_t limit) {
	double at = (pixel + 0.5) / factor - 0.5;
	if (at <= 0) {
		return (struct tap){0, 0, 0};
	}
	double low = floor(at);
	uint32_t from = (uint32_t)low;
	if (from >= limit - 1) {
		return (struct tap){limit - 1, limit - 1, 0};
	}
	return (struct tap){from, from + 1, (uint32_t)lround((at - low) * 256)};
}

static const uint8_t bayer[4][4] = {
	{0, 8, 2, 10},
	{12, 4, 14, 6},
	{3, 11, 1, 9},
	{15, 7, 13, 5},
};

static bool scale_row(uint8_t *out, const uint32_t *row, size_t count,
	const struct tap *across, uint32_t width, uint32_t y) {
	if (count < CHANNELS) {
		return false;
	}
	for (uint32_t x = 0; x < width; x++, out += 4) {
		struct tap t = across[x];
		size_t a = (size_t)t.from * CHANNELS;
		size_t b = (size_t)t.to * CHANNELS;
		if (a > count - CHANNELS || b > count - CHANNELS) {
			return false;
		}
		const uint32_t *from = row + a;
		const uint32_t *to = row + b;
		uint32_t threshold = bayer[y & 3][x & 3] * 16u + 8;
		for (int c = 0; c < CHANNELS; c++) {
			uint32_t value = (from[c] * (256 - t.weight) +
						 to[c] * t.weight + 128) >>
					 8;
			uint32_t byte = (value + threshold) >> 8;
			out[c] = (uint8_t)(byte > 255 ? 255 : byte);
		}
		out[3] = 0;
	}
	return true;
}

static bool upscale(const uint16_t *wide, uint32_t rw, uint32_t rh,
	uint32_t factor, uint8_t *pixels, uint32_t width, uint32_t height) {
	if (rw == 0 || rh == 0 || rw > MAX_DIMENSION || rh > MAX_DIMENSION) {
		return false;
	}
	size_t count = (size_t)rw * CHANNELS;
	struct tap *across = calloc(width, sizeof(*across));
	uint32_t *row = calloc(count, sizeof(*row));
	bool ok = across != NULL && row != NULL;
	if (ok) {
		for (uint32_t x = 0; x < width; x++) {
			across[x] = tap_at(x, factor, rw);
		}
		for (uint32_t y = 0; y < height; y++) {
			struct tap down = tap_at(y, factor, rh);
			const uint16_t *top = wide + (size_t)down.from * count;
			const uint16_t *bottom = wide + (size_t)down.to * count;
			for (size_t i = 0; i < count; i++) {
				row[i] = (top[i] * (256 - down.weight) +
						 bottom[i] * down.weight +
						 128) >>
					 8;
			}
			if (!scale_row(pixels + (size_t)y * width * 4, row,
				    count, across, width, y)) {
				ok = false;
				break;
			}
		}
	}
	free(across);
	free(row);
	return ok;
}

bool sweetbg_blur(
	uint8_t *pixels, uint32_t width, uint32_t height, double sigma) {
	if (pixels == NULL || width == 0 || height == 0 ||
		width > MAX_DIMENSION || height > MAX_DIMENSION ||
		(uint64_t)width * height * 4 > INT32_MAX || !isfinite(sigma) ||
		sigma < 0 || sigma > 100.0 * MAX_DIMENSION) {
		return false;
	}
	if (sigma == 0) {
		return true;
	}
	// keep reduced sigma near three, as in Sweetlock's CPU blur
	uint32_t factor = sigma < 6 ? 1 : (uint32_t)floor(sigma / 3);
	uint32_t rw = (width + factor - 1) / factor;
	uint32_t rh = (height + factor - 1) / factor;
	uint16_t *wide = reduce(pixels, width, height, factor, rw, rh);
	bool ok = wide != NULL &&
		  sweetbg_blur_passes(
			  wide, (int32_t)rw, (int32_t)rh, sigma / factor) &&
		  upscale(wide, rw, rh, factor, pixels, width, height);
	free(wide);
	return ok;
}
