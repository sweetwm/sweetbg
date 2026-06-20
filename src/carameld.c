// Daemon entry point and lifecycle wiring. Real startup (Wayland connection,
// socket listener, signal handling, event loop) lands in later slices.

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
	if (argc > 1) {
		const char *arg = argv[1];

		if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
			printf("carameld %s\n", CARAMEL_VERSION);
			return 0;
		}

		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			printf("usage: carameld [-h|--help] [-V|--version]\n");
			return 0;
		}

		fprintf(stderr, "carameld: unknown argument '%s'\n", arg);
		return 2;
	}

	fprintf(stderr,
		"carameld: the wayland daemon is not implemented yet\n");
	return 1;
}
