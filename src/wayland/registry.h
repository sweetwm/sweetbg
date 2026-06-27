#ifndef MANJU_WAYLAND_REGISTRY_H
#define MANJU_WAYLAND_REGISTRY_H

#include <stdbool.h>
#include <wayland-client.h>

struct zwlr_layer_shell_v1;
struct wp_viewporter;
struct wp_fractional_scale_manager_v1;

struct manju_registry {
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct wp_viewporter *viewporter;
	struct wp_fractional_scale_manager_v1 *fractional_scale_manager;
	struct wl_list outputs;
};

bool manju_registry_init(
	struct manju_registry *reg, struct wl_display *display);

// Destroy bound globals and tracked outputs. Safe after a partial init
void manju_registry_finish(struct manju_registry *reg);

#endif
