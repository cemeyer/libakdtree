/*
 * A KD-tree implementation. MIT-licensed, see LICENSE.
 *
 * Somewhat derived from the also-MIT-licensed OfflineReverseGeocode by Daniel
 * Glasson.
 *
 * Should work in most POSIX-ey userspace environments.
 */

#ifdef __linux__
# define _GNU_SOURCE
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include "akdtree.h"

/*
 * Utility macros and functions
 */
#define ASSERT(cond, ...) do {							\
	if ((intptr_t)(cond))							\
		break;								\
										\
	assert_fail(#cond, __func__, __FILE__, __LINE__, ##__VA_ARGS__, NULL);	\
} while (false)
#define EXPORT_SYM __attribute__((visibility("default")))
#define NELEM(arr) (sizeof(arr) / sizeof(arr[0]))

#ifndef __must_check
# define __must_check __attribute__((warn_unused_result))
#endif
#ifndef __printflike
# define __printflike(fmtarg, firstvararg) \
	__attribute__((format(printf, fmtarg, firstvararg)))
#endif
#ifndef __dead2
# define __dead2 __attribute__((noreturn))
#endif

static void __dead2 __printflike(5, 6)
assert_fail(const char *an, const char *fn, const char *file, unsigned line,
    const char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "ASSERTION `%s' FAILED", an);
	if (fmt) {
		fputs(": ", stderr);

		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);
	}

	fprintf(stderr, " at %s() (%s:%u)\n", fn, file, line);
	exit(EX_SOFTWARE);
}

/*
 * KD-tree stuff starts.
 */
struct akd_node {
	struct akd_node	*an_left,
			*an_right;
	char		 an_userdata[];
};

struct akd_tree {
	struct akd_node		*at_root;
	struct akd_param_block	 at_params;
};

struct akd_cmp_thunk {
	const struct akd_param_block	*params;
	unsigned			 axis;
};

/* Free a tree of nodes */
void
free_node(struct akd_node *node)
{

	if (node == NULL)
		return;

	free_node(node->an_left);
	free_node(node->an_right);
	free(node);
}

/* Sorted tree building */
#ifdef __linux__
static int
pick_cmp(const void *a, const void *b, void *thunkv)
{
	struct akd_cmp_thunk *t = thunkv;

	return t->params->ap_cmp(t->axis, a, b);
}
#else /* !__linux__ */
static int
pick_cmp(void *thunkv, const void *a, const void *b)
{
	struct akd_cmp_thunk *t = thunkv;

	return t->params->ap_cmp(t->axis, a, b);
}
#endif

static int __must_check
build_tree(void *items, size_t nmemb, unsigned depth,
    const struct akd_param_block *pb, struct akd_node **node_out)
{
	struct akd_cmp_thunk sort_thunk;
	struct akd_node *node = NULL;
	size_t middle;
	int error = 0;
	char *midp;

	*node_out = NULL;
	if (nmemb == 0)
		goto out;

	sort_thunk.axis = depth % pb->ap_k;
	sort_thunk.params = pb;
#ifdef __linux__
	qsort_r(items, nmemb, pb->ap_size, pick_cmp, &sort_thunk);
#else
	qsort_r(items, nmemb, pb->ap_size, &sort_thunk, pick_cmp);
#endif

	node = malloc(sizeof(*node) + pb->ap_size);
	if (node == NULL) {
		error = ENOMEM;
		goto out;
	}

	memset(node, 0, sizeof(*node) + pb->ap_size);

	middle = nmemb / 2;
	error = build_tree(items, middle, depth + 1, pb, &node->an_left);
	if (error)
		goto out;

	midp = (char *)items + (middle * pb->ap_size);

	error = build_tree(midp + pb->ap_size, nmemb - (middle + 1), depth + 1,
	    pb, &node->an_right);
	if (error)
		goto out;

	memcpy(&node->an_userdata, midp, pb->ap_size);

out:
	if (error && node)
		free_node(node);
	return error;
}

/* Search */
static struct akd_node *
find_nearest_node(const struct akd_param_block *pb, struct akd_node *root,
    const void *key, unsigned depth)
{
	struct akd_node *best, *next, *other, *maybe;
	int cmp;

	cmp = pb->ap_cmp(depth % pb->ap_k, key, root);
	if (cmp < 0) {
		next = root->an_left;
		other = root->an_right;
	} else {
		next = root->an_right;
		other = root->an_left;
	}

	best = root;
	if (next) {
		maybe = find_nearest_node(pb, next, key, depth + 1);
		if (pb->_u._double.ap_squared_dist(maybe, key) <=
		    pb->_u._double.ap_squared_dist(root, key))
			best = maybe;
	}

	if (other == NULL)
		goto out;

	if (pb->_u._double.ap_axis_squared_dist(root, key, depth % pb->ap_k) >=
	    pb->_u._double.ap_squared_dist(best, key))
		goto out;

	maybe = find_nearest_node(pb, other, key, depth + 1);
	if (pb->_u._double.ap_squared_dist(maybe, key) <
	    pb->_u._double.ap_squared_dist(best, key))
		best = maybe;

out:
	return best;
}

/*
 * API routines
 */
EXPORT_SYM int
akd_create(void *items, size_t nmemb, const struct akd_param_block *pb,
    struct akd_tree **tree_out)
{
	const unsigned valid_flags = (AKD_SINGLE_PREC | AKD_INTEGRAL);

	struct akd_tree *t = NULL;
	int error = EINVAL;

	/* Stupid check */
	if (pb->ap_size == 0)
		goto out;

	if (pb->ap_k == 0)
		goto out;

	if (tree_out == NULL)
		goto out;

	if (pb->ap_flags & ~valid_flags)
		goto out;

	*tree_out = NULL;

	error = ENOMEM;
	t = malloc(sizeof(*t));
	if (t == NULL)
		goto out;

	memset(t, 0, sizeof(*t));
	memcpy(&t->at_params, pb, sizeof(t->at_params));

	error = build_tree(items, nmemb, 0, &t->at_params, &t->at_root);
	if (error)
		goto out;

	*tree_out = t;

out:
	if (error && t)
		akd_free(t);
	return error;
}

EXPORT_SYM void
akd_free(struct akd_tree *tree)
{

	if (tree == NULL)
		return;

	free_node(tree->at_root);
	free(tree);
}

EXPORT_SYM const struct akd_param_block *
akd_get_param_block(const struct akd_tree *tree)
{

	return &tree->at_params;
}

EXPORT_SYM const void *
akd_find_nearest(const struct akd_tree *tree, const void *key)
{
	struct akd_node *n;

	if (tree->at_root == NULL)
		return NULL;

	n = find_nearest_node(&tree->at_params, tree->at_root, key, 0);
	if (n == NULL)
		return NULL;

	return &n->an_userdata;
}
