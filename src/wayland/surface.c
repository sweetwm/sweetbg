#include "wayland/surface.h"

#include <stdlib.h>

#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#define BACKGROUND_NAMESPACE "sweetbg"
#define FRACTIONAL_SCALE_DENOM 120

static void handle_configure(void *data,
	struct zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
	uint32_t width, uint32_t height) {
	struct sweetbg_surface *surface = data;
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
	struct sweetbg_surface *surface = data;
	(void)layer_surface;

	sweetbg_surface_destroy(surface);
}

static const struct zwlr_layer_surface_v1_listener layer_surface_listener = {
	.configure = handle_configure,
	.closed = handle_closed,
};

static void handle_preferred_scale(
	void *data, struct wp_fractional_scale_v1 *fractional, uint32_t scale) {
	struct sweetbg_surface *surface = data;
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

bool sweetbg_surface_create(struct sweetbg_surface *surface,
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
	surface->buffer = NULL;
	surface->retired_buffers = NULL;
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

void sweetbg_surface_buffer_size(const struct sweetbg_surface *surface,
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

static void free_buffer(struct sweetbg_buffer *buffer);
static void retire_buffer(struct sweetbg_surface *surface);

static bool prepare_buffer(struct sweetbg_surface *surface, struct wl_shm *shm,
	int32_t scale, uint32_t *pixel_width, uint32_t *pixel_height) {
	if (!surface->configured || surface->wl_surface == NULL) {
		return false;
	}
	uint32_t pw;
	uint32_t ph;
	sweetbg_surface_buffer_size(surface, scale, &pw, &ph);

	struct sweetbg_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		return false;
	}
	if (!sweetbg_buffer_create(buffer, shm, pw, ph)) {
		free(buffer);
		return false;
	}
	retire_buffer(surface);
	surface->buffer = buffer;
	*pixel_width = pw;
	*pixel_height = ph;
	return true;
}

static void present(struct sweetbg_surface *surface, int32_t scale,
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
	wl_surface_attach(
		surface->wl_surface, surface->buffer->wl_buffer, 0, 0);
	wl_surface_damage_buffer(surface->wl_surface, 0, 0,
		(int32_t)pixel_width, (int32_t)pixel_height);
	wl_surface_commit(surface->wl_surface);
	surface->needs_repaint = false;
}

bool sweetbg_surface_paint_color(struct sweetbg_surface *surface,
	struct wl_shm *shm, int32_t scale, uint32_t color) {
	uint32_t pw;
	uint32_t ph;
	if (!prepare_buffer(surface, shm, scale, &pw, &ph)) {
		return false;
	}
	sweetbg_buffer_fill(surface->buffer, color);
	sweetbg_buffer_unmap(surface->buffer);
	present(surface, scale, pw, ph);
	return true;
}

bool sweetbg_surface_attach_prepared(struct sweetbg_surface *surface,
	struct wl_shm *shm, int32_t scale, int fd, uint32_t width,
	uint32_t height) {
	if (!surface->configured || surface->wl_surface == NULL) {
		return false;
	}
	if (scale < 1) {
		scale = 1;
	}

	struct sweetbg_buffer *buffer = calloc(1, sizeof(*buffer));
	if (buffer == NULL) {
		return false;
	}
	if (!sweetbg_buffer_from_fd(buffer, shm, fd, width, height)) {
		free(buffer);
		return false;
	}
	retire_buffer(surface);
	surface->buffer = buffer;
	present(surface, scale, width, height);
	return true;
}

static void free_buffer(struct sweetbg_buffer *buffer) {
	if (buffer == NULL) {
		return;
	}
	sweetbg_buffer_destroy(buffer);
	free(buffer);
}

static void collect_retired_buffers(struct sweetbg_surface *surface) {
	struct sweetbg_buffer **cursor = &surface->retired_buffers;
	while (*cursor != NULL) {
		struct sweetbg_buffer *buffer = *cursor;
		if (!buffer->released) {
			cursor = &buffer->next;
			continue;
		}
		*cursor = buffer->next;
		free_buffer(buffer);
	}
}

static void retire_buffer(struct sweetbg_surface *surface) {
	collect_retired_buffers(surface);
	if (surface->buffer == NULL) {
		return;
	}
	if (surface->buffer->released) {
		free_buffer(surface->buffer);
	} else {
		surface->buffer->next = surface->retired_buffers;
		surface->retired_buffers = surface->buffer;
	}
	surface->buffer = NULL;
}

void sweetbg_surface_collect(struct sweetbg_surface *surface) {
	collect_retired_buffers(surface);
	if (surface->buffer != NULL && surface->buffer->released) {
		free_buffer(surface->buffer);
		surface->buffer = NULL;
	}
}

void sweetbg_surface_destroy(struct sweetbg_surface *surface) {
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
	free_buffer(surface->buffer);
	surface->buffer = NULL;
	while (surface->retired_buffers != NULL) {
		struct sweetbg_buffer *buffer = surface->retired_buffers;
		surface->retired_buffers = buffer->next;
		free_buffer(buffer);
	}
	surface->configured = false;
}
