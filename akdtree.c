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
#include <stdbool.h>
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
	akd_userdata_t	 an_userdata[];
};

struct akd_tree {
	struct akd_node		*at_root;
	struct akd_param_block	 at_params;
};

struct akd_cmp_thunk {
	const struct akd_param_block	*params;
	unsigned			 axis;
};

static bool
node_eq_key(const struct akd_param_block *pb, const struct akd_node *n,
    const akd_userdata_t *key)
{
	unsigned i;
	bool eq;

	if (memcmp(key, n->an_userdata, pb->ap_size) == 0)
		return (true);

	eq = true;
	for (i = 0; i < pb->ap_k; i++) {
		if (pb->ap_cmp(i, key, n->an_userdata) == 0)
			continue;
		eq = false;
		break;
	}

	return (eq);
}

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
build_tree(akd_userdata_t *items, size_t nmemb, unsigned depth,
    const struct akd_param_block *pb, struct akd_node **node_out)
{
	struct akd_cmp_thunk sort_thunk;
	struct akd_node *node = NULL;
	size_t middle;
	int error = 0;
	akd_userdata_t *midp;

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

	midp = (uint8_t *)items + (middle * pb->ap_size);
	error = build_tree(midp + pb->ap_size, nmemb - (middle + 1), depth + 1,
	    pb, &node->an_right);
	if (error)
		goto out;

	memcpy(node->an_userdata, midp, pb->ap_size);
	*node_out = node;

out:
	if (error && node)
		free_node(node);
	return error;
}

/* Search */
static struct akd_node *
find_nearest_node_ex(const struct akd_param_block *pb, struct akd_node *root,
    const akd_userdata_t *key, unsigned depth, unsigned flags)
{
	struct akd_node *best, *next, *other, *maybe;
	bool badrooteq;
	int cmp;

	badrooteq = false;

	cmp = pb->ap_cmp(depth % pb->ap_k, key, root->an_userdata);
	if (cmp < 0) {
		next = root->an_left;
		other = root->an_right;
	} else if (cmp > 0) {
		next = root->an_right;
		other = root->an_left;
	} else {
		/* Root == key and we asked for something NEQ key */
		if ((flags & AKD_NOT_EQUAL) != 0)
			badrooteq = true;

		next = root->an_right;
		other = root->an_left;
	}

	best = root;
	if (next) {
		maybe = find_nearest_node_ex(pb, next, key, depth + 1, flags);
		if (maybe && (badrooteq ||
		    pb->_u._double.ap_squared_dist(maybe->an_userdata, key) <=
		    pb->_u._double.ap_squared_dist(root->an_userdata, key)))
			best = maybe;
	}

	if (other == NULL)
		goto out;

	if ((!badrooteq || best != root) &&
	    pb->_u._double.ap_axis_squared_dist(root->an_userdata, key, depth % pb->ap_k) >=
	    pb->_u._double.ap_squared_dist(best->an_userdata, key))
		goto out;

	maybe = find_nearest_node_ex(pb, other, key, depth + 1, flags);
	if (maybe == NULL)
		goto out;

	if (pb->_u._double.ap_squared_dist(maybe->an_userdata, key) <
	    pb->_u._double.ap_squared_dist(best->an_userdata, key) ||
	    (best == root && badrooteq))
		best = maybe;

out:
	if (best == root && badrooteq)
		best = NULL;
	ASSERT((flags & AKD_NOT_EQUAL) == 0 || best == NULL ||
	    !node_eq_key(pb, best, key));
	return (best);
}

static struct akd_node *
find_nearest_node(const struct akd_param_block *pb, struct akd_node *root,
    const akd_userdata_t *key, unsigned depth)
{

	return find_nearest_node_ex(pb, root, key, depth, 0);
}

static int
tree_walk(struct akd_node *root, akd_node_cb cb, unsigned depth, unsigned flags)
{
	int error;

	if (root->an_left) {
		error = tree_walk(root->an_left, cb, depth + 1, flags);
		if (error)
			return (error);
	}

	error = cb(depth, root->an_userdata);
	if (error)
		return (error);

	if (root->an_right)
		error = tree_walk(root->an_right, cb, depth + 1, flags);
	return (error);
}

/*
 * API routines
 */
EXPORT_SYM int
akd_create(akd_userdata_t *items, size_t nmemb,
    const struct akd_param_block *pb, struct akd_tree **tree_out)
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

EXPORT_SYM const akd_userdata_t *
akd_find_nearest(const struct akd_tree *tree, const akd_userdata_t *key)
{
	struct akd_node *n;

	if (tree->at_root == NULL)
		return NULL;

	n = find_nearest_node(&tree->at_params, tree->at_root, key, 0);
	if (n == NULL)
		return NULL;

	return (n->an_userdata);
}

EXPORT_SYM const akd_userdata_t *
akd_find_nearest_ex(const struct akd_tree *tree, const akd_userdata_t *key, unsigned flags)
{
	struct akd_node *n;

	if (flags & ~AKD_NOT_EQUAL)
		return NULL;

	if (tree->at_root == NULL)
		return NULL;

	n = find_nearest_node_ex(&tree->at_params, tree->at_root, key, 0,
	    flags);
	if (n == NULL)
		return NULL;

	return (n->an_userdata);
}

EXPORT_SYM int
akd_tree_walk(const struct akd_tree *tree, akd_node_cb cb, unsigned flags)
{

	if (tree->at_root == NULL)
		return (EINVAL);

	if (flags != 0)
		return (EINVAL);

	return (tree_walk(tree->at_root, cb, 0, flags));
}
