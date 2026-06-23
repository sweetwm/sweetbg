// Framing round-trip and rejection tests over a socketpair. Uses an explicit
// CHECK (not assert) so results hold regardless of NDEBUG.

#include "ipc/protocol.h"

#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL line %d: %s\n", __LINE__,        \
				#cond);                                        \
			return 1;                                              \
		}                                                              \
	} while (0)

static int test_roundtrip(void) {
	int sv[2];
	CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

	const char *msg = "/path/to/wall.png";
	uint32_t msg_len = (uint32_t)strlen(msg);
	CHECK(caramel_ipc_send_frame(sv[0], CARAMEL_CMD_STOP, msg, msg_len));

	uint8_t type;
	uint8_t buf[CARAMEL_IPC_MAX_PAYLOAD];
	uint32_t len;
	CHECK(caramel_ipc_recv_frame(sv[1], &type, buf, &len, sizeof(buf)));
	CHECK(type == CARAMEL_CMD_STOP);
	CHECK(len == msg_len);
	CHECK(memcmp(buf, msg, msg_len) == 0);

	close(sv[0]);
	close(sv[1]);
	return 0;
}

static int test_empty_payload(void) {
	int sv[2];
	CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

	CHECK(caramel_ipc_send_frame(sv[0], CARAMEL_STATUS_OK, NULL, 0));

	uint8_t type;
	uint8_t buf[16];
	uint32_t len;
	CHECK(caramel_ipc_recv_frame(sv[1], &type, buf, &len, sizeof(buf)));
	CHECK(type == CARAMEL_STATUS_OK);
	CHECK(len == 0);

	close(sv[0]);
	close(sv[1]);
	return 0;
}

static int test_send_rejects_oversize(void) {
	int sv[2];
	CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

	// Length over the cap must be refused before any byte is written
	CHECK(!caramel_ipc_send_frame(
		sv[0], CARAMEL_CMD_STOP, NULL, CARAMEL_IPC_MAX_PAYLOAD + 1));

	close(sv[0]);
	close(sv[1]);
	return 0;
}

static int test_recv_rejects_bad_version(void) {
	int sv[2];
	CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

	uint8_t header[CARAMEL_IPC_HEADER_SIZE] = {
		99, CARAMEL_CMD_STOP, 0, 0, 0, 0, 0, 0};
	CHECK(write(sv[0], header, sizeof(header)) == (ssize_t)sizeof(header));

	uint8_t type;
	uint8_t buf[16];
	uint32_t len;
	CHECK(!caramel_ipc_recv_frame(sv[1], &type, buf, &len, sizeof(buf)));

	close(sv[0]);
	close(sv[1]);
	return 0;
}

static int test_recv_rejects_oversize_length(void) {
	int sv[2];
	CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

	// A header claiming a 4 GiB payload must be rejected, not allocated for
	uint8_t header[CARAMEL_IPC_HEADER_SIZE] = {CARAMEL_IPC_VERSION,
		CARAMEL_CMD_STOP, 0, 0, 0xff, 0xff, 0xff, 0xff};
	CHECK(write(sv[0], header, sizeof(header)) == (ssize_t)sizeof(header));

	uint8_t type;
	uint8_t buf[16];
	uint32_t len;
	CHECK(!caramel_ipc_recv_frame(sv[1], &type, buf, &len, sizeof(buf)));

	close(sv[0]);
	close(sv[1]);
	return 0;
}

static int test_fd_passing(void) {
	int sv[2];
	CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);

	int memfd = memfd_create("caramel-test", MFD_CLOEXEC);
	CHECK(memfd >= 0);
	const char *content = "PIXELS";
	CHECK(write(memfd, content, 6) == 6);

	const char *meta = "DP-1";
	CHECK(caramel_ipc_send_frame_fd(
		sv[0], CARAMEL_CMD_IMG_PREPARED, meta, 4, memfd));

	uint8_t type;
	uint8_t buf[64];
	uint32_t len;
	int rfd = -1;
	CHECK(caramel_ipc_recv_frame_fd(
		sv[1], &type, buf, &len, sizeof(buf), &rfd));
	CHECK(type == CARAMEL_CMD_IMG_PREPARED);
	CHECK(len == 4 && memcmp(buf, meta, 4) == 0);
	CHECK(rfd >= 0 && rfd != memfd);

	char back[8] = {0};
	CHECK(pread(rfd, back, 6, 0) == 6);
	CHECK(memcmp(back, content, 6) == 0);

	close(rfd);
	close(memfd);
	close(sv[0]);
	close(sv[1]);
	return 0;
}

// A frame sent without an fd must arrive with out_fd left at -1.
static int test_no_fd(void) {
	int sv[2];
	CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
	CHECK(caramel_ipc_send_frame_fd(sv[0], CARAMEL_CMD_STOP, NULL, 0, -1));

	uint8_t type;
	uint8_t buf[16];
	uint32_t len;
	int rfd = 0;
	CHECK(caramel_ipc_recv_frame_fd(
		sv[1], &type, buf, &len, sizeof(buf), &rfd));
	CHECK(type == CARAMEL_CMD_STOP && len == 0 && rfd == -1);

	close(sv[0]);
	close(sv[1]);
	return 0;
}

int main(void) {
	int rc = 0;
	rc |= test_roundtrip();
	rc |= test_empty_payload();
	rc |= test_send_rejects_oversize();
	rc |= test_recv_rejects_bad_version();
	rc |= test_recv_rejects_oversize_length();
	rc |= test_fd_passing();
	rc |= test_no_fd();
	if (rc == 0) {
		printf("ipc protocol: all checks passed\n");
	}
	return rc;
}
