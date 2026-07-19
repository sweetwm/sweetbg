#include "image/decoders.h"

#include <png.h>
#include <stdlib.h>

#define BYTES_PER_PIXEL 4

// Swap R and B and clear the pad byte: libpng gives RGBA, we store XRGB8888
static void rgba_to_xrgb(uint8_t *pixels, size_t pixel_count) {
	for (size_t i = 0; i < pixel_count; i++) {
		uint8_t *p = pixels + i * BYTES_PER_PIXEL;
		uint8_t r = p[0];
		p[0] = p[2];
		p[2] = r;
		p[3] = 0;
	}
}

bool sweetbg_decode_png(
	FILE *fp, struct sweetbg_image *img, char *err, size_t err_size) {
	png_structp png =
		png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png == NULL) {
		snprintf(err, err_size, "png: out of memory");
		return false;
	}
	png_infop info = png_create_info_struct(png);
	if (info == NULL) {
		png_destroy_read_struct(&png, NULL, NULL);
		snprintf(err, err_size, "png: out of memory");
		return false;
	}

	png_bytep *volatile rows = NULL;
	if (setjmp(png_jmpbuf(png))) {
		// libpng longjmps here on any error during the calls below
		free((void *)rows);
		free(img->pixels);
		img->pixels = NULL;
		png_destroy_read_struct(&png, &info, NULL);
		snprintf(err, err_size, "png: decode failed");
		return false;
	}

	png_init_io(png, fp);
	png_read_info(png, info);

	uint32_t width = png_get_image_width(png, info);
	uint32_t height = png_get_image_height(png, info);
	if (!sweetbg_image_dimensions_ok(width, height)) {
		snprintf(err, err_size, "png: image too large");
		png_destroy_read_struct(&png, &info, NULL);
		return false;
	}

	// Normalize every input to 8-bit RGBA
	int bit_depth = png_get_bit_depth(png, info);
	int color_type = png_get_color_type(png, info);
	if (bit_depth == 16) {
		png_set_strip_16(png);
	}
	if (color_type == PNG_COLOR_TYPE_PALETTE) {
		png_set_palette_to_rgb(png);
	}
	if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
		png_set_expand_gray_1_2_4_to_8(png);
	}
	if (png_get_valid(png, info, PNG_INFO_tRNS)) {
		png_set_tRNS_to_alpha(png);
	}
	if (color_type == PNG_COLOR_TYPE_GRAY ||
		color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		png_set_gray_to_rgb(png);
	}
	png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
	png_read_update_info(png, info);

	img->width = width;
	img->height = height;
	img->pixels = malloc((size_t)width * height * BYTES_PER_PIXEL);
	rows = (png_bytep *)malloc((size_t)height * sizeof(png_bytep));
	if (img->pixels == NULL || rows == NULL) {
		snprintf(err, err_size, "png: out of memory");
		png_longjmp(png, 1);
	}

	size_t stride = (size_t)width * BYTES_PER_PIXEL;
	for (uint32_t y = 0; y < height; y++) {
		rows[y] = img->pixels + (size_t)y * stride;
	}
	png_read_image(png, rows);
	png_read_end(png, NULL);

	free((void *)rows);
	png_destroy_read_struct(&png, &info, NULL);
	rgba_to_xrgb(img->pixels, (size_t)width * height);
	return true;
}
