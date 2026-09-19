#include "ipc/prepared.h"

#include <sys/mman.h>
#include <unistd.h>

#define MAX_PREPARE_DIMENSION 32768u

int sweetbg_prepared_buffer_create(const struct sweetbg_image *image,
	enum sweetbg_fit fit, uint32_t width, uint32_t height, uint32_t color,
	const struct sweetbg_placement *placement) {
	if (width == 0 || height == 0 || width > MAX_PREPARE_DIMENSION ||
		height > MAX_PREPARE_DIMENSION) {
		return -1;
	}
	size_t stride = (size_t)width * 4;
	size_t size = stride * height;

	int fd = memfd_create("sweetbg-wallpaper", MFD_CLOEXEC);
	if (fd < 0) {
		return -1;
	}
	if (ftruncate(fd, (off_t)size) < 0) {
		close(fd);
		return -1;
	}
	void *data =
		mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (data == MAP_FAILED) {
		close(fd);
		return -1;
	}
	bool ok = placement != NULL ? sweetbg_image_render_placement(image,
					      placement, width, height, data)
				    : sweetbg_image_render(image, fit, width,
					      height, color, data);
	munmap(data, size);
	if (!ok) {
		close(fd);
		return -1;
	}
	return fd;
}
