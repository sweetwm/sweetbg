#include "image/pick.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static const char *const supported_extensions[] = {
	".jpg",
	".jpeg",
	".png",
	".webp",
};

static bool ends_with_ci(const char *name, const char *suffix) {
	size_t n = strlen(name);
	size_t s = strlen(suffix);
	return s < n && strcasecmp(name + (n - s), suffix) == 0;
}

static bool has_supported_extension(const char *name) {
	size_t count =
		sizeof(supported_extensions) / sizeof(supported_extensions[0]);
	for (size_t i = 0; i < count; i++) {
		if (ends_with_ci(name, supported_extensions[i])) {
			return true;
		}
	}
	return false;
}

static bool is_regular(const char *full, const struct dirent *entry) {
#ifdef _DIRENT_HAVE_D_TYPE
	if (entry->d_type == DT_REG) {
		return true;
	}
	// Symlinks and DT_UNKNOWN need a stat to classify
	if (entry->d_type != DT_UNKNOWN && entry->d_type != DT_LNK) {
		return false;
	}
#endif
	struct stat st;
	return stat(full, &st) == 0 && S_ISREG(st.st_mode);
}

bool sweetbg_pick_random_image(const char *dir, char *out, size_t out_size,
	char *err, size_t err_size) {
	DIR *dp = opendir(dir);
	if (dp == NULL) {
		snprintf(err, err_size, "cannot read directory '%s': %s", dir,
			strerror(errno));
		return false;
	}

	// Reservoir sampling keeps this to one pass and one candidate buffer,
	// so a directory of any size costs the same memory
	// slways filled on the first candidate (arc4random_uniform(1) is 0)
	char chosen[PATH_MAX] = "";
	uint32_t seen = 0;
	bool truncated = false;

	// readdir reports errors only through errno, and the stat in is_regular
	// may leave one behind, so clear it before every call
	const struct dirent *entry;
	for (errno = 0; (entry = readdir(dp)) != NULL; errno = 0) {
		if (entry->d_name[0] == '.' ||
			!has_supported_extension(entry->d_name)) {
			continue;
		}
		char full[PATH_MAX];
		if (snprintf(full, sizeof(full), "%s/%s", dir, entry->d_name) >=
			(int)sizeof(full)) {
			truncated = true;
			continue;
		}
		if (!is_regular(full, entry)) {
			continue;
		}
		seen++;
		if (arc4random_uniform(seen) == 0) {
			memcpy(chosen, full, strlen(full) + 1);
		}
	}
	int read_errno = errno;
	closedir(dp);

	if (read_errno != 0) {
		snprintf(err, err_size, "cannot read directory '%s': %s", dir,
			strerror(read_errno));
		return false;
	}
	if (seen == 0) {
		snprintf(err, err_size,
			truncated ? "no usable images in '%s' (paths too long)"
				  : "no JPEG, PNG, or WebP images in '%s'",
			dir);
		return false;
	}
	if (strlen(chosen) >= out_size) {
		snprintf(err, err_size, "chosen path is too long");
		return false;
	}
	memcpy(out, chosen, strlen(chosen) + 1);
	return true;
}
