#ifndef SWEETBG_WAYLAND_SHM_H
#define SWEETBG_WAYLAND_SHM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct wl_buffer;
struct wl_shm;

// An owned wl_buffer, optionally backed by mapped XRGB8888 shm pixels
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

// Take ownership of an existing wl_buffer and track its release
bool sweetbg_buffer_wrap(struct sweetbg_buffer *buffer,
	struct wl_buffer *wl_buffer, uint32_t width, uint32_t height);

// Fill every pixel with one XRGB8888 color (0x00RRGGBB)
void sweetbg_buffer_fill(struct sweetbg_buffer *buffer, uint32_t color);

void sweetbg_buffer_unmap(struct sweetbg_buffer *buffer);

// Unmap the pixels and destroy the wl_buffer. Idempotent
void sweetbg_buffer_destroy(struct sweetbg_buffer *buffer);

#endif
