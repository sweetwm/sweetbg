#ifndef CARAMEL_IPC_CLIENT_H
#define CARAMEL_IPC_CLIENT_H

#include <stdint.h>

int caramel_client_request(
	uint8_t command, const void *payload, uint32_t payload_len);

int caramel_client_set_image(const char *path, const char *output);

int caramel_client_prepare_output(const char *name, const char *path);

#endif
