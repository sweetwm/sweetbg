#ifndef MANJU_IPC_SERVER_H
#define MANJU_IPC_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct manju_ipc_server {
	int fd;
	char path[108];
};

typedef uint8_t (*manju_ipc_dispatch_fn)(void *data, uint8_t command,
	const uint8_t *payload, uint32_t len, int fd, char *message,
	size_t message_size, bool *stop);

bool manju_ipc_server_init(struct manju_ipc_server *server);

void manju_ipc_server_handle(struct manju_ipc_server *server,
	manju_ipc_dispatch_fn dispatch, void *data, bool *stop);

void manju_ipc_server_finish(struct manju_ipc_server *server);

#endif
