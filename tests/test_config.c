// Config parser tests. Feeds strings to manju_config_parse via fmemopen.

#include "config/config.h"

#include <stdio.h>
#include <string.h>

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL line %d: %s\n", __LINE__,        \
				#cond);                                        \
			return 1;                                              \
		}                                                              \
	} while (0)

static bool parse(const char *text, struct manju_config *cfg, char *err,
	size_t err_size) {
	manju_config_defaults(cfg);
	FILE *fp = fmemopen((void *)text, strlen(text), "r");
	if (fp == NULL) {
		return false;
	}
	bool ok = manju_config_parse(fp, "test", cfg, err, err_size);
	fclose(fp);
	return ok;
}

static int test_full_config(void) {
	struct manju_config cfg;
	char err[256];
	const char *text = "# a comment\n"
			   "image = \"/home/me/wall.jpg\"\n"
			   "color = \"#1e2e3f\"\n"
			   "fit = \"cover\"  # trailing comment\n";
	CHECK(parse(text, &cfg, err, sizeof(err)));
	CHECK(strcmp(cfg.image, "/home/me/wall.jpg") == 0);
	CHECK(cfg.color == 0x1e2e3f);
	CHECK(cfg.fit == MANJU_FIT_COVER);
	return 0;
}

static int test_defaults_when_empty(void) {
	struct manju_config cfg;
	char err[256];
	CHECK(parse("\n  \n# only comments\n", &cfg, err, sizeof(err)));
	CHECK(cfg.image[0] == '\0');
	CHECK(cfg.color == 0x1e1e2e);
	return 0;
}

static int test_fit_modes(void) {
	struct manju_config cfg;
	char err[256];
	CHECK(parse("fit = \"contain\"\n", &cfg, err, sizeof(err)));
	CHECK(cfg.fit == MANJU_FIT_CONTAIN);
	CHECK(parse("fit = \"center\"\n", &cfg, err, sizeof(err)));
	CHECK(cfg.fit == MANJU_FIT_CENTER);
	CHECK(parse("fit = \"tile\"\n", &cfg, err, sizeof(err)));
	CHECK(cfg.fit == MANJU_FIT_TILE);
	CHECK(!parse("fit = \"stretch\"\n", &cfg, err, sizeof(err)));
	CHECK(strstr(err, "fit must be") != NULL);
	return 0;
}

static int test_fit_and_color_parsers(void) {
	enum manju_fit fit;
	CHECK(manju_fit_from_name("cover", &fit) && fit == MANJU_FIT_COVER);
	CHECK(manju_fit_from_name("contain", &fit) && fit == MANJU_FIT_CONTAIN);
	CHECK(manju_fit_from_name("center", &fit) && fit == MANJU_FIT_CENTER);
	CHECK(manju_fit_from_name("tile", &fit) && fit == MANJU_FIT_TILE);
	CHECK(!manju_fit_from_name("stretch", &fit));

	uint32_t color;
	CHECK(manju_config_parse_color("#1e2e3f", &color) && color == 0x1e2e3f);
	CHECK(manju_config_parse_color("#ABCDEF", &color) && color == 0xabcdef);
	CHECK(!manju_config_parse_color("1e2e3f", &color));
	CHECK(!manju_config_parse_color("#12345", &color));
	return 0;
}

static int test_unknown_key_fails(void) {
	struct manju_config cfg;
	char err[256];
	CHECK(!parse("wallpaper = \"x\"\n", &cfg, err, sizeof(err)));
	CHECK(strstr(err, "unknown key") != NULL);
	return 0;
}

static int test_bad_color_fails(void) {
	struct manju_config cfg;
	char err[256];
	CHECK(!parse("color = \"1e1e2e\"\n", &cfg, err, sizeof(err)));
	CHECK(!parse("color = \"#12345\"\n", &cfg, err, sizeof(err)));
	CHECK(!parse("color = \"#gggggg\"\n", &cfg, err, sizeof(err)));
	return 0;
}

static int test_unquoted_and_missing_eq_fail(void) {
	struct manju_config cfg;
	char err[256];
	CHECK(!parse("image = /home/me/wall.jpg\n", &cfg, err, sizeof(err)));
	CHECK(!parse("image \"x\"\n", &cfg, err, sizeof(err)));
	return 0;
}

static int test_per_output_sections(void) {
	struct manju_config cfg;
	char err[256];
	const char *text = "image = \"/default.jpg\"\n"
			   "[output.DP-1]\n"
			   "image = \"/left.jpg\"\n"
			   "fit = \"contain\"\n"
			   "[output.HDMI-A-1]  # the tv\n"
			   "image = \"/tv.png\"\n";
	CHECK(parse(text, &cfg, err, sizeof(err)));
	CHECK(strcmp(cfg.image, "/default.jpg") == 0);
	CHECK(cfg.output_count == 2);
	CHECK(strcmp(cfg.outputs[0].name, "DP-1") == 0);
	CHECK(cfg.outputs[0].has_image);
	CHECK(strcmp(cfg.outputs[0].image, "/left.jpg") == 0);
	CHECK(cfg.outputs[0].has_fit);
	CHECK(cfg.outputs[0].fit == MANJU_FIT_CONTAIN);
	CHECK(strcmp(cfg.outputs[1].name, "HDMI-A-1") == 0);
	CHECK(cfg.outputs[1].has_image);
	CHECK(strcmp(cfg.outputs[1].image, "/tv.png") == 0);
	CHECK(!cfg.outputs[1].has_fit);
	return 0;
}

static int test_blank_output_image(void) {
	struct manju_config cfg;
	char err[256];
	const char *text = "image = \"/default.jpg\"\n"
			   "[output.DP-1]\n"
			   "image = \"\"\n";
	CHECK(parse(text, &cfg, err, sizeof(err)));
	CHECK(strcmp(cfg.image, "/default.jpg") == 0);
	CHECK(cfg.output_count == 1);
	CHECK(strcmp(cfg.outputs[0].name, "DP-1") == 0);
	CHECK(cfg.outputs[0].has_image);
	CHECK(cfg.outputs[0].image[0] == '\0');
	return 0;
}

static int test_bad_sections_fail(void) {
	struct manju_config cfg;
	char err[256];
	CHECK(!parse("[monitor.DP-1]\n", &cfg, err, sizeof(err)));
	CHECK(!parse("[output.DP-1\n", &cfg, err, sizeof(err)));
	CHECK(!parse("[output.DP-1]\ncolor = \"#ffffff\"\n", &cfg, err,
		sizeof(err)));
	CHECK(!parse(
		"[output.DP-1]\nfit = \"stretch\"\n", &cfg, err, sizeof(err)));
	return 0;
}

int main(void) {
	int rc = 0;
	rc |= test_full_config();
	rc |= test_fit_modes();
	rc |= test_fit_and_color_parsers();
	rc |= test_defaults_when_empty();
	rc |= test_unknown_key_fails();
	rc |= test_bad_color_fails();
	rc |= test_unquoted_and_missing_eq_fail();
	rc |= test_per_output_sections();
	rc |= test_blank_output_image();
	rc |= test_bad_sections_fail();
	if (rc == 0) {
		printf("config: all checks passed\n");
	}
	return rc;
}
