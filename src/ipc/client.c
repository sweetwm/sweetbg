#include "ipc/client.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "ipc/protocol.h"

// Bound on waiting for the daemon's reply so a wedged daemon cannot hang the
// CLI
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
