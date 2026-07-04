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

__attribute__((format(printf, 3, 4))) static void doctor_line(
	enum doctor_status status, const char *label, const char *fmt, ...) {
	const char *prefix = "ok";
	if (status == DOCTOR_WARN) {
		prefix = "warn";
	} else if (status == DOCTOR_FAIL) {
		prefix = "fail";
	}

	printf("%-4s %-12s ", prefix, label);
	va_list ap;
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	putchar('\n');
}

static void doctor_count(
	enum doctor_status status, int *warnings, int *failures) {
	if (status == DOCTOR_WARN) {
		(*warnings)++;
	} else if (status == DOCTOR_FAIL) {
		(*failures)++;
	}
}

static enum doctor_status doctor_image_path(
	const char *label, const char *scope, const char *path) {
	if (path[0] == '\0') {
		doctor_line(
			DOCTOR_OK, label, "%s is intentionally blank", scope);
		return DOCTOR_OK;
	}
	if (path[0] != '/') {
		doctor_line(DOCTOR_WARN, label, "%s uses relative path %s",
			scope, path);
		return DOCTOR_WARN;
	}
	if (access(path, R_OK) != 0) {
		doctor_line(DOCTOR_FAIL, label, "%s uses %s: %s", scope, path,
			strerror(errno));
		return DOCTOR_FAIL;
	}
	doctor_line(DOCTOR_OK, label, "%s uses readable %s", scope, path);
	return DOCTOR_OK;
}

static void doctor_check_environment(int *warnings, int *failures) {
	const char *wayland = getenv("WAYLAND_DISPLAY");
	enum doctor_status wayland_status =
		wayland != NULL && wayland[0] != '\0' ? DOCTOR_OK : DOCTOR_WARN;
	if (wayland_status == DOCTOR_OK) {
		doctor_line(wayland_status, "wayland", "WAYLAND_DISPLAY=%s",
			wayland);
	} else {
		doctor_line(
			wayland_status, "wayland", "WAYLAND_DISPLAY is unset");
	}
	doctor_count(wayland_status, warnings, failures);

	const char *runtime = getenv("XDG_RUNTIME_DIR");
	if (runtime == NULL || runtime[0] == '\0') {
		doctor_line(DOCTOR_FAIL, "runtime", "XDG_RUNTIME_DIR is unset");
		doctor_count(DOCTOR_FAIL, warnings, failures);
		return;
	}

	struct stat st;
	if (stat(runtime, &st) != 0) {
		doctor_line(DOCTOR_FAIL, "runtime", "%s: %s", runtime,
			strerror(errno));
		doctor_count(DOCTOR_FAIL, warnings, failures);
		return;
	}
	if (!S_ISDIR(st.st_mode)) {
		doctor_line(DOCTOR_FAIL, "runtime", "%s is not a directory",
			runtime);
		doctor_count(DOCTOR_FAIL, warnings, failures);
		return;
	}
	if (st.st_uid != getuid()) {
		doctor_line(DOCTOR_FAIL, "runtime",
			"%s is not owned by this user", runtime);
		doctor_count(DOCTOR_FAIL, warnings, failures);
		return;
	}
	if ((st.st_mode & 077u) != 0) {
		doctor_line(DOCTOR_WARN, "runtime",
			"%s is usable but not private", runtime);
		doctor_count(DOCTOR_WARN, warnings, failures);
		return;
	}
	doctor_line(DOCTOR_OK, "runtime", "%s is private", runtime);
}

static void doctor_check_config(int *warnings, int *failures) {
	char path[PATH_MAX];
	if (!sweetbg_config_path(path, sizeof(path))) {
		doctor_line(DOCTOR_WARN, "config",
			"HOME and XDG_CONFIG_HOME do not produce a config "
			"path");
		doctor_count(DOCTOR_WARN, warnings, failures);
		return;
	}

	FILE *fp = fopen(path, "r");
	if (fp == NULL) {
		if (errno == ENOENT) {
			doctor_line(DOCTOR_OK, "config",
				"no config at %s; defaults will be used", path);
			return;
		}
		doctor_line(
			DOCTOR_FAIL, "config", "%s: %s", path, strerror(errno));
		doctor_count(DOCTOR_FAIL, warnings, failures);
		return;
	}

	struct sweetbg_config cfg;
	sweetbg_config_defaults(&cfg);
	char err[256];
	bool ok = sweetbg_config_parse(fp, path, &cfg, err, sizeof(err));
	fclose(fp);
	if (!ok) {
		doctor_line(DOCTOR_FAIL, "config", "%s", err);
		doctor_count(DOCTOR_FAIL, warnings, failures);
		return;
	}
	doctor_line(DOCTOR_OK, "config", "%s parses", path);

	if (cfg.image[0] != '\0') {
		enum doctor_status status =
			doctor_image_path("image", "default", cfg.image);
		doctor_count(status, warnings, failures);
	}
	for (size_t i = 0; i < cfg.output_count; i++) {
		const struct sweetbg_config_output *out = &cfg.outputs[i];
		if (!out->has_image) {
			continue;
		}
		enum doctor_status status =
			doctor_image_path("image", out->name, out->image);
		doctor_count(status, warnings, failures);
	}
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

static void doctor_check_socket_and_daemon(int *warnings, int *failures) {
	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	if (!sweetbg_ipc_socket_path(addr.sun_path, sizeof(addr.sun_path))) {
		doctor_line(DOCTOR_FAIL, "socket",
			"socket path cannot be built from XDG_RUNTIME_DIR");
		doctor_count(DOCTOR_FAIL, warnings, failures);
		return;
	}

	struct stat st;
	bool socket_usable = false;
	bool socket_missing = false;
	if (lstat(addr.sun_path, &st) != 0) {
		socket_missing = errno == ENOENT;
		enum doctor_status status =
			socket_missing ? DOCTOR_WARN : DOCTOR_FAIL;
		doctor_line(status, "socket", "%s: %s", addr.sun_path,
			strerror(errno));
		doctor_count(status, warnings, failures);
	} else if (!S_ISSOCK(st.st_mode)) {
		doctor_line(DOCTOR_FAIL, "socket", "%s is not a socket",
			addr.sun_path);
		doctor_count(DOCTOR_FAIL, warnings, failures);
	} else if (st.st_uid != getuid()) {
		doctor_line(DOCTOR_FAIL, "socket",
			"%s is not owned by this user", addr.sun_path);
		doctor_count(DOCTOR_FAIL, warnings, failures);
	} else {
		socket_usable = true;
		doctor_line(DOCTOR_OK, "socket", "%s exists", addr.sun_path);
	}
	if (!socket_usable) {
		doctor_line(DOCTOR_FAIL, "daemon",
			socket_missing ? "no daemon is running"
				       : "control socket is not usable");
		doctor_count(DOCTOR_FAIL, warnings, failures);
		return;
	}

	uint8_t type;
	uint8_t response[SWEETBG_IPC_MAX_PAYLOAD];
	uint32_t len;
	char err[256];
	if (sweetbg_client_raw_request(SWEETBG_CMD_QUERY, NULL, 0, &type,
		    response, &len, sizeof(response), err, sizeof(err)) != 0) {
		doctor_line(DOCTOR_FAIL, "daemon", "%s", err);
		doctor_count(DOCTOR_FAIL, warnings, failures);
		return;
	}
	if (type != SWEETBG_STATUS_OK) {
		doctor_line(DOCTOR_FAIL, "daemon", "%.*s", (int)len,
			(const char *)response);
		doctor_count(DOCTOR_FAIL, warnings, failures);
		return;
	}
	doctor_line(DOCTOR_OK, "daemon", "reachable");
	doctor_print_response(response, len);
}

int sweetbg_doctor_run(void) {
	int warnings = 0;
	int failures = 0;

	printf("sweetbg doctor %s\n", SWEETBG_VERSION);
	doctor_check_environment(&warnings, &failures);
	doctor_check_config(&warnings, &failures);
	doctor_check_socket_and_daemon(&warnings, &failures);

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
