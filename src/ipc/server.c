#include "ipc/server.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#include "ipc/protocol.h"

// Bound on how long a single client may take to send its request before the
// daemon abandons it, so a stalled client cannot freeze wallpaper updates
#define CLIENT_RECV_TIMEOUT_SECONDS 2

enum socket_state {
	SOCKET_ABSENT,
	SOCKET_STALE,
	SOCKET_LIVE,
	SOCKET_ERROR,
};

// Probe an existing socket path by trying to connect: a successful connect
// means a live daemon, ECONNREFUSED means a stale file, ENOENT means none
static enum socket_state probe_socket(const struct sockaddr_un *addr) {
	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		return SOCKET_ERROR;
	}

	enum socket_state state;
	if (connect(fd, (const struct sockaddr *)addr, sizeof(*addr)) == 0) {
		state = SOCKET_LIVE;
	} else if (errno == ECONNREFUSED) {
		state = SOCKET_STALE;
	} else if (errno == ENOENT) {
		state = SOCKET_ABSENT;
	} else {
		state = SOCKET_ERROR;
	}

	close(fd);
	return state;
}

static bool clear_existing_socket(const struct sockaddr_un *addr) {
	switch (probe_socket(addr)) {
	case SOCKET_ABSENT:
		return true;
	case SOCKET_STALE:
		if (unlink(addr->sun_path) < 0) {
			fprintf(stderr,
				"carameld: cannot remove stale socket %s: %s\n",
				addr->sun_path, strerror(errno));
			return false;
		}
		return true;
	case SOCKET_LIVE:
		fprintf(stderr, "carameld: a daemon is already running at %s\n",
			addr->sun_path);
		return false;
	case SOCKET_ERROR:
	default:
		fprintf(stderr, "carameld: cannot check socket %s: %s\n",
			addr->sun_path, strerror(errno));
		return false;
	}
}

bool caramel_ipc_server_init(struct caramel_ipc_server *server) {
	server->fd = -1;
	server->path[0] = '\0';

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (!caramel_ipc_socket_path(addr.sun_path, sizeof(addr.sun_path))) {
		fprintf(stderr,
			"carameld: XDG_RUNTIME_DIR is unset or the socket "
			"path is too long\n");
		return false;
	}

	if (!clear_existing_socket(&addr)) {
		return false;
	}

	int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0) {
		fprintf(stderr, "carameld: cannot create socket: %s\n",
			strerror(errno));
		return false;
	}

	if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "carameld: cannot bind %s: %s\n", addr.sun_path,
			strerror(errno));
		close(fd);
		return false;
	}

	if (chmod(addr.sun_path, S_IRUSR | S_IWUSR) < 0 || listen(fd, 4) < 0) {
		fprintf(stderr, "carameld: cannot listen on %s: %s\n",
			addr.sun_path, strerror(errno));
		close(fd);
		unlink(addr.sun_path);
		return false;
	}

	server->fd = fd;
	memcpy(server->path, addr.sun_path, sizeof(server->path));
	return true;
}

static void dispatch_request(int client, uint8_t type, bool *stop_requested) {
	switch (type) {
	case CARAMEL_CMD_STOP:
		caramel_ipc_send_frame(client, CARAMEL_STATUS_OK, NULL, 0);
		*stop_requested = true;
		break;
	default: {
		const char *msg = "unknown command";
		caramel_ipc_send_frame(client,
			CARAMEL_STATUS_ERR_UNKNOWN_COMMAND, msg,
			(uint32_t)strlen(msg));
		break;
	}
	}
}

void caramel_ipc_server_handle(
	struct caramel_ipc_server *server, bool *stop_requested) {
	*stop_requested = false;

	int client = accept4(server->fd, NULL, NULL, SOCK_CLOEXEC);
	if (client < 0) {
		return;
	}

	struct timeval timeout = {
		.tv_sec = CLIENT_RECV_TIMEOUT_SECONDS,
		.tv_usec = 0,
	};
	setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	uint8_t type;
	uint8_t payload[CARAMEL_IPC_MAX_PAYLOAD];
	uint32_t len;
	if (caramel_ipc_recv_frame(
		    client, &type, payload, &len, sizeof(payload))) {
		dispatch_request(client, type, stop_requested);
	} else {
		caramel_ipc_send_frame(
			client, CARAMEL_STATUS_ERR_BAD_REQUEST, NULL, 0);
	}

	close(client);
}

void caramel_ipc_server_finish(struct caramel_ipc_server *server) {
	if (server->fd >= 0) {
		close(server->fd);
		server->fd = -1;
	}
	if (server->path[0] != '\0') {
		unlink(server->path);
		server->path[0] = '\0';
	}
}
