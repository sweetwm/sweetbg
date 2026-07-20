#include "image/pick.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/random.h>
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
	if (s >= n) {
		return false;
	}
	const char *tail = name + (n - s);
	for (size_t i = 0; i < s; i++) {
		char a = tail[i];
		if (a >= 'A' && a <= 'Z') {
			a = (char)(a - 'A' + 'a');
		}
		if (a != suffix[i]) {
			return false;
		}
	}
	return true;
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

static bool seed_random(uint64_t *seed) {
	size_t got = 0;
	while (got < sizeof(*seed)) {
		ssize_t n = getrandom(
			(uint8_t *)seed + got, sizeof(*seed) - got, 0);
		if (n < 0) {
			if (errno == EINTR) {
				continue;
			}
			return false;
		}
		got += (size_t)n;
	}
	return true;
}

static uint64_t next_random(uint64_t *state) {
	// splitmix64: one getrandom call seeds it, the walk needs no more
	*state += 0x9e3779b97f4a7c15ULL;
	uint64_t z = *state;
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
	return z ^ (z >> 31);
}

static bool is_regular(const char *dir, const struct dirent *entry) {
#ifdef _DIRENT_HAVE_D_TYPE
	if (entry->d_type == DT_REG) {
		return true;
	}
	// Symlinks and DT_UNKNOWN need a stat to classify
	if (entry->d_type != DT_UNKNOWN && entry->d_type != DT_LNK) {
		return false;
	}
#endif
	char full[PATH_MAX];
	if (snprintf(full, sizeof(full), "%s/%s", dir, entry->d_name) >=
		(int)sizeof(full)) {
		return false;
	}
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

	uint64_t state;
	if (!seed_random(&state)) {
		closedir(dp);
		snprintf(err, err_size, "cannot get randomness: %s",
			strerror(errno));
		return false;
	}

	// Reservoir sampling keeps this to one pass and one candidate buffer,
	// so a directory of any size costs the same memory
	char chosen[PATH_MAX];
	uint64_t seen = 0;
	bool truncated = false;

	errno = 0;
	const struct dirent *entry;
	while ((entry = readdir(dp)) != NULL) {
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
		if (!is_regular(dir, entry)) {
			continue;
		}
		seen++;
		if (next_random(&state) % seen == 0) {
			memcpy(chosen, full, strlen(full) + 1);
		}
		errno = 0;
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
