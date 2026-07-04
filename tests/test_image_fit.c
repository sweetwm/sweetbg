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
	struct sweetbg_rect r;
	sweetbg_cover_rect(1920, 1080, 3840, 2160, &r);
	CHECK(r.x == 0 && r.y == 0);
	CHECK(r.w == 1920 && r.h == 1080);
	return 0;
}

// A source wider than the target keeps full height and crops the sides
static int test_wider_source(void) {
	struct sweetbg_rect r;
	sweetbg_cover_rect(4000, 1000, 1000, 1000, &r);
	CHECK(r.h == 1000);
	CHECK(r.w == 1000);
	CHECK(r.y == 0);
	CHECK(r.x == 1500); // (4000 - 1000) / 2
	return 0;
}

// A source taller than the target keeps full width and crops top and bottom
static int test_taller_source(void) {
	struct sweetbg_rect r;
	sweetbg_cover_rect(1000, 4000, 1000, 1000, &r);
	CHECK(r.w == 1000);
	CHECK(r.h == 1000);
	CHECK(r.x == 0);
	CHECK(r.y == 1500);
	return 0;
}

// The crop must never exceed the source bounds
static int test_crop_within_bounds(void) {
	struct sweetbg_rect r;
	sweetbg_cover_rect(1366, 768, 2560, 1440, &r);
	CHECK(r.x + r.w <= 1366);
	CHECK(r.y + r.h <= 768);
	return 0;
}

// Contain letterboxes a wide source: full output width, centered vertically
static int test_contain_wide(void) {
	struct sweetbg_placement p;
	sweetbg_contain_rects(2000, 1000, 1000, 1000, &p);
	CHECK(p.src.x == 0 && p.src.y == 0);
	CHECK(p.src.w == 2000 && p.src.h == 1000);
	CHECK(p.dst.w == 1000); // bound by output width
	CHECK(p.dst.h == 500);	// 1000 * 1000 / 2000
	CHECK(p.dst.x == 0);
	CHECK(p.dst.y == 250); // (1000 - 500) / 2
	return 0;
}

// Contain pillarboxes a tall source: full output height, centered horizontally
static int test_contain_tall(void) {
	struct sweetbg_placement p;
	sweetbg_contain_rects(1000, 2000, 1000, 1000, &p);
	CHECK(p.dst.h == 1000);
	CHECK(p.dst.w == 500);
	CHECK(p.dst.y == 0);
	CHECK(p.dst.x == 250);
	return 0;
}

// Center pads a small source equally on both axes at native size
static int test_center_smaller(void) {
	struct sweetbg_placement p;
	sweetbg_center_rects(400, 200, 1000, 1000, &p);
	CHECK(p.src.x == 0 && p.src.y == 0 && p.src.w == 400 && p.src.h == 200);
	CHECK(p.dst.w == 400 && p.dst.h == 200);
	CHECK(p.dst.x == 300); // (1000 - 400) / 2
	CHECK(p.dst.y == 400); // (1000 - 200) / 2
	return 0;
}

// Center crops a large source to the output, centered in source space
static int test_center_larger(void) {
	struct sweetbg_placement p;
	sweetbg_center_rects(3000, 3000, 1000, 1000, &p);
	CHECK(p.src.w == 1000 && p.src.h == 1000);
	CHECK(p.src.x == 1000 && p.src.y == 1000); // (3000 - 1000) / 2
	CHECK(p.dst.x == 0 && p.dst.y == 0);
	CHECK(p.dst.w == 1000 && p.dst.h == 1000);
	return 0;
}

int main(void) {
	int rc = 0;
	rc |= test_same_aspect();
	rc |= test_wider_source();
	rc |= test_taller_source();
	rc |= test_crop_within_bounds();
	rc |= test_contain_wide();
	rc |= test_contain_tall();
	rc |= test_center_smaller();
	rc |= test_center_larger();
	if (rc == 0) {
		printf("image fit: all checks passed\n");
	}
	return rc;
}
