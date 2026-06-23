#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wayland-client.h>

#include "config/config.h"
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

static void client_binary(char *out, size_t out_size) {
	ssize_t n = readlink("/proc/self/exe", out, out_size - 1);
	if (n > 0 && (size_t)n < out_size) {
		// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
		out[n] = '\0';
		char *slash = strrchr(out, '/');
		if (slash != NULL) {
			size_t dir_len = (size_t)(slash - out) + 1;
			const char *bin = "caramel";
			if (dir_len + strlen(bin) + 1 <= out_size) {
				memcpy(out + dir_len, bin, strlen(bin) + 1);
				if (access(out, X_OK) == 0) {
					return;
				}
			}
		}
	}
	snprintf(out, out_size, "caramel");
}

static void spawn_prepare(const char *name, const char *path) {
	if (name == NULL) {
		return;
	}
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "carameld: cannot spawn client: %s\n",
			strerror(errno));
		return;
	}
	if (pid == 0) {
		char bin[PATH_MAX];
		client_binary(bin, sizeof(bin));
		execlp(bin, "caramel", "prepare", name, path, (char *)NULL);
		_exit(127);
	}
}

static void reconcile_paint(struct daemon *daemon) {
	struct caramel_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (!output->surface.configured ||
			!output->surface.needs_repaint) {
			continue;
		}
		const char *path = effective_path(daemon, output);
		if (path[0] == '\0') {
			caramel_surface_paint_color(&output->surface,
				daemon->reg->shm, output->scale, daemon->color);
		} else {
			output->surface.needs_repaint = false;
			spawn_prepare(output->name, path);
		}
	}
}

struct prepared_request {
	uint32_t mode;
	int32_t scale;
	uint32_t width;
	uint32_t height;
	char name[64];
	char path[PATH_MAX];
};

static bool parse_prepared(
	const uint8_t *p, uint32_t len, struct prepared_request *req) {
	if (len < 20) {
		return false;
	}
	req->mode = caramel_get_u32(p);
	req->scale = (int32_t)caramel_get_u32(p + 4);
	req->width = caramel_get_u32(p + 8);
	req->height = caramel_get_u32(p + 12);

	uint32_t name_len = caramel_get_u32(p + 16);
	size_t off = 20;
	if (name_len == 0 || name_len >= sizeof(req->name) ||
		off + name_len > len) {
		return false;
	}
	memcpy(req->name, p + off, name_len);
	req->name[name_len] = '\0';
	off += name_len;

	if (off + 4 > len) {
		return false;
	}
	uint32_t path_len = caramel_get_u32(p + off);
	off += 4;
	if (path_len == 0 || path_len >= sizeof(req->path) ||
		off + path_len > len) {
		return false;
	}
	memcpy(req->path, p + off, path_len);
	req->path[path_len] = '\0';
	return true;
}

static uint8_t handle_img_prepared(struct daemon *daemon,
	const uint8_t *payload, uint32_t len, int fd, char *message,
	size_t message_size) {
	struct prepared_request req;
	if (fd < 0 || !parse_prepared(payload, len, &req)) {
		snprintf(message, message_size, "invalid prepared image");
		return CARAMEL_STATUS_ERR_BAD_REQUEST;
	}

	struct caramel_output *output;
	struct caramel_output *match = NULL;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (output->name != NULL &&
			strcmp(output->name, req.name) == 0) {
			match = output;
			break;
		}
	}
	if (match == NULL) {
		snprintf(message, message_size, "no output named %s", req.name);
		return CARAMEL_STATUS_ERR_BAD_REQUEST;
	}

	if (!caramel_surface_attach_prepared(&match->surface, daemon->reg->shm,
		    req.scale, fd, req.width, req.height)) {
		snprintf(message, message_size, "could not attach buffer");
		return CARAMEL_STATUS_ERR_IMAGE;
	}

	// Update remembered assignments unless this is a daemon-driven repaint
	if (req.mode == CARAMEL_IMG_DEFAULT) {
		memcpy(daemon->default_path, req.path, strlen(req.path) + 1);
		match->wallpaper_override[0] = '\0';
	} else if (req.mode == CARAMEL_IMG_OVERRIDE) {
		memcpy(match->wallpaper_override, req.path,
			strlen(req.path) + 1);
	}
	snprintf(message, message_size, "applied %s to %s", req.path, req.name);
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

static uint8_t handle_query_outputs(
	struct daemon *daemon, char *message, size_t message_size) {
	size_t off = 0;
	struct caramel_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (!output->surface.configured || output->name == NULL) {
			continue;
		}
		uint32_t scale =
			output->scale > 0 ? (uint32_t)output->scale : 1;
		append_line(message, message_size, &off, "%s %u %u %u\n",
			output->name, output->surface.width * scale,
			output->surface.height * scale, scale);
	}
	if (off > 0 && off <= message_size && message[off - 1] == '\n') {
		message[off - 1] = '\0';
	}
	return CARAMEL_STATUS_OK;
}

static uint8_t dispatch(void *data, uint8_t command, const uint8_t *payload,
	uint32_t len, int fd, char *message, size_t message_size, bool *stop) {
	struct daemon *daemon = data;
	switch (command) {
	case CARAMEL_CMD_STOP:
		*stop = true;
		return CARAMEL_STATUS_OK;
	case CARAMEL_CMD_QUERY:
		return handle_query(daemon, message, message_size);
	case CARAMEL_CMD_QUERY_OUTPUTS:
		return handle_query_outputs(daemon, message, message_size);
	case CARAMEL_CMD_IMG_PREPARED:
		return handle_img_prepared(
			daemon, payload, len, fd, message, message_size);
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

	// Auto-reap the short-lived `caramel prepare` children we spawn
	struct sigaction chld;
	memset(&chld, 0, sizeof(chld));
	chld.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &chld, NULL);
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
	// The daemon does not decode; just check the file is readable here and
	// let the spawned client report any decode error when it runs
	if (access(cfg.image, R_OK) != 0) {
		fprintf(stderr, "carameld: configured image %s: %s\n",
			cfg.image, strerror(errno));
		return;
	}
	memcpy(daemon->default_path, cfg.image, strlen(cfg.image) + 1);
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
