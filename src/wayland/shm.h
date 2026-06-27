#ifndef MANJU_WAYLAND_SHM_H
#define MANJU_WAYLAND_SHM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wl_buffer;
struct wl_shm;

// A single wl_shm buffer and its mapped pixels. Pixel format is XRGB8888:
// one little-endian 32-bit word per pixel, 0x00RRGGBB, opaque (X unused)
struct manju_buffer {
	struct wl_buffer *wl_buffer;
	void *data;
	size_t size;
	uint32_t width;
	uint32_t height;
};

bool manju_buffer_create(struct manju_buffer *buffer, struct wl_shm *shm,
	uint32_t width, uint32_t height);

bool manju_buffer_from_fd(struct manju_buffer *buffer, struct wl_shm *shm,
	int fd, uint32_t width, uint32_t height);

// Fill every pixel with one XRGB8888 color (0x00RRGGBB)
void manju_buffer_fill(struct manju_buffer *buffer, uint32_t color);

// Unmap the pixels and destroy the wl_buffer. Idempotent
void manju_buffer_destroy(struct manju_buffer *buffer);

#endif
