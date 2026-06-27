#include "config/config_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(cond)                                                            \
	do {                                                                   \
		if (!(cond)) {                                                 \
			fprintf(stderr, "FAIL line %d: %s\n", __LINE__,        \
				#cond);                                        \
			return 1;                                              \
		}                                                              \
	} while (0)

static int expect(const char *input, const char *output_name, const char *image,
	const char *want) {
	char *out = NULL;
	char err[256];
	if (!manju_config_patch_image(
		    input, output_name, image, &out, err, sizeof(err))) {
		fprintf(stderr, "patch failed: %s\n", err);
		return 1;
	}
	int rc = 0;
	if (strcmp(out, want) != 0) {
		fprintf(stderr, "got:\n%s\nwant:\n%s\n", out, want);
		rc = 1;
	}
	free(out);
	return rc;
}

// Patch a top-level setting and assert the whole result equals `want`
static int expect_setting(const char *input, const char *key, const char *value,
	const char *want) {
	char *out = NULL;
	char err[256];
	if (!manju_config_patch_setting(
		    input, key, value, &out, err, sizeof(err))) {
		fprintf(stderr, "patch failed: %s\n", err);
		return 1;
	}
	int rc = 0;
	if (strcmp(out, want) != 0) {
		fprintf(stderr, "got:\n%s\nwant:\n%s\n", out, want);
		rc = 1;
	}
	free(out);
	return rc;
}

static int expect_output_setting(const char *input, const char *output_name,
	const char *key, const char *value, const char *want) {
	char *out = NULL;
	char err[256];
	if (!manju_config_patch_output_setting(
		    input, output_name, key, value, &out, err, sizeof(err))) {
		fprintf(stderr, "patch failed: %s\n", err);
		return 1;
	}
	int rc = 0;
	if (strcmp(out, want) != 0) {
		fprintf(stderr, "got:\n%s\nwant:\n%s\n", out, want);
		rc = 1;
	}
	free(out);
	return rc;
}

static int expect_clear_image(
	const char *input, const char *output_name, const char *want) {
	char *out = NULL;
	char err[256];
	if (!manju_config_patch_clear_image(
		    input, output_name, &out, err, sizeof(err))) {
		fprintf(stderr, "patch failed: %s\n", err);
		return 1;
	}
	int rc = 0;
	if (strcmp(out, want) != 0) {
		fprintf(stderr, "got:\n%s\nwant:\n%s\n", out, want);
		rc = 1;
	}
	free(out);
	return rc;
}

static int expect_clear_fit(
	const char *input, const char *output_name, const char *want) {
	char *out = NULL;
	char err[256];
	if (!manju_config_patch_clear_fit(
		    input, output_name, &out, err, sizeof(err))) {
		fprintf(stderr, "patch failed: %s\n", err);
		return 1;
	}
	int rc = 0;
	if (strcmp(out, want) != 0) {
		fprintf(stderr, "got:\n%s\nwant:\n%s\n", out, want);
		rc = 1;
	}
	free(out);
	return rc;
}

static int expect_blank_output(
	const char *input, const char *output_name, const char *want) {
	char *out = NULL;
	char err[256];
	if (!manju_config_patch_blank_output(
		    input, output_name, &out, err, sizeof(err))) {
		fprintf(stderr, "patch failed: %s\n", err);
		return 1;
	}
	int rc = 0;
	if (strcmp(out, want) != 0) {
		fprintf(stderr, "got:\n%s\nwant:\n%s\n", out, want);
		rc = 1;
	}
	free(out);
	return rc;
}

static int test_replace_default(void) {
	return expect("color = \"#111111\"\n"
		      "image = \"/old.jpg\"\n"
		      "fit = \"cover\"\n",
		NULL, "/new.jpg",
		"color = \"#111111\"\n"
		"image = \"/new.jpg\"\n"
		"fit = \"cover\"\n");
}

static int test_insert_default_before_section(void) {
	return expect("# comment\n"
		      "[output.DP-1]\n"
		      "image = \"/a.jpg\"\n",
		NULL, "/def.jpg",
		"# comment\n"
		"image = \"/def.jpg\"\n"
		"[output.DP-1]\n"
		"image = \"/a.jpg\"\n");
}

static int test_empty_input(void) {
	return expect("", NULL, "/x.jpg", "image = \"/x.jpg\"\n");
}

static int test_replace_output(void) {
	return expect("[output.DP-1]\n"
		      "image = \"/a.jpg\"\n",
		"DP-1", "/b.jpg",
		"[output.DP-1]\n"
		"image = \"/b.jpg\"\n");
}

static int test_insert_into_output(void) {
	return expect("[output.DP-1]\n"
		      "# note\n",
		"DP-1", "/b.jpg",
		"[output.DP-1]\n"
		"# note\n"
		"image = \"/b.jpg\"\n");
}

static int test_append_output(void) {
	return expect("image = \"/d.jpg\"\n", "DP-2", "/c.jpg",
		"image = \"/d.jpg\"\n"
		"\n"
		"[output.DP-2]\n"
		"image = \"/c.jpg\"\n");
}

static int test_default_keeps_sections(void) {
	return expect("image = \"/old.jpg\"\n"
		      "[output.DP-1]\n"
		      "image = \"/a.jpg\"\n",
		NULL, "/new.jpg",
		"image = \"/new.jpg\"\n"
		"[output.DP-1]\n"
		"image = \"/a.jpg\"\n");
}

// A top-level setting replaces its existing key in place
static int test_set_replace_top(void) {
	return expect_setting("color = \"#000000\"\n"
			      "fit = \"cover\"\n",
		"fit", "tile",
		"color = \"#000000\"\n"
		"fit = \"tile\"\n");
}

// A new top-level setting is inserted before the first section
static int test_set_insert_before_section(void) {
	return expect_setting("image = \"/a.jpg\"\n"
			      "[output.DP-1]\n"
			      "image = \"/b.jpg\"\n",
		"color", "#223344",
		"image = \"/a.jpg\"\n"
		"color = \"#223344\"\n"
		"[output.DP-1]\n"
		"image = \"/b.jpg\"\n");
}

static int test_set_replace_output(void) {
	return expect_output_setting("[output.DP-1]\n"
				     "image = \"/a.jpg\"\n"
				     "fit = \"cover\"\n",
		"DP-1", "fit", "contain",
		"[output.DP-1]\n"
		"image = \"/a.jpg\"\n"
		"fit = \"contain\"\n");
}

static int test_set_append_output(void) {
	return expect_output_setting("fit = \"cover\"\n", "DP-1", "fit",
		"contain",
		"fit = \"cover\"\n"
		"\n"
		"[output.DP-1]\n"
		"fit = \"contain\"\n");
}

static int test_clear_all_images(void) {
	return expect_clear_image("image = \"/default.jpg\"\n"
				  "color = \"#101010\"\n"
				  "[output.DP-1]\n"
				  "image = \"/left.jpg\"\n"
				  "fit = \"contain\"\n"
				  "[output.HDMI-A-1]\n"
				  "# keep the section\n"
				  "image = \"/tv.jpg\"\n",
		NULL,
		"color = \"#101010\"\n"
		"[output.DP-1]\n"
		"fit = \"contain\"\n"
		"[output.HDMI-A-1]\n"
		"# keep the section\n");
}

static int test_clear_one_output_image(void) {
	return expect_clear_image("image = \"/default.jpg\"\n"
				  "[output.DP-1]\n"
				  "image = \"/left.jpg\"\n"
				  "fit = \"contain\"\n"
				  "[output.HDMI-A-1]\n"
				  "image = \"/tv.jpg\"\n",
		"DP-1",
		"image = \"/default.jpg\"\n"
		"[output.DP-1]\n"
		"fit = \"contain\"\n"
		"[output.HDMI-A-1]\n"
		"image = \"/tv.jpg\"\n");
}

static int test_clear_all_fit(void) {
	return expect_clear_fit("fit = \"tile\"\n"
				"image = \"/default.jpg\"\n"
				"[output.DP-1]\n"
				"fit = \"contain\"\n"
				"image = \"/left.jpg\"\n",
		NULL,
		"image = \"/default.jpg\"\n"
		"[output.DP-1]\n"
		"image = \"/left.jpg\"\n");
}

static int test_clear_missing_key_keeps_input(void) {
	return expect_clear_fit("image = \"/default.jpg\"\n"
				"[output.DP-1]\n"
				"image = \"/left.jpg\"\n",
		"DP-1",
		"image = \"/default.jpg\"\n"
		"[output.DP-1]\n"
		"image = \"/left.jpg\"\n");
}

static int test_blank_output_image(void) {
	return expect_blank_output("image = \"/default.jpg\"\n"
				   "[output.DP-1]\n"
				   "image = \"/left.jpg\"\n"
				   "fit = \"contain\"\n",
		"DP-1",
		"image = \"/default.jpg\"\n"
		"[output.DP-1]\n"
		"image = \"\"\n"
		"fit = \"contain\"\n");
}

static int test_blank_output_appends_section(void) {
	return expect_blank_output("image = \"/default.jpg\"\n", "DP-1",
		"image = \"/default.jpg\"\n"
		"\n"
		"[output.DP-1]\n"
		"image = \"\"\n");
}

static int test_rejects_bad_input(void) {
	char *out = NULL;
	char err[256];
	CHECK(!manju_config_patch_image(
		"", NULL, "/has\"quote.jpg", &out, err, sizeof(err)));
	CHECK(!manju_config_patch_image(
		"", "bad name", "/ok.jpg", &out, err, sizeof(err)));
	CHECK(!manju_config_patch_image(
		"", "a]b", "/ok.jpg", &out, err, sizeof(err)));
	CHECK(!manju_config_patch_output_setting(
		"", "bad name", "fit", "cover", &out, err, sizeof(err)));
	CHECK(!manju_config_patch_clear_image(
		"", "bad name", &out, err, sizeof(err)));
	CHECK(!manju_config_patch_clear_fit("", "a]b", &out, err, sizeof(err)));
	CHECK(!manju_config_patch_blank_output(
		"", NULL, &out, err, sizeof(err)));
	CHECK(!manju_config_patch_blank_output(
		"", "bad name", &out, err, sizeof(err)));
	return 0;
}

int main(void) {
	int rc = 0;
	rc |= test_replace_default();
	rc |= test_insert_default_before_section();
	rc |= test_empty_input();
	rc |= test_replace_output();
	rc |= test_insert_into_output();
	rc |= test_append_output();
	rc |= test_default_keeps_sections();
	rc |= test_set_replace_top();
	rc |= test_set_insert_before_section();
	rc |= test_set_replace_output();
	rc |= test_set_append_output();
	rc |= test_clear_all_images();
	rc |= test_clear_one_output_image();
	rc |= test_clear_all_fit();
	rc |= test_clear_missing_key_keeps_input();
	rc |= test_blank_output_image();
	rc |= test_blank_output_appends_section();
	rc |= test_rejects_bad_input();
	if (rc == 0) {
		printf("config write: all checks passed\n");
	}
	return rc;
}
