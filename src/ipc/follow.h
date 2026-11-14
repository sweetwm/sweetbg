#ifndef SWEETBG_IPC_FOLLOW_H
#define SWEETBG_IPC_FOLLOW_H
#include "ipc/protocol.h"
#include <poll.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SWEETBG_FOLLOW_CLIENTS 4
#define SWEETBG_FOLLOW_SUBSCRIBE 10
#define SWEETBG_FOLLOW_STATE 11
#define SWEETBG_FOLLOW_DONE 12
#define SWEETBG_FOLLOW_CLAIM 13
#define SWEETBG_FOLLOW_RELEASE 14
#define SWEETBG_FOLLOW_ACK 15

struct sweetbg_follower {
	int fd;
	uint8_t rx[80];
	size_t received;
	uint8_t tx[SWEETBG_IPC_MAX_PAYLOAD + SWEETBG_IPC_HEADER_SIZE];
	size_t sent, length, index;
	uint32_t streaming, delivered;
	uint8_t ack[72];
	size_t ack_length;
};

typedef size_t (*sweetbg_follow_snapshot)(
	void *data, size_t index, uint8_t *out, size_t cap);
// name == NULL releases all outputs owned by this connection
typedef bool (*sweetbg_follow_handoff)(
	void *data, int owner, const char *name, bool claim);

struct sweetbg_follow {
	struct sweetbg_follower clients[SWEETBG_FOLLOW_CLIENTS];
	sweetbg_follow_snapshot snapshot;
	sweetbg_follow_handoff handoff;
	void *data;
	uint64_t fingerprint;
	uint32_t revision;
};
void sweetbg_follow_init(struct sweetbg_follow *follow, void *data,
	sweetbg_follow_snapshot snapshot, sweetbg_follow_handoff handoff);
bool sweetbg_follow_accept(struct sweetbg_follow *follow, int fd);
void sweetbg_follow_publish(struct sweetbg_follow *follow);
void sweetbg_follow_poll(struct sweetbg_follow *follow, struct pollfd *fds);
void sweetbg_follow_dispatch(
	struct sweetbg_follow *follow, const struct pollfd *fds);
void sweetbg_follow_finish(struct sweetbg_follow *follow);
#endif
