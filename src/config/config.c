#include "config/config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_COLOR 0x1e1e2e
#define CONFIG_LINE_MAX (PATH_MAX + 64)

void caramel_config_defaults(struct caramel_config *cfg) {
	cfg->image[0] = '\0';
	cfg->color = DEFAULT_COLOR;
	cfg->fit = CARAMEL_FIT_COVER;
}

// Skip leading blanks and strip trailing blanks/newline in place
static char *trim(char *s) {
	while (*s == ' ' || *s == '\t') {
		s++;
	}
	char *end = s + strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' ||
				  end[-1] == '\r' || end[-1] == '\n')) {
		*--end = '\0';
	}
	return s;
}

static bool parse_string(const char *value, char *out, size_t out_size) {
	if (value[0] != '"') {
		return false;
	}
	const char *start = value + 1;
	const char *end = strchr(start, '"');
	if (end == NULL) {
		return false;
	}
	const char *after = end + 1;
	while (*after == ' ' || *after == '\t') {
		after++;
	}
	if (*after != '\0' && *after != '#') {
		return false;
	}
	size_t len = (size_t)(end - start);
	if (len >= out_size) {
		return false;
	}
	memcpy(out, start, len);
	out[len] = '\0';
	return true;
}

static int hex_value(char c) {
	if (c >= '0' && c <= '9') {
		return c - '0';
	}
	if (c >= 'a' && c <= 'f') {
		return c - 'a' + 10;
	}
	if (c >= 'A' && c <= 'F') {
		return c - 'A' + 10;
	}
	return -1;
}

// Parse "#rrggbb" into an XRGB8888 word (0x00RRGGBB)
static bool parse_color(const char *s, uint32_t *out) {
	if (strlen(s) != 7 || s[0] != '#') {
		return false;
	}
	uint32_t value = 0;
	for (int i = 1; i <= 6; i++) {
		int digit = hex_value(s[i]);
		if (digit < 0) {
			return false;
		}
		value = (value << 4) | (uint32_t)digit;
	}
	*out = value;
	return true;
}

static bool apply_pair(struct caramel_config *cfg, const char *key,
	const char *value, const char *name, int line, char *err,
	size_t err_size) {
	char text[PATH_MAX];
	if (!parse_string(value, text, sizeof(text))) {
		snprintf(err, err_size, "%s:%d: value must be a quoted string",
			name, line);
		return false;
	}

	if (strcmp(key, "image") == 0) {
		memcpy(cfg->image, text, strlen(text) + 1);
		return true;
	}
	if (strcmp(key, "color") == 0) {
		if (!parse_color(text, &cfg->color)) {
			snprintf(err, err_size,
				"%s:%d: color must be \"#rrggbb\"", name, line);
			return false;
		}
		return true;
	}
	if (strcmp(key, "fit") == 0) {
		if (strcmp(text, "cover") != 0) {
			snprintf(err, err_size, "%s:%d: fit must be \"cover\"",
				name, line);
			return false;
		}
		cfg->fit = CARAMEL_FIT_COVER;
		return true;
	}

	snprintf(err, err_size, "%s:%d: unknown key '%s'", name, line, key);
	return false;
}

bool caramel_config_parse(FILE *fp, const char *name,
	struct caramel_config *cfg, char *err, size_t err_size) {
	char buffer[CONFIG_LINE_MAX];
	int line = 0;
	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		line++;
		if (strchr(buffer, '\n') == NULL && feof(fp) == 0) {
			snprintf(err, err_size, "%s:%d: line too long", name,
				line);
			return false;
		}

		char *text = trim(buffer);
		if (*text == '\0' || *text == '#') {
			continue;
		}

		char *eq = strchr(text, '=');
		if (eq == NULL) {
			snprintf(err, err_size, "%s:%d: expected key = value",
				name, line);
			return false;
		}
		*eq = '\0';
		char *key = trim(text);
		char *value = trim(eq + 1);
		if (*key == '\0') {
			snprintf(err, err_size, "%s:%d: missing key", name,
				line);
			return false;
		}
		if (!apply_pair(cfg, key, value, name, line, err, err_size)) {
			return false;
		}
	}
	return true;
}

// Build the config path, preferring XDG_CONFIG_HOME over ~/.config
static bool config_path(char *out, size_t out_size) {
	const char *xdg = getenv("XDG_CONFIG_HOME");
	if (xdg != NULL && xdg[0] != '\0') {
		int n = snprintf(out, out_size, "%s/caramel/config.toml", xdg);
		return n > 0 && (size_t)n < out_size;
	}
	const char *home = getenv("HOME");
	if (home != NULL && home[0] != '\0') {
		int n = snprintf(
			out, out_size, "%s/.config/caramel/config.toml", home);
		return n > 0 && (size_t)n < out_size;
	}
	return false;
}

bool caramel_config_load(
	struct caramel_config *cfg, char *err, size_t err_size) {
	caramel_config_defaults(cfg);

	char path[PATH_MAX];
	if (!config_path(path, sizeof(path))) {
		return true;
	}

	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		if (errno == ENOENT) {
			return true;
		}
		snprintf(err, err_size, "%s: %s", path, strerror(errno));
		return false;
	}

	bool ok = caramel_config_parse(fp, path, cfg, err, err_size);
	fclose(fp);
	if (!ok) {
		caramel_config_defaults(cfg);
	}
	return ok;
}
