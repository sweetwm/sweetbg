#ifndef CARAMEL_IPC_SERVER_H
#define CARAMEL_IPC_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct caramel_ipc_server {
	int fd;
	char path[108];
};

typedef uint8_t (*caramel_ipc_dispatch_fn)(void *data, uint8_t command,
	const uint8_t *payload, uint32_t len, int fd, char *message,
	size_t message_size, bool *stop);

bool caramel_ipc_server_init(struct caramel_ipc_server *server);

void caramel_ipc_server_handle(struct caramel_ipc_server *server,
	caramel_ipc_dispatch_fn dispatch, void *data, bool *stop);

void caramel_ipc_server_finish(struct caramel_ipc_server *server);

#endif
