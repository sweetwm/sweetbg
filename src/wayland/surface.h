#ifndef SWEETBG_WAYLAND_SURFACE_H
#define SWEETBG_WAYLAND_SURFACE_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

#include "wayland/shm.h"

struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;
struct wp_viewporter;
struct wp_viewport;
struct wp_fractional_scale_manager_v1;
struct wp_fractional_scale_v1;

struct sweetbg_surface {
	struct wl_surface *wl_surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	// Logical (surface-local) size from the layer-surface configure
	uint32_t width;
	uint32_t height;
	bool configured;
	bool needs_repaint;
	struct sweetbg_buffer *buffer;
	struct sweetbg_buffer *retired_buffers;
	struct wp_viewport *viewport;
	struct wp_fractional_scale_v1 *fractional;
	// Preferred scale as 120ths (e.g. 180 = 1.5x); 0 until one is received
	uint32_t fractional_scale;
};

bool sweetbg_surface_create(struct sweetbg_surface *surface,
	struct wl_compositor *compositor,
	struct zwlr_layer_shell_v1 *layer_shell, struct wl_output *output,
	struct wp_viewporter *viewporter,
	struct wp_fractional_scale_manager_v1 *fractional_manager);

void sweetbg_surface_buffer_size(const struct sweetbg_surface *surface,
	int32_t int_scale, uint32_t *pixel_width, uint32_t *pixel_height);

bool sweetbg_surface_paint_color(struct sweetbg_surface *surface,
	struct wl_shm *shm, int32_t scale, uint32_t color);

bool sweetbg_surface_attach_prepared(struct sweetbg_surface *surface,
	struct wl_shm *shm, int32_t scale, int fd, uint32_t width,
	uint32_t height);

void sweetbg_surface_collect(struct sweetbg_surface *surface);

void sweetbg_surface_destroy(struct sweetbg_surface *surface);

#endif
