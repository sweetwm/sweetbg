#include "wayland/shm.h"

#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

#define BYTES_PER_PIXEL 4

static void handle_release(void *data, struct wl_buffer *wl_buffer) {
	struct caramel_buffer *buffer = data;
	(void)wl_buffer;

	caramel_buffer_destroy(buffer);
}

static const struct wl_buffer_listener buffer_listener = {
	.release = handle_release,
};

static bool buffer_size(uint32_t width, uint32_t height, uint32_t *stride_out,
	size_t *size_out) {
	if (width == 0 || height == 0) {
		return false;
	}
	if (width > (uint32_t)INT32_MAX / BYTES_PER_PIXEL) {
		return false;
	}
	uint32_t stride = width * BYTES_PER_PIXEL;
	if (height > (size_t)INT32_MAX / stride) {
		return false;
	}
	*stride_out = stride;
	*size_out = (size_t)stride * height;
	return true;
}

bool caramel_buffer_create(struct caramel_buffer *buffer, struct wl_shm *shm,
	uint32_t width, uint32_t height) {
	buffer->wl_buffer = NULL;
	buffer->data = NULL;
	buffer->size = 0;
	buffer->width = width;
	buffer->height = height;

	uint32_t stride;
	size_t size;
	if (!buffer_size(width, height, &stride, &size)) {
		return false;
	}

	int fd = memfd_create("caramel-shm", MFD_CLOEXEC);
	if (fd < 0) {
		return false;
	}
	if (ftruncate(fd, (off_t)size) < 0) {
		close(fd);
		return false;
	}

	void *data =
		mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return false;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)size);
	if (pool == NULL) {
		munmap(data, size);
		close(fd);
		return false;
	}

	buffer->wl_buffer = wl_shm_pool_create_buffer(pool, 0, (int32_t)width,
		(int32_t)height, (int32_t)stride, WL_SHM_FORMAT_XRGB8888);

	wl_shm_pool_destroy(pool);
	close(fd);

	if (buffer->wl_buffer == NULL) {
		munmap(data, size);
		return false;
	}

	wl_buffer_add_listener(buffer->wl_buffer, &buffer_listener, buffer);
	buffer->data = data;
	buffer->size = size;
	return true;
}

void caramel_buffer_fill(struct caramel_buffer *buffer, uint32_t color) {
	uint32_t *pixels = buffer->data;
	size_t count = buffer->size / BYTES_PER_PIXEL;
	for (size_t i = 0; i < count; i++) {
		pixels[i] = color;
	}
}

void caramel_buffer_destroy(struct caramel_buffer *buffer) {
	if (buffer->wl_buffer != NULL) {
		wl_buffer_destroy(buffer->wl_buffer);
		buffer->wl_buffer = NULL;
	}
	if (buffer->data != NULL) {
		munmap(buffer->data, buffer->size);
		buffer->data = NULL;
	}
	buffer->size = 0;
}
