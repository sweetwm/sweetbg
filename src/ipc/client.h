#ifndef MANJU_IPC_CLIENT_H
#define MANJU_IPC_CLIENT_H

#include <stdint.h>

int manju_client_request(
	uint8_t command, const void *payload, uint32_t payload_len);

int manju_client_set_image(const char *path, const char *output);

int manju_client_prepare_output(const char *name, const char *path);

#endif
