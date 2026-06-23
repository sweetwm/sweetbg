#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ipc/client.h"
#include "ipc/protocol.h"

static int cmd_img(const char *arg, const char *output) {
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
	return caramel_client_set_image(resolved, output);
}

static int run_img(int argc, char **argv) {
	const char *path = NULL;
	const char *output = NULL;
	for (int i = 2; i < argc; i++) {
		const char *a = argv[i];
		if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
			if (i + 1 >= argc) {
				fprintf(stderr,
					"caramel: --output needs a name\n");
				return 2;
			}
			output = argv[++i];
		} else if (strncmp(a, "--output=", 9) == 0) {
			output = a + 9;
		} else if (a[0] == '-' && a[1] != '\0') {
			fprintf(stderr, "caramel: unknown img option '%s'\n",
				a);
			return 2;
		} else if (path == NULL) {
			path = a;
		} else {
			fprintf(stderr, "caramel: img takes a single path\n");
			return 2;
		}
	}
	if (path == NULL) {
		fprintf(stderr,
			"usage: caramel img <path> [--output <name>]\n");
		return 2;
	}
	return cmd_img(path, output);
}

static void usage(FILE *out) {
	fputs("usage: caramel <command> [args]\n"
	      "\n"
	      "commands:\n"
	      "  img <path> [--output <name>]   set the wallpaper\n"
	      "  query                          print daemon and output "
	      "status\n"
	      "  stop                           stop the running daemon\n"
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
