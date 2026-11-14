#include "daemon/follow.h"
#include "daemon/state.h"
#include "ipc/follow.h"
#include <string.h>

#define FOLLOW_PATH_MAX (SWEETBG_IPC_MAX_PAYLOAD - 84)

static bool follow_handoff(
	void *data, int owner, const char *name, bool claim) {
	struct daemon *daemon = data;
	struct sweetbg_output *output;
	bool accepted = !claim;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (name != NULL && (output->name == NULL ||
					    strcmp(name, output->name) != 0)) {
			continue;
		}
		struct sweetbg_surface *surface = &output->surface;
		if (claim) {
			if (sweetbg_daemon_blur(daemon, output) != 0 ||
				!surface->configured ||
				surface->viewport == NULL ||
				sweetbg_daemon_fit(daemon, output) !=
					SWEETBG_FIT_COVER ||
				sweetbg_daemon_path(daemon, output)[0] ==
					'\0' ||
				strlen(sweetbg_daemon_path(daemon, output)) >
					FOLLOW_PATH_MAX ||
				(surface->renderer != 0 &&
					surface->renderer != owner + 1)) {
				return false;
			}
			if (surface->renderer == 0) {
				if (!sweetbg_surface_transparent(
					    surface, daemon->reg->shm)) {
					return false;
				}
				surface->renderer = owner + 1;
			}
			accepted = true;
		} else if (surface->renderer == owner + 1) {
			surface->renderer = 0;
			surface->needs_repaint = true;
		}
	}
	return accepted;
}

static size_t follow_snapshot(
	void *data, size_t index, uint8_t *out, size_t cap) {
	struct daemon *daemon = data;
	struct sweetbg_output *output;
	size_t current = 0;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (output->name == NULL || !output->surface.configured) {
			continue;
		}
		if (current++ != index) {
			continue;
		}
		const char *path = sweetbg_daemon_path(daemon, output);
		uint32_t fit = (uint32_t)sweetbg_daemon_fit(daemon, output);
		if (strlen(path) > FOLLOW_PATH_MAX ||
			sweetbg_daemon_blur(daemon, output) != 0) {
			path = "";
			fit = UINT32_MAX;
		}
		size_t name_len = strlen(output->name), path_len = strlen(path);
		if (name_len >= 64 || 16 + name_len + path_len > cap) {
			return 0;
		}
		sweetbg_put_u32(out, output->generation);
		sweetbg_put_u32(out + 4, fit);
		sweetbg_put_u32(out + 8, (uint32_t)name_len);
		sweetbg_put_u32(out + 12, (uint32_t)path_len);
		memcpy(out + 16, output->name, name_len);
		// Binary payload: path is length-prefixed, not NUL terminated
		// NOLINTNEXTLINE(bugprone-not-null-terminated-result)
		memcpy(out + 16 + name_len, path, path_len);
		return 16 + name_len + path_len;
	}
	return 0;
}

void sweetbg_daemon_follow_reconcile(
	struct daemon *daemon, struct sweetbg_follow *follow) {
	struct sweetbg_output *output;
	wl_list_for_each(output, &daemon->reg->outputs, link) {
		if (output->surface.renderer != 0 &&
			(sweetbg_daemon_blur(daemon, output) != 0 ||
				sweetbg_daemon_fit(daemon, output) !=
					SWEETBG_FIT_COVER ||
				sweetbg_daemon_path(daemon, output)[0] ==
					'\0' ||
				strlen(sweetbg_daemon_path(daemon, output)) >
					FOLLOW_PATH_MAX)) {
			output->surface.renderer = 0;
			output->surface.needs_repaint = true;
		}
	}
	sweetbg_daemon_repaint(daemon);
	sweetbg_follow_publish(follow);
}

void sweetbg_daemon_follow_init(
	struct sweetbg_follow *follow, struct daemon *daemon) {
	sweetbg_follow_init(follow, daemon, follow_snapshot, follow_handoff);
}
