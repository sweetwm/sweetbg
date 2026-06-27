#ifndef MANJU_IPC_PROTOCOL_H
#define MANJU_IPC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MANJU_IPC_VERSION 1
#define MANJU_IPC_HEADER_SIZE 8
#define MANJU_IPC_MAX_PAYLOAD 4096

enum manju_ipc_command {
	MANJU_CMD_STOP = 1,
	MANJU_CMD_IMG = 2,
	MANJU_CMD_QUERY = 3,
	MANJU_CMD_QUERY_OUTPUTS = 4,
	MANJU_CMD_IMG_PREPARED = 5,
	MANJU_CMD_SET = 6,
	MANJU_CMD_CLEAR = 7,
};

enum manju_set_field {
	MANJU_SET_FIT = 1,
	MANJU_SET_COLOR = 2,
};

enum manju_ipc_status {
	MANJU_STATUS_OK = 0,
	MANJU_STATUS_ERR_BAD_REQUEST = 1,
	MANJU_STATUS_ERR_UNKNOWN_COMMAND = 2,
	MANJU_STATUS_ERR_IMAGE = 3,
};

enum manju_img_mode {
	MANJU_IMG_DEFAULT = 0,
	MANJU_IMG_OVERRIDE = 1,
	MANJU_IMG_REPAINT = 2,
};

enum manju_clear_flags {
	MANJU_CLEAR_IMAGE = 1u << 0,
	MANJU_CLEAR_FIT = 1u << 1,
	MANJU_CLEAR_BLANK = 1u << 2,
};

void manju_put_u32(uint8_t *p, uint32_t value);
uint32_t manju_get_u32(const uint8_t *p);

bool manju_ipc_socket_path(char *out, size_t out_size);
bool manju_ipc_read_full(int fd, void *buf, size_t n);
bool manju_ipc_write_full(int fd, const void *buf, size_t n);
bool manju_ipc_send_frame(
	int fd, uint8_t type, const void *payload, uint32_t len);
bool manju_ipc_recv_frame(
	int fd, uint8_t *type, void *payload, uint32_t *len, uint32_t max);

bool manju_ipc_send_frame_fd(
	int fd, uint8_t type, const void *payload, uint32_t len, int pass_fd);
bool manju_ipc_recv_frame_fd(int fd, uint8_t *type, void *payload,
	uint32_t *len, uint32_t max, int *out_fd);

#endif
