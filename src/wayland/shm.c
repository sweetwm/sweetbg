#include "wayland/shm.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wayland-client.h>

#define BYTES_PER_PIXEL 4

static void handle_release(void *data, struct wl_buffer *wl_buffer) {
	struct sweetbg_buffer *buffer = data;
	(void)wl_buffer;

	buffer->released = true;
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

bool sweetbg_buffer_create(struct sweetbg_buffer *buffer, struct wl_shm *shm,
	uint32_t width, uint32_t height) {
	buffer->wl_buffer = NULL;
	buffer->data = NULL;
	buffer->size = 0;
	buffer->width = width;
	buffer->height = height;
	buffer->released = false;
	buffer->next = NULL;

	uint32_t stride;
	size_t size;
	if (!buffer_size(width, height, &stride, &size)) {
		return false;
	}

	int fd = memfd_create("sweetbg-shm", MFD_CLOEXEC);
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

bool sweetbg_buffer_from_fd(struct sweetbg_buffer *buffer, struct wl_shm *shm,
	int fd, uint32_t width, uint32_t height) {
	buffer->wl_buffer = NULL;
	buffer->data = NULL;
	buffer->size = 0;
	buffer->width = width;
	buffer->height = height;
	buffer->released = false;
	buffer->next = NULL;

	uint32_t stride;
	size_t size;
	if (!buffer_size(width, height, &stride, &size)) {
		return false;
	}

	struct stat info;
	if (fstat(fd, &info) != 0 || info.st_size < 0 ||
		(size_t)info.st_size < size) {
		return false;
	}

	struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, (int32_t)size);
	if (pool == NULL) {
		return false;
	}
	buffer->wl_buffer = wl_shm_pool_create_buffer(pool, 0, (int32_t)width,
		(int32_t)height, (int32_t)stride, WL_SHM_FORMAT_XRGB8888);
	wl_shm_pool_destroy(pool);
	if (buffer->wl_buffer == NULL) {
		return false;
	}

	wl_buffer_add_listener(buffer->wl_buffer, &buffer_listener, buffer);
	buffer->size = size;
	return true;
}

void sweetbg_buffer_fill(struct sweetbg_buffer *buffer, uint32_t color) {
	uint32_t *pixels = buffer->data;
	size_t count = buffer->size / BYTES_PER_PIXEL;
	for (size_t i = 0; i < count; i++) {
		pixels[i] = color;
	}
}

void sweetbg_buffer_unmap(struct sweetbg_buffer *buffer) {
	if (buffer->data != NULL) {
		munmap(buffer->data, buffer->size);
		buffer->data = NULL;
	}
}

void sweetbg_buffer_destroy(struct sweetbg_buffer *buffer) {
	if (buffer->wl_buffer != NULL) {
		wl_buffer_destroy(buffer->wl_buffer);
		buffer->wl_buffer = NULL;
	}
	if (buffer->data != NULL) {
		munmap(buffer->data, buffer->size);
		buffer->data = NULL;
	}
	buffer->size = 0;
	buffer->released = true;
}
