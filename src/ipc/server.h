#ifndef SWEETBG_IPC_SERVER_H
#define SWEETBG_IPC_SERVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct sweetbg_ipc_server {
	int fd;
	char path[108];
};

typedef uint8_t (*sweetbg_ipc_dispatch_fn)(void *data, uint8_t command,
	const uint8_t *payload, uint32_t len, int fd, char *message,
	size_t message_size, bool *stop);

bool sweetbg_ipc_server_init(struct sweetbg_ipc_server *server);

void sweetbg_ipc_server_handle(struct sweetbg_ipc_server *server,
	sweetbg_ipc_dispatch_fn dispatch, void *data, bool *stop);

void sweetbg_ipc_server_finish(struct sweetbg_ipc_server *server);

#endif
