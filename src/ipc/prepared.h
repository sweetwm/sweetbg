#ifndef SWEETBG_IPC_PREPARED_H
#define SWEETBG_IPC_PREPARED_H

#include <stdint.h>

#include "image/image.h"
#include "image/layout.h"

#define SWEETBG_MAX_OUTPUTS 64

struct sweetbg_output_info {
	char name[64];
	uint32_t width;
	uint32_t height;
	uint32_t generation;
	int32_t scale;
	enum sweetbg_fit fit;
	bool has_generation;
	struct sweetbg_layout_output logical;
};

struct sweetbg_prepared_buffer {
	uint32_t width;
	uint32_t height;
	enum sweetbg_fit fit;
	int fd;
};

int sweetbg_prepared_buffer_create(const struct sweetbg_image *image,
	enum sweetbg_fit fit, uint32_t width, uint32_t height, uint32_t color,
	const struct sweetbg_placement *placement);

int sweetbg_prepared_buffer_find(const struct sweetbg_prepared_buffer *buffers,
	size_t count, uint32_t width, uint32_t height, enum sweetbg_fit fit);

int sweetbg_prepared_buffer_for_output(const struct sweetbg_image *image,
	const struct sweetbg_output_info *outputs, int output_count, int index,
	uint32_t color);

void sweetbg_prepared_decode_target(struct sweetbg_decode_target *target,
	const struct sweetbg_output_info *outputs, int output_count, int index);

#endif
