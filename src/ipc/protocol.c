#include "ipc/protocol.h"

#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

void sweetbg_put_u32(uint8_t *p, uint32_t value) {
	p[0] = (uint8_t)(value & 0xff);
	p[1] = (uint8_t)((value >> 8) & 0xff);
	p[2] = (uint8_t)((value >> 16) & 0xff);
	p[3] = (uint8_t)((value >> 24) & 0xff);
}

uint32_t sweetbg_get_u32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
	       ((uint32_t)p[3] << 24);
}

bool sweetbg_ipc_socket_path(char *out, size_t out_size) {
	const char *dir = getenv("XDG_RUNTIME_DIR");
	if (dir == NULL || dir[0] == '\0') {
		return false;
	}
	int n = snprintf(out, out_size, "%s/sweetbg.sock", dir);
	if (n < 0 || (size_t)n >= out_size) {
		return false;
	}
	return true;
}

bool sweetbg_ipc_read_full(int fd, void *buf, size_t n) {
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

bool sweetbg_ipc_write_full(int fd, const void *buf, size_t n) {
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

static void fill_header(uint8_t *header, uint8_t type, uint32_t len) {
	header[0] = SWEETBG_IPC_VERSION;
	header[1] = type;
	header[2] = 0;
	header[3] = 0;
	header[4] = (uint8_t)(len & 0xff);
	header[5] = (uint8_t)((len >> 8) & 0xff);
	header[6] = (uint8_t)((len >> 16) & 0xff);
	header[7] = (uint8_t)((len >> 24) & 0xff);
}

bool sweetbg_ipc_send_frame(
	int fd, uint8_t type, const void *payload, uint32_t len) {
	if (len > SWEETBG_IPC_MAX_PAYLOAD) {
		return false;
	}

	uint8_t header[SWEETBG_IPC_HEADER_SIZE];
	fill_header(header, type, len);

	if (!sweetbg_ipc_write_full(fd, header, sizeof(header))) {
		return false;
	}
	if (len > 0 && !sweetbg_ipc_write_full(fd, payload, len)) {
		return false;
	}
	return true;
}

bool sweetbg_ipc_recv_frame(
	int fd, uint8_t *type, void *payload, uint32_t *len, uint32_t max) {
	uint8_t header[SWEETBG_IPC_HEADER_SIZE];
	if (!sweetbg_ipc_read_full(fd, header, sizeof(header))) {
		return false;
	}
	if (header[0] != SWEETBG_IPC_VERSION) {
		return false;
	}

	uint32_t plen = (uint32_t)header[4] | ((uint32_t)header[5] << 8) |
			((uint32_t)header[6] << 16) |
			((uint32_t)header[7] << 24);
	if (plen > max || plen > SWEETBG_IPC_MAX_PAYLOAD) {
		return false;
	}
	if (plen > 0 && !sweetbg_ipc_read_full(fd, payload, plen)) {
		return false;
	}

	*type = header[1];
	*len = plen;
	return true;
}

bool sweetbg_ipc_send_frame_fd(
	int fd, uint8_t type, const void *payload, uint32_t len, int pass_fd) {
	if (len > SWEETBG_IPC_MAX_PAYLOAD) {
		return false;
	}

	uint8_t buffer[SWEETBG_IPC_HEADER_SIZE + SWEETBG_IPC_MAX_PAYLOAD];
	fill_header(buffer, type, len);
	if (len > 0) {
		memcpy(buffer + SWEETBG_IPC_HEADER_SIZE, payload, len);
	}
	size_t total = (size_t)SWEETBG_IPC_HEADER_SIZE + len;

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
		return sweetbg_ipc_write_full(
			fd, buffer + sent, total - (size_t)sent);
	}
	return true;
}

static bool make_deadline(struct timespec *deadline, uint32_t timeout_ms) {
	if (clock_gettime(CLOCK_MONOTONIC, deadline) != 0) {
		return false;
	}
	deadline->tv_sec += (time_t)(timeout_ms / 1000);
	deadline->tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
	if (deadline->tv_nsec >= 1000000000L) {
		deadline->tv_sec++;
		deadline->tv_nsec -= 1000000000L;
	}
	return true;
}

static bool wait_readable(int fd, const struct timespec *deadline) {
	for (;;) {
		struct timespec now;
		if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) {
			return false;
		}
		time_t seconds = deadline->tv_sec - now.tv_sec;
		long nanoseconds = deadline->tv_nsec - now.tv_nsec;
		if (nanoseconds < 0) {
			seconds--;
			nanoseconds += 1000000000L;
		}
		if (seconds < 0 || (seconds == 0 && nanoseconds == 0)) {
			errno = ETIMEDOUT;
			return false;
		}

		uint64_t millis = (uint64_t)seconds * 1000u +
				  ((uint64_t)nanoseconds + 999999u) / 1000000u;
		int timeout = millis > INT_MAX ? INT_MAX : (int)millis;
		struct pollfd pollfd = {.fd = fd, .events = POLLIN};
		int ready = poll(&pollfd, 1, timeout);
		if (ready > 0) {
			return true;
		}
		if (ready == 0) {
			errno = ETIMEDOUT;
			return false;
		}
		if (errno != EINTR) {
			return false;
		}
	}
}

static bool read_full_until(
	int fd, void *buf, size_t len, const struct timespec *deadline) {
	uint8_t *p = buf;
	size_t got = 0;
	while (got < len) {
		ssize_t n = recv(fd, p + got, len - got, MSG_DONTWAIT);
		if (n > 0) {
			got += (size_t)n;
			continue;
		}
		if (n == 0) {
			return false;
		}
		if (errno == EINTR) {
			continue;
		}
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			return false;
		}
		if (!wait_readable(fd, deadline)) {
			return false;
		}
	}
	return true;
}

static ssize_t recvmsg_until(
	int fd, struct msghdr *msg, const struct timespec *deadline) {
	for (;;) {
		ssize_t got = recvmsg(fd, msg, MSG_CMSG_CLOEXEC | MSG_DONTWAIT);
		if (got >= 0) {
			return got;
		}
		if (errno == EINTR) {
			continue;
		}
		if ((errno != EAGAIN && errno != EWOULDBLOCK) ||
			!wait_readable(fd, deadline)) {
			return -1;
		}
	}
}

bool sweetbg_ipc_recv_frame_fd(int fd, uint8_t *type, void *payload,
	uint32_t *len, uint32_t max, int *out_fd, uint32_t timeout_ms) {
	*out_fd = -1;
	struct timespec deadline;
	if (!make_deadline(&deadline, timeout_ms)) {
		return false;
	}

	uint8_t header[SWEETBG_IPC_HEADER_SIZE];
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

	ssize_t got = recvmsg_until(fd, &msg, &deadline);
	if (got <= 0) {
		return false;
	}

	size_t received_fd_count = 0;
	for (struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL;
		cmsg = CMSG_NXTHDR(&msg, cmsg)) {
		if (cmsg->cmsg_level != SOL_SOCKET ||
			cmsg->cmsg_type != SCM_RIGHTS ||
			cmsg->cmsg_len < CMSG_LEN(0)) {
			continue;
		}

		size_t count = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
		for (size_t i = 0; i < count; i++) {
			int received;
			memcpy(&received,
				CMSG_DATA(cmsg) + i * sizeof(received),
				sizeof(received));
			if (*out_fd < 0) {
				*out_fd = received;
			} else {
				close(received);
			}
			received_fd_count++;
		}
	}
	// exactly zero or one fd is valid; reject confused or hostile peers
	if (received_fd_count > 1 || (msg.msg_flags & MSG_CTRUNC) != 0) {
		goto fail;
	}

	if ((size_t)got < sizeof(header) &&
		!read_full_until(fd, header + got, sizeof(header) - (size_t)got,
			&deadline)) {
		goto fail;
	}
	if (header[0] != SWEETBG_IPC_VERSION) {
		goto fail;
	}

	uint32_t plen = (uint32_t)header[4] | ((uint32_t)header[5] << 8) |
			((uint32_t)header[6] << 16) |
			((uint32_t)header[7] << 24);
	if (plen > max || plen > SWEETBG_IPC_MAX_PAYLOAD) {
		goto fail;
	}
	if (plen > 0 && !read_full_until(fd, payload, plen, &deadline)) {
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
