#ifndef CARAMEL_WAYLAND_SURFACE_H
#define CARAMEL_WAYLAND_SURFACE_H

#include <stdbool.h>
#include <stdint.h>
#include <wayland-client.h>

#include "image/image.h"
#include "wayland/shm.h"

struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;

struct caramel_surface {
	struct wl_surface *wl_surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	uint32_t width;
	uint32_t height;
	bool configured;
	bool needs_repaint;
	struct caramel_buffer buffer;
};

bool caramel_surface_create(struct caramel_surface *surface,
	struct wl_compositor *compositor,
	struct zwlr_layer_shell_v1 *layer_shell, struct wl_output *output);

bool caramel_surface_paint_color(struct caramel_surface *surface,
	struct wl_shm *shm, int32_t scale, uint32_t color);

bool caramel_surface_paint_image(struct caramel_surface *surface,
	struct wl_shm *shm, int32_t scale, const struct caramel_image *image);

void caramel_surface_destroy(struct caramel_surface *surface);

#endif
