// Config parser tests. Feeds strings to caramel_config_parse via fmemopen.

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

static bool parse(const char *text, struct caramel_config *cfg, char *err,
	size_t err_size) {
	caramel_config_defaults(cfg);
	FILE *fp = fmemopen((void *)text, strlen(text), "r");
	if (fp == NULL) {
		return false;
	}
	bool ok = caramel_config_parse(fp, "test", cfg, err, err_size);
	fclose(fp);
	return ok;
}

static int test_full_config(void) {
	struct caramel_config cfg;
	char err[256];
	const char *text = "# a comment\n"
			   "image = \"/home/me/wall.jpg\"\n"
			   "color = \"#1e2e3f\"\n"
			   "fit = \"cover\"  # trailing comment\n";
	CHECK(parse(text, &cfg, err, sizeof(err)));
	CHECK(strcmp(cfg.image, "/home/me/wall.jpg") == 0);
	CHECK(cfg.color == 0x1e2e3f);
	CHECK(cfg.fit == CARAMEL_FIT_COVER);
	return 0;
}

static int test_defaults_when_empty(void) {
	struct caramel_config cfg;
	char err[256];
	CHECK(parse("\n  \n# only comments\n", &cfg, err, sizeof(err)));
	CHECK(cfg.image[0] == '\0');
	CHECK(cfg.color == 0x1e1e2e);
	return 0;
}

static int test_unknown_key_fails(void) {
	struct caramel_config cfg;
	char err[256];
	CHECK(!parse("wallpaper = \"x\"\n", &cfg, err, sizeof(err)));
	CHECK(strstr(err, "unknown key") != NULL);
	return 0;
}

static int test_bad_color_fails(void) {
	struct caramel_config cfg;
	char err[256];
	CHECK(!parse("color = \"1e1e2e\"\n", &cfg, err, sizeof(err)));
	CHECK(!parse("color = \"#12345\"\n", &cfg, err, sizeof(err)));
	CHECK(!parse("color = \"#gggggg\"\n", &cfg, err, sizeof(err)));
	return 0;
}

static int test_unquoted_and_missing_eq_fail(void) {
	struct caramel_config cfg;
	char err[256];
	CHECK(!parse("image = /home/me/wall.jpg\n", &cfg, err, sizeof(err)));
	CHECK(!parse("image \"x\"\n", &cfg, err, sizeof(err)));
	return 0;
}

int main(void) {
	int rc = 0;
	rc |= test_full_config();
	rc |= test_defaults_when_empty();
	rc |= test_unknown_key_fails();
	rc |= test_bad_color_fails();
	rc |= test_unquoted_and_missing_eq_fail();
	if (rc == 0) {
		printf("config: all checks passed\n");
	}
	return rc;
}
