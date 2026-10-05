#include "doctor/doctor.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "config/config.h"
#include "ipc/client.h"
#include "ipc/protocol.h"

enum doctor_status {
	DOCTOR_OK,
	DOCTOR_WARN,
	DOCTOR_FAIL,
};

static int warnings;
static int failures;

__attribute__((format(printf, 3, 4))) static void report(
	enum doctor_status status, const char *label, const char *fmt, ...) {
	const char *prefix = "ok";
	if (status == DOCTOR_WARN) {
		prefix = "warn";
		warnings++;
	} else if (status == DOCTOR_FAIL) {
		prefix = "fail";
		failures++;
	}

	printf("%-4s %-12s ", prefix, label);
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
}

static void doctor_image_path(const char *scope, const char *path) {
	if (path[0] == '\0') {
		report(DOCTOR_OK, "image", "%s is intentionally blank", scope);
	} else if (path[0] != '/') {
		report(DOCTOR_WARN, "image", "%s uses relative path %s", scope,
			path);
	} else if (access(path, R_OK) != 0) {
		report(DOCTOR_FAIL, "image", "%s uses %s: %s", scope, path,
			strerror(errno));
	} else {
		report(DOCTOR_OK, "image", "%s uses readable %s", scope, path);
	}
}

static void doctor_check_environment(void) {
	const char *wayland = getenv("WAYLAND_DISPLAY");
	if (wayland != NULL && wayland[0] != '\0') {
		report(DOCTOR_OK, "wayland", "WAYLAND_DISPLAY=%s", wayland);
	} else {
		report(DOCTOR_WARN, "wayland", "WAYLAND_DISPLAY is unset");
	}

	const char *runtime = getenv("XDG_RUNTIME_DIR");
	if (runtime == NULL || runtime[0] == '\0') {
		report(DOCTOR_FAIL, "runtime", "XDG_RUNTIME_DIR is unset");
		return;
	}

	struct stat st;
	if (stat(runtime, &st) != 0) {
		report(DOCTOR_FAIL, "runtime", "%s: %s", runtime,
			strerror(errno));
		return;
	}
	if (!S_ISDIR(st.st_mode)) {
		report(DOCTOR_FAIL, "runtime", "%s is not a directory",
			runtime);
		return;
	}
	if (st.st_uid != getuid()) {
		report(DOCTOR_FAIL, "runtime", "%s is not owned by this user",
			runtime);
		return;
	}
	if ((st.st_mode & 077u) != 0) {
		report(DOCTOR_WARN, "runtime", "%s is usable but not private",
			runtime);
		return;
	}
	report(DOCTOR_OK, "runtime", "%s is private", runtime);
}

static void doctor_check_config(void) {
	char path[PATH_MAX];
	if (!sweetbg_config_path(path, sizeof(path))) {
		report(DOCTOR_WARN, "config",
			"HOME and XDG_CONFIG_HOME do not produce a config "
			"path");
		return;
	}

	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		if (errno == ENOENT) {
			report(DOCTOR_OK, "config",
				"no config at %s; defaults will be used", path);
			return;
		}
		report(DOCTOR_FAIL, "config", "%s: %s", path, strerror(errno));
		return;
	}

	struct sweetbg_config cfg;
	sweetbg_config_defaults(&cfg);
	char err[256];
	bool ok = sweetbg_config_parse(fp, path, &cfg, err, sizeof(err));
	fclose(fp);
	if (!ok) {
		report(DOCTOR_FAIL, "config", "%s", err);
		sweetbg_config_free(&cfg);
		return;
	}
	report(DOCTOR_OK, "config", "%s parses", path);

	if (cfg.image != NULL && cfg.image[0] != '\0') {
		doctor_image_path("default", cfg.image);
	}
	for (size_t i = 0; i < cfg.output_count; i++) {
		const struct sweetbg_config_output *out = &cfg.outputs[i];
		if (!out->has_image) {
			continue;
		}
		doctor_image_path(out->name, out->image);
	}
	sweetbg_config_free(&cfg);
}

static void doctor_print_response(const uint8_t *response, uint32_t len) {
	if (len == 0) {
		return;
	}
	char text[SWEETBG_IPC_MAX_PAYLOAD + 1];
	memcpy(text, response, len);
	text[len] = '\0';
	printf("info daemon status:\n");
	char *save = NULL;
	for (char *line = strtok_r(text, "\n", &save); line != NULL;
		line = strtok_r(NULL, "\n", &save)) {
		printf("     %s\n", line);
	}
}

static void doctor_check_socket_and_daemon(void) {
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (!sweetbg_ipc_socket_path(addr.sun_path, sizeof(addr.sun_path))) {
		report(DOCTOR_FAIL, "socket",
			"socket path cannot be built from XDG_RUNTIME_DIR");
		return;
	}

	struct stat st;
	bool socket_usable = false;
	bool socket_missing = false;
	if (lstat(addr.sun_path, &st) != 0) {
		socket_missing = errno == ENOENT;
		enum doctor_status status =
			socket_missing ? DOCTOR_WARN : DOCTOR_FAIL;
		report(status, "socket", "%s: %s", addr.sun_path,
			strerror(errno));
	} else if (!S_ISSOCK(st.st_mode)) {
		report(DOCTOR_FAIL, "socket", "%s is not a socket",
			addr.sun_path);
	} else if (st.st_uid != getuid()) {
		report(DOCTOR_FAIL, "socket", "%s is not owned by this user",
			addr.sun_path);
	} else {
		socket_usable = true;
		report(DOCTOR_OK, "socket", "%s exists", addr.sun_path);
	}
	if (!socket_usable) {
		report(DOCTOR_FAIL, "daemon",
			socket_missing ? "no daemon is running"
				       : "control socket is not usable");
		return;
	}

	uint8_t type;
	uint8_t response[SWEETBG_IPC_MAX_PAYLOAD];
	uint32_t len;
	char err[256];
	if (sweetbg_client_raw_request(SWEETBG_CMD_QUERY, NULL, 0, -1, &type,
		    response, &len, sizeof(response), err, sizeof(err)) != 0) {
		report(DOCTOR_FAIL, "daemon", "%s", err);
		return;
	}
	if (type != SWEETBG_STATUS_OK) {
		report(DOCTOR_FAIL, "daemon", "%.*s", (int)len,
			(const char *)response);
		return;
	}
	report(DOCTOR_OK, "daemon", "reachable");
	doctor_print_response(response, len);
}

int sweetbg_doctor_run(void) {
	printf("sweetbg doctor %s\n", SWEETBG_VERSION);
	doctor_check_environment();
	doctor_check_config();
	doctor_check_socket_and_daemon();

	if (failures > 0) {
		printf("doctor: %d failure%s, %d warning%s\n", failures,
			failures == 1 ? "" : "s", warnings,
			warnings == 1 ? "" : "s");
		return 1;
	}
	printf("doctor: ready with %d warning%s\n", warnings,
		warnings == 1 ? "" : "s");
	return 0;
}
