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
	if (!caramel_config_patch_image(
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

static int test_rejects_bad_input(void) {
	char *out = NULL;
	char err[256];
	CHECK(!caramel_config_patch_image(
		"", NULL, "/has\"quote.jpg", &out, err, sizeof(err)));
	CHECK(!caramel_config_patch_image(
		"", "bad name", "/ok.jpg", &out, err, sizeof(err)));
	CHECK(!caramel_config_patch_image(
		"", "a]b", "/ok.jpg", &out, err, sizeof(err)));
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
	rc |= test_rejects_bad_input();
	if (rc == 0) {
		printf("config write: all checks passed\n");
	}
	return rc;
}
