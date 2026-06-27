#include "config/config.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_COLOR 0x1e1e2e
#define CONFIG_LINE_MAX (PATH_MAX + 64)

const char *manju_fit_name(enum manju_fit fit) {
	switch (fit) {
	case MANJU_FIT_CONTAIN:
		return "contain";
	case MANJU_FIT_CENTER:
		return "center";
	case MANJU_FIT_TILE:
		return "tile";
	case MANJU_FIT_COVER:
		break;
	}
	return "cover";
}

bool manju_fit_from_name(const char *name, enum manju_fit *out) {
	if (strcmp(name, "cover") == 0) {
		*out = MANJU_FIT_COVER;
	} else if (strcmp(name, "contain") == 0) {
		*out = MANJU_FIT_CONTAIN;
	} else if (strcmp(name, "center") == 0) {
		*out = MANJU_FIT_CENTER;
	} else if (strcmp(name, "tile") == 0) {
		*out = MANJU_FIT_TILE;
	} else {
		return false;
	}
	return true;
}

void manju_config_defaults(struct manju_config *cfg) {
	cfg->image[0] = '\0';
	cfg->color = DEFAULT_COLOR;
	cfg->fit = MANJU_FIT_COVER;
	cfg->output_count = 0;
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

bool manju_config_parse_color(const char *s, uint32_t *out) {
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

static bool apply_pair(struct manju_config *cfg, const char *key,
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
		if (!manju_config_parse_color(text, &cfg->color)) {
			snprintf(err, err_size,
				"%s:%d: color must be \"#rrggbb\"", name, line);
			return false;
		}
		return true;
	}
	if (strcmp(key, "fit") == 0) {
		if (!manju_fit_from_name(text, &cfg->fit)) {
			snprintf(err, err_size,
				"%s:%d: fit must be \"cover\", \"contain\", "
				"\"center\", or \"tile\"",
				name, line);
			return false;
		}
		return true;
	}

	snprintf(err, err_size, "%s:%d: unknown key '%s'", name, line, key);
	return false;
}

static bool apply_output_pair(struct manju_config_output *output,
	const char *key, const char *value, const char *name, int line,
	char *err, size_t err_size) {
	char text[PATH_MAX];
	if (!parse_string(value, text, sizeof(text))) {
		snprintf(err, err_size, "%s:%d: value must be a quoted string",
			name, line);
		return false;
	}
	if (strcmp(key, "image") == 0) {
		memcpy(output->image, text, strlen(text) + 1);
		return true;
	}
	if (strcmp(key, "fit") == 0) {
		if (!manju_fit_from_name(text, &output->fit)) {
			snprintf(err, err_size,
				"%s:%d: fit must be \"cover\", \"contain\", "
				"\"center\", or \"tile\"",
				name, line);
			return false;
		}
		output->has_fit = true;
		return true;
	}
	snprintf(err, err_size, "%s:%d: unknown key '%s' in [output]", name,
		line, key);
	return false;
}

static struct manju_config_output *parse_section(struct manju_config *cfg,
	char *line, const char *name, int lineno, char *err, size_t err_size) {
	char *close = strchr(line, ']');
	const char *after = close != NULL ? close + 1 : NULL;
	while (after != NULL && (*after == ' ' || *after == '\t')) {
		after++;
	}
	if (close == NULL || (*after != '\0' && *after != '#')) {
		snprintf(err, err_size, "%s:%d: malformed section", name,
			lineno);
		return NULL;
	}
	*close = '\0';

	const char *prefix = "output.";
	const char *inner = line + 1;
	if (strncmp(inner, prefix, strlen(prefix)) != 0) {
		snprintf(err, err_size, "%s:%d: unknown section [%s]", name,
			lineno, inner);
		return NULL;
	}
	const char *output_name = inner + strlen(prefix);
	if (output_name[0] == '\0' ||
		strlen(output_name) >= sizeof(cfg->outputs[0].name)) {
		snprintf(err, err_size, "%s:%d: invalid output name", name,
			lineno);
		return NULL;
	}

	for (size_t i = 0; i < cfg->output_count; i++) {
		if (strcmp(cfg->outputs[i].name, output_name) == 0) {
			return &cfg->outputs[i];
		}
	}
	if (cfg->output_count >= MANJU_CONFIG_MAX_OUTPUTS) {
		snprintf(err, err_size, "%s:%d: too many [output] sections",
			name, lineno);
		return NULL;
	}
	struct manju_config_output *out = &cfg->outputs[cfg->output_count++];
	memcpy(out->name, output_name, strlen(output_name) + 1);
	out->image[0] = '\0';
	out->fit = MANJU_FIT_COVER;
	out->has_fit = false;
	return out;
}

bool manju_config_parse(FILE *fp, const char *name, struct manju_config *cfg,
	char *err, size_t err_size) {
	char buffer[CONFIG_LINE_MAX];
	int line = 0;
	struct manju_config_output *current = NULL;
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
		if (*text == '[') {
			current = parse_section(
				cfg, text, name, line, err, err_size);
			if (current == NULL) {
				return false;
			}
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

		bool ok = current == NULL
				  ? apply_pair(cfg, key, value, name, line, err,
					    err_size)
				  : apply_output_pair(current, key, value, name,
					    line, err, err_size);
		if (!ok) {
			return false;
		}
	}
	return true;
}

bool manju_config_path(char *out, size_t out_size) {
	const char *xdg = getenv("XDG_CONFIG_HOME");
	if (xdg != NULL && xdg[0] != '\0') {
		int n = snprintf(out, out_size, "%s/manju/config.toml", xdg);
		return n > 0 && (size_t)n < out_size;
	}
	const char *home = getenv("HOME");
	if (home != NULL && home[0] != '\0') {
		int n = snprintf(
			out, out_size, "%s/.config/manju/config.toml", home);
		return n > 0 && (size_t)n < out_size;
	}
	return false;
}

bool manju_config_load(struct manju_config *cfg, char *err, size_t err_size) {
	manju_config_defaults(cfg);

	char path[PATH_MAX];
	if (!manju_config_path(path, sizeof(path))) {
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

	bool ok = manju_config_parse(fp, path, cfg, err, err_size);
	fclose(fp);
	if (!ok) {
		manju_config_defaults(cfg);
	}
	return ok;
}
