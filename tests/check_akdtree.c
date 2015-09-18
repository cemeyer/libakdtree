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

#define	NELEM(arr)	(sizeof(arr) / sizeof(arr[0]))
#define	CV(a)		((const void *)a)

static struct akd_tree *g_tree;
static int int2_cmp(unsigned dim, const akd_userdata_t *, const akd_userdata_t *);
static double int2_sdist(const akd_userdata_t *, const akd_userdata_t *);
static double int2_asdist(const akd_userdata_t *, const akd_userdata_t *, unsigned);

struct akd_param_block g_int2_params = {
	.ap_k = 2,
	.ap_size = sizeof(int) * 2,
	.ap_cmp = int2_cmp,
	.ap_flags = 0,
	._u = {
	._double = {
		.ap_squared_dist = int2_sdist,
		.ap_axis_squared_dist = int2_asdist,
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
		return (EXIT_SUCCESS);
	return (EXIT_FAILURE);
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

static int
int2_cmp(unsigned dim, const akd_userdata_t *a, const akd_userdata_t *b)
{
	const int *ad = CV(a), *bd = CV(b);

	fail_if(dim != 0 && dim != 1);

	if (dim) {
		ad++;
		bd++;
	}

	if (*ad > *bd)
		return (1);
	else if (*ad == *bd)
		return (0);
	else
		return (-1);
}

static double
int2_asdist(const akd_userdata_t *a, const akd_userdata_t *b, unsigned dim)
{
	const int *ad = CV(a), *bd = CV(b);
	double d;

	if (dim) {
		ad++;
		bd++;
	}

	d = (*ad - *bd);
	return (d * d);
}

static double
int2_sdist(const akd_userdata_t *a, const akd_userdata_t *b)
{

	return (int2_asdist(a, b, 0) + int2_asdist(a, b, 1));
}

#define	PRINT_DEBUG	1
#ifdef	PRINT_DEBUG
int
print_int2_cb(unsigned level, const akd_userdata_t *datum)
{
	const int *idat;
	unsigned i;

	for (i = 0; i < level; i++)
		putchar('\t');

	idat = CV(datum);
	printf("%d, %d\n", idat[0], idat[1]);
	return (0);
}
#endif

START_TEST(test_simple)
{
	const void *vnn;
	int input[] = {
		1, 2,
		3, 4,
		5, 5,
	};
	int key1[] = { 3, 3 },
	    key2[] = { 1, 2 },
	    key3[] = { 3, 4 };
	const int *found;
	int error;

	error = akd_create((void*)input, NELEM(input) / 2, &g_int2_params, &g_tree);
	fail_if(error);

#ifdef	PRINT_DEBUG
	(void)akd_tree_walk(g_tree, print_int2_cb, 0);
#endif

	vnn = akd_find_nearest_ex(g_tree, CV(key1), 0);
	fail_unless(vnn);
	found = vnn;
	fail_unless(found[0] == 3 && found[1] == 4);

	vnn = akd_find_nearest_ex(g_tree, CV(key2), 0);
	fail_unless(vnn);
	found = vnn;
	fail_unless(found[0] == 1 && found[1] == 2);

	vnn = akd_find_nearest_ex(g_tree, CV(key2), AKD_NOT_EQUAL);
	fail_unless(vnn);
	found = vnn;
	fail_unless(found[0] == 3 && found[1] == 4);

	vnn = akd_find_nearest_ex(g_tree, CV(key3), AKD_NOT_EQUAL);
	fail_unless(vnn);
	found = vnn;
	fail_unless(found[0] == 5 && found[1] == 5);
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
	tcase_add_test(tc, test_simple);

	suite_add_tcase(s, tc);
	return (s);
}
