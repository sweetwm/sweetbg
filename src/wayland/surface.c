#include "wayland/surface.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define BACKGROUND_NAMESPACE "caramel"

static void handle_configure(void *data,
	struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
	uint32_t width, uint32_t height) {
	struct caramel_surface *surface = data;
	zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
	surface->width = width;
	surface->height = height;
	surface->configured = true;

	wl_surface_commit(surface->wl_surface);
}

static void handle_closed(
	void *data, struct zwlr_layer_surface_v1 *layer_surface) {
	struct caramel_surface *surface = data;
	(void)layer_surface;

	caramel_surface_destroy(surface);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = handle_configure,
	.closed = handle_closed,
};

bool caramel_surface_create(struct caramel_surface *surface,
	struct wl_compositor *compositor,
	struct zwlr_layer_shell_v1 *layer_shell, struct wl_output *output) {
	surface->wl_surface = NULL;
	surface->layer_surface = NULL;
	surface->width = 0;
	surface->height = 0;
	surface->configured = false;

	surface->wl_surface = wl_compositor_create_surface(compositor);
	if (surface->wl_surface == NULL) {
		return false;
	}

	surface->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
		layer_shell, surface->wl_surface, output,
		ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, BACKGROUND_NAMESPACE);
	if (surface->layer_surface == NULL) {
		wl_surface_destroy(surface->wl_surface);
		surface->wl_surface = NULL;
		return false;
	}

	zwlr_layer_surface_v1_add_listener(
		surface->layer_surface, &layer_surface_listener, surface);

	uint32_t anchor = ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
			  ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
			  ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT |
			  ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT;
	zwlr_layer_surface_v1_set_anchor(surface->layer_surface, anchor);
	zwlr_layer_surface_v1_set_exclusive_zone(surface->layer_surface, -1);
	zwlr_layer_surface_v1_set_size(surface->layer_surface, 0, 0);

	wl_surface_commit(surface->wl_surface);
	return true;
}

void caramel_surface_destroy(struct caramel_surface *surface) {
	if (surface->layer_surface != NULL) {
		zwlr_layer_surface_v1_destroy(surface->layer_surface);
		surface->layer_surface = NULL;
	}
	if (surface->wl_surface != NULL) {
		wl_surface_destroy(surface->wl_surface);
		surface->wl_surface = NULL;
	}
	surface->configured = false;
}
