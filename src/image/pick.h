#ifndef SWEETBG_IMAGE_PICK_H
#define SWEETBG_IMAGE_PICK_H

#include <stdbool.h>
#include <stddef.h>

bool sweetbg_pick_random_image(const char *dir, char *out, size_t out_size,
	char *err, size_t err_size);

#endif
