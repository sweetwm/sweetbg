#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

#include "image/image.h"
#include "ipc/protocol.h"
#include "ipc/server.h"
#include "wayland/output.h"
#include "wayland/registry.h"
#include "wayland/surface.h"

// Placeholder wallpaper until an image is set: opaque XRGB8888 0x00RRGGBB
#define DEFAULT_COLOR 0x1e1e2e

struct daemon {
	struct wl_display *display;
	struct caramel_registry *reg;
	char current_path[PATH_MAX];
};

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

static uint8_t handle_img(struct daemon *daemon, const uint8_t *payload,
	uint32_t len, char *message, size_t message_size) {
	if (len == 0 || len >= PATH_MAX || memchr(payload, '\0', len) != NULL) {
		snprintf(message, message_size, "invalid image path");
		return CARAMEL_STATUS_ERR_BAD_REQUEST;
	}

	char path[PATH_MAX];
	memcpy(path, payload, len);
	path[len] = '\0';

	struct caramel_image image;
	char err[128];
	if (!caramel_image_load(&image, path, err, sizeof(err))) {
		snprintf(message, message_size, "%s", err);
		return CARAMEL_STATUS_ERR_IMAGE;
	}

	struct caramel_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		caramel_surface_paint_image(&output->surface, daemon->reg->shm,
			output->scale, &image);
	}
	caramel_image_free(&image);

	memcpy(daemon->current_path, path, (size_t)len + 1);
	snprintf(message, message_size, "applied %s", path);
	return CARAMEL_STATUS_OK;
}

static uint8_t dispatch(void *data, uint8_t command, const uint8_t *payload,
	uint32_t len, char *message, size_t message_size, bool *stop) {
	struct daemon *daemon = data;
	switch (command) {
	case CARAMEL_CMD_STOP:
		*stop = true;
		return CARAMEL_STATUS_OK;
	case CARAMEL_CMD_IMG:
		return handle_img(daemon, payload, len, message, message_size);
	default:
		snprintf(message, message_size, "unknown command");
		return CARAMEL_STATUS_ERR_UNKNOWN_COMMAND;
	}
}

static void install_signal_handlers(void) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
}

static void run_loop(struct daemon *daemon, struct caramel_ipc_server *ipc) {
	struct wl_display *display = daemon->display;
	struct pollfd fds[2];
	fds[0].fd = wl_display_get_fd(display);
	fds[0].events = POLLIN;
	fds[1].fd = ipc->fd;
	fds[1].events = POLLIN;

	while (g_running) {
		while (wl_display_prepare_read(display) != 0) {
			wl_display_dispatch_pending(display);
		}
		wl_display_flush(display);

		if (poll(fds, 2, -1) < 0) {
			wl_display_cancel_read(display);
			if (errno == EINTR) {
				continue;
			}
			break;
		}

		if ((fds[0].revents & POLLIN) != 0) {
			wl_display_read_events(display);
		} else {
			wl_display_cancel_read(display);
		}
		if (wl_display_dispatch_pending(display) < 0) {
			break;
		}

		if ((fds[1].revents & POLLIN) != 0) {
			bool stop = false;
			caramel_ipc_server_handle(ipc, dispatch, daemon, &stop);
			if (stop) {
				g_running = 0;
			}
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

static bool serve(struct wl_display *display, struct caramel_ipc_server *ipc) {
	struct caramel_registry reg;
	if (!caramel_registry_init(&reg, display)) {
		return false;
	}

	// A second roundtrip delivers the configure for each new surface
	if (!create_surfaces(&reg) || wl_display_roundtrip(display) < 0) {
		caramel_registry_finish(&reg);
		return false;
	}

	printf("carameld: connected; %d output(s), wlr-layer-shell and "
	       "wl_shm available\n",
		wl_list_length(&reg.outputs));
	report_outputs(&reg);

	paint_surfaces(&reg);
	if (wl_display_roundtrip(display) < 0) {
		caramel_registry_finish(&reg);
		return false;
	}

	struct daemon daemon = {
		.display = display,
		.reg = &reg,
		.current_path = {0},
	};
	run_loop(&daemon, ipc);
	caramel_registry_finish(&reg);
	return true;
}

static int run(void) {
	struct caramel_ipc_server ipc;
	if (!caramel_ipc_server_init(&ipc)) {
		return 1;
	}

	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr,
			"carameld: cannot connect to a wayland display; "
			"is WAYLAND_DISPLAY set?\n");
		caramel_ipc_server_finish(&ipc);
		return 1;
	}

	install_signal_handlers();
	bool ok = serve(display, &ipc);

	wl_display_disconnect(display);
	caramel_ipc_server_finish(&ipc);
	return ok ? 0 : 1;
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
