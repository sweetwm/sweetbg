#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

#ifdef __GLIBC__
#include <malloc.h>
#endif

#include "config/config.h"
#include "image/image.h"
#include "ipc/protocol.h"
#include "ipc/server.h"
#include "wayland/output.h"
#include "wayland/registry.h"
#include "wayland/surface.h"

struct daemon {
	struct wl_display *display;
	struct caramel_registry *reg;
	// Placeholder color shown when no image is set (XRGB8888 0x00RRGGBB)
	uint32_t color;
	char default_path[PATH_MAX];
};

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int signal_number) {
	(void)signal_number;
	g_running = 0;
}

static void trim_heap(void) {
#ifdef __GLIBC__
	malloc_trim(0);
#endif
}

static bool ensure_surfaces(struct caramel_registry *reg) {
	struct caramel_output *output;
	wl_list_for_each(output, &reg->outputs, link) {
		if (output->surface.layer_surface != NULL) {
			continue;
		}
		if (!caramel_surface_create(&output->surface, reg->compositor,
			    reg->layer_shell, output->wl_output)) {
			fprintf(stderr,
				"carameld: failed to create a layer surface\n");
			return false;
		}
	}
	return true;
}

static const char *effective_path(
	const struct daemon *daemon, const struct caramel_output *output) {
	if (output->wallpaper_override[0] != '\0') {
		return output->wallpaper_override;
	}
	return daemon->default_path;
}

static bool paint_output(struct daemon *daemon, struct caramel_output *output,
	const char *path) {
	if (!output->surface.configured) {
		return false;
	}
	if (path[0] != '\0') {
		struct caramel_image image;
		char err[128];
		if (caramel_image_load(&image, path, err, sizeof(err))) {
			caramel_surface_paint_image(&output->surface,
				daemon->reg->shm, output->scale, &image);
			caramel_image_free(&image);
			return true;
		}
		fprintf(stderr, "carameld: %s\n", err);
	}
	caramel_surface_paint_color(&output->surface, daemon->reg->shm,
		output->scale, daemon->color);
	return false;
}

static void reconcile_paint(struct daemon *daemon) {
	struct caramel_output *output;
	bool decoded = false;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (!output->surface.configured ||
			!output->surface.needs_repaint) {
			continue;
		}
		decoded |= paint_output(
			daemon, output, effective_path(daemon, output));
	}
	if (decoded) {
		trim_heap();
	}
}

static bool split_img_payload(const uint8_t *payload, uint32_t len, char *path,
	size_t path_size, char *target, size_t target_size) {
	const uint8_t *nul = memchr(payload, '\0', len);
	size_t path_len = nul != NULL ? (size_t)(nul - payload) : (size_t)len;
	if (path_len == 0 || path_len >= path_size) {
		return false;
	}
	memcpy(path, payload, path_len);
	path[path_len] = '\0';

	target[0] = '\0';
	if (nul != NULL) {
		size_t target_len = (size_t)len - path_len - 1;
		if (target_len == 0 || target_len >= target_size ||
			memchr(nul + 1, '\0', target_len) != NULL) {
			return false;
		}
		memcpy(target, nul + 1, target_len);
		target[target_len] = '\0';
	}
	return true;
}

static uint8_t handle_img(struct daemon *daemon, const uint8_t *payload,
	uint32_t len, char *message, size_t message_size) {
	char path[PATH_MAX];
	char target[64];
	if (len == 0 || !split_img_payload(payload, len, path, sizeof(path),
				target, sizeof(target))) {
		snprintf(message, message_size, "invalid image request");
		return CARAMEL_STATUS_ERR_BAD_REQUEST;
	}

	// Resolve a named output up front so a typo fails before decoding
	struct caramel_output *match = NULL;
	struct caramel_output *output;
	if (target[0] != '\0') {
		wl_list_for_each(output, &daemon->reg->outputs, link) {
			if (output->name != NULL &&
				strcmp(output->name, target) == 0) {
				match = output;
				break;
			}
		}
		if (match == NULL) {
			snprintf(message, message_size, "no output named %s",
				target);
			return CARAMEL_STATUS_ERR_BAD_REQUEST;
		}
	}

	struct caramel_image image;
	char err[128];
	if (!caramel_image_load(&image, path, err, sizeof(err))) {
		snprintf(message, message_size, "%s", err);
		return CARAMEL_STATUS_ERR_IMAGE;
	}

	if (match != NULL) {
		memcpy(match->wallpaper_override, path, strlen(path) + 1);
		if (match->surface.configured) {
			caramel_surface_paint_image(&match->surface,
				daemon->reg->shm, match->scale, &image);
		}
		snprintf(message, message_size, "applied %s to %s", path,
			target);
	} else {
		// No target: this becomes the default and clears overrides
		memcpy(daemon->default_path, path, strlen(path) + 1);
		wl_list_for_each(output, &daemon->reg->outputs, link) {
			output->wallpaper_override[0] = '\0';
			if (output->surface.configured) {
				caramel_surface_paint_image(&output->surface,
					daemon->reg->shm, output->scale,
					&image);
			}
		}
		snprintf(message, message_size, "applied %s", path);
	}

	caramel_image_free(&image);
	trim_heap();
	return CARAMEL_STATUS_OK;
}

__attribute__((format(printf, 4, 5))) static void append_line(
	char *out, size_t out_size, size_t *off, const char *fmt, ...) {
	if (*off >= out_size) {
		return;
	}
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(out + *off, out_size - *off, fmt, ap);
	va_end(ap);
	if (n < 0) {
		return;
	}
	if ((size_t)n >= out_size - *off) {
		*off = out_size;
	} else {
		*off += (size_t)n;
	}
}

static uint8_t handle_query(
	struct daemon *daemon, char *message, size_t message_size) {
	size_t off = 0;
	if (daemon->default_path[0] != '\0') {
		append_line(message, message_size, &off, "default: %s\n",
			daemon->default_path);
	} else {
		append_line(message, message_size, &off,
			"default: color #%06x\n", daemon->color & 0xffffffu);
	}

	struct caramel_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		const char *name =
			output->name != NULL ? output->name : "(unnamed)";
		if (output->wallpaper_override[0] != '\0') {
			append_line(message, message_size, &off,
				"%s: %dx%d scale %d (override: %s)\n", name,
				output->pixel_width, output->pixel_height,
				output->scale, output->wallpaper_override);
		} else {
			append_line(message, message_size, &off,
				"%s: %dx%d scale %d\n", name,
				output->pixel_width, output->pixel_height,
				output->scale);
		}
	}

	// Drop the trailing newline; the client prints its own
	if (off > 0 && off <= message_size && message[off - 1] == '\n') {
		message[off - 1] = '\0';
	}
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
	case CARAMEL_CMD_QUERY:
		return handle_query(daemon, message, message_size);
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

		ensure_surfaces(daemon->reg);
		reconcile_paint(daemon);

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

static void apply_config(struct daemon *daemon) {
	struct caramel_config cfg;
	char err[256];
	if (!caramel_config_load(&cfg, err, sizeof(err))) {
		fprintf(stderr, "carameld: %s\n", err);
	}
	daemon->color = cfg.color;

	if (cfg.image[0] == '\0') {
		return;
	}
	struct caramel_image probe;
	char image_err[128];
	if (caramel_image_load(
		    &probe, cfg.image, image_err, sizeof(image_err))) {
		caramel_image_free(&probe);
		memcpy(daemon->default_path, cfg.image, strlen(cfg.image) + 1);
	} else {
		fprintf(stderr, "carameld: configured image: %s\n", image_err);
	}
}

static bool serve(struct wl_display *display, struct caramel_ipc_server *ipc) {
	struct caramel_registry reg;
	if (!caramel_registry_init(&reg, display)) {
		return false;
	}

	// A second roundtrip delivers the configure for each new surface
	if (!ensure_surfaces(&reg) || wl_display_roundtrip(display) < 0) {
		caramel_registry_finish(&reg);
		return false;
	}

	printf("carameld: connected; %d output(s), wlr-layer-shell and "
	       "wl_shm available\n",
		wl_list_length(&reg.outputs));
	report_outputs(&reg);

	struct daemon daemon = {
		.display = display,
		.reg = &reg,
		.default_path = {0},
	};
	apply_config(&daemon);

	// The initial configures flagged each surface; paint config image/color
	reconcile_paint(&daemon);
	if (wl_display_roundtrip(display) < 0) {
		caramel_registry_finish(&reg);
		return false;
	}

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
