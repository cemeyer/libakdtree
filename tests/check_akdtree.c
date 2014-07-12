#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <check.h>

#include <akdtree.h>

#define NELEM(arr) (sizeof(arr) / sizeof(arr[0]))

static struct akd_tree *g_tree;
static int int2_cmp(unsigned dim, const void *, const void *);
static double int2_sdist(const void *, const void *);
static double int2_asdist(const void *, const void *, unsigned);

struct akd_param_block g_int2_params = {
	.ap_k = 2,
	.ap_size = sizeof(int),
	.ap_cmp = NULL,//int2_cmp,
	.ap_flags = 0,
	._u = {
	._double = {
		.ap_squared_dist = NULL,//int2_sdist,
		.ap_axis_squared_dist = NULL,//int2_asdist,
	}},
};

static Suite *t_kdtree(void);

int
main(int argc, char **argv)
{
	bool success = true;
	Suite *(*suites[])(void) = {
		t_kdtree
	};

	(void)argc;
	(void)argv;

	// Find and run all tests
	for (unsigned i = 0; i < NELEM(suites); i++) {
		Suite *s = suites[i]();
		SRunner *sr = srunner_create(s);

		srunner_run_all(sr, CK_NORMAL);

		if (srunner_ntests_failed(sr) > 0)
			success = false;

		srunner_free(sr);
	}

	if (success)
		return EXIT_SUCCESS;
	return EXIT_FAILURE;
}

static void
setup(void)
{
}

static void
teardown(void)
{

	akd_free(g_tree);
	g_tree = NULL;
}

START_TEST(test_empty)
{
	int error;

	error = akd_create(NULL, 0, &g_int2_params, &g_tree);
	fail_if(error);
}
END_TEST

START_TEST(test_bogus_inputs)
{
	struct akd_param_block p;
	int error;

	memcpy(&p, &g_int2_params, sizeof(p));

	p.ap_k = 0;
	error = akd_create(NULL, 0, &p, &g_tree);
	fail_unless(error);
	p.ap_k = g_int2_params.ap_k;

	p.ap_size = 0;
	error = akd_create(NULL, 0, &p, &g_tree);
	fail_unless(error);
	p.ap_size = g_int2_params.ap_k;

	p.ap_flags = 0x4;
	error = akd_create(NULL, 0, &p, &g_tree);
	fail_unless(error);
}
END_TEST

static Suite *
t_kdtree(void)
{
	Suite *s;
	TCase *tc;

	s = suite_create("akdtree");
	tc = tcase_create("basic");

	tcase_add_checked_fixture(tc, setup, teardown);

	tcase_add_test(tc, test_empty);
	tcase_add_test(tc, test_bogus_inputs);

	suite_add_tcase(s, tc);
	return s;
}
