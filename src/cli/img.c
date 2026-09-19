#include "cli/img.h"

#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "config/config_write.h"
#include "image/pick.h"
#include "ipc/client.h"

#define MAX_IMG_OVERRIDES 16

struct img_override {
	const char *name;
	size_t name_len;
	const char *path;
};

struct img_args {
	const char *default_path;
	const char *flag_output;
	bool persist;
	struct img_override overrides[MAX_IMG_OVERRIDES];
	int override_count;
};

static bool valid_output_name(const char *output) {
	return output != NULL && output[0] != '\0' && strlen(output) <= 63;
}

static int apply_image(const char *arg, const char *output, bool persist,
	const char *const *skip_names, size_t skip_count, char *resolved_out) {
	if (resolved_out != NULL) {
		resolved_out[0] = '\0';
	}
	char resolved[PATH_MAX];
	if (realpath(arg, resolved) == NULL) {
		fprintf(stderr, "sweetbg: cannot use '%s': %s\n", arg,
			strerror(errno));
		return 1;
	}

	// A directory is resolved to one file here and never travels further,
	// so what the daemon stores and what --persist writes stay a real image
	struct stat st;
	if (stat(resolved, &st) == 0 && S_ISDIR(st.st_mode)) {
		char picked[PATH_MAX];
		char err[256];
		if (!sweetbg_pick_random_image(resolved, picked, sizeof(picked),
			    err, sizeof(err))) {
			fprintf(stderr, "sweetbg: %s\n", err);
			return 1;
		}
		memcpy(resolved, picked, strlen(picked) + 1);
	}

	if (output != NULL && !valid_output_name(output)) {
		fprintf(stderr, "sweetbg: invalid --output name\n");
		return 2;
	}
	if (resolved_out != NULL) {
		memcpy(resolved_out, resolved, strlen(resolved) + 1);
	}
	int rc = sweetbg_client_set_image(
		resolved, output, skip_names, skip_count);
	if (rc != 0 || !persist) {
		return rc;
	}
	char err[256];
	if (!sweetbg_config_persist_image(output, resolved, err, sizeof(err))) {
		fprintf(stderr,
			"sweetbg: applied but could not save config: %s\n",
			err);
		return 1;
	}
	return 0;
}

static bool is_override_token(const char *arg, struct img_override *out) {
	const char *eq = strchr(arg, '=');
	if (eq == NULL || eq == arg ||
		memchr(arg, '/', (size_t)(eq - arg)) != NULL) {
		return false;
	}
	out->name = arg;
	out->name_len = (size_t)(eq - arg);
	out->path = eq + 1;
	return true;
}

static int parse_args(int argc, char **argv, struct img_args *args) {
	for (int i = 2; i < argc; i++) {
		const char *a = argv[i];
		if (strcmp(a, "-p") == 0 || strcmp(a, "--persist") == 0) {
			args->persist = true;
			continue;
		}
		if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr,
					"sweetbg: --output needs a name\n");
				return 2;
			}
			args->flag_output = argv[++i];
			continue;
		}
		if (strncmp(a, "--output=", 9) == 0) {
			args->flag_output = a + 9;
			continue;
		}
		if (a[0] == '-' && a[1] != '\0') {
			fprintf(stderr, "sweetbg: unknown img option '%s'\n",
				a);
			return 2;
		}

		struct img_override token;
		if (is_override_token(a, &token)) {
			if (args->override_count >= MAX_IMG_OVERRIDES) {
				fprintf(stderr, "sweetbg: too many outputs\n");
				return 2;
			}
			args->overrides[args->override_count++] = token;
		} else if (args->default_path == NULL) {
			args->default_path = a;
		} else {
			fprintf(stderr,
				"sweetbg: img takes one default path\n");
			return 2;
		}
	}
	return 0;
}

static bool copy_override_names(const struct img_args *args, char names[][64],
	const char **skip_names) {
	for (int i = 0; i < args->override_count; i++) {
		if (args->overrides[i].name_len >= sizeof(names[i])) {
			fprintf(stderr, "sweetbg: invalid output name\n");
			return false;
		}
		memcpy(names[i], args->overrides[i].name,
			args->overrides[i].name_len);
		names[i][args->overrides[i].name_len] = '\0';
		skip_names[i] = names[i];
	}
	return true;
}

int sweetbg_cmd_img(int argc, char **argv) {
	struct img_args args = {0};
	int parsed = parse_args(argc, argv, &args);
	if (parsed != 0) {
		return parsed;
	}

	if (args.flag_output != NULL) {
		if (args.override_count > 0 || args.default_path == NULL) {
			fprintf(stderr, "sweetbg: use either '--output <name>' "
					"with one path or <name>=<path> "
					"arguments\n");
			return 2;
		}
		return apply_image(args.default_path, args.flag_output,
			args.persist, NULL, 0, NULL);
	}

	if (args.default_path == NULL && args.override_count == 0) {
		fprintf(stderr,
			"usage: sweetbg img <path> | <name>=<path>... | "
			"<path> --output <name>\n");
		return 2;
	}

	char names[MAX_IMG_OVERRIDES][64];
	const char *skip_names[MAX_IMG_OVERRIDES];
	if (!copy_override_names(&args, names, skip_names)) {
		return 2;
	}

	int rc = 0;
	char default_resolved[PATH_MAX] = {0};
	if (args.default_path != NULL) {
		rc |= apply_image(args.default_path, NULL, args.persist,
			skip_names, (size_t)args.override_count,
			default_resolved);
	}
	for (int i = 0; i < args.override_count; i++) {
		int one = apply_image(args.overrides[i].path, names[i],
			args.persist, NULL, 0, NULL);
		rc |= one;
		if (one != 0 && default_resolved[0] != '\0') {
			// Restore the default only if this override did not
			// stick; a successful or ambiguous apply is rejected as
			// superseded
			(void)sweetbg_client_prepare_output(
				names[i], default_resolved);
		}
	}
	return rc;
}
