#ifndef CARAMEL_WAYLAND_SHM_H
#define CARAMEL_WAYLAND_SHM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wl_buffer;
struct wl_shm;

// A single wl_shm buffer and its mapped pixels. Pixel format is XRGB8888:
// one little-endian 32-bit word per pixel, 0x00RRGGBB, opaque (X unused)
struct caramel_buffer {
	struct wl_buffer *wl_buffer;
	void *data;
	size_t size;
	uint32_t width;
	uint32_t height;
};

bool caramel_buffer_create(struct caramel_buffer *buffer, struct wl_shm *shm,
	uint32_t width, uint32_t height);

// Fill every pixel with one XRGB8888 color (0x00RRGGBB).
void caramel_buffer_fill(struct caramel_buffer *buffer, uint32_t color);

// Unmap the pixels and destroy the wl_buffer. Idempotent.
void caramel_buffer_destroy(struct caramel_buffer *buffer);

#endif
