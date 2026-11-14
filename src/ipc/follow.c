#include "ipc/follow.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void disconnect_client(
	struct sweetbg_follow *follow, struct sweetbg_follower *client) {
	if (client->fd < 0) {
		return;
	}
	follow->handoff(follow->data, client->fd, NULL, false);
	close(client->fd);
	client->fd = -1;
}

void sweetbg_follow_init(struct sweetbg_follow *follow, void *data,
	sweetbg_follow_snapshot snapshot, sweetbg_follow_handoff handoff) {
	memset(follow, 0, sizeof(*follow));
	follow->data = data;
	follow->snapshot = snapshot;
	follow->handoff = handoff;
	for (size_t i = 0; i < SWEETBG_FOLLOW_CLIENTS; i++) {
		follow->clients[i].fd = -1;
	}
}

bool sweetbg_follow_accept(struct sweetbg_follow *follow, int fd) {
	for (size_t i = 0; i < SWEETBG_FOLLOW_CLIENTS; i++) {
		if (follow->clients[i].fd < 0) {
			follow->clients[i] =
				(struct sweetbg_follower){.fd = fd};
			return true;
		}
	}
	return false;
}

void sweetbg_follow_publish(struct sweetbg_follow *follow) {
	uint64_t hash = UINT64_C(14695981039346656037);
	uint8_t payload[SWEETBG_IPC_MAX_PAYLOAD];
	for (size_t i = 0; i < SWEETBG_MAX_OUTPUTS; i++) {
		size_t n = follow->snapshot(
			follow->data, i, payload, sizeof(payload) - 4);
		if (n == 0) {
			break;
		}
		for (size_t j = 0; j < n; j++) {
			hash ^= payload[j];
			hash *= UINT64_C(1099511628211);
		}
		hash ^= UINT64_C(255);
	}
	if (follow->revision == 0 || hash != follow->fingerprint) {
		follow->fingerprint = hash;
		follow->revision++;
		if (follow->revision == 0) {
			follow->revision = 1;
		}
	}
}

static void frame(struct sweetbg_follower *client, uint8_t type,
	const uint8_t *payload, size_t len) {
	client->tx[0] = SWEETBG_IPC_VERSION;
	client->tx[1] = type;
	client->tx[2] = 0;
	client->tx[3] = 0;
	sweetbg_put_u32(client->tx + 4, (uint32_t)len);
	memcpy(client->tx + SWEETBG_IPC_HEADER_SIZE, payload, len);
	client->length = len + SWEETBG_IPC_HEADER_SIZE;
	client->sent = 0;
}

static void next_frame(
	struct sweetbg_follow *follow, struct sweetbg_follower *client) {
	if (client->ack_length > 0) {
		frame(client, SWEETBG_FOLLOW_ACK, client->ack,
			client->ack_length);
		client->ack_length = 0;
		return;
	}
	if (client->delivered == follow->revision) {
		return;
	}
	if (client->streaming != follow->revision) {
		client->streaming = follow->revision;
		client->index = 0;
	}
	uint8_t payload[SWEETBG_IPC_MAX_PAYLOAD];
	sweetbg_put_u32(payload, follow->revision);
	size_t len = follow->snapshot(
		follow->data, client->index, payload + 4, sizeof(payload) - 4);
	frame(client, len == 0 ? SWEETBG_FOLLOW_DONE : SWEETBG_FOLLOW_STATE,
		payload, len + 4);
	if (len == 0) {
		client->delivered = follow->revision;
	} else {
		client->index++;
	}
}

static bool write_pending(
	struct sweetbg_follow *follow, struct sweetbg_follower *client) {
	if (client->length == 0) {
		next_frame(follow, client);
	}
	if (client->length == 0) {
		return true;
	}
	ssize_t n = send(client->fd, client->tx + client->sent,
		client->length - client->sent, MSG_NOSIGNAL | MSG_DONTWAIT);
	if (n < 0) {
		return errno == EAGAIN || errno == EWOULDBLOCK ||
		       errno == EINTR;
	}
	if (n == 0) {
		return false;
	}
	client->sent += (size_t)n;
	if (client->sent == client->length) {
		client->sent = 0;
		client->length = 0;
	}
	return true;
}

static bool request(
	struct sweetbg_follow *follow, struct sweetbg_follower *client) {
	uint8_t type = client->rx[1];
	size_t len = client->received - SWEETBG_IPC_HEADER_SIZE;
	const uint8_t *payload = client->rx + SWEETBG_IPC_HEADER_SIZE;
	size_t offset = type == SWEETBG_FOLLOW_CLAIM ? 4 : 0;
	if ((type != SWEETBG_FOLLOW_CLAIM && type != SWEETBG_FOLLOW_RELEASE) ||
		len < offset || len - offset >= 64 ||
		memchr(payload + offset, 0, len - offset) != NULL) {
		return false;
	}
	char name[64] = {0};
	memcpy(name, payload + offset, len - offset);
	if (type == SWEETBG_FOLLOW_RELEASE) {
		follow->handoff(follow->data, client->fd,
			name[0] == '\0' ? NULL : name, false);
		return true;
	}
	if (name[0] == '\0' || client->ack_length != 0) {
		return false;
	}
	uint32_t revision = sweetbg_get_u32(payload);
	bool accepted = revision == follow->revision &&
			follow->handoff(follow->data, client->fd, name, true);
	sweetbg_put_u32(client->ack, revision);
	sweetbg_put_u32(client->ack + 4, accepted ? 1u : 0u);
	memcpy(client->ack + 8, name, len - offset);
	client->ack_length = len - offset + 8;
	return true;
}

static bool read_pending(
	struct sweetbg_follow *follow, struct sweetbg_follower *client) {
	unsigned requests = 0;
	while (requests < 16) {
		size_t need = SWEETBG_IPC_HEADER_SIZE;
		if (client->received >= need) {
			if (client->rx[0] != SWEETBG_IPC_VERSION ||
				client->rx[2] != 0 || client->rx[3] != 0) {
				return false;
			}
			uint32_t len = sweetbg_get_u32(client->rx + 4);
			if (len > sizeof(client->rx) - need) {
				return false;
			}
			need += len;
		}
		if (client->received == need) {
			if (!request(follow, client)) {
				return false;
			}
			client->received = 0;
			requests++;
			continue;
		}
		ssize_t n = recv(client->fd, client->rx + client->received,
			need - client->received, MSG_DONTWAIT);
		if (n < 0) {
			return errno == EAGAIN || errno == EWOULDBLOCK ||
			       errno == EINTR;
		}
		if (n == 0) {
			return false;
		}
		client->received += (size_t)n;
	}
	return true;
}

void sweetbg_follow_poll(struct sweetbg_follow *follow, struct pollfd *fds) {
	for (size_t i = 0; i < SWEETBG_FOLLOW_CLIENTS; i++) {
		struct sweetbg_follower *client = &follow->clients[i];
		bool pending = client->length != 0 || client->ack_length != 0 ||
			       client->delivered != follow->revision;
		fds[i] = (struct pollfd){.fd = client->fd,
			.events = POLLIN | (pending ? POLLOUT : 0)};
	}
}

void sweetbg_follow_dispatch(
	struct sweetbg_follow *follow, const struct pollfd *fds) {
	for (size_t i = 0; i < SWEETBG_FOLLOW_CLIENTS; i++) {
		struct sweetbg_follower *client = &follow->clients[i];
		if (client->fd < 0) {
			continue;
		}
		short events = fds[i].revents;
		if ((events & (POLLERR | POLLHUP | POLLNVAL)) != 0 ||
			((events & POLLIN) != 0 &&
				!read_pending(follow, client)) ||
			((events & POLLOUT) != 0 &&
				!write_pending(follow, client))) {
			disconnect_client(follow, client);
		}
	}
}

void sweetbg_follow_finish(struct sweetbg_follow *follow) {
	for (size_t i = 0; i < SWEETBG_FOLLOW_CLIENTS; i++) {
		disconnect_client(follow, &follow->clients[i]);
	}
}
