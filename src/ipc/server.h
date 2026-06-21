#ifndef CARAMEL_IPC_SERVER_H
#define CARAMEL_IPC_SERVER_H

#include <stdbool.h>

struct caramel_ipc_server {
	int fd;
	char path[108];
};

bool caramel_ipc_server_init(struct caramel_ipc_server *server);

void caramel_ipc_server_handle(
	struct caramel_ipc_server *server, bool *stop_requested);

// Close the socket and unlink its path. Idempotent.
void caramel_ipc_server_finish(struct caramel_ipc_server *server);

#endif
