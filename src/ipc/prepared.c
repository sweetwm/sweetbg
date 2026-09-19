#include "ipc/prepared.h"

#include <string.h>
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

int sweetbg_prepared_buffer_find(const struct sweetbg_prepared_buffer *buffers,
	size_t count, uint32_t width, uint32_t height, enum sweetbg_fit fit) {
	if (fit == SWEETBG_FIT_SPAN) {
		return -1;
	}
	for (size_t i = 0; i < count; i++) {
		if (buffers[i].width == width && buffers[i].height == height &&
			buffers[i].fit == fit) {
			return buffers[i].fd;
		}
	}
	return -1;
}

static bool span_layout(const struct sweetbg_output_info *outputs, int count,
	int index, uint32_t *layout_width, uint32_t *layout_height,
	struct sweetbg_rect *slice) {
	if (count < 1 || count > SWEETBG_MAX_OUTPUTS || index < 0 ||
		index >= count) {
		return false;
	}
	struct sweetbg_layout_output boxes[SWEETBG_MAX_OUTPUTS];
	for (int i = 0; i < count; i++) {
		boxes[i] = outputs[i].logical;
	}
	return sweetbg_layout_slice(boxes, (size_t)count, (size_t)index,
		layout_width, layout_height, slice);
}

static bool span_placement(const struct sweetbg_image *image,
	const struct sweetbg_output_info *outputs, int count, int index,
	struct sweetbg_placement *out) {
	uint32_t layout_width;
	uint32_t layout_height;
	struct sweetbg_rect slice;
	if (!span_layout(outputs, count, index, &layout_width, &layout_height,
		    &slice)) {
		return false;
	}
	sweetbg_span_rects(image->width, image->height, layout_width,
		layout_height, &slice, outputs[index].width,
		outputs[index].height, out);
	return true;
}

int sweetbg_prepared_buffer_for_output(const struct sweetbg_image *image,
	const struct sweetbg_output_info *outputs, int output_count, int index,
	uint32_t color) {
	struct sweetbg_placement place;
	const struct sweetbg_placement *placement = NULL;
	enum sweetbg_fit fit = outputs[index].fit;
	if (fit == SWEETBG_FIT_SPAN &&
		span_placement(image, outputs, output_count, index, &place)) {
		placement = &place;
	} else if (fit == SWEETBG_FIT_SPAN) {
		fit = SWEETBG_FIT_COVER;
	}
	return sweetbg_prepared_buffer_create(image, fit, outputs[index].width,
		outputs[index].height, color, placement);
}

void sweetbg_prepared_decode_target(struct sweetbg_decode_target *target,
	const struct sweetbg_output_info *outputs, int output_count,
	int index) {
	memset(target, 0, sizeof(*target));
	target->fit = outputs[index].fit;
	target->width = outputs[index].width;
	target->height = outputs[index].height;
	if (target->fit == SWEETBG_FIT_SPAN) {
		(void)span_layout(outputs, output_count, index,
			&target->layout_width, &target->layout_height,
			&target->slice);
	}
}
