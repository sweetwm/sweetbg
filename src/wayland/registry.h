#ifndef CARAMEL_WAYLAND_REGISTRY_H
#define CARAMEL_WAYLAND_REGISTRY_H

#include <stdbool.h>
#include <wayland-client.h>

struct zwlr_layer_shell_v1;

struct caramel_registry {
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wl_list outputs;
};

bool caramel_registry_init(
	struct caramel_registry *reg, struct wl_display *display);

// Destroy bound globals and tracked outputs. Safe after a partial init
void caramel_registry_finish(struct caramel_registry *reg);

#endif
