#include "query/json.h"

#include <stdarg.h>
#include <stdio.h>

struct json_out {
	char *text;
	size_t size;
	size_t off;
	bool ok;
};

__attribute__((format(printf, 2, 3))) static void json_append(
	struct json_out *json, const char *fmt, ...) {
	if (!json->ok || json->off >= json->size) {
		json->ok = false;
		return;
	}
	va_list ap;
	va_start(ap, fmt);
	int n = vsnprintf(
		json->text + json->off, json->size - json->off, fmt, ap);
	va_end(ap);
	if (n < 0 || (size_t)n >= json->size - json->off) {
		json->ok = false;
		return;
	}
	json->off += (size_t)n;
}

static void json_append_string(struct json_out *json, const char *s) {
	json_append(json, "\"");
	for (const unsigned char *p = (const unsigned char *)s;
		json->ok && *p != 0; p++) {
		switch (*p) {
		case '"':
			json_append(json, "\\\"");
			break;
		case '\\':
			json_append(json, "\\\\");
			break;
		case '\b':
			json_append(json, "\\b");
			break;
		case '\f':
			json_append(json, "\\f");
			break;
		case '\n':
			json_append(json, "\\n");
			break;
		case '\r':
			json_append(json, "\\r");
			break;
		case '\t':
			json_append(json, "\\t");
			break;
		default:
			if (*p < 0x20) {
				json_append(json, "\\u%04x", (unsigned)*p);
			} else {
				json_append(json, "%c", *p);
			}
			break;
		}
	}
	json_append(json, "\"");
}

static void json_append_nullable_string(struct json_out *json, const char *s) {
	if (s == NULL || s[0] == '\0') {
		json_append(json, "null");
	} else {
		json_append_string(json, s);
	}
}

static void json_append_colors(
	struct json_out *json, const uint32_t *colors, size_t count) {
	json_append(json, "[");
	for (size_t i = 0; i < count; i++) {
		if (i > 0) {
			json_append(json, ",");
		}
		json_append(json, "\"#%06x\"", colors[i] & 0xffffffu);
	}
	json_append(json, "]");
}

bool sweetbg_query_json_write(const struct sweetbg_query_json_state *state,
	char *out, size_t out_size) {
	struct json_out json = {
		.text = out,
		.size = out_size,
		.off = 0,
		.ok = true,
	};

	json_append(&json, "{\"default\":{\"image\":");
	json_append_nullable_string(&json, state->default_image);
	json_append(&json,
		",\"color\":\"#%06x\",\"fit\":", state->color & 0xffffffu);
	json_append_string(&json, state->default_fit);
	json_append(&json, ",\"colors\":");
	json_append_colors(
		&json, state->default_colors, state->default_color_count);
	json_append(&json, "},\"outputs\":[");

	for (size_t i = 0; i < state->output_count; i++) {
		const struct sweetbg_query_json_output *output =
			&state->outputs[i];
		if (i > 0) {
			json_append(&json, ",");
		}
		json_append(&json, "{\"name\":");
		json_append_nullable_string(&json, output->name);
		json_append(&json,
			",\"width\":%d,\"height\":%d,\"scale\":%d,"
			"\"configured\":%s,\"image\":",
			output->width, output->height, output->scale,
			output->configured ? "true" : "false");
		json_append_nullable_string(&json, output->image);
		json_append(&json,
			",\"imageOverride\":%s,\"blank\":%s,\"fit\":",
			output->image_override ? "true" : "false",
			output->blank ? "true" : "false");
		json_append_string(&json, output->fit);
		json_append(&json, ",\"fitOverride\":%s,\"colors\":",
			output->fit_override ? "true" : "false");
		json_append_colors(&json, output->colors, output->color_count);
		json_append(&json, "}");
	}

	json_append(&json, "]}");
	return json.ok;
}
