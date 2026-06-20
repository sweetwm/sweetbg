// Daemon entry point and lifecycle wiring. This slice connects to Wayland,
// enumerates outputs, paints each background surface a solid color, and runs a
// minimal dispatch loop until signalled. The IPC socket, config, and image
// decoding land in later slices.

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

#include "wayland/output.h"
#include "wayland/registry.h"
#include "wayland/surface.h"

// Placeholder wallpaper until `caramel img` exists: opaque XRGB8888 0x00RRGGBB
#define DEFAULT_COLOR 0x1e1e2e

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int signal_number) {
	(void)signal_number;
	g_running = 0;
}

static bool create_surfaces(struct caramel_registry *reg) {
	struct caramel_output *output;
	wl_list_for_each(output, &reg->outputs, link) {
		if (!caramel_surface_create(&output->surface, reg->compositor,
			    reg->layer_shell, output->wl_output)) {
			fprintf(stderr,
				"carameld: failed to create a layer surface\n");
			return false;
		}
	}
	return true;
}

static void paint_surfaces(struct caramel_registry *reg) {
	struct caramel_output *output;
	wl_list_for_each(output, &reg->outputs, link) {
		if (!caramel_surface_paint_color(&output->surface, reg->shm,
			    output->scale, DEFAULT_COLOR)) {
			fprintf(stderr, "carameld: failed to paint output %s\n",
				output->name != NULL ? output->name
						     : "(unnamed)");
		}
	}
}

static void run_loop(struct wl_display *display) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	// Block on Wayland events; a signal interrupts dispatch and clears the
	// flag so we tear down cleanly. The IPC fd joins this loop via poll
	// in a later slice
	while (g_running) {
		if (wl_display_dispatch(display) < 0) {
			break;
		}
	}
}

static void report_outputs(struct caramel_registry *reg) {
	struct caramel_output *output;
	wl_list_for_each(output, &reg->outputs, link) {
		printf("  output %s: %dx%d scale %d -> surface %ux%u%s\n",
			output->name != NULL ? output->name : "(unnamed)",
			output->pixel_width, output->pixel_height,
			output->scale, output->surface.width,
			output->surface.height,
			output->surface.configured ? "" : " (unconfigured)");
	}
}

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

	// A second roundtrip delivers the configure for each new surface
	if (!create_surfaces(&reg) || wl_display_roundtrip(display) < 0) {
		caramel_registry_finish(&reg);
		wl_display_disconnect(display);
		return 1;
	}

	printf("carameld: connected; %d output(s), wlr-layer-shell and "
	       "wl_shm available\n",
		wl_list_length(&reg.outputs));
	report_outputs(&reg);

	paint_surfaces(&reg);
	if (wl_display_roundtrip(display) < 0) {
		caramel_registry_finish(&reg);
		wl_display_disconnect(display);
		return 1;
	}

	run_loop(display);

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
