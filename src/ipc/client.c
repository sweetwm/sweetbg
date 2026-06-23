#include "ipc/client.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "image/image.h"
#include "ipc/protocol.h"

#define MAX_OUTPUTS 64
#define MAX_PREPARE_DIMENSION 32768u

#define REPLY_TIMEOUT_SECONDS 5

static int connect_to_daemon(void) {
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (!caramel_ipc_socket_path(addr.sun_path, sizeof(addr.sun_path))) {
		fprintf(stderr, "caramel: XDG_RUNTIME_DIR is unset or the "
				"socket path is too long\n");
		return -1;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		fprintf(stderr, "caramel: cannot create socket: %s\n",
			strerror(errno));
		return -1;
	}

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		if (errno == ENOENT || errno == ECONNREFUSED) {
			fprintf(stderr, "caramel: no daemon is running\n");
		} else {
			fprintf(stderr, "caramel: cannot connect to %s: %s\n",
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

int caramel_client_request(
	uint8_t command, const void *payload, uint32_t payload_len) {
	int fd = connect_to_daemon();
	if (fd < 0) {
		return 1;
	}

	if (!caramel_ipc_send_frame(fd, command, payload, payload_len)) {
		fprintf(stderr, "caramel: failed to send request\n");
		close(fd);
		return 1;
	}

	uint8_t type;
	uint8_t response[CARAMEL_IPC_MAX_PAYLOAD];
	uint32_t len;
	if (!caramel_ipc_recv_frame(
		    fd, &type, response, &len, sizeof(response))) {
		fprintf(stderr, "caramel: no valid response from daemon\n");
		close(fd);
		return 1;
	}
	close(fd);

	bool ok = type == CARAMEL_STATUS_OK;
	if (len > 0) {
		fprintf(ok ? stdout : stderr, "%.*s\n", (int)len, response);
	}
	return ok ? 0 : 1;
}

struct output_info {
	char name[64];
	uint32_t width;
	uint32_t height;
	int32_t scale;
};

static int query_outputs(struct output_info *list, int max) {
	int fd = connect_to_daemon();
	if (fd < 0) {
		return -1;
	}
	if (!caramel_ipc_send_frame(fd, CARAMEL_CMD_QUERY_OUTPUTS, NULL, 0)) {
		close(fd);
		fprintf(stderr, "caramel: failed to query outputs\n");
		return -1;
	}

	uint8_t type;
	uint8_t resp[CARAMEL_IPC_MAX_PAYLOAD];
	uint32_t len;
	bool got = caramel_ipc_recv_frame(fd, &type, resp, &len, sizeof(resp));
	close(fd);
	if (!got) {
		fprintf(stderr, "caramel: no response from daemon\n");
		return -1;
	}
	if (type != CARAMEL_STATUS_OK) {
		fprintf(stderr, "caramel: %.*s\n", (int)len, resp);
		return -1;
	}

	char text[CARAMEL_IPC_MAX_PAYLOAD + 1];
	memcpy(text, resp, len);
	text[len] = '\0';

	int count = 0;
	char *line_save = NULL;
	for (char *line = strtok_r(text, "\n", &line_save);
		line != NULL && count < max;
		line = strtok_r(NULL, "\n", &line_save)) {
		char *field_save = NULL;
		const char *name = strtok_r(line, " ", &field_save);
		const char *ws = strtok_r(NULL, " ", &field_save);
		const char *hs = strtok_r(NULL, " ", &field_save);
		const char *ss = strtok_r(NULL, " ", &field_save);
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
		count++;
	}
	return count;
}

static int prepare_memfd(
	const struct caramel_image *image, uint32_t width, uint32_t height) {
	if (width == 0 || height == 0 || width > MAX_PREPARE_DIMENSION ||
		height > MAX_PREPARE_DIMENSION) {
		return -1;
	}
	size_t stride = (size_t)width * 4;
	size_t size = stride * height;

	int fd = memfd_create("caramel-wallpaper", MFD_CLOEXEC);
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
	bool ok = caramel_image_render_cover(image, width, height, data);
	munmap(data, size);
	if (!ok) {
		close(fd);
		return -1;
	}
	return fd;
}

static int send_prepared(const struct output_info *out, bool is_default,
	const char *path, int memfd) {
	size_t name_len = strlen(out->name);
	size_t path_len = strlen(path);
	size_t total = 20 + name_len + 4 + path_len;
	if (total > CARAMEL_IPC_MAX_PAYLOAD) {
		fprintf(stderr, "caramel: image request too long\n");
		return 1;
	}

	uint8_t payload[CARAMEL_IPC_MAX_PAYLOAD];
	caramel_put_u32(payload, is_default ? 1 : 0);
	caramel_put_u32(payload + 4, (uint32_t)out->scale);
	caramel_put_u32(payload + 8, out->width);
	caramel_put_u32(payload + 12, out->height);
	caramel_put_u32(payload + 16, (uint32_t)name_len);
	// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
	memcpy(payload + 20, out->name, name_len);
	size_t off = 20 + name_len;
	caramel_put_u32(payload + off, (uint32_t)path_len);
	off += 4;
	// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
	memcpy(payload + off, path, path_len);
	off += path_len;

	int fd = connect_to_daemon();
	if (fd < 0) {
		return 1;
	}
	if (!caramel_ipc_send_frame_fd(fd, CARAMEL_CMD_IMG_PREPARED, payload,
		    (uint32_t)off, memfd)) {
		close(fd);
		fprintf(stderr, "caramel: failed to send image\n");
		return 1;
	}

	uint8_t type;
	uint8_t resp[CARAMEL_IPC_MAX_PAYLOAD];
	uint32_t len;
	bool got = caramel_ipc_recv_frame(fd, &type, resp, &len, sizeof(resp));
	close(fd);
	if (!got) {
		fprintf(stderr, "caramel: no response from daemon\n");
		return 1;
	}
	if (type != CARAMEL_STATUS_OK) {
		fprintf(stderr, "caramel: %.*s\n", (int)len, resp);
		return 1;
	}
	return 0;
}

int caramel_client_set_image(const char *path, const char *output) {
	struct output_info outputs[MAX_OUTPUTS];
	int count = query_outputs(outputs, MAX_OUTPUTS);
	if (count < 0) {
		return 1;
	}
	if (count == 0) {
		fprintf(stderr, "caramel: daemon has no configured outputs\n");
		return 1;
	}

	struct caramel_image image;
	char err[128];
	if (!caramel_image_load(&image, path, err, sizeof(err))) {
		fprintf(stderr, "caramel: %s\n", err);
		return 1;
	}

	bool is_default = output == NULL;
	int rc = 0;
	int applied = 0;
	for (int i = 0; i < count; i++) {
		if (output != NULL && strcmp(outputs[i].name, output) != 0) {
			continue;
		}
		int memfd = prepare_memfd(
			&image, outputs[i].width, outputs[i].height);
		if (memfd < 0) {
			fprintf(stderr, "caramel: failed to prepare %s\n",
				outputs[i].name);
			rc = 1;
			continue;
		}
		if (send_prepared(&outputs[i], is_default, path, memfd) != 0) {
			rc = 1;
		} else {
			applied++;
		}
		close(memfd);
	}
	caramel_image_free(&image);

	if (output != NULL && applied == 0 && rc == 0) {
		fprintf(stderr, "caramel: no output named %s\n", output);
		return 1;
	}
	if (applied > 0) {
		printf("applied %s%s%s\n", path, output != NULL ? " to " : "",
			output != NULL ? output : "");
	}
	return rc;
}
