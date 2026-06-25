#include "config/config_write.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config/config.h"

#define CONFIG_LINE_MAX (PATH_MAX + 64)
#define SECTION_NAME_MAX 128
#define MAX_CONFIG_BYTES (1u << 20)

static bool valid_value(const char *s) {
	return strlen(s) < PATH_MAX && strchr(s, '"') == NULL &&
	       strchr(s, '\n') == NULL && strchr(s, '\r') == NULL;
}

static bool valid_output_name(const char *s) {
	size_t len = strlen(s);
	if (len == 0 || len >= 64) {
		return false;
	}
	return s[strcspn(s, " \t\r\n\"#[]")] == '\0';
}

static void logical_line(
	const char *p, size_t linelen, char *out, size_t out_size) {
	while (linelen > 0 &&
		(p[linelen - 1] == '\n' || p[linelen - 1] == '\r')) {
		linelen--;
	}
	size_t start = 0;
	while (start < linelen && (p[start] == ' ' || p[start] == '\t')) {
		start++;
	}
	while (linelen > start &&
		(p[linelen - 1] == ' ' || p[linelen - 1] == '\t')) {
		linelen--;
	}
	size_t n = linelen - start;
	if (n >= out_size) {
		n = out_size - 1;
	}
	memcpy(out, p + start, n);
	out[n] = '\0';
}

static bool is_section(const char *t, char *name_out, size_t out_size) {
	if (t[0] != '[') {
		return false;
	}
	const char *close = strchr(t, ']');
	if (close == NULL) {
		return false;
	}
	size_t n = (size_t)(close - (t + 1));
	if (n >= out_size) {
		n = out_size - 1;
	}
	memcpy(name_out, t + 1, n);
	name_out[n] = '\0';
	return true;
}

static bool section_is_target(const char *secname, const char *output_name) {
	const char *prefix = "output.";
	size_t plen = strlen(prefix);
	return strncmp(secname, prefix, plen) == 0 &&
	       strcmp(secname + plen, output_name) == 0;
}

static bool line_has_key(const char *t, const char *key) {
	if (t[0] == '\0' || t[0] == '#' || t[0] == '[') {
		return false;
	}
	const char *eq = strchr(t, '=');
	if (eq == NULL) {
		return false;
	}
	size_t klen = (size_t)(eq - t);
	while (klen > 0 && (t[klen - 1] == ' ' || t[klen - 1] == '\t')) {
		klen--;
	}
	return klen == strlen(key) && strncmp(t, key, klen) == 0;
}

static void emit(
	FILE *ms, const char *data, size_t len, bool *at_start, bool *any) {
	if (len == 0) {
		return;
	}
	fwrite(data, 1, len, ms);
	*at_start = data[len - 1] == '\n';
	*any = true;
}

static void emit_image(FILE *ms, const char *path, bool *at_start, bool *any) {
	if (!*at_start) {
		fputc('\n', ms);
		*at_start = true;
	}
	char line[PATH_MAX + 16];
	int n = snprintf(line, sizeof(line), "image = \"%s\"\n", path);
	if (n > 0) {
		emit(ms, line, (size_t)n, at_start, any);
	}
}

bool caramel_config_patch_image(const char *input, const char *output_name,
	const char *image_path, char **out, char *err, size_t err_size) {
	if (!valid_value(image_path)) {
		snprintf(err, err_size,
			"image path is too long or contains quotes/newlines");
		return false;
	}
	if (output_name != NULL && !valid_output_name(output_name)) {
		snprintf(err, err_size, "invalid output name");
		return false;
	}

	char *buf = NULL;
	size_t buf_size = 0;
	FILE *ms = open_memstream(&buf, &buf_size);
	if (ms == NULL) {
		snprintf(err, err_size, "out of memory");
		return false;
	}

	bool target_default = output_name == NULL;
	enum { SCOPE_TOP, SCOPE_TARGET, SCOPE_OTHER } scope = SCOPE_TOP;
	bool written = false;
	bool target_seen = false;
	bool at_start = true;
	bool any = false;

	const char *p = input != NULL ? input : "";
	while (*p != '\0') {
		const char *eol = strchr(p, '\n');
		size_t linelen =
			eol != NULL ? (size_t)(eol - p) + 1 : strlen(p);

		char t[CONFIG_LINE_MAX];
		logical_line(p, linelen, t, sizeof(t));

		char secname[SECTION_NAME_MAX];
		bool want_scope = target_default ? scope == SCOPE_TOP
						 : scope == SCOPE_TARGET;
		if (is_section(t, secname, sizeof(secname))) {
			// The current scope is ending; insert here if still
			// owed
			if (!written && want_scope) {
				emit_image(ms, image_path, &at_start, &any);
				written = true;
			}
			if (!target_default &&
				section_is_target(secname, output_name)) {
				scope = SCOPE_TARGET;
				target_seen = true;
			} else {
				scope = SCOPE_OTHER;
			}
			emit(ms, p, linelen, &at_start, &any);
		} else if (want_scope && line_has_key(t, "image")) {
			if (!written) {
				emit_image(ms, image_path, &at_start, &any);
				written = true;
			}
			// drop the replaced line (and any later duplicate)
		} else {
			emit(ms, p, linelen, &at_start, &any);
		}
		p += linelen;
	}

	if (!written) {
		if (target_default || target_seen) {
			emit_image(ms, image_path, &at_start, &any);
		} else {
			if (!at_start) {
				fputc('\n', ms);
			}
			if (any) {
				fputc('\n', ms);
			}
			fprintf(ms, "[output.%s]\n", output_name);
			at_start = true;
			emit_image(ms, image_path, &at_start, &any);
		}
	}

	if (fclose(ms) != 0) {
		free(buf);
		snprintf(err, err_size, "out of memory");
		return false;
	}
	*out = buf;
	return true;
}

static bool read_config(
	const char *path, char **out, char *err, size_t err_size) {
	*out = NULL;
	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		if (errno == ENOENT) {
			return true; // missing file: patch an empty document
		}
		snprintf(err, err_size, "%s: %s", path, strerror(errno));
		return false;
	}
	if (fseek(fp, 0, SEEK_END) != 0) {
		fclose(fp);
		snprintf(err, err_size, "%s: %s", path, strerror(errno));
		return false;
	}
	long size = ftell(fp);
	if (size < 0 || fseek(fp, 0, SEEK_SET) != 0) {
		fclose(fp);
		snprintf(err, err_size, "%s: %s", path, strerror(errno));
		return false;
	}
	if ((unsigned long)size > MAX_CONFIG_BYTES) {
		fclose(fp);
		snprintf(err, err_size, "%s: config file is too large", path);
		return false;
	}
	char *buf = malloc((size_t)size + 1);
	if (buf == NULL) {
		fclose(fp);
		snprintf(err, err_size, "out of memory");
		return false;
	}
	size_t got = fread(buf, 1, (size_t)size, fp);
	fclose(fp);
	// got <= size by the fread contract and buf holds size + 1 bytes
	// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
	buf[got] = '\0';
	*out = buf;
	return true;
}

// Create the config directory and any missing parents (mode 0700)
static bool ensure_parent_dir(const char *path, char *err, size_t err_size) {
	char dir[PATH_MAX];
	int n = snprintf(dir, sizeof(dir), "%s", path);
	if (n <= 0 || (size_t)n >= sizeof(dir)) {
		snprintf(err, err_size, "config path is too long");
		return false;
	}
	char *slash = strrchr(dir, '/');
	if (slash == NULL || slash == dir) {
		return true;
	}
	*slash = '\0';
	for (char *q = dir + 1; *q != '\0'; q++) {
		if (*q != '/') {
			continue;
		}
		*q = '\0';
		if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
			snprintf(err, err_size, "%s: %s", dir, strerror(errno));
			return false;
		}
		*q = '/';
	}
	if (mkdir(dir, 0700) != 0 && errno != EEXIST) {
		snprintf(err, err_size, "%s: %s", dir, strerror(errno));
		return false;
	}
	return true;
}

static bool write_atomic(
	const char *path, const char *data, char *err, size_t err_size) {
	if (!ensure_parent_dir(path, err, err_size)) {
		return false;
	}
	char tmp[PATH_MAX];
	int n = snprintf(tmp, sizeof(tmp), "%s.tmp", path);
	if (n <= 0 || (size_t)n >= sizeof(tmp)) {
		snprintf(err, err_size, "config path is too long");
		return false;
	}
	FILE *fp = fopen(tmp, "w");
	if (fp == NULL) {
		snprintf(err, err_size, "%s: %s", tmp, strerror(errno));
		return false;
	}
	size_t len = strlen(data);
	bool ok = fwrite(data, 1, len, fp) == len && fflush(fp) == 0;
	if (fclose(fp) != 0) {
		ok = false;
	}
	if (!ok) {
		unlink(tmp);
		snprintf(err, err_size, "failed to write %s", tmp);
		return false;
	}
	if (rename(tmp, path) != 0) {
		snprintf(err, err_size, "%s: %s", path, strerror(errno));
		unlink(tmp);
		return false;
	}
	return true;
}

bool caramel_config_persist_image(const char *output_name,
	const char *image_path, char *err, size_t err_size) {
	char path[PATH_MAX];
	if (!caramel_config_path(path, sizeof(path))) {
		snprintf(err, err_size,
			"cannot resolve config path (set XDG_CONFIG_HOME or "
			"HOME)");
		return false;
	}

	char *content = NULL;
	if (!read_config(path, &content, err, err_size)) {
		return false;
	}

	char *patched = NULL;
	bool ok = caramel_config_patch_image(
		content, output_name, image_path, &patched, err, err_size);
	free(content);
	if (!ok) {
		return false;
	}

	ok = write_atomic(path, patched, err, err_size);
	free(patched);
	return ok;
}
