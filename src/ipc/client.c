#include "ipc/client.h"

#include <errno.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "image/image.h"
#include "image/palette.h"
#include "ipc/prepared.h"
#include "ipc/protocol.h"

#define REPLY_TIMEOUT_MS 5000

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
	return fd;
}

int sweetbg_client_request(
	uint8_t command, const void *payload, uint32_t payload_len) {
	uint8_t type;
	uint8_t response[SWEETBG_IPC_MAX_PAYLOAD];
	uint32_t len;

	if (sweetbg_client_raw_request(command, payload, payload_len, -1, &type,
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
	uint32_t payload_len, int pass_fd, uint8_t *type, void *response,
	uint32_t *len, uint32_t max, char *err, size_t err_size) {
	int fd = connect_to_daemon(err, err_size);
	if (fd < 0) {
		return -1;
	}

	if (!sweetbg_ipc_send_frame(
		    fd, command, payload, payload_len, pass_fd)) {
		client_error(err, err_size, "failed to send request");
		close(fd);
		return -1;
	}

	int reply_fd = -1;
	bool got = sweetbg_ipc_recv_frame(
		fd, type, response, len, max, &reply_fd, REPLY_TIMEOUT_MS);
	close(fd);
	// daemon never replies with an fd, drop one rather than leak it
	if (reply_fd >= 0) {
		close(reply_fd);
	}
	if (!got) {
		client_error(err, err_size, "no valid response from daemon");
		return -1;
	}
	return 0;
}

// send one request and require an OK reply, failures are printed
static bool request_ok(uint8_t command, const void *payload,
	uint32_t payload_len, int pass_fd,
	uint8_t response[SWEETBG_IPC_MAX_PAYLOAD], uint32_t *len) {
	uint8_t type;
	if (sweetbg_client_raw_request(command, payload, payload_len, pass_fd,
		    &type, response, len, SWEETBG_IPC_MAX_PAYLOAD, NULL,
		    0) != 0) {
		return false;
	}
	if (type != SWEETBG_STATUS_OK) {
		fprintf(stderr, "sweetbg: %.*s\n", (int)*len, response);
		return false;
	}
	return true;
}

static int query_outputs(struct sweetbg_output_info *list, int max,
	uint32_t *color, bool *color_auto) {
	*color = 0;
	*color_auto = false;
	uint8_t resp[SWEETBG_IPC_MAX_PAYLOAD];
	uint32_t len;
	if (!request_ok(SWEETBG_CMD_QUERY_OUTPUTS, NULL, 0, -1, resp, &len)) {
		return -1;
	}

	char text[SWEETBG_IPC_MAX_PAYLOAD + 1];
	memcpy(text, resp, len);
	text[len] = '\0';

	// daemon formats these from int32/uint32 fields, so sscanf never
	// sees an out-of-range value
	int count = 0;
	char *save = NULL;
	for (char *line = strtok_r(text, "\n", &save);
		line != NULL && count < max;
		line = strtok_r(NULL, "\n", &save)) {
		unsigned auto_flag;
		// meta: default fit (unused here), colour, auto flag
		// NOLINTNEXTLINE(bugprone-unchecked-string-to-number-conversion)
		if (sscanf(line, "meta %*u %" SCNu32 " %u", color,
			    &auto_flag) == 2) {
			*color_auto = auto_flag != 0;
			continue;
		}
		// name, buffer w h, scale, fit, logical x y w h, generation
		struct sweetbg_output_info *out = &list[count];
		unsigned fit;
		// NOLINTNEXTLINE(bugprone-unchecked-string-to-number-conversion)
		if (sscanf(line,
			    "%63s %" SCNu32 " %" SCNu32 " %" SCNd32
			    " %u %" SCNd32 " %" SCNd32 " %" SCNu32 " %" SCNu32
			    " %" SCNu32,
			    out->name, &out->width, &out->height, &out->scale,
			    &fit, &out->logical.x, &out->logical.y,
			    &out->logical.w, &out->logical.h,
			    &out->generation) != 10 ||
			fit > SWEETBG_FIT_SPAN) {
			continue;
		}
		out->fit = (enum sweetbg_fit)fit;
		count++;
	}
	return count;
}

static int send_prepared(const struct sweetbg_output_info *out, uint32_t mode,
	const char *path, int memfd, const uint32_t *colors,
	size_t color_count) {
	size_t name_len = strlen(out->name);
	size_t path_len = strlen(path);
	size_t total = 20 + name_len + 4 + path_len + 4 + color_count * 4 + 4;
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
	sweetbg_put_u32(payload + off, out->generation);
	off += 4;

	uint8_t resp[SWEETBG_IPC_MAX_PAYLOAD];
	uint32_t len;
	return request_ok(SWEETBG_CMD_IMG_PREPARED, payload, (uint32_t)off,
		       memfd, resp, &len)
		       ? 0
		       : 1;
}

static bool output_requested(const char *name, const char *output,
	const char *const *names, size_t name_count) {
	if (names == NULL) {
		return output == NULL || strcmp(name, output) == 0;
	}
	for (size_t i = 0; i < name_count; i++) {
		if (strcmp(name, names[i]) == 0) {
			return true;
		}
	}
	return false;
}

static bool output_skipped(
	const char *name, const char *const *skip_names, size_t skip_count) {
	for (size_t i = 0; i < skip_count; i++) {
		if (strcmp(name, skip_names[i]) == 0) {
			return true;
		}
	}
	return false;
}

static int prepare_outputs(const char *path, const char *output,
	const char *const *names, size_t name_count,
	const char *const *skip_names, size_t skip_count, uint32_t mode) {
	struct sweetbg_output_info outputs[SWEETBG_MAX_OUTPUTS];
	uint32_t color;
	bool color_auto;
	int count = query_outputs(
		outputs, SWEETBG_MAX_OUTPUTS, &color, &color_auto);
	if (count < 0) {
		return 1;
	}
	if (count == 0 && mode != SWEETBG_IMG_REPAINT) {
		fprintf(stderr, "sweetbg: daemon has no configured outputs\n");
		return 1;
	}
	bool selected_outputs[SWEETBG_MAX_OUTPUTS] = {false};
	int selected = 0;
	for (int i = 0; i < count; i++) {
		selected_outputs[i] = output_requested(outputs[i].name, output,
					      names, name_count) &&
				      !output_skipped(outputs[i].name,
					      skip_names, skip_count);
		selected += selected_outputs[i];
	}
	// A DEFAULT frame also carries the state update. If every current
	// output has an override, use one as a carrier and replace it afterward
	int state_carrier =
		selected == 0 && mode == SWEETBG_IMG_DEFAULT && count > 0 ? 0
									  : -1;
	selected += state_carrier >= 0;
	if (state_carrier >= 0) {
		selected_outputs[state_carrier] = true;
	}
	if (selected == 0) {
		if (mode == SWEETBG_IMG_REPAINT) {
			// The requested outputs went away between spawn and now
			return 0;
		}
		fprintf(stderr, "sweetbg: no output named %s\n", output);
		return 1;
	}

	struct sweetbg_decode_target targets[SWEETBG_MAX_OUTPUTS];
	size_t target_count = 0;
	for (int i = 0; i < count; i++) {
		if (selected_outputs[i]) {
			sweetbg_prepared_decode_target(
				&targets[target_count++], outputs, count, i);
		}
	}
	struct sweetbg_image image;
	char err[128];
	if (!sweetbg_image_load(
		    &image, path, targets, target_count, err, sizeof(err))) {
		fprintf(stderr, "sweetbg: %s\n", err);
		return 1;
	}

	uint32_t colors[SWEETBG_MAX_PALETTE];
	size_t color_count;
	sweetbg_palette_extract(
		&image, colors, SWEETBG_MAX_PALETTE, &color_count);
	// auto letterboxes with the image's dominant colour; the palette is
	// already computed, so this costs nothing extra
	uint32_t fill = color_auto && color_count > 0 ? colors[0] : color;

	struct sweetbg_prepared_buffer buffers[SWEETBG_MAX_OUTPUTS];
	size_t buffer_count = 0;
	int rc = 0;
	int applied = 0;
	for (int i = 0; i < count; i++) {
		if (!selected_outputs[i]) {
			continue;
		}
		int memfd = sweetbg_prepared_buffer_find(buffers, buffer_count,
			outputs[i].width, outputs[i].height, outputs[i].fit);
		bool reusable = memfd >= 0;
		if (!reusable) {
			memfd = sweetbg_prepared_buffer_for_output(
				&image, outputs, count, i, fill);
		}
		if (memfd < 0) {
			fprintf(stderr, "sweetbg: failed to prepare %s\n",
				outputs[i].name);
			rc = 1;
			continue;
		}
		if (!reusable && outputs[i].fit != SWEETBG_FIT_SPAN) {
			buffers[buffer_count++] =
				(struct sweetbg_prepared_buffer){
					outputs[i].width, outputs[i].height,
					outputs[i].fit, memfd};
		}
		if (send_prepared(&outputs[i], mode, path, memfd, colors,
			    color_count) != 0) {
			rc = 1;
		} else {
			applied++;
		}
		if (outputs[i].fit == SWEETBG_FIT_SPAN) {
			close(memfd);
		}
	}
	for (size_t i = 0; i < buffer_count; i++) {
		close(buffers[i].fd);
	}
	sweetbg_image_free(&image);

	if (mode != SWEETBG_IMG_REPAINT && applied > 0) {
		printf("applied %s%s%s\n", path, output != NULL ? " to " : "",
			output != NULL ? output : "");
	}
	return rc;
}

int sweetbg_client_set_image(const char *path, const char *output,
	const char *const *skip_names, size_t skip_count) {
	uint32_t mode =
		output == NULL ? SWEETBG_IMG_DEFAULT : SWEETBG_IMG_OVERRIDE;
	return prepare_outputs(
		path, output, NULL, 0, skip_names, skip_count, mode);
}

int sweetbg_client_prepare_outputs(
	const char *path, const char *const *names, size_t name_count) {
	return prepare_outputs(
		path, NULL, names, name_count, NULL, 0, SWEETBG_IMG_REPAINT);
}
