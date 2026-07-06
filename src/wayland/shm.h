#ifndef SWEETBG_WAYLAND_SHM_H
#define SWEETBG_WAYLAND_SHM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wl_buffer;
struct wl_shm;

// A single wl_shm buffer and its mapped pixels. Pixel format is XRGB8888:
// one little-endian 32-bit word per pixel, 0x00RRGGBB, opaque (X unused)
struct sweetbg_buffer {
	struct wl_buffer *wl_buffer;
	void *data;
	size_t size;
	uint32_t width;
	uint32_t height;
	bool released;
	struct sweetbg_buffer *next;
};

bool sweetbg_buffer_create(struct sweetbg_buffer *buffer, struct wl_shm *shm,
	uint32_t width, uint32_t height);

bool sweetbg_buffer_from_fd(struct sweetbg_buffer *buffer, struct wl_shm *shm,
	int fd, uint32_t width, uint32_t height);

// Fill every pixel with one XRGB8888 color (0x00RRGGBB)
void sweetbg_buffer_fill(struct sweetbg_buffer *buffer, uint32_t color);

// Unmap the pixels and destroy the wl_buffer. Idempotent
void sweetbg_buffer_destroy(struct sweetbg_buffer *buffer);

#endif
