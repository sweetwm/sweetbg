#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ipc/client.h"
#include "ipc/protocol.h"

static int cmd_img(const char *arg) {
	char resolved[PATH_MAX];
	if (realpath(arg, resolved) == NULL) {
		fprintf(stderr, "caramel: cannot use '%s': %s\n", arg,
			strerror(errno));
		return 1;
	}
	return caramel_client_request(
		CARAMEL_CMD_IMG, resolved, (uint32_t)strlen(resolved));
}

static void usage(FILE *out) {
	fputs("usage: caramel <command> [args]\n"
	      "\n"
	      "commands:\n"
	      "  img <path>   set the wallpaper from an image file\n"
	      "  query        print daemon and output status\n"
	      "  stop         stop the running daemon\n"
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
		if (argc < 3) {
			fprintf(stderr, "usage: caramel img <path>\n");
			return 2;
		}
		return cmd_img(argv[2]);
	}

	if (strcmp(cmd, "query") == 0) {
		return caramel_client_request(CARAMEL_CMD_QUERY, NULL, 0);
	}

	fprintf(stderr, "caramel: unknown command '%s'\n", cmd);
	usage(stderr);
	return 2;
}
