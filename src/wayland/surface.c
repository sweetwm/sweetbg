#include "wayland/surface.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define BACKGROUND_NAMESPACE "caramel"

static void handle_configure(void *data,
	struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
	uint32_t width, uint32_t height) {
	struct caramel_surface *surface = data;
	zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

	if (width != surface->width || height != surface->height) {
		surface->needs_repaint = true;
	}
	surface->width = width;
	surface->height = height;
	surface->configured = true;
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
	surface->needs_repaint = false;

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

static bool prepare_buffer(struct caramel_surface *surface, struct wl_shm *shm,
	int32_t scale, uint32_t *pixel_width, uint32_t *pixel_height) {
	if (!surface->configured || surface->wl_surface == NULL) {
		return false;
	}
	if (scale < 1) {
		scale = 1;
	}

	uint32_t pw = surface->width * (uint32_t)scale;
	uint32_t ph = surface->height * (uint32_t)scale;

	// Release any previous buffer before replacing it
	caramel_buffer_destroy(&surface->buffer);
	if (!caramel_buffer_create(&surface->buffer, shm, pw, ph)) {
		return false;
	}
	*pixel_width = pw;
	*pixel_height = ph;
	return true;
}

// Attach the freshly filled buffer, damage the whole surface, and commit
static void present(struct caramel_surface *surface, int32_t scale,
	uint32_t pixel_width, uint32_t pixel_height) {
	if (scale < 1) {
		scale = 1;
	}
	wl_surface_set_buffer_scale(surface->wl_surface, scale);
	wl_surface_attach(surface->wl_surface, surface->buffer.wl_buffer, 0, 0);
	wl_surface_damage_buffer(surface->wl_surface, 0, 0,
		(int32_t)pixel_width, (int32_t)pixel_height);
	wl_surface_commit(surface->wl_surface);
	surface->needs_repaint = false;
}

bool caramel_surface_paint_color(struct caramel_surface *surface,
	struct wl_shm *shm, int32_t scale, uint32_t color) {
	uint32_t pw;
	uint32_t ph;
	if (!prepare_buffer(surface, shm, scale, &pw, &ph)) {
		return false;
	}
	caramel_buffer_fill(&surface->buffer, color);
	present(surface, scale, pw, ph);
	return true;
}

bool caramel_surface_paint_image(struct caramel_surface *surface,
	struct wl_shm *shm, int32_t scale, const struct caramel_image *image) {
	uint32_t pw;
	uint32_t ph;
	if (!prepare_buffer(surface, shm, scale, &pw, &ph)) {
		return false;
	}
	if (!caramel_image_render_cover(image, pw, ph, surface->buffer.data)) {
		return false;
	}
	present(surface, scale, pw, ph);
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
	// Free pixels only after the surface stops referencing the buffer
	caramel_buffer_destroy(&surface->buffer);
	surface->configured = false;
}
