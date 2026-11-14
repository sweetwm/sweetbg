#ifndef SWEETBG_DAEMON_STATE_H
#define SWEETBG_DAEMON_STATE_H
#include "config/config.h"
#include "wayland/output.h"
#include "wayland/registry.h"

struct palette {
	uint32_t colors[SWEETBG_MAX_PALETTE];
	uint8_t count;
};

struct assignment {
	char name[64];
	char *path;
	enum sweetbg_fit fit;
	uint32_t blur;
	bool has_fit;
	bool has_blur;
	struct palette palette;
};

struct daemon {
	struct wl_display *display;
	struct sweetbg_registry *reg;
	uint32_t color;
	bool color_auto;
	enum sweetbg_fit fit;
	uint32_t blur;
	char *default_path;
	struct palette default_palette;
	struct assignment assignments[SWEETBG_MAX_OVERRIDES];
	size_t assignment_count;
};

const char *sweetbg_daemon_path(
	struct daemon *daemon, const struct sweetbg_output *output);
enum sweetbg_fit sweetbg_daemon_fit(
	struct daemon *daemon, const struct sweetbg_output *output);
uint32_t sweetbg_daemon_blur(
	struct daemon *daemon, const struct sweetbg_output *output);
void sweetbg_daemon_repaint(struct daemon *daemon);
#endif
