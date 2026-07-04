#ifndef SWEETBG_IPC_CLIENT_H
#define SWEETBG_IPC_CLIENT_H

#include <stddef.h>
#include <stdint.h>

int sweetbg_client_request(
	uint8_t command, const void *payload, uint32_t payload_len);

int sweetbg_client_raw_request(uint8_t command, const void *payload,
	uint32_t payload_len, uint8_t *type, void *response, uint32_t *len,
	uint32_t max, char *err, size_t err_size);

int sweetbg_client_set_image(const char *path, const char *output);

int sweetbg_client_prepare_output(const char *name, const char *path);

#endif
