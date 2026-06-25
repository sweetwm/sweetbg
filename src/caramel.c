#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config.h"
#include "config/config_write.h"
#include "ipc/client.h"
#include "ipc/protocol.h"

#define MAX_IMG_OVERRIDES 16

static int cmd_img(const char *arg, const char *output, bool persist) {
	char resolved[PATH_MAX];
	if (realpath(arg, resolved) == NULL) {
		fprintf(stderr, "caramel: cannot use '%s': %s\n", arg,
			strerror(errno));
		return 1;
	}
	if (output != NULL && (output[0] == '\0' || strlen(output) > 63)) {
		fprintf(stderr, "caramel: invalid --output name\n");
		return 2;
	}
	int rc = caramel_client_set_image(resolved, output);
	if (rc != 0 || !persist) {
		return rc;
	}
	char err[256];
	if (!caramel_config_persist_image(output, resolved, err, sizeof(err))) {
		fprintf(stderr,
			"caramel: applied but could not save config: %s\n",
			err);
		return 1;
	}
	return 0;
}

struct img_override {
	const char *name;
	size_t name_len;
	const char *path;
};

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

static int run_img(int argc, char **argv) {
	const char *default_path = NULL;
	const char *flag_output = NULL;
	bool persist = false;
	struct img_override overrides[MAX_IMG_OVERRIDES];
	int override_count = 0;

	for (int i = 2; i < argc; i++) {
		const char *a = argv[i];
		if (strcmp(a, "-p") == 0 || strcmp(a, "--persist") == 0) {
			persist = true;
			continue;
		}
		if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr,
					"caramel: --output needs a name\n");
				return 2;
			}
			flag_output = argv[++i];
			continue;
		}
		if (strncmp(a, "--output=", 9) == 0) {
			flag_output = a + 9;
			continue;
		}
		if (a[0] == '-' && a[1] != '\0') {
			fprintf(stderr, "caramel: unknown img option '%s'\n",
				a);
			return 2;
		}

		struct img_override token;
		if (is_override_token(a, &token)) {
			if (override_count >= MAX_IMG_OVERRIDES) {
				fprintf(stderr, "caramel: too many outputs\n");
				return 2;
			}
			overrides[override_count++] = token;
		} else if (default_path == NULL) {
			default_path = a;
		} else {
			fprintf(stderr,
				"caramel: img takes one default path\n");
			return 2;
		}
	}

	if (flag_output != NULL) {
		if (override_count > 0 || default_path == NULL) {
			fprintf(stderr, "caramel: use either '--output <name>' "
					"with one path or <name>=<path> "
					"arguments\n");
			return 2;
		}
		return cmd_img(default_path, flag_output, persist);
	}

	if (default_path == NULL && override_count == 0) {
		fprintf(stderr,
			"usage: caramel img <path> | <name>=<path>... | "
			"<path> --output <name>\n");
		return 2;
	}

	int rc = 0;
	if (default_path != NULL) {
		rc |= cmd_img(default_path, NULL, persist);
	}
	for (int i = 0; i < override_count; i++) {
		char name[64];
		if (overrides[i].name_len >= sizeof(name)) {
			fprintf(stderr, "caramel: invalid output name\n");
			rc = 2;
			continue;
		}
		memcpy(name, overrides[i].name, overrides[i].name_len);
		name[overrides[i].name_len] = '\0';
		rc |= cmd_img(overrides[i].path, name, persist);
	}
	return rc;
}

static int cmd_set(int argc, char **argv) {
	const char *field = NULL;
	const char *value = NULL;
	bool persist = false;
	for (int i = 2; i < argc; i++) {
		const char *a = argv[i];
		if (strcmp(a, "-p") == 0 || strcmp(a, "--persist") == 0) {
			persist = true;
		} else if (field == NULL) {
			field = a;
		} else if (value == NULL) {
			value = a;
		} else {
			fprintf(stderr, "caramel: set takes <field> <value>\n");
			return 2;
		}
	}
	if (field == NULL || value == NULL) {
		fprintf(stderr, "usage: caramel set fit <mode> | "
				"color <#rrggbb> [--persist]\n");
		return 2;
	}

	uint32_t set_field;
	uint32_t set_value;
	char canon[16];
	const char *persist_value;
	if (strcmp(field, "fit") == 0) {
		enum caramel_fit fit;
		if (!caramel_fit_from_name(value, &fit)) {
			fprintf(stderr, "caramel: fit must be cover, contain, "
					"center, or tile\n");
			return 2;
		}
		set_field = CARAMEL_SET_FIT;
		set_value = (uint32_t)fit;
		persist_value = caramel_fit_name(fit);
	} else if (strcmp(field, "color") == 0) {
		uint32_t color;
		if (!caramel_config_parse_color(value, &color)) {
			fprintf(stderr, "caramel: color must be \"#rrggbb\"\n");
			return 2;
		}
		set_field = CARAMEL_SET_COLOR;
		set_value = color;
		snprintf(canon, sizeof(canon), "#%06x", color & 0xffffffu);
		persist_value = canon;
	} else {
		fprintf(stderr,
			"caramel: unknown set field '%s' (use fit or color)\n",
			field);
		return 2;
	}

	uint8_t payload[8];
	caramel_put_u32(payload, set_field);
	caramel_put_u32(payload + 4, set_value);
	int rc = caramel_client_request(
		CARAMEL_CMD_SET, payload, sizeof(payload));
	if (rc != 0 || !persist) {
		return rc;
	}
	char err[256];
	if (!caramel_config_persist_setting(
		    field, persist_value, err, sizeof(err))) {
		fprintf(stderr,
			"caramel: applied but could not save config: %s\n",
			err);
		return 1;
	}
	return 0;
}

static void usage(FILE *out) {
	fputs("usage: caramel <command> [args]\n"
	      "\n"
	      "commands:\n"
	      "  img <path>                 set the wallpaper on all outputs\n"
	      "  img <name>=<path> ...      set a wallpaper per output\n"
	      "  img <path> --output <name> set the wallpaper on one output\n"
	      "  set fit <mode>             set fit: "
	      "cover|contain|center|tile\n"
	      "  set color <#rrggbb>        set the background color\n"
	      "  query                      print daemon and output status\n"
	      "  stop                       stop the running daemon\n"
	      "\n"
	      "img/set options:\n"
	      "  -o, --output <name>  target a single output (img only)\n"
	      "  -p, --persist        also save the change to the config\n"
	      "\n"
	      "options:\n"
	      "  -h, --help     show this help and exit\n"
	      "  -V, --version  show version and exit\n",
		out);
}

int main(int argc, char **argv) {
	if (argc < 2) {
		usage(stderr);
		return 2;
	}

	const char *cmd = argv[1];

	if (strcmp(cmd, "-h") == 0 || strcmp(cmd, "--help") == 0) {
		usage(stdout);
		return 0;
	}

	if (strcmp(cmd, "-V") == 0 || strcmp(cmd, "--version") == 0) {
		printf("caramel %s\n", CARAMEL_VERSION);
		return 0;
	}

	if (strcmp(cmd, "stop") == 0) {
		return caramel_client_request(CARAMEL_CMD_STOP, NULL, 0);
	}

	if (strcmp(cmd, "img") == 0) {
		return run_img(argc, argv);
	}

	if (strcmp(cmd, "set") == 0) {
		return cmd_set(argc, argv);
	}

	if (strcmp(cmd, "prepare") == 0) {
		if (argc < 4) {
			fprintf(stderr,
				"usage: caramel prepare <output> <path>\n");
			return 2;
		}
		return caramel_client_prepare_output(argv[2], argv[3]);
	}

	if (strcmp(cmd, "query") == 0) {
		return caramel_client_request(CARAMEL_CMD_QUERY, NULL, 0);
	}

	fprintf(stderr, "caramel: unknown command '%s'\n", cmd);
	usage(stderr);
	return 2;
}
