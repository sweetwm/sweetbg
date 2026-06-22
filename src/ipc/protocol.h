#ifndef CARAMEL_IPC_PROTOCOL_H
#define CARAMEL_IPC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CARAMEL_IPC_VERSION 1
#define CARAMEL_IPC_HEADER_SIZE 8
#define CARAMEL_IPC_MAX_PAYLOAD 4096

enum caramel_ipc_command {
	CARAMEL_CMD_STOP = 1,
	CARAMEL_CMD_IMG = 2,
};

enum caramel_ipc_status {
	CARAMEL_STATUS_OK = 0,
	CARAMEL_STATUS_ERR_BAD_REQUEST = 1,
	CARAMEL_STATUS_ERR_UNKNOWN_COMMAND = 2,
	CARAMEL_STATUS_ERR_IMAGE = 3,
};

bool caramel_ipc_socket_path(char *out, size_t out_size);
bool caramel_ipc_read_full(int fd, void *buf, size_t n);
bool caramel_ipc_write_full(int fd, const void *buf, size_t n);
bool caramel_ipc_send_frame(
	int fd, uint8_t type, const void *payload, uint32_t len);
bool caramel_ipc_recv_frame(
	int fd, uint8_t *type, void *payload, uint32_t *len, uint32_t max);

#endif
