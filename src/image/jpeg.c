#include "image/decoders.h"

#include <jpeglib.h>
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#define BYTES_PER_PIXEL 4

struct jpeg_guard {
	struct jpeg_error_mgr base;
	void (*default_emit_message)(j_common_ptr cinfo, int msg_level);
	int eof_warning_code;
	jmp_buf jmp;
};

static void on_error(j_common_ptr cinfo) {
	struct jpeg_guard *guard = (struct jpeg_guard *)cinfo->err;
	longjmp(guard->jmp, 1);
}

static void on_message(j_common_ptr cinfo, int msg_level) {
	struct jpeg_guard *guard = (struct jpeg_guard *)cinfo->err;
	if (msg_level < 0 && guard->base.msg_code == guard->eof_warning_code) {
		longjmp(guard->jmp, 2);
	}
	guard->default_emit_message(cinfo, msg_level);
}

static int eof_warning_code(const struct jpeg_error_mgr *errors) {
	// Message enum values can differ across libjpeg ABI compatibility modes
	if (errors->jpeg_message_table == NULL) {
		return -1;
	}
	for (int i = 0; i <= errors->last_jpeg_message; i++) {
		if (errors->jpeg_message_table[i] != NULL &&
			strcmp(errors->jpeg_message_table[i],
				"Premature end of JPEG file") == 0) {
			return i;
		}
	}
	return -1;
}

bool sweetbg_decode_jpeg(FILE *fp, struct sweetbg_image *img,
	const struct sweetbg_image_load_options *options, char *err,
	size_t err_size) {
	struct jpeg_decompress_struct cinfo;
	struct jpeg_guard guard = {0};
	cinfo.err = jpeg_std_error(&guard.base);
	guard.default_emit_message = guard.base.emit_message;
	guard.eof_warning_code = eof_warning_code(&guard.base);
	guard.base.error_exit = on_error;
	guard.base.emit_message = on_message;

	int jump_reason = setjmp(guard.jmp);
	if (jump_reason != 0) {
		jpeg_destroy_decompress(&cinfo);
		free(img->pixels);
		img->pixels = NULL;
		snprintf(err, err_size,
			jump_reason == 2 ? "jpeg: file is truncated"
					 : "jpeg: decode failed");
		return false;
	}

	jpeg_create_decompress(&cinfo);
	jpeg_stdio_src(&cinfo, fp);
	jpeg_read_header(&cinfo, TRUE);

	if (!sweetbg_image_dimensions_ok(
		    cinfo.image_width, cinfo.image_height)) {
		snprintf(err, err_size, "jpeg: image too large");
		jpeg_destroy_decompress(&cinfo);
		return false;
	}

	cinfo.out_color_space = JCS_EXT_BGRX;
	static const unsigned int denominators[] = {8, 4, 2};
	cinfo.scale_num = 1;
	for (size_t i = 0; i < sizeof(denominators) / sizeof(denominators[0]);
		i++) {
		cinfo.scale_denom = denominators[i];
		jpeg_calc_output_dimensions(&cinfo);
		if (sweetbg_decode_targets_fit(
			    cinfo.output_width, cinfo.output_height, options)) {
			break;
		}
		cinfo.scale_denom = 1;
	}
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
