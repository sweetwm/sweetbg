#ifndef SWEETBG_WAYLAND_OUTPUT_H
#define SWEETBG_WAYLAND_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

#include "wayland/surface.h"

struct sweetbg_output {
	struct wl_list link;
	struct wl_output *wl_output;
	uint32_t global_name;
	int32_t scale;
	int32_t transform;
	int32_t pixel_width;
	int32_t pixel_height;
	char *name;
	char *description;
	struct sweetbg_surface surface;
};

bool sweetbg_output_create(struct wl_list *outputs,
	struct wl_registry *registry, uint32_t global_name, uint32_t version);

void sweetbg_output_remove(struct wl_list *outputs, uint32_t global_name);

void sweetbg_outputs_finish(struct wl_list *outputs);

#endif
