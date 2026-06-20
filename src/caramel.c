// CLI entry point and argument routing. The daemon owns all Wayland and
// wallpaper state; this process parses one command, talks to carameld, exits.

#include <stdio.h>
#include <string.h>

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

	if (strcmp(cmd, "img") == 0 || strcmp(cmd, "query") == 0 ||
		strcmp(cmd, "stop") == 0) {
		fprintf(stderr, "caramel: '%s' is not implemented yet\n", cmd);
		return 1;
	}

	fprintf(stderr, "caramel: unknown command '%s'\n", cmd);
	usage(stderr);
	return 2;
}
