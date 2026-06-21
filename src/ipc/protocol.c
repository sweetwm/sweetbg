#include "ipc/protocol.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

bool caramel_ipc_socket_path(char *out, size_t out_size) {
	const char *dir = getenv("XDG_RUNTIME_DIR");
	if (dir == NULL || dir[0] == '\0') {
		return false;
	}
	int n = snprintf(out, out_size, "%s/caramel.sock", dir);
	if (n < 0 || (size_t)n >= out_size) {
		return false;
	}
	return true;
}

bool caramel_ipc_read_full(int fd, void *buf, size_t n) {
	uint8_t *p = buf;
	size_t got = 0;
	while (got < n) {
		ssize_t r = recv(fd, p + got, n - got, 0);
		if (r < 0) {
			if (errno == EINTR) {
				continue;
			}
			return false;
		}
		if (r == 0) {
			return false;
		}
		got += (size_t)r;
	}
	return true;
}

bool caramel_ipc_write_full(int fd, const void *buf, size_t n) {
	const uint8_t *p = buf;
	size_t sent = 0;
	while (sent < n) {
		ssize_t w = send(fd, p + sent, n - sent, MSG_NOSIGNAL);
		if (w < 0) {
			if (errno == EINTR) {
				continue;
			}
			return false;
		}
		sent += (size_t)w;
	}
	return true;
}

bool caramel_ipc_send_frame(
	int fd, uint8_t type, const void *payload, uint32_t len) {
	if (len > CARAMEL_IPC_MAX_PAYLOAD) {
		return false;
	}

	uint8_t header[CARAMEL_IPC_HEADER_SIZE];
	header[0] = CARAMEL_IPC_VERSION;
	header[1] = type;
	header[2] = 0;
	header[3] = 0;
	header[4] = (uint8_t)(len & 0xff);
	header[5] = (uint8_t)((len >> 8) & 0xff);
	header[6] = (uint8_t)((len >> 16) & 0xff);
	header[7] = (uint8_t)((len >> 24) & 0xff);

	if (!caramel_ipc_write_full(fd, header, sizeof(header))) {
		return false;
	}
	if (len > 0 && !caramel_ipc_write_full(fd, payload, len)) {
		return false;
	}
	return true;
}

bool caramel_ipc_recv_frame(
	int fd, uint8_t *type, void *payload, uint32_t *len, uint32_t max) {
	uint8_t header[CARAMEL_IPC_HEADER_SIZE];
	if (!caramel_ipc_read_full(fd, header, sizeof(header))) {
		return false;
	}
	if (header[0] != CARAMEL_IPC_VERSION) {
		return false;
	}

	uint32_t plen = (uint32_t)header[4] | ((uint32_t)header[5] << 8) |
			((uint32_t)header[6] << 16) |
			((uint32_t)header[7] << 24);
	if (plen > max || plen > CARAMEL_IPC_MAX_PAYLOAD) {
		return false;
	}
	if (plen > 0 && !caramel_ipc_read_full(fd, payload, plen)) {
		return false;
	}

	*type = header[1];
	*len = plen;
	return true;
}
