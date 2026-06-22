// Cover-fit geometry tests. Pure: links only fit.c.

#include "image/fit.h"

#include <stdio.h>

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL line %d: %s\n", __LINE__,        \
				#cond);                                        \
			return 1;                                              \
		}                                                              \
	} while (0)

// A source already matching the target aspect is used whole
static int test_same_aspect(void) {
	struct caramel_rect r;
	caramel_cover_rect(1920, 1080, 3840, 2160, &r);
	CHECK(r.x == 0 && r.y == 0);
	CHECK(r.w == 1920 && r.h == 1080);
	return 0;
}

// A source wider than the target keeps full height and crops the sides
static int test_wider_source(void) {
	struct caramel_rect r;
	caramel_cover_rect(4000, 1000, 1000, 1000, &r);
	CHECK(r.h == 1000);
	CHECK(r.w == 1000);
	CHECK(r.y == 0);
	CHECK(r.x == 1500); // (4000 - 1000) / 2
	return 0;
}

// A source taller than the target keeps full width and crops top and bottom
static int test_taller_source(void) {
	struct caramel_rect r;
	caramel_cover_rect(1000, 4000, 1000, 1000, &r);
	CHECK(r.w == 1000);
	CHECK(r.h == 1000);
	CHECK(r.x == 0);
	CHECK(r.y == 1500);
	return 0;
}

// The crop must never exceed the source bounds
static int test_crop_within_bounds(void) {
	struct caramel_rect r;
	caramel_cover_rect(1366, 768, 2560, 1440, &r);
	CHECK(r.x + r.w <= 1366);
	CHECK(r.y + r.h <= 768);
	return 0;
}

int main(void) {
	int rc = 0;
	rc |= test_same_aspect();
	rc |= test_wider_source();
	rc |= test_taller_source();
	rc |= test_crop_within_bounds();
	if (rc == 0) {
		printf("image fit: all checks passed\n");
	}
	return rc;
}
