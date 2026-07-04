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
#include "query/json.h"
#include "wayland/output.h"
#include "wayland/registry.h"
#include "wayland/surface.h"

struct assignment {
	char name[64];
	char path[PATH_MAX];
	enum sweetbg_fit fit;
	bool has_image;
	bool has_fit;
};

#define MAX_ASSIGNMENTS 16
#define MAX_QUERY_OUTPUTS 64

struct daemon {
	struct wl_display *display;
	struct sweetbg_registry *reg;
	uint32_t color;
	enum sweetbg_fit fit;
	char default_path[PATH_MAX];
	struct assignment assignments[MAX_ASSIGNMENTS];
	size_t assignment_count;
};

static volatile sig_atomic_t g_running = 1;

static void compact_assignments(struct daemon *daemon);

static void handle_signal(int signal_number) {
	(void)signal_number;
	g_running = 0;
}

static struct assignment *assignment_for(
	struct daemon *daemon, const char *name) {
	if (name == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < daemon->assignment_count; i++) {
		if (strcmp(daemon->assignments[i].name, name) == 0) {
			return &daemon->assignments[i];
		}
	}
	return NULL;
}

static const struct assignment *const_assignment_for(
	const struct daemon *daemon, const char *name) {
	if (name == NULL) {
		return NULL;
	}
	for (size_t i = 0; i < daemon->assignment_count; i++) {
		if (strcmp(daemon->assignments[i].name, name) == 0) {
			return &daemon->assignments[i];
		}
	}
	return NULL;
}

static struct assignment *ensure_assignment(
	struct daemon *daemon, const char *name) {
	struct assignment *entry = assignment_for(daemon, name);
	if (entry != NULL) {
		return entry;
	}
	if (daemon->assignment_count >= MAX_ASSIGNMENTS) {
		return NULL;
	}
	entry = &daemon->assignments[daemon->assignment_count++];
	memset(entry, 0, sizeof(*entry));
	snprintf(entry->name, sizeof(entry->name), "%s", name);
	return entry;
}

static void set_image_assignment(
	struct daemon *daemon, const char *name, const char *path) {
	struct assignment *entry = ensure_assignment(daemon, name);
	if (entry == NULL) {
		return;
	}
	snprintf(entry->path, sizeof(entry->path), "%s", path);
	entry->has_image = true;
}

static bool set_blank_assignment(struct daemon *daemon, const char *name) {
	struct assignment *entry = ensure_assignment(daemon, name);
	if (entry == NULL) {
		return false;
	}
	entry->path[0] = '\0';
	entry->has_image = true;
	return true;
}

static void set_fit_assignment(
	struct daemon *daemon, const char *name, enum sweetbg_fit fit) {
	struct assignment *entry = ensure_assignment(daemon, name);
	if (entry == NULL) {
		return;
	}
	entry->fit = fit;
	entry->has_fit = true;
}

static bool clear_image_assignment(struct daemon *daemon, const char *name) {
	struct assignment *entry = assignment_for(daemon, name);
	if (entry == NULL || !entry->has_image) {
		return false;
	}
	entry->has_image = false;
	entry->path[0] = '\0';
	compact_assignments(daemon);
	return true;
}

static bool clear_fit_assignment(struct daemon *daemon, const char *name) {
	struct assignment *entry = assignment_for(daemon, name);
	if (entry == NULL || !entry->has_fit) {
		return false;
	}
	entry->has_fit = false;
	compact_assignments(daemon);
	return true;
}

static void compact_assignments(struct daemon *daemon) {
	size_t out = 0;
	for (size_t i = 0; i < daemon->assignment_count; i++) {
		if (!daemon->assignments[i].has_image &&
			!daemon->assignments[i].has_fit) {
			continue;
		}
		if (out != i) {
			daemon->assignments[out] = daemon->assignments[i];
		}
		out++;
	}
	daemon->assignment_count = out;
}

static void clear_image_assignments(struct daemon *daemon) {
	for (size_t i = 0; i < daemon->assignment_count; i++) {
		daemon->assignments[i].has_image = false;
		daemon->assignments[i].path[0] = '\0';
	}
	compact_assignments(daemon);
}

static bool clear_fit_assignments(struct daemon *daemon) {
	bool changed = false;
	for (size_t i = 0; i < daemon->assignment_count; i++) {
		if (daemon->assignments[i].has_fit) {
			daemon->assignments[i].has_fit = false;
			changed = true;
		}
	}
	compact_assignments(daemon);
	return changed;
}

static bool ensure_surfaces(struct sweetbg_registry *reg) {
	struct sweetbg_output *output;
	wl_list_for_each(output, &reg->outputs, link) {
		if (output->surface.layer_surface != NULL) {
			continue;
		}
		if (!sweetbg_surface_create(&output->surface, reg->compositor,
			    reg->layer_shell, output->wl_output,
			    reg->viewporter, reg->fractional_scale_manager)) {
			fprintf(stderr,
				"sweetbgd: failed to create a layer surface\n");
			return false;
		}
	}
	return true;
}

static const char *effective_path(
	const struct daemon *daemon, const struct sweetbg_output *output) {
	const struct assignment *assigned =
		const_assignment_for(daemon, output->name);
	return assigned != NULL && assigned->has_image ? assigned->path
						       : daemon->default_path;
}

static enum sweetbg_fit effective_fit(
	const struct daemon *daemon, const struct sweetbg_output *output) {
	const struct assignment *assigned =
		const_assignment_for(daemon, output->name);
	return assigned != NULL && assigned->has_fit ? assigned->fit
						     : daemon->fit;
}

static void client_binary(char *out, size_t out_size) {
	ssize_t n = readlink("/proc/self/exe", out, out_size - 1);
	if (n > 0 && (size_t)n < out_size) {
		// NOLINTNEXTLINE(clang-analyzer-security.ArrayBound)
		out[n] = '\0';
		char *slash = strrchr(out, '/');
		if (slash != NULL) {
			size_t dir_len = (size_t)(slash - out) + 1;
			const char *bin = "sweetbg";
			if (dir_len + strlen(bin) + 1 <= out_size) {
				memcpy(out + dir_len, bin, strlen(bin) + 1);
				if (access(out, X_OK) == 0) {
					return;
				}
			}
		}
	}
	snprintf(out, out_size, "sweetbg");
}

static void spawn_prepare(const char *name, const char *path) {
	if (name == NULL) {
		return;
	}
	pid_t pid = fork();
	if (pid < 0) {
		fprintf(stderr, "sweetbgd: cannot spawn client: %s\n",
			strerror(errno));
		return;
	}
	if (pid == 0) {
		char bin[PATH_MAX];
		client_binary(bin, sizeof(bin));
		execlp(bin, "sweetbg", "prepare", name, path, (char *)NULL);
		_exit(127);
	}
}

static void reconcile_paint(struct daemon *daemon) {
	struct sweetbg_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (!output->surface.configured ||
			!output->surface.needs_repaint) {
			continue;
		}
		const char *path = effective_path(daemon, output);
		if (path[0] == '\0') {
			sweetbg_surface_paint_color(&output->surface,
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
	req->mode = sweetbg_get_u32(p);
	req->scale = (int32_t)sweetbg_get_u32(p + 4);
	req->width = sweetbg_get_u32(p + 8);
	req->height = sweetbg_get_u32(p + 12);

	uint32_t name_len = sweetbg_get_u32(p + 16);
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
	uint32_t path_len = sweetbg_get_u32(p + off);
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
		return SWEETBG_STATUS_ERR_BAD_REQUEST;
	}

	struct sweetbg_output *output;
	struct sweetbg_output *match = NULL;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (output->name != NULL &&
			strcmp(output->name, req.name) == 0) {
			match = output;
			break;
		}
	}
	if (match == NULL) {
		snprintf(message, message_size, "no output named %s", req.name);
		return SWEETBG_STATUS_ERR_BAD_REQUEST;
	}

	if (!sweetbg_surface_attach_prepared(&match->surface, daemon->reg->shm,
		    req.scale, fd, req.width, req.height)) {
		snprintf(message, message_size, "could not attach buffer");
		return SWEETBG_STATUS_ERR_IMAGE;
	}

	// Update remembered assignments unless this is a daemon-driven repaint
	if (req.mode == SWEETBG_IMG_DEFAULT) {
		memcpy(daemon->default_path, req.path, strlen(req.path) + 1);
		clear_image_assignments(daemon);
	} else if (req.mode == SWEETBG_IMG_OVERRIDE) {
		set_image_assignment(daemon, req.name, req.path);
	}
	snprintf(message, message_size, "applied %s to %s", req.path, req.name);
	return SWEETBG_STATUS_OK;
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
	append_line(message, message_size, &off, "fit: %s\n",
		sweetbg_fit_name(daemon->fit));

	struct sweetbg_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		const char *name =
			output->name != NULL ? output->name : "(unnamed)";
		const struct assignment *assigned =
			const_assignment_for(daemon, output->name);
		const bool has_image = assigned != NULL && assigned->has_image;
		const bool has_fit = assigned != NULL && assigned->has_fit;
		if (has_image && has_fit) {
			if (assigned->path[0] == '\0') {
				append_line(message, message_size, &off,
					"%s: %dx%d scale %d "
					"(blank, fit: %s)\n",
					name, output->pixel_width,
					output->pixel_height, output->scale,
					sweetbg_fit_name(assigned->fit));
			} else {
				append_line(message, message_size, &off,
					"%s: %dx%d scale %d "
					"(override: %s, fit: %s)\n",
					name, output->pixel_width,
					output->pixel_height, output->scale,
					assigned->path,
					sweetbg_fit_name(assigned->fit));
			}
		} else if (has_image) {
			if (assigned->path[0] == '\0') {
				append_line(message, message_size, &off,
					"%s: %dx%d scale %d (blank)\n", name,
					output->pixel_width,
					output->pixel_height, output->scale);
			} else {
				append_line(message, message_size, &off,
					"%s: %dx%d scale %d (override: %s)\n",
					name, output->pixel_width,
					output->pixel_height, output->scale,
					assigned->path);
			}
		} else if (has_fit) {
			append_line(message, message_size, &off,
				"%s: %dx%d scale %d (fit: %s)\n", name,
				output->pixel_width, output->pixel_height,
				output->scale, sweetbg_fit_name(assigned->fit));
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
	return SWEETBG_STATUS_OK;
}

static uint8_t handle_query_json(
	struct daemon *daemon, char *message, size_t message_size) {
	struct sweetbg_query_json_output outputs[MAX_QUERY_OUTPUTS];
	size_t output_count = 0;

	struct sweetbg_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (output_count >= MAX_QUERY_OUTPUTS) {
			snprintf(message, message_size, "too many outputs");
			return SWEETBG_STATUS_ERR_BAD_REQUEST;
		}

		const struct assignment *assigned =
			const_assignment_for(daemon, output->name);
		const bool has_image = assigned != NULL && assigned->has_image;
		const bool has_fit = assigned != NULL && assigned->has_fit;
		const bool blank = has_image && assigned->path[0] == '\0';
		const char *path =
			blank ? NULL : effective_path(daemon, output);

		outputs[output_count++] = (struct sweetbg_query_json_output){
			.name = output->name,
			.width = output->pixel_width,
			.height = output->pixel_height,
			.scale = output->scale,
			.configured = output->surface.configured,
			.image = path,
			.image_override = has_image,
			.blank = blank,
			.fit = sweetbg_fit_name(effective_fit(daemon, output)),
			.fit_override = has_fit,
		};
	}

	const struct sweetbg_query_json_state state = {
		.default_image = daemon->default_path,
		.color = daemon->color,
		.default_fit = sweetbg_fit_name(daemon->fit),
		.outputs = outputs,
		.output_count = output_count,
	};
	if (!sweetbg_query_json_write(&state, message, message_size)) {
		snprintf(message, message_size, "query response too large");
		return SWEETBG_STATUS_ERR_BAD_REQUEST;
	}
	return SWEETBG_STATUS_OK;
}

static uint8_t handle_query_outputs(
	struct daemon *daemon, char *message, size_t message_size) {
	size_t off = 0;
	append_line(message, message_size, &off, "meta %u %u\n",
		(unsigned)daemon->fit, daemon->color & 0xffffffu);
	struct sweetbg_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (!output->surface.configured || output->name == NULL) {
			continue;
		}
		uint32_t scale =
			output->scale > 0 ? (uint32_t)output->scale : 1;
		uint32_t pw;
		uint32_t ph;
		sweetbg_surface_buffer_size(
			&output->surface, output->scale, &pw, &ph);
		append_line(message, message_size, &off, "%s %u %u %u %u\n",
			output->name, pw, ph, scale,
			(unsigned)effective_fit(daemon, output));
	}
	if (off > 0 && off <= message_size && message[off - 1] == '\n') {
		message[off - 1] = '\0';
	}
	return SWEETBG_STATUS_OK;
}

static bool fit_uses_color(enum sweetbg_fit fit) {
	return fit == SWEETBG_FIT_CONTAIN || fit == SWEETBG_FIT_CENTER;
}

static void repaint_after_change(
	struct daemon *daemon, bool color_changed, bool fit_changed) {
	struct sweetbg_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (!output->surface.configured) {
			continue;
		}
		bool is_placeholder = effective_path(daemon, output)[0] == '\0';
		bool image_needs =
			fit_changed ||
			(color_changed &&
				fit_uses_color(effective_fit(daemon, output)));
		if (is_placeholder ? color_changed : image_needs) {
			output->surface.needs_repaint = true;
		}
	}
	reconcile_paint(daemon);
}

static struct sweetbg_output *find_output(
	struct daemon *daemon, const char *name) {
	struct sweetbg_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (output->name != NULL && strcmp(output->name, name) == 0) {
			return output;
		}
	}
	return NULL;
}

static void repaint_output_after_fit(struct daemon *daemon,
	struct sweetbg_output *output, enum sweetbg_fit old_fit) {
	if (!output->surface.configured) {
		return;
	}
	if (old_fit == effective_fit(daemon, output)) {
		return;
	}
	if (effective_path(daemon, output)[0] == '\0') {
		return;
	}
	output->surface.needs_repaint = true;
	reconcile_paint(daemon);
}

struct set_request {
	uint32_t field;
	uint32_t value;
	char output[64];
};

static bool parse_set(
	const uint8_t *payload, uint32_t len, struct set_request *req) {
	if (len != 8 && len < 12) {
		return false;
	}
	req->field = sweetbg_get_u32(payload);
	req->value = sweetbg_get_u32(payload + 4);
	req->output[0] = '\0';
	if (len == 8) {
		return true;
	}
	uint32_t output_len = sweetbg_get_u32(payload + 8);
	if (output_len == 0 || output_len >= sizeof(req->output) ||
		12 + output_len != len) {
		return false;
	}
	memcpy(req->output, payload + 12, output_len);
	req->output[output_len] = '\0';
	return true;
}

static uint8_t handle_set(struct daemon *daemon, const uint8_t *payload,
	uint32_t len, char *message, size_t message_size) {
	struct set_request req;
	if (!parse_set(payload, len, &req)) {
		snprintf(message, message_size, "invalid set request");
		return SWEETBG_STATUS_ERR_BAD_REQUEST;
	}

	if (req.field == SWEETBG_SET_FIT) {
		if (req.value > SWEETBG_FIT_TILE) {
			snprintf(message, message_size, "invalid fit mode");
			return SWEETBG_STATUS_ERR_BAD_REQUEST;
		}
		enum sweetbg_fit fit = (enum sweetbg_fit)req.value;
		if (req.output[0] != '\0') {
			struct sweetbg_output *output =
				find_output(daemon, req.output);
			if (output == NULL) {
				snprintf(message, message_size,
					"no output named %s", req.output);
				return SWEETBG_STATUS_ERR_BAD_REQUEST;
			}
			enum sweetbg_fit old_fit =
				effective_fit(daemon, output);
			set_fit_assignment(daemon, req.output, fit);
			repaint_output_after_fit(daemon, output, old_fit);
			snprintf(message, message_size, "fit %s for %s",
				sweetbg_fit_name(fit), req.output);
			return SWEETBG_STATUS_OK;
		}
		bool changed = fit != daemon->fit;
		bool cleared = clear_fit_assignments(daemon);
		if (changed) {
			daemon->fit = fit;
		}
		if (changed || cleared) {
			repaint_after_change(daemon, false, true);
		}
		snprintf(message, message_size, "fit %s",
			sweetbg_fit_name(daemon->fit));
		return SWEETBG_STATUS_OK;
	}
	if (req.field == SWEETBG_SET_COLOR) {
		if (req.output[0] != '\0') {
			snprintf(message, message_size,
				"color cannot be scoped to an output");
			return SWEETBG_STATUS_ERR_BAD_REQUEST;
		}
		if (req.value > 0xffffffu) {
			snprintf(message, message_size, "invalid color");
			return SWEETBG_STATUS_ERR_BAD_REQUEST;
		}
		if (req.value != daemon->color) {
			daemon->color = req.value;
			repaint_after_change(daemon, true, false);
		}
		snprintf(message, message_size, "color #%06x",
			daemon->color & 0xffffffu);
		return SWEETBG_STATUS_OK;
	}
	snprintf(message, message_size, "unknown set field");
	return SWEETBG_STATUS_ERR_BAD_REQUEST;
}

struct clear_request {
	uint32_t flags;
	char output[64];
};

static bool parse_clear(
	const uint8_t *payload, uint32_t len, struct clear_request *req) {
	if (len != 8 && len < 9) {
		return false;
	}
	req->flags = sweetbg_get_u32(payload);
	req->output[0] = '\0';
	uint32_t output_len = sweetbg_get_u32(payload + 4);
	if (output_len == 0) {
		return len == 8;
	}
	if (output_len >= sizeof(req->output) || 8 + output_len != len) {
		return false;
	}
	memcpy(req->output, payload + 8, output_len);
	req->output[output_len] = '\0';
	return true;
}

static bool any_image_assignment(const struct daemon *daemon) {
	for (size_t i = 0; i < daemon->assignment_count; i++) {
		if (daemon->assignments[i].has_image) {
			return true;
		}
	}
	return false;
}

static void mark_all_repaint(struct daemon *daemon) {
	struct sweetbg_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (output->surface.configured) {
			output->surface.needs_repaint = true;
		}
	}
}

static void clear_message(
	const struct clear_request *req, char *message, size_t message_size) {
	const bool image = (req->flags & SWEETBG_CLEAR_IMAGE) != 0;
	const bool fit = (req->flags & SWEETBG_CLEAR_FIT) != 0;
	const bool blank = (req->flags & SWEETBG_CLEAR_BLANK) != 0;
	if (blank) {
		snprintf(message, message_size,
			fit ? "blanked image and cleared fit for %s"
			    : "blanked image for %s",
			req->output);
		return;
	}
	const char *what = image && fit ? "image and fit"
			   : image	? "image"
					: "fit";
	if (req->output[0] != '\0') {
		snprintf(message, message_size, "cleared %s for %s", what,
			req->output);
	} else {
		snprintf(message, message_size, "cleared %s", what);
	}
}

static uint8_t handle_clear(struct daemon *daemon, const uint8_t *payload,
	uint32_t len, char *message, size_t message_size) {
	struct clear_request req;
	if (!parse_clear(payload, len, &req)) {
		snprintf(message, message_size, "invalid clear request");
		return SWEETBG_STATUS_ERR_BAD_REQUEST;
	}
	uint32_t allowed =
		SWEETBG_CLEAR_IMAGE | SWEETBG_CLEAR_FIT | SWEETBG_CLEAR_BLANK;
	if (req.flags == 0 || (req.flags & ~allowed) != 0) {
		snprintf(message, message_size, "invalid clear target");
		return SWEETBG_STATUS_ERR_BAD_REQUEST;
	}
	if ((req.flags & SWEETBG_CLEAR_BLANK) != 0 && req.output[0] == '\0') {
		snprintf(message, message_size, "blank requires an output");
		return SWEETBG_STATUS_ERR_BAD_REQUEST;
	}
	if ((req.flags & SWEETBG_CLEAR_BLANK) != 0 &&
		(req.flags & SWEETBG_CLEAR_IMAGE) != 0) {
		snprintf(message, message_size,
			"blank and image clear are mutually exclusive");
		return SWEETBG_STATUS_ERR_BAD_REQUEST;
	}

	if (req.output[0] != '\0') {
		struct sweetbg_output *output = find_output(daemon, req.output);
		if (output == NULL) {
			snprintf(message, message_size, "no output named %s",
				req.output);
			return SWEETBG_STATUS_ERR_BAD_REQUEST;
		}
		const char *old_path = effective_path(daemon, output);
		bool had_image = old_path[0] != '\0';
		enum sweetbg_fit old_fit = effective_fit(daemon, output);
		bool image_changed = false;
		bool fit_changed = false;

		if ((req.flags & SWEETBG_CLEAR_BLANK) != 0) {
			if (!set_blank_assignment(daemon, req.output)) {
				snprintf(message, message_size,
					"too many output overrides");
				return SWEETBG_STATUS_ERR_BAD_REQUEST;
			}
			image_changed = had_image;
		}
		if ((req.flags & SWEETBG_CLEAR_IMAGE) != 0) {
			image_changed =
				clear_image_assignment(daemon, req.output);
		}
		if ((req.flags & SWEETBG_CLEAR_FIT) != 0) {
			fit_changed =
				clear_fit_assignment(daemon, req.output) &&
				old_fit != effective_fit(daemon, output);
		}
		if (image_changed || (fit_changed && had_image &&
					     output->surface.configured)) {
			output->surface.needs_repaint = true;
			reconcile_paint(daemon);
		}
		clear_message(&req, message, message_size);
		return SWEETBG_STATUS_OK;
	}

	bool image_changed = false;
	bool fit_changed = false;
	if ((req.flags & SWEETBG_CLEAR_IMAGE) != 0) {
		image_changed = daemon->default_path[0] != '\0' ||
				any_image_assignment(daemon);
		daemon->default_path[0] = '\0';
		clear_image_assignments(daemon);
	}
	if ((req.flags & SWEETBG_CLEAR_FIT) != 0) {
		fit_changed = daemon->fit != SWEETBG_FIT_COVER;
		if (fit_changed) {
			daemon->fit = SWEETBG_FIT_COVER;
		}
		fit_changed = clear_fit_assignments(daemon) || fit_changed;
	}
	if (image_changed) {
		mark_all_repaint(daemon);
		reconcile_paint(daemon);
	} else if (fit_changed) {
		repaint_after_change(daemon, false, true);
	}
	clear_message(&req, message, message_size);
	return SWEETBG_STATUS_OK;
}

static uint8_t dispatch(void *data, uint8_t command, const uint8_t *payload,
	uint32_t len, int fd, char *message, size_t message_size, bool *stop) {
	struct daemon *daemon = data;
	switch (command) {
	case SWEETBG_CMD_STOP:
		*stop = true;
		return SWEETBG_STATUS_OK;
	case SWEETBG_CMD_QUERY:
		return handle_query(daemon, message, message_size);
	case SWEETBG_CMD_QUERY_JSON:
		return handle_query_json(daemon, message, message_size);
	case SWEETBG_CMD_QUERY_OUTPUTS:
		return handle_query_outputs(daemon, message, message_size);
	case SWEETBG_CMD_IMG_PREPARED:
		return handle_img_prepared(
			daemon, payload, len, fd, message, message_size);
	case SWEETBG_CMD_SET:
		return handle_set(daemon, payload, len, message, message_size);
	case SWEETBG_CMD_CLEAR:
		return handle_clear(
			daemon, payload, len, message, message_size);
	default:
		snprintf(message, message_size, "unknown command");
		return SWEETBG_STATUS_ERR_UNKNOWN_COMMAND;
	}
}

static void install_signal_handlers(void) {
	struct sigaction sa;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handle_signal;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	// Auto-reap the short-lived `sweetbg prepare` children we spawn
	struct sigaction chld;
	memset(&chld, 0, sizeof(chld));
	chld.sa_handler = SIG_IGN;
	sigaction(SIGCHLD, &chld, NULL);
}

static void run_loop(struct daemon *daemon, struct sweetbg_ipc_server *ipc) {
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
			sweetbg_ipc_server_handle(ipc, dispatch, daemon, &stop);
			if (stop) {
				g_running = 0;
			}
		}
	}
}

static void report_outputs(struct sweetbg_registry *reg) {
	struct sweetbg_output *output;
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
	struct sweetbg_config cfg;
	char err[256];
	if (!sweetbg_config_load(&cfg, err, sizeof(err))) {
		fprintf(stderr, "sweetbgd: %s\n", err);
	}
	daemon->color = cfg.color;
	daemon->fit = cfg.fit;

	if (cfg.image[0] != '\0') {
		if (access(cfg.image, R_OK) == 0) {
			memcpy(daemon->default_path, cfg.image,
				strlen(cfg.image) + 1);
		} else {
			fprintf(stderr, "sweetbgd: configured image %s: %s\n",
				cfg.image, strerror(errno));
		}
	}

	for (size_t i = 0; i < cfg.output_count; i++) {
		const struct sweetbg_config_output *out = &cfg.outputs[i];
		if (out->has_fit) {
			set_fit_assignment(daemon, out->name, out->fit);
		}
		if (out->has_image) {
			if (out->image[0] == '\0') {
				set_blank_assignment(daemon, out->name);
			} else if (access(out->image, R_OK) == 0) {
				set_image_assignment(
					daemon, out->name, out->image);
			} else {
				fprintf(stderr,
					"sweetbgd: configured image %s: %s\n",
					out->image, strerror(errno));
			}
		}
	}
}

static bool serve(struct wl_display *display, struct sweetbg_ipc_server *ipc) {
	struct sweetbg_registry reg;
	if (!sweetbg_registry_init(&reg, display)) {
		return false;
	}

	// A second roundtrip delivers the configure for each new surface
	if (!ensure_surfaces(&reg) || wl_display_roundtrip(display) < 0) {
		sweetbg_registry_finish(&reg);
		return false;
	}

	printf("sweetbgd: connected; %d output(s), wlr-layer-shell and "
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
		sweetbg_registry_finish(&reg);
		return false;
	}

	run_loop(&daemon, ipc);
	sweetbg_registry_finish(&reg);
	return true;
}

static int run(void) {
	struct sweetbg_ipc_server ipc;
	if (!sweetbg_ipc_server_init(&ipc)) {
		return 1;
	}

	struct wl_display *display = wl_display_connect(NULL);
	if (display == NULL) {
		fprintf(stderr,
			"sweetbgd: cannot connect to a wayland display; "
			"is WAYLAND_DISPLAY set?\n");
		sweetbg_ipc_server_finish(&ipc);
		return 1;
	}

	install_signal_handlers();
	bool ok = serve(display, &ipc);

	wl_display_disconnect(display);
	sweetbg_ipc_server_finish(&ipc);
	return ok ? 0 : 1;
}

int main(int argc, char **argv) {
	if (argc > 1) {
		const char *arg = argv[1];

		if (strcmp(arg, "-V") == 0 || strcmp(arg, "--version") == 0) {
			printf("sweetbgd %s\n", SWEETBG_VERSION);
			return 0;
		}

		if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
			printf("usage: sweetbgd [-h|--help] [-V|--version]\n");
			return 0;
		}

		fprintf(stderr, "sweetbgd: unknown argument '%s'\n", arg);
		return 2;
	}

	return run();
}
