#include "image/palette.h"

#include <string.h>

#define BYTES_PER_PIXEL 4

#define PAL_BITS 3
#define PAL_LEVELS (1u << PAL_BITS)
#define PAL_BUCKETS (PAL_LEVELS * PAL_LEVELS * PAL_LEVELS)
#define PAL_SHIFT (8 - PAL_BITS)

#define PAL_SAMPLE_TARGET 2000000u

struct pal_bucket {
	uint64_t n;
	uint64_t r;
	uint64_t g;
	uint64_t b;
};

void sweetbg_palette_extract(const struct sweetbg_image *img, uint32_t *colors,
	size_t max, size_t *count) {
	*count = 0;
	if (colors == NULL || max == 0 || img == NULL || img->pixels == NULL ||
		img->width == 0 || img->height == 0) {
		return;
	}

	struct pal_bucket buckets[PAL_BUCKETS];
	memset(buckets, 0, sizeof(buckets));

	uint64_t total = (uint64_t)img->width * img->height;
	uint64_t stride = total / PAL_SAMPLE_TARGET;
	if (stride == 0) {
		stride = 1;
	}

	for (uint64_t i = 0; i < total; i += stride) {
		const uint8_t *p = img->pixels + i * BYTES_PER_PIXEL;
		uint32_t b = p[0];
		uint32_t g = p[1];
		uint32_t r = p[2];
		uint32_t idx = ((r >> PAL_SHIFT) << (2 * PAL_BITS)) |
			       ((g >> PAL_SHIFT) << PAL_BITS) |
			       (b >> PAL_SHIFT);
		buckets[idx].n++;
		buckets[idx].r += r;
		buckets[idx].g += g;
		buckets[idx].b += b;
	}

	// max is small, so repeated max-selection beats sorting all 512 buckets
	bool used[PAL_BUCKETS];
	memset(used, 0, sizeof(used));
	for (size_t k = 0; k < max; k++) {
		uint64_t best = 0;
		int best_i = -1;
		for (int i = 0; i < (int)PAL_BUCKETS; i++) {
			if (!used[i] && buckets[i].n > best) {
				best = buckets[i].n;
				best_i = i;
			}
		}
		if (best_i < 0) {
			break;
		}
		used[best_i] = true;
		const struct pal_bucket *bk = &buckets[best_i];
		uint32_t r = (uint32_t)(bk->r / bk->n);
		uint32_t g = (uint32_t)(bk->g / bk->n);
		uint32_t b = (uint32_t)(bk->b / bk->n);
		colors[(*count)++] = (r << 16) | (g << 8) | b;
	}
}
