#include "ipc/client.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "image/image.h"
#include "image/layout.h"
#include "image/palette.h"
#include "ipc/protocol.h"

#define MAX_OUTPUTS 64
#define MAX_PREPARE_DIMENSION 32768u

#define REPLY_TIMEOUT_SECONDS 5

static void client_error(char *err, size_t err_size, const char *message) {
	if (err != NULL && err_size > 0) {
		snprintf(err, err_size, "%s", message);
	} else {
		fprintf(stderr, "sweetbg: %s\n", message);
	}
}

__attribute__((format(printf, 3, 4))) static void client_errorf(
	char *err, size_t err_size, const char *fmt, ...) {
	char message[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	client_error(err, err_size, message);
}

static int connect_to_daemon(char *err, size_t err_size) {
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (!sweetbg_ipc_socket_path(addr.sun_path, sizeof(addr.sun_path))) {
		client_error(err, err_size,
			"XDG_RUNTIME_DIR is unset or the socket path is too "
			"long");
		return -1;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		client_errorf(err, err_size, "cannot create socket: %s",
			strerror(errno));
		return -1;
	}

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		if (errno == ENOENT || errno == ECONNREFUSED) {
			client_error(err, err_size, "no daemon is running");
		} else {
			client_errorf(err, err_size, "cannot connect to %s: %s",
				addr.sun_path, strerror(errno));
		}
		close(fd);
		return -1;
	}

	struct timeval timeout = {
		.tv_sec = REPLY_TIMEOUT_SECONDS,
		.tv_usec = 0,
	};
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
	return fd;
}

int sweetbg_client_request(
	uint8_t command, const void *payload, uint32_t payload_len) {
	uint8_t type;
	uint8_t response[SWEETBG_IPC_MAX_PAYLOAD];
	uint32_t len;

	if (sweetbg_client_raw_request(command, payload, payload_len, &type,
		    response, &len, sizeof(response), NULL, 0) != 0) {
		return 1;
	}

	bool ok = type == SWEETBG_STATUS_OK;
	if (len > 0) {
		fprintf(ok ? stdout : stderr, "%.*s\n", (int)len, response);
	}
	return ok ? 0 : 1;
}

int sweetbg_client_raw_request(uint8_t command, const void *payload,
	uint32_t payload_len, uint8_t *type, void *response, uint32_t *len,
	uint32_t max, char *err, size_t err_size) {
	int fd = connect_to_daemon(err, err_size);
	if (fd < 0) {
		return -1;
	}

	if (!sweetbg_ipc_send_frame(fd, command, payload, payload_len)) {
		client_error(err, err_size, "failed to send request");
		close(fd);
		return -1;
	}

	if (!sweetbg_ipc_recv_frame(fd, type, response, len, max)) {
		client_error(err, err_size, "no valid response from daemon");
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

struct output_info {
	char name[64];
	uint32_t width;
	uint32_t height;
	int32_t scale;
	enum sweetbg_fit fit;
	struct sweetbg_layout_output logical;
};

static int query_outputs(struct output_info *list, int max,
	enum sweetbg_fit *fit, uint32_t *color) {
	*fit = SWEETBG_FIT_COVER;
	*color = 0;
	int fd = connect_to_daemon(NULL, 0);
	if (fd < 0) {
		return -1;
	}
	if (!sweetbg_ipc_send_frame(fd, SWEETBG_CMD_QUERY_OUTPUTS, NULL, 0)) {
		close(fd);
		fprintf(stderr, "sweetbg: failed to query outputs\n");
		return -1;
	}

	uint8_t type;
	uint8_t resp[SWEETBG_IPC_MAX_PAYLOAD];
	uint32_t len;
	bool got = sweetbg_ipc_recv_frame(fd, &type, resp, &len, sizeof(resp));
	close(fd);
	if (!got) {
		fprintf(stderr, "sweetbg: no response from daemon\n");
		return -1;
	}
	if (type != SWEETBG_STATUS_OK) {
		fprintf(stderr, "sweetbg: %.*s\n", (int)len, resp);
		return -1;
	}

	char text[SWEETBG_IPC_MAX_PAYLOAD + 1];
	memcpy(text, resp, len);
	text[len] = '\0';

	int count = 0;
	char *line_save = NULL;
	for (char *line = strtok_r(text, "\n", &line_save);
		line != NULL && count < max;
		line = strtok_r(NULL, "\n", &line_save)) {
		char *field_save = NULL;
		const char *name = strtok_r(line, " ", &field_save);
		if (name != NULL && strcmp(name, "meta") == 0) {
			const char *fs = strtok_r(NULL, " ", &field_save);
			const char *cs = strtok_r(NULL, " ", &field_save);
			if (fs != NULL && cs != NULL) {
				*fit = (enum sweetbg_fit)strtoul(fs, NULL, 10);
				*color = (uint32_t)strtoul(cs, NULL, 10);
			}
			continue;
		}
		const char *ws = strtok_r(NULL, " ", &field_save);
		const char *hs = strtok_r(NULL, " ", &field_save);
		const char *ss = strtok_r(NULL, " ", &field_save);
		const char *fs = strtok_r(NULL, " ", &field_save);
		const char *lxs = strtok_r(NULL, " ", &field_save);
		const char *lys = strtok_r(NULL, " ", &field_save);
		const char *lws = strtok_r(NULL, " ", &field_save);
		const char *lhs = strtok_r(NULL, " ", &field_save);
		if (name == NULL || ws == NULL || hs == NULL || ss == NULL ||
			strlen(name) >= sizeof(list[count].name)) {
			continue;
		}

		char *end_w;
		char *end_h;
		char *end_s;
		unsigned long w = strtoul(ws, &end_w, 10);
		unsigned long h = strtoul(hs, &end_h, 10);
		unsigned long s = strtoul(ss, &end_s, 10);
		if (*end_w != '\0' || *end_h != '\0' || *end_s != '\0') {
			continue;
		}

		memcpy(list[count].name, name, strlen(name) + 1);
		list[count].width = (uint32_t)w;
		list[count].height = (uint32_t)h;
		list[count].scale = (int32_t)s;
		list[count].fit = *fit;
		list[count].logical =
			(struct sweetbg_layout_output){0, 0, 0, 0};
		if (lxs != NULL && lys != NULL && lws != NULL && lhs != NULL) {
			char *end_lx;
			char *end_ly;
			char *end_lw;
			char *end_lh;
			long lx = strtol(lxs, &end_lx, 10);
			long ly = strtol(lys, &end_ly, 10);
			unsigned long lw = strtoul(lws, &end_lw, 10);
			unsigned long lh = strtoul(lhs, &end_lh, 10);
			if (*end_lx == '\0' && *end_ly == '\0' &&
				*end_lw == '\0' && *end_lh == '\0' &&
				lx >= INT32_MIN && lx <= INT32_MAX &&
				ly >= INT32_MIN && ly <= INT32_MAX) {
				list[count].logical.x = (int32_t)lx;
				list[count].logical.y = (int32_t)ly;
				list[count].logical.w = (uint32_t)lw;
				list[count].logical.h = (uint32_t)lh;
			}
		}
		if (fs != NULL) {
			char *end_f;
			unsigned long f = strtoul(fs, &end_f, 10);
			if (*end_f == '\0' && f <= SWEETBG_FIT_SPAN) {
				list[count].fit = (enum sweetbg_fit)f;
			}
		}
		count++;
	}
	return count;
}

static bool span_placement(const struct sweetbg_image *image,
	const struct output_info *list, int count, int index, uint32_t width,
	uint32_t height, struct sweetbg_placement *out) {
	struct sweetbg_layout_output boxes[MAX_OUTPUTS];
	for (int i = 0; i < count; i++) {
		boxes[i] = list[i].logical;
	}

	uint32_t layout_w;
	uint32_t layout_h;
	struct sweetbg_rect slice;
	if (!sweetbg_layout_slice(boxes, (size_t)count, (size_t)index,
		    &layout_w, &layout_h, &slice)) {
		return false;
	}

	sweetbg_span_rects(image->width, image->height, layout_w, layout_h,
		&slice, width, height, out);
	return true;
}

static int prepare_memfd(const struct sweetbg_image *image,
	const struct output_info *list, int count, int index, uint32_t color) {
	uint32_t width = list[index].width;
	uint32_t height = list[index].height;
	enum sweetbg_fit fit = list[index].fit;
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
	struct sweetbg_placement place;
	bool ok;
	if (fit == SWEETBG_FIT_SPAN && span_placement(image, list, count, index,
					       width, height, &place)) {
		ok = sweetbg_image_render_placement(
			image, &place, width, height, data);
	} else {
		// Also the path when span has no usable layout yet
		ok = sweetbg_image_render(image,
			fit == SWEETBG_FIT_SPAN ? SWEETBG_FIT_COVER : fit,
			width, height, color, data);
	}
	munmap(data, size);
	if (!ok) {
		close(fd);
		return -1;
	}
	return fd;
}

static int send_prepared(const struct output_info *out, uint32_t mode,
	const char *path, int memfd, const uint32_t *colors,
	size_t color_count) {
	size_t name_len = strlen(out->name);
	size_t path_len = strlen(path);
	size_t total = 20 + name_len + 4 + path_len + 4 + color_count * 4;
	if (total > SWEETBG_IPC_MAX_PAYLOAD) {
		fprintf(stderr, "sweetbg: image request too long\n");
		return 1;
	}

	uint8_t payload[SWEETBG_IPC_MAX_PAYLOAD];
	sweetbg_put_u32(payload, mode);
	sweetbg_put_u32(payload + 4, (uint32_t)out->scale);
	sweetbg_put_u32(payload + 8, out->width);
	sweetbg_put_u32(payload + 12, out->height);
	sweetbg_put_u32(payload + 16, (uint32_t)name_len);
	// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
	memcpy(payload + 20, out->name, name_len);
	size_t off = 20 + name_len;
	sweetbg_put_u32(payload + off, (uint32_t)path_len);
	off += 4;
	// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
	memcpy(payload + off, path, path_len);
	off += path_len;
	sweetbg_put_u32(payload + off, (uint32_t)color_count);
	off += 4;
	for (size_t i = 0; i < color_count; i++) {
		sweetbg_put_u32(payload + off, colors[i]);
		off += 4;
	}

	int fd = connect_to_daemon(NULL, 0);
	if (fd < 0) {
		return 1;
	}
	if (!sweetbg_ipc_send_frame_fd(fd, SWEETBG_CMD_IMG_PREPARED, payload,
		    (uint32_t)off, memfd)) {
		close(fd);
		fprintf(stderr, "sweetbg: failed to send image\n");
		return 1;
	}

	uint8_t type;
	uint8_t resp[SWEETBG_IPC_MAX_PAYLOAD];
	uint32_t len;
	bool got = sweetbg_ipc_recv_frame(fd, &type, resp, &len, sizeof(resp));
	close(fd);
	if (!got) {
		fprintf(stderr, "sweetbg: no response from daemon\n");
		return 1;
	}
	if (type != SWEETBG_STATUS_OK) {
		fprintf(stderr, "sweetbg: %.*s\n", (int)len, resp);
		return 1;
	}
	return 0;
}

int sweetbg_client_set_image(const char *path, const char *output) {
	struct output_info outputs[MAX_OUTPUTS];
	enum sweetbg_fit fit;
	uint32_t color;
	int count = query_outputs(outputs, MAX_OUTPUTS, &fit, &color);
	if (count < 0) {
		return 1;
	}
	if (count == 0) {
		fprintf(stderr, "sweetbg: daemon has no configured outputs\n");
		return 1;
	}

	struct sweetbg_image image;
	char err[128];
	if (!sweetbg_image_load(&image, path, err, sizeof(err))) {
		fprintf(stderr, "sweetbg: %s\n", err);
		return 1;
	}

	uint32_t colors[SWEETBG_MAX_PALETTE];
	size_t color_count;
	sweetbg_palette_extract(
		&image, colors, SWEETBG_MAX_PALETTE, &color_count);

	uint32_t mode =
		output == NULL ? SWEETBG_IMG_DEFAULT : SWEETBG_IMG_OVERRIDE;
	int rc = 0;
	int applied = 0;
	for (int i = 0; i < count; i++) {
		if (output != NULL && strcmp(outputs[i].name, output) != 0) {
			continue;
		}
		int memfd = prepare_memfd(&image, outputs, count, i, color);
		if (memfd < 0) {
			fprintf(stderr, "sweetbg: failed to prepare %s\n",
				outputs[i].name);
			rc = 1;
			continue;
		}
		if (send_prepared(&outputs[i], mode, path, memfd, colors,
			    color_count) != 0) {
			rc = 1;
		} else {
			applied++;
		}
		close(memfd);
	}
	sweetbg_image_free(&image);

	if (output != NULL && applied == 0 && rc == 0) {
		fprintf(stderr, "sweetbg: no output named %s\n", output);
		return 1;
	}
	if (applied > 0) {
		printf("applied %s%s%s\n", path, output != NULL ? " to " : "",
			output != NULL ? output : "");
	}
	return rc;
}

int sweetbg_client_prepare_output(const char *name, const char *path) {
	struct output_info outputs[MAX_OUTPUTS];
	enum sweetbg_fit fit;
	uint32_t color;
	int count = query_outputs(outputs, MAX_OUTPUTS, &fit, &color);
	if (count < 0) {
		return 1;
	}

	const struct output_info *target = NULL;
	int index = -1;
	for (int i = 0; i < count; i++) {
		if (strcmp(outputs[i].name, name) == 0) {
			target = &outputs[i];
			index = i;
			break;
		}
	}
	if (target == NULL) {
		// The output went away between spawn and now; nothing to do
		return 0;
	}

	struct sweetbg_image image;
	char err[128];
	if (!sweetbg_image_load(&image, path, err, sizeof(err))) {
		fprintf(stderr, "sweetbg: %s\n", err);
		return 1;
	}

	uint32_t colors[SWEETBG_MAX_PALETTE];
	size_t color_count;
	sweetbg_palette_extract(
		&image, colors, SWEETBG_MAX_PALETTE, &color_count);

	int memfd = prepare_memfd(&image, outputs, count, index, color);
	int rc = 1;
	if (memfd >= 0) {
		rc = send_prepared(target, SWEETBG_IMG_REPAINT, path, memfd,
			colors, color_count);
		close(memfd);
	}
	sweetbg_image_free(&image);
	return rc;
}
