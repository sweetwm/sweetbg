#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "cli/flags.h"
#include "cli/img.h"
#include "config/config.h"
#include "config/config_write.h"
#include "doctor/doctor.h"
#include "ipc/client.h"
#include "ipc/protocol.h"

static int cmd_set(int argc, char **argv) {
	const char *field = NULL;
	const char *value = NULL;
	const char *output = NULL;
	bool persist = false;
	for (int i = 2; i < argc; i++) {
		const char *a = argv[i];
		int flag = sweetbg_cli_target_flag(
			argc, argv, &i, &output, &persist);
		if (flag < 0) {
			return 2;
		}
		if (flag > 0) {
			continue;
		}
		if (field == NULL) {
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
				"color <#rrggbb|auto> [--output <name>] "
				"[--persist]\n");
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
					"center, tile, or span\n");
			return 2;
		}
		if (output != NULL && sweetbg_fit_is_global_only(fit)) {
			fprintf(stderr,
				"sweetbg: fit %s cannot be scoped to an "
				"output\n",
				sweetbg_fit_name(fit));
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
		set_field = SWEETBG_SET_COLOR;
		if (strcmp(value, "auto") == 0) {
			set_value = SWEETBG_COLOR_AUTO;
			persist_value = "auto";
		} else {
			uint32_t color;
			if (!sweetbg_config_parse_color(value, &color)) {
				fprintf(stderr,
					"sweetbg: color must be \"#rrggbb\" or "
					"\"auto\"\n");
				return 2;
			}
			set_value = color;
			snprintf(canon, sizeof(canon), "#%06x",
				color & 0xffffffu);
			persist_value = canon;
		}
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
	if (!sweetbg_config_persist(
		    output, field, persist_value, err, sizeof(err))) {
		fprintf(stderr,
			"sweetbg: applied but could not save config: %s\n",
			err);
		return 1;
	}
	return 0;
}

static int persist_clear(uint32_t flags, const char *output) {
	char err[256];
	bool ok = true;
	if ((flags & SWEETBG_CLEAR_BLANK) != 0) {
		ok = sweetbg_config_persist(
			output, "image", "", err, sizeof(err));
	}
	if (ok && (flags & SWEETBG_CLEAR_IMAGE) != 0) {
		ok = sweetbg_config_persist(
			output, "image", NULL, err, sizeof(err));
	}
	if (ok && (flags & SWEETBG_CLEAR_FIT) != 0) {
		ok = sweetbg_config_persist(
			output, "fit", NULL, err, sizeof(err));
	}
	if (!ok) {
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
		int flag = sweetbg_cli_target_flag(
			argc, argv, &i, &output, &persist);
		if (flag < 0) {
			return 2;
		}
		if (flag > 0) {
			continue;
		}
		if (strcmp(a, "--image") == 0) {
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
	      "cover|contain|center|tile|span\n"
	      "  set fit <mode> --output <name>\n"
	      "                             set fit on one output\n"
	      "  set color <#rrggbb|auto>   set the background color "
	      "(auto: from image)\n"
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
		return sweetbg_cmd_img(argc, argv);
	}

	if (strcmp(cmd, "set") == 0) {
		return cmd_set(argc, argv);
	}

	if (strcmp(cmd, "clear") == 0) {
		return cmd_clear(argc, argv);
	}
	if (strcmp(cmd, "prepare-set") == 0) {
		if (argc < 4) {
			fprintf(stderr, "usage: sweetbg prepare-set <path> "
					"<output>...\n");
			return 2;
		}
		return sweetbg_client_prepare_outputs(argv[2],
			(const char *const *)&argv[3], (size_t)(argc - 3));
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
