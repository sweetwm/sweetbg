#include "ipc/protocol.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

void manju_put_u32(uint8_t *p, uint32_t value) {
	p[0] = (uint8_t)(value & 0xff);
	p[1] = (uint8_t)((value >> 8) & 0xff);
	p[2] = (uint8_t)((value >> 16) & 0xff);
	p[3] = (uint8_t)((value >> 24) & 0xff);
}

uint32_t manju_get_u32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

bool manju_ipc_socket_path(char *out, size_t out_size) {
	const char *dir = getenv("XDG_RUNTIME_DIR");
	if (dir == NULL || dir[0] == '\0') {
		return false;
	}
	int n = snprintf(out, out_size, "%s/manju.sock", dir);
	if (n < 0 || (size_t)n >= out_size) {
		return false;
	}
	return true;
}

bool manju_ipc_read_full(int fd, void *buf, size_t n) {
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

bool manju_ipc_write_full(int fd, const void *buf, size_t n) {
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

bool manju_ipc_send_frame(
	int fd, uint8_t type, const void *payload, uint32_t len) {
	if (len > MANJU_IPC_MAX_PAYLOAD) {
		return false;
	}

	uint8_t header[MANJU_IPC_HEADER_SIZE];
	header[0] = MANJU_IPC_VERSION;
	header[1] = type;
	header[2] = 0;
	header[3] = 0;
	header[4] = (uint8_t)(len & 0xff);
	header[5] = (uint8_t)((len >> 8) & 0xff);
	header[6] = (uint8_t)((len >> 16) & 0xff);
	header[7] = (uint8_t)((len >> 24) & 0xff);

	if (!manju_ipc_write_full(fd, header, sizeof(header))) {
		return false;
	}
	if (len > 0 && !manju_ipc_write_full(fd, payload, len)) {
		return false;
	}
	return true;
}

bool manju_ipc_recv_frame(
	int fd, uint8_t *type, void *payload, uint32_t *len, uint32_t max) {
	uint8_t header[MANJU_IPC_HEADER_SIZE];
	if (!manju_ipc_read_full(fd, header, sizeof(header))) {
		return false;
	}
	if (header[0] != MANJU_IPC_VERSION) {
		return false;
	}

	uint32_t plen = (uint32_t)header[4] | ((uint32_t)header[5] << 8) |
			((uint32_t)header[6] << 16) |
			((uint32_t)header[7] << 24);
	if (plen > max || plen > MANJU_IPC_MAX_PAYLOAD) {
		return false;
	}
	if (plen > 0 && !manju_ipc_read_full(fd, payload, plen)) {
		return false;
	}

	*type = header[1];
	*len = plen;
	return true;
}

static void fill_header(uint8_t *header, uint8_t type, uint32_t len) {
	header[0] = MANJU_IPC_VERSION;
	header[1] = type;
	header[2] = 0;
	header[3] = 0;
	header[4] = (uint8_t)(len & 0xff);
	header[5] = (uint8_t)((len >> 8) & 0xff);
	header[6] = (uint8_t)((len >> 16) & 0xff);
	header[7] = (uint8_t)((len >> 24) & 0xff);
}

bool manju_ipc_send_frame_fd(
	int fd, uint8_t type, const void *payload, uint32_t len, int pass_fd) {
	if (len > MANJU_IPC_MAX_PAYLOAD) {
		return false;
	}

	uint8_t buffer[MANJU_IPC_HEADER_SIZE + MANJU_IPC_MAX_PAYLOAD];
	fill_header(buffer, type, len);
	if (len > 0) {
		memcpy(buffer + MANJU_IPC_HEADER_SIZE, payload, len);
	}
	size_t total = (size_t)MANJU_IPC_HEADER_SIZE + len;

	struct iovec iov = {.iov_base = buffer, .iov_len = total};
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	union {
		char bytes[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} control;
	if (pass_fd >= 0) {
		memset(&control, 0, sizeof(control));
		msg.msg_control = control.bytes;
		msg.msg_controllen = sizeof(control.bytes);
		struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_RIGHTS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(cmsg), &pass_fd, sizeof(int));
	}

	ssize_t sent;
	do {
		sent = sendmsg(fd, &msg, MSG_NOSIGNAL);
	} while (sent < 0 && errno == EINTR);
	if (sent <= 0) {
		return false;
	}

	if ((size_t)sent < total) {
		return manju_ipc_write_full(
			fd, buffer + sent, total - (size_t)sent);
	}
	return true;
}

bool manju_ipc_recv_frame_fd(int fd, uint8_t *type, void *payload,
	uint32_t *len, uint32_t max, int *out_fd) {
	*out_fd = -1;

	uint8_t header[MANJU_IPC_HEADER_SIZE];
	struct iovec iov = {.iov_base = header, .iov_len = sizeof(header)};
	struct msghdr msg;
	memset(&msg, 0, sizeof(msg));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	union {
		char bytes[CMSG_SPACE(sizeof(int))];
		struct cmsghdr align;
	} control;
	memset(&control, 0, sizeof(control));
	msg.msg_control = control.bytes;
	msg.msg_controllen = sizeof(control.bytes);

	ssize_t got;
	do {
		got = recvmsg(fd, &msg, 0);
	} while (got < 0 && errno == EINTR);
	if (got <= 0) {
		return false;
	}

	struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
	if (cmsg != NULL && cmsg->cmsg_level == SOL_SOCKET &&
		cmsg->cmsg_type == SCM_RIGHTS &&
		cmsg->cmsg_len == CMSG_LEN(sizeof(int))) {
		memcpy(out_fd, CMSG_DATA(cmsg), sizeof(int));
	}
	// A truncated control message could leave a dangling fd in the kernel
	if ((msg.msg_flags & MSG_CTRUNC) != 0) {
		if (*out_fd >= 0) {
			close(*out_fd);
			*out_fd = -1;
		}
		return false;
	}

	if ((size_t)got < sizeof(header) &&
		!manju_ipc_read_full(
			fd, header + got, sizeof(header) - (size_t)got)) {
		goto fail;
	}
	if (header[0] != MANJU_IPC_VERSION) {
		goto fail;
	}

	uint32_t plen = (uint32_t)header[4] | ((uint32_t)header[5] << 8) |
			((uint32_t)header[6] << 16) |
			((uint32_t)header[7] << 24);
	if (plen > max || plen > MANJU_IPC_MAX_PAYLOAD) {
		goto fail;
	}
	if (plen > 0 && !manju_ipc_read_full(fd, payload, plen)) {
		goto fail;
	}

	*type = header[1];
	*len = plen;
	return true;

fail:
	if (*out_fd >= 0) {
		close(*out_fd);
		*out_fd = -1;
	}
	return false;
}
