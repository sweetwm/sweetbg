#ifndef SWEETBG_IPC_PREPARED_H
#define SWEETBG_IPC_PREPARED_H

#include <stdint.h>

#include "image/image.h"

int sweetbg_prepared_buffer_create(const struct sweetbg_image *image,
	enum sweetbg_fit fit, uint32_t width, uint32_t height, uint32_t color,
	const struct sweetbg_placement *placement);

#endif
