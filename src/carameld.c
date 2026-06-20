// Daemon entry point and lifecycle wiring. This slice connects to Wayland and
// enumerates the compositor's outputs. The socket listener, signal handling,
// background surfaces, and event loop land next.

#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

#include "wayland/output.h"
#include "wayland/registry.h"

static int run(void) {
	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr,
			"carameld: cannot connect to a wayland display; "
			"is WAYLAND_DISPLAY set?\n");
		return 1;
	}

	struct caramel_registry reg;
	if (!caramel_registry_init(&reg, display)) {
		caramel_registry_finish(&reg);
		wl_display_disconnect(display);
		return 1;
	}

	printf("carameld: connected; %d output(s), wlr-layer-shell and "
	       "wl_shm available\n",
		wl_list_length(&reg.outputs));

	struct caramel_output *output;
	wl_list_for_each(output, &reg.outputs, link) {
		printf("  output %s: %dx%d scale %d\n",
			output->name != NULL ? output->name : "(unnamed)",
			output->pixel_width, output->pixel_height,
			output->scale);
	}

	// No persistent loop yet: tear everything down and exit cleanly so the
	// slice stays leak-free under valgrind
	caramel_registry_finish(&reg);
	wl_display_disconnect(display);
	return 0;
}

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

	return run();
}
