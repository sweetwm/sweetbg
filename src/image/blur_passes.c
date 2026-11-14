#include "image/blur_passes.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum { PASSES = 3 };

static void radii(double sigma, int32_t radius[PASSES]) {
	double variance = sigma * sigma;
	int32_t low = (int32_t)floor((sqrt(1 + 4 * variance) - 1) / 2);
	double step = 2.0 * (low + 1) / (double)PASSES;
	double wider = round((variance - (double)low * (low + 1)) / step);
	for (int i = 0; i < PASSES; i++) {
		radius[i] = low + (i < wider ? 1 : 0);
	}
}

static int32_t clamp(int32_t i, int32_t count) {
	return i < 0 ? 0 : i >= count ? count - 1 : i;
}

enum {
	// values per column strip in the vertical pass, so a strip stays in
	// cache
	STRIP = 64,
};

static void box(const uint16_t *in, uint16_t *out, int32_t count, int32_t lanes,
	int32_t radius) {
	uint32_t sum[STRIP] = {0};
	uint32_t width = 2 * (uint32_t)radius + 1;
	// sum / width, exact for the sums a box of 16-bit values can reach
	uint64_t inverse = (((uint64_t)1 << 32) + width - 1) / width;
	size_t row = (size_t)lanes;
	for (int32_t i = -radius; i <= radius; i++) {
		const uint16_t *sample = in + (size_t)clamp(i, count) * row;
		for (int32_t l = 0; l < lanes; l++) {
			sum[l] += sample[l];
		}
	}
	for (int32_t i = 0; i < count; i++) {
		const uint16_t *enter =
			in + (size_t)clamp(i + radius + 1, count) * row;
		const uint16_t *leave =
			in + (size_t)clamp(i - radius, count) * row;
		uint16_t *result = out + (size_t)i * row;
		for (int32_t l = 0; l < lanes; l++) {
			result[l] = (uint16_t)((sum[l] + width / 2) * inverse >>
					       32);
			sum[l] += (uint32_t)enter[l] - leave[l];
		}
	}
}

// line is count samples of lanes values; a and b each hold one such line
static void line_passes(uint16_t *line, uint16_t *a, uint16_t *b, int32_t count,
	int32_t lanes, const int32_t radius[PASSES]) {
	box(line, a, count, lanes, radius[0]);
	box(a, b, count, lanes, radius[1]);
	box(b, line, count, lanes, radius[2]);
}

// columns in strips of STRIP values, copied out and back
static void columns(uint16_t *pixels, int32_t width, int32_t height,
	uint16_t *scratch, const int32_t radius[PASSES]) {
	size_t row = (size_t)width * SWEETBG_BLUR_CHANNELS;
	size_t size = (size_t)height * STRIP;
	uint16_t *strip = scratch;
	for (size_t x = 0; x < row; x += STRIP) {
		size_t lanes = row - x < STRIP ? row - x : STRIP;
		for (int32_t y = 0; y < height; y++) {
			memcpy(strip + (size_t)y * lanes,
				pixels + (size_t)y * row + x,
				lanes * sizeof *strip);
		}
		line_passes(strip, scratch + size, scratch + 2 * size, height,
			(int32_t)lanes, radius);
		for (int32_t y = 0; y < height; y++) {
			memcpy(pixels + (size_t)y * row + x,
				strip + (size_t)y * lanes,
				lanes * sizeof *strip);
		}
	}
}

bool sweetbg_blur_passes(
	uint16_t *pixels, int32_t width, int32_t height, double sigma) {
	int32_t radius[PASSES];
	radii(sigma, radius);
	if (width < 1 || height < 1 || radius[0] < 1) {
		return true;
	}
	size_t row = (size_t)width * SWEETBG_BLUR_CHANNELS;
	size_t strip = (size_t)height * STRIP;
	size_t line = row > strip ? row : strip;
	uint16_t *scratch = calloc(3 * line, sizeof *scratch);
	if (!scratch) {
		return false;
	}
	for (int32_t y = 0; y < height; y++) {
		line_passes(pixels + (size_t)y * row, scratch, scratch + line,
			width, SWEETBG_BLUR_CHANNELS, radius);
	}
	columns(pixels, width, height, scratch, radius);
	free(scratch);
	return true;
}
