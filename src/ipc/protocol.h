#ifndef SWEETBG_IPC_PROTOCOL_H
#define SWEETBG_IPC_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SWEETBG_IPC_VERSION 1
#define SWEETBG_IPC_HEADER_SIZE 8
#define SWEETBG_IPC_MAX_PAYLOAD 4096

enum sweetbg_ipc_command {
	SWEETBG_CMD_STOP = 1,
	SWEETBG_CMD_IMG = 2,
	SWEETBG_CMD_QUERY = 3,
	SWEETBG_CMD_QUERY_OUTPUTS = 4,
	SWEETBG_CMD_IMG_PREPARED = 5,
	SWEETBG_CMD_SET = 6,
	SWEETBG_CMD_CLEAR = 7,
	SWEETBG_CMD_QUERY_JSON = 8,
	SWEETBG_CMD_RELOAD = 9,
};

enum sweetbg_set_field {
	SWEETBG_SET_FIT = 1,
	SWEETBG_SET_COLOR = 2,
};

enum sweetbg_ipc_status {
	SWEETBG_STATUS_OK = 0,
	SWEETBG_STATUS_ERR_BAD_REQUEST = 1,
	SWEETBG_STATUS_ERR_UNKNOWN_COMMAND = 2,
	SWEETBG_STATUS_ERR_IMAGE = 3,
};

enum sweetbg_img_mode {
	SWEETBG_IMG_DEFAULT = 0,
	SWEETBG_IMG_OVERRIDE = 1,
	SWEETBG_IMG_REPAINT = 2,
};

enum sweetbg_clear_flags {
	SWEETBG_CLEAR_IMAGE = 1u << 0,
	SWEETBG_CLEAR_FIT = 1u << 1,
	SWEETBG_CLEAR_BLANK = 1u << 2,
};

void sweetbg_put_u32(uint8_t *p, uint32_t value);
uint32_t sweetbg_get_u32(const uint8_t *p);

bool sweetbg_ipc_socket_path(char *out, size_t out_size);
bool sweetbg_ipc_read_full(int fd, void *buf, size_t n);
bool sweetbg_ipc_write_full(int fd, const void *buf, size_t n);
bool sweetbg_ipc_send_frame(
	int fd, uint8_t type, const void *payload, uint32_t len);
bool sweetbg_ipc_recv_frame(
	int fd, uint8_t *type, void *payload, uint32_t *len, uint32_t max);

bool sweetbg_ipc_send_frame_fd(
	int fd, uint8_t type, const void *payload, uint32_t len, int pass_fd);
bool sweetbg_ipc_recv_frame_fd(int fd, uint8_t *type, void *payload,
	uint32_t *len, uint32_t max, int *out_fd);

#endif
