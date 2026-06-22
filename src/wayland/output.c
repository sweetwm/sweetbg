#include "wayland/output.h"

#include <stdlib.h>
#include <string.h>

#define OUTPUT_MAX_VERSION 4

// strdup is POSIX, not C11; this keeps the module strictly standard
static char *dup_string(const char *s) {
	size_t len = strlen(s) + 1;
	char *copy = malloc(len);
	if (copy != NULL) {
		memcpy(copy, s, len);
	}
	return copy;
}

static void handle_geometry(void *data, struct wl_output *wl_output, int32_t x,
	int32_t y, int32_t physical_width, int32_t physical_height,
	int32_t subpixel, const char *make, const char *model,
	int32_t transform) {
	struct caramel_output *output = data;
	output->transform = transform;
	(void)wl_output;
	(void)x;
	(void)y;
	(void)physical_width;
	(void)physical_height;
	(void)subpixel;
	(void)make;
	(void)model;
}

static void handle_mode(void *data, struct wl_output *wl_output, uint32_t flags,
	int32_t width, int32_t height, int32_t refresh) {
	struct caramel_output *output = data;
	(void)wl_output;
	(void)refresh;
	if ((flags & WL_OUTPUT_MODE_CURRENT) != 0) {
		output->pixel_width = width;
		output->pixel_height = height;
	}
}

static void handle_done(void *data, struct wl_output *wl_output) {
	(void)data;
	(void)wl_output;
}

static void handle_scale(
	void *data, struct wl_output *wl_output, int32_t scale) {
	struct caramel_output *output = data;
	(void)wl_output;
	if (scale != output->scale) {
		output->scale = scale;
		output->surface.needs_repaint = true;
	}
}

static void handle_name(
	void *data, struct wl_output *wl_output, const char *name) {
	struct caramel_output *output = data;
	(void)wl_output;
	free(output->name);
	output->name = dup_string(name);
}

static void handle_description(
	void *data, struct wl_output *wl_output, const char *description) {
	struct caramel_output *output = data;
	(void)wl_output;
	free(output->description);
	output->description = dup_string(description);
}

static const struct wl_output_listener output_listener = {
	.geometry = handle_geometry,
	.mode = handle_mode,
	.done = handle_done,
	.scale = handle_scale,
	.name = handle_name,
	.description = handle_description,
};

bool caramel_output_create(struct wl_list *outputs,
	struct wl_registry *registry, uint32_t global_name, uint32_t version) {
	struct caramel_output *output = calloc(1, sizeof(*output));
	if (output == NULL) {
		return false;
	}

	uint32_t bind_version =
		version < OUTPUT_MAX_VERSION ? version : OUTPUT_MAX_VERSION;
	output->wl_output = wl_registry_bind(
		registry, global_name, &wl_output_interface, bind_version);
	if (output->wl_output == NULL) {
		free(output);
		return false;
	}

	output->global_name = global_name;
	output->scale = 1;
	wl_output_add_listener(output->wl_output, &output_listener, output);
	wl_list_insert(outputs, &output->link);
	return true;
}

static void output_destroy(struct caramel_output *output) {
	wl_list_remove(&output->link);
	// Tear the layer surface down before its output
	caramel_surface_destroy(&output->surface);
	if (output->wl_output != NULL) {
		wl_output_destroy(output->wl_output);
	}
	free(output->name);
	free(output->description);
	free(output);
}

void caramel_output_remove(struct wl_list *outputs, uint32_t global_name) {
	struct caramel_output *output;
	struct caramel_output *tmp;
	wl_list_for_each_safe(output, tmp, outputs, link) {
		if (output->global_name == global_name) {
			output_destroy(output);
			return;
		}
	}
}

void caramel_outputs_finish(struct wl_list *outputs) {
	struct caramel_output *output;
	struct caramel_output *tmp;
	wl_list_for_each_safe(output, tmp, outputs, link) {
		output_destroy(output);
	}
}
