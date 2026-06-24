#include "wayland/surface.h"

#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define BACKGROUND_NAMESPACE "caramel"
#define FRACTIONAL_SCALE_DENOM 120

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

static void handle_preferred_scale(
	void *data, struct wp_fractional_scale_v1 *fractional, uint32_t scale) {
	struct caramel_surface *surface = data;
	(void)fractional;
	if (scale != surface->fractional_scale) {
		surface->fractional_scale = scale;
		surface->needs_repaint = true;
	}
}

static const struct wp_fractional_scale_v1_listener fractional_scale_listener =
	{
		.preferred_scale = handle_preferred_scale,
};

bool caramel_surface_create(struct caramel_surface *surface,
	struct wl_compositor *compositor,
	struct zwlr_layer_shell_v1 *layer_shell, struct wl_output *output,
	struct wp_viewporter *viewporter,
	struct wp_fractional_scale_manager_v1 *fractional_manager) {
	surface->wl_surface = NULL;
	surface->layer_surface = NULL;
	surface->width = 0;
	surface->height = 0;
	surface->configured = false;
	surface->needs_repaint = false;
	surface->viewport = NULL;
	surface->fractional = NULL;
	surface->fractional_scale = 0;

	surface->wl_surface = wl_compositor_create_surface(compositor);
	if (surface->wl_surface == NULL) {
		return false;
	}

	if (viewporter != NULL && fractional_manager != NULL) {
		surface->viewport = wp_viewporter_get_viewport(
			viewporter, surface->wl_surface);
		surface->fractional =
			wp_fractional_scale_manager_v1_get_fractional_scale(
				fractional_manager, surface->wl_surface);
		if (surface->fractional != NULL) {
			wp_fractional_scale_v1_add_listener(surface->fractional,
				&fractional_scale_listener, surface);
		}
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

void caramel_surface_buffer_size(const struct caramel_surface *surface,
	int32_t int_scale, uint32_t *pixel_width, uint32_t *pixel_height) {
	if (surface->fractional_scale > 0) {
		*pixel_width =
			(uint32_t)(((uint64_t)surface->width *
						   surface->fractional_scale +
					   FRACTIONAL_SCALE_DENOM - 1) /
				   FRACTIONAL_SCALE_DENOM);
		*pixel_height =
			(uint32_t)(((uint64_t)surface->height *
						   surface->fractional_scale +
					   FRACTIONAL_SCALE_DENOM - 1) /
				   FRACTIONAL_SCALE_DENOM);
		return;
	}
	if (int_scale < 1) {
		int_scale = 1;
	}
	*pixel_width = surface->width * (uint32_t)int_scale;
	*pixel_height = surface->height * (uint32_t)int_scale;
}

static bool prepare_buffer(struct caramel_surface *surface, struct wl_shm *shm,
	int32_t scale, uint32_t *pixel_width, uint32_t *pixel_height) {
	if (!surface->configured || surface->wl_surface == NULL) {
		return false;
	}
	uint32_t pw;
	uint32_t ph;
	caramel_surface_buffer_size(surface, scale, &pw, &ph);

	// Release any previous buffer before replacing it
	caramel_buffer_destroy(&surface->buffer);
	if (!caramel_buffer_create(&surface->buffer, shm, pw, ph)) {
		return false;
	}
	*pixel_width = pw;
	*pixel_height = ph;
	return true;
}

static void present(struct caramel_surface *surface, int32_t scale,
	uint32_t pixel_width, uint32_t pixel_height) {
	if (surface->viewport != NULL && surface->fractional_scale > 0) {
		wl_surface_set_buffer_scale(surface->wl_surface, 1);
		wp_viewport_set_destination(surface->viewport,
			(int32_t)surface->width, (int32_t)surface->height);
	} else {
		if (scale < 1) {
			scale = 1;
		}
		wl_surface_set_buffer_scale(surface->wl_surface, scale);
	}
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

bool caramel_surface_attach_prepared(struct caramel_surface *surface,
	struct wl_shm *shm, int32_t scale, int fd, uint32_t width,
	uint32_t height) {
	if (!surface->configured || surface->wl_surface == NULL) {
		return false;
	}
	if (scale < 1) {
		scale = 1;
	}

	// Release any previous buffer before replacing it
	caramel_buffer_destroy(&surface->buffer);
	if (!caramel_buffer_from_fd(&surface->buffer, shm, fd, width, height)) {
		return false;
	}
	present(surface, scale, width, height);
	return true;
}

void caramel_surface_destroy(struct caramel_surface *surface) {
	if (surface->fractional != NULL) {
		wp_fractional_scale_v1_destroy(surface->fractional);
		surface->fractional = NULL;
	}
	if (surface->viewport != NULL) {
		wp_viewport_destroy(surface->viewport);
		surface->viewport = NULL;
	}
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
