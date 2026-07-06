#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/config.h"
#include "config/config_write.h"
#include "doctor/doctor.h"
#include "ipc/client.h"
#include "ipc/protocol.h"

#define MAX_IMG_OVERRIDES 16

static bool valid_output_arg(const char *output) {
	return output != NULL && output[0] != '\0' && strlen(output) <= 63;
}

static int cmd_img(const char *arg, const char *output, bool persist) {
	char resolved[PATH_MAX];
	if (realpath(arg, resolved) == NULL) {
		fprintf(stderr, "sweetbg: cannot use '%s': %s\n", arg,
			strerror(errno));
		return 1;
	}
	if (output != NULL && !valid_output_arg(output)) {
		fprintf(stderr, "sweetbg: invalid --output name\n");
		return 2;
	}
	int rc = sweetbg_client_set_image(resolved, output);
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
					"sweetbg: --output needs a name\n");
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
			fprintf(stderr, "sweetbg: unknown img option '%s'\n",
				a);
			return 2;
		}

		struct img_override token;
		if (is_override_token(a, &token)) {
			if (override_count >= MAX_IMG_OVERRIDES) {
				fprintf(stderr, "sweetbg: too many outputs\n");
				return 2;
			}
			overrides[override_count++] = token;
		} else if (default_path == NULL) {
			default_path = a;
		} else {
			fprintf(stderr,
				"sweetbg: img takes one default path\n");
			return 2;
		}
	}

	if (flag_output != NULL) {
		if (override_count > 0 || default_path == NULL) {
			fprintf(stderr, "sweetbg: use either '--output <name>' "
					"with one path or <name>=<path> "
					"arguments\n");
			return 2;
		}
		return cmd_img(default_path, flag_output, persist);
	}

	if (default_path == NULL && override_count == 0) {
		fprintf(stderr,
			"usage: sweetbg img <path> | <name>=<path>... | "
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
			fprintf(stderr, "sweetbg: invalid output name\n");
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
	const char *output = NULL;
	bool persist = false;
	for (int i = 2; i < argc; i++) {
		const char *a = argv[i];
		if (strcmp(a, "-p") == 0 || strcmp(a, "--persist") == 0) {
			persist = true;
		} else if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr,
					"sweetbg: --output needs a name\n");
				return 2;
			}
			output = argv[++i];
		} else if (strncmp(a, "--output=", 9) == 0) {
			output = a + 9;
		} else if (field == NULL) {
			field = a;
		} else if (value == NULL) {
			value = a;
		} else {
			fprintf(stderr, "sweetbg: set takes <field> <value>\n");
			return 2;
		}
	}
	if (field == NULL || value == NULL) {
		fprintf(stderr, "usage: sweetbg set fit <mode> | "
				"color <#rrggbb> [--output <name>] "
				"[--persist]\n");
		return 2;
	}
	if (output != NULL && !valid_output_arg(output)) {
		fprintf(stderr, "sweetbg: invalid --output name\n");
		return 2;
	}

	uint32_t set_field;
	uint32_t set_value;
	char canon[16];
	const char *persist_value;
	if (strcmp(field, "fit") == 0) {
		enum sweetbg_fit fit;
		if (!sweetbg_fit_from_name(value, &fit)) {
			fprintf(stderr, "sweetbg: fit must be cover, contain, "
					"center, or tile\n");
			return 2;
		}
		set_field = SWEETBG_SET_FIT;
		set_value = (uint32_t)fit;
		persist_value = sweetbg_fit_name(fit);
	} else if (strcmp(field, "color") == 0) {
		if (output != NULL) {
			fprintf(stderr, "sweetbg: color cannot be scoped to an "
					"output\n");
			return 2;
		}
		uint32_t color;
		if (!sweetbg_config_parse_color(value, &color)) {
			fprintf(stderr, "sweetbg: color must be \"#rrggbb\"\n");
			return 2;
		}
		set_field = SWEETBG_SET_COLOR;
		set_value = color;
		snprintf(canon, sizeof(canon), "#%06x", color & 0xffffffu);
		persist_value = canon;
	} else {
		fprintf(stderr,
			"sweetbg: unknown set field '%s' (use fit or color)\n",
			field);
		return 2;
	}

	uint8_t payload[12 + 63];
	sweetbg_put_u32(payload, set_field);
	sweetbg_put_u32(payload + 4, set_value);
	uint32_t payload_len = 8;
	if (output != NULL) {
		size_t output_len = strlen(output);
		sweetbg_put_u32(payload + 8, (uint32_t)output_len);
		// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
		memcpy(payload + 12, output, output_len);
		payload_len = (uint32_t)(12 + output_len);
	}
	int rc = sweetbg_client_request(SWEETBG_CMD_SET, payload, payload_len);
	if (rc != 0 || !persist) {
		return rc;
	}
	char err[256];
	bool saved = output == NULL
			     ? sweetbg_config_persist_setting(
				       field, persist_value, err, sizeof(err))
			     : sweetbg_config_persist_output_setting(output,
				       field, persist_value, err, sizeof(err));
	if (!saved) {
		fprintf(stderr,
			"sweetbg: applied but could not save config: %s\n",
			err);
		return 1;
	}
	return 0;
}

static int persist_clear(uint32_t flags, const char *output) {
	char err[256];
	if ((flags & SWEETBG_CLEAR_BLANK) != 0 &&
		!sweetbg_config_persist_blank_output(
			output, err, sizeof(err))) {
		fprintf(stderr,
			"sweetbg: applied but could not save config: %s\n",
			err);
		return 1;
	}
	if ((flags & SWEETBG_CLEAR_IMAGE) != 0 &&
		!sweetbg_config_persist_clear_image(output, err, sizeof(err))) {
		fprintf(stderr,
			"sweetbg: applied but could not save config: %s\n",
			err);
		return 1;
	}
	if ((flags & SWEETBG_CLEAR_FIT) != 0 &&
		!sweetbg_config_persist_clear_fit(output, err, sizeof(err))) {
		fprintf(stderr,
			"sweetbg: applied but could not save config: %s\n",
			err);
		return 1;
	}
	return 0;
}

static int cmd_clear(int argc, char **argv) {
	const char *output = NULL;
	bool persist = false;
	uint32_t flags = 0;

	for (int i = 2; i < argc; i++) {
		const char *a = argv[i];
		if (strcmp(a, "-p") == 0 || strcmp(a, "--persist") == 0) {
			persist = true;
		} else if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr,
					"sweetbg: --output needs a name\n");
				return 2;
			}
			output = argv[++i];
		} else if (strncmp(a, "--output=", 9) == 0) {
			output = a + 9;
		} else if (strcmp(a, "--image") == 0) {
			flags |= SWEETBG_CLEAR_IMAGE;
		} else if (strcmp(a, "--fit") == 0) {
			flags |= SWEETBG_CLEAR_FIT;
		} else if (strcmp(a, "--blank") == 0) {
			flags |= SWEETBG_CLEAR_BLANK;
		} else {
			fprintf(stderr, "sweetbg: unknown clear option '%s'\n",
				a);
			return 2;
		}
	}

	if (output != NULL && !valid_output_arg(output)) {
		fprintf(stderr, "sweetbg: invalid --output name\n");
		return 2;
	}
	if ((flags & SWEETBG_CLEAR_BLANK) != 0 && output == NULL) {
		fprintf(stderr, "sweetbg: --blank requires --output <name>\n");
		return 2;
	}
	if ((flags & SWEETBG_CLEAR_BLANK) != 0 &&
		(flags & SWEETBG_CLEAR_IMAGE) != 0) {
		fprintf(stderr,
			"sweetbg: use either --blank or --image, not both\n");
		return 2;
	}
	if (flags == 0) {
		flags = SWEETBG_CLEAR_IMAGE;
	}

	uint8_t payload[8 + 63];
	sweetbg_put_u32(payload, flags);
	uint32_t payload_len = 8;
	if (output == NULL) {
		sweetbg_put_u32(payload + 4, 0);
	} else {
		size_t output_len = strlen(output);
		sweetbg_put_u32(payload + 4, (uint32_t)output_len);
		// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
		memcpy(payload + 8, output, output_len);
		payload_len = (uint32_t)(8 + output_len);
	}

	int rc =
		sweetbg_client_request(SWEETBG_CMD_CLEAR, payload, payload_len);
	if (rc != 0 || !persist) {
		return rc;
	}
	return persist_clear(flags, output);
}

static int cmd_query(int argc, char **argv) {
	bool json = false;
	for (int i = 2; i < argc; i++) {
		if (strcmp(argv[i], "--json") == 0) {
			json = true;
		} else {
			fprintf(stderr, "sweetbg: unknown query option '%s'\n",
				argv[i]);
			return 2;
		}
	}
	return sweetbg_client_request(
		json ? SWEETBG_CMD_QUERY_JSON : SWEETBG_CMD_QUERY, NULL, 0);
}

static void usage(FILE *out) {
	fputs("usage: sweetbg <command> [args]\n"
	      "\n"
	      "commands:\n"
	      "  img <path>                 set the wallpaper on all outputs\n"
	      "  img <name>=<path> ...      set a wallpaper per output\n"
	      "  img <path> --output <name> set the wallpaper on one output\n"
	      "  set fit <mode>             set fit: "
	      "cover|contain|center|tile\n"
	      "  set fit <mode> --output <name>\n"
	      "                             set fit on one output\n"
	      "  set color <#rrggbb>        set the background color\n"
	      "  clear                      clear images to the background "
	      "color\n"
	      "  clear --output <name>      clear one output image override\n"
	      "  clear --output <name> --blank\n"
	      "                             show color on one output\n"
	      "  clear --fit [--output <name>]\n"
	      "                             clear fit state\n"
	      "  query                      print daemon and output status\n"
	      "  query --json               print daemon status as JSON\n"
	      "  doctor                     check setup and daemon "
	      "reachability\n"
	      "  reload                     reread config.toml once\n"
	      "  stop                       stop the running daemon\n"
	      "\n"
	      "img/set/clear options:\n"
	      "  -o, --output <name>  target a single output\n"
	      "  -p, --persist        also save the change to the config\n"
	      "  --image              clear image state (clear only)\n"
	      "  --fit                clear fit state (clear only)\n"
	      "  --blank              show color on one output (clear only)\n"
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
		printf("sweetbg %s\n", SWEETBG_VERSION);
		return 0;
	}

	if (strcmp(cmd, "stop") == 0) {
		return sweetbg_client_request(SWEETBG_CMD_STOP, NULL, 0);
	}

	if (strcmp(cmd, "reload") == 0) {
		return sweetbg_client_request(SWEETBG_CMD_RELOAD, NULL, 0);
	}

	if (strcmp(cmd, "img") == 0) {
		return run_img(argc, argv);
	}

	if (strcmp(cmd, "set") == 0) {
		return cmd_set(argc, argv);
	}

	if (strcmp(cmd, "clear") == 0) {
		return cmd_clear(argc, argv);
	}

	if (strcmp(cmd, "prepare") == 0) {
		if (argc < 4) {
			fprintf(stderr,
				"usage: sweetbg prepare <output> <path>\n");
			return 2;
		}
		return sweetbg_client_prepare_output(argv[2], argv[3]);
	}

	if (strcmp(cmd, "query") == 0) {
		return cmd_query(argc, argv);
	}

	if (strcmp(cmd, "doctor") == 0) {
		return sweetbg_doctor_run();
	}

	fprintf(stderr, "sweetbg: unknown command '%s'\n", cmd);
	usage(stderr);
	return 2;
}
