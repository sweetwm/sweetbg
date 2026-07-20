#ifndef SWEETBG_IMAGE_PICK_H
#define SWEETBG_IMAGE_PICK_H

#include <stdbool.h>
#include <stddef.h>

// Choose a random supported image directly inside `dir` and write its absolute
// path to `out`. Only the client resolves directories: the daemon remembers the
// chosen file, so re-preparing an output never reshuffles the wallpaper.
bool sweetbg_pick_random_image(const char *dir, char *out, size_t out_size,
	char *err, size_t err_size);

#endif
