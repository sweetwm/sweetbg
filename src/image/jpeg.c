#include "image/decoders.h"

#include <jpeglib.h>
#include <setjmp.h>
#include <stdlib.h>

#define BYTES_PER_PIXEL 4

struct jpeg_guard {
	struct jpeg_error_mgr base;
	jmp_buf jmp;
};

static void on_error(j_common_ptr cinfo) {
	struct jpeg_guard *guard = (struct jpeg_guard *)cinfo->err;
	longjmp(guard->jmp, 1);
}

bool caramel_decode_jpeg(
	FILE *fp, struct caramel_image *img, char *err, size_t err_size) {
	struct jpeg_decompress_struct cinfo;
	struct jpeg_guard guard;
	cinfo.err = jpeg_std_error(&guard.base);
	guard.base.error_exit = on_error;

	if (setjmp(guard.jmp)) {
		jpeg_destroy_decompress(&cinfo);
		free(img->pixels);
		img->pixels = NULL;
		snprintf(err, err_size, "jpeg: decode failed");
		return false;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fp);
	jpeg_read_header(&cinfo, TRUE);

	if (!caramel_image_dimensions_ok(
		    cinfo.image_width, cinfo.image_height)) {
		snprintf(err, err_size, "jpeg: image too large");
		jpeg_destroy_decompress(&cinfo);
		return false;
	}

	cinfo.out_color_space = JCS_EXT_BGRX;
	jpeg_start_decompress(&cinfo);

	uint32_t width = cinfo.output_width;
	uint32_t height = cinfo.output_height;
	img->width = width;
	img->height = height;
	img->pixels = malloc((size_t)width * height * BYTES_PER_PIXEL);
	if (img->pixels == NULL) {
		snprintf(err, err_size, "jpeg: out of memory");
		jpeg_destroy_decompress(&cinfo);
		return false;
	}

	size_t stride = (size_t)width * BYTES_PER_PIXEL;
	while (cinfo.output_scanline < height) {
		JSAMPROW row =
			img->pixels + (size_t)cinfo.output_scanline * stride;
		jpeg_read_scanlines(&cinfo, &row, 1);
	}

	jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	return true;
}
