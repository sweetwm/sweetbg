#ifndef CARAMEL_WAYLAND_REGISTRY_H
#define CARAMEL_WAYLAND_REGISTRY_H

#include <stdbool.h>
#include <stdint.h>

struct wl_display;
struct wl_registry;
struct wl_compositor;
struct wl_shm;
struct zwlr_layer_shell_v1;

struct caramel_registry {
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zwlr_layer_shell_v1 *layer_shell;
	uint32_t output_count;
};

bool caramel_registry_init(
	struct caramel_registry *reg, struct wl_display *display);

void caramel_registry_finish(struct caramel_registry *reg);

#endif
