#ifndef SWEETBG_WAYLAND_OUTPUT_H
#define SWEETBG_WAYLAND_OUTPUT_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

#include "wayland/surface.h"

struct zxdg_output_manager_v1;
struct zxdg_output_v1;

struct sweetbg_output {
	struct wl_list link;
	struct wl_output *wl_output;
	uint32_t global_name;
	int32_t scale;
	int32_t transform;
	int32_t pixel_width;
	int32_t pixel_height;
	struct zxdg_output_v1 *xdg_output;
	int32_t logical_x;
	int32_t logical_y;
	uint32_t logical_width;
	uint32_t logical_height;
	char *name;
	char *description;
	struct sweetbg_surface surface;
};

bool sweetbg_output_create(struct wl_list *outputs,
	struct wl_registry *registry, uint32_t global_name, uint32_t version);

void sweetbg_output_attach_xdg(
	struct sweetbg_output *output, struct zxdg_output_manager_v1 *manager);

// Returns true when an output was actually removed
bool sweetbg_output_remove(struct wl_list *outputs, uint32_t global_name);

void sweetbg_outputs_finish(struct wl_list *outputs);

#endif
