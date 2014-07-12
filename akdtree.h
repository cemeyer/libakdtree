#pragma once

/*
 * A KD-tree implementation. MIT-licensed, see LICENSE.
 *
 * Somewhat derived from the also-MIT-licensed OfflineReverseGeocode by Daniel
 * Glasson.
 */

struct akd_param_block {
	unsigned	  ap_k;			/* K dimensions */
	size_t		  ap_size;		/* item size */

	/* Compare 'a' to 'b' in dimension 'dim'. 'dim' in [0, K) */
	int		(*ap_cmp)(unsigned dim, const void *a, const void *b);

	unsigned	  ap_flags;		/* Flags: */
#define AKD_SINGLE_PREC	0x0001		/* If unset, defaults to double. */
#define AKD_INTEGRAL	0x0002		/* If unset, defaults to floats. */

	/*
	 * Pick one set of functions to implement and set ap_flags
	 * appropriately:
	 */
	union {
	struct {
		/* Return squared distance between two items. */
		double	(*ap_squared_dist)(const void *a, const void *b);
		/* Return squared distance between two items in 'dim' axis */
		double	(*ap_axis_squared_dist)(const void *a, const void *b,
						unsigned dim);
	} _double;
	struct {
		float	(*ap_squared_dist)(const void *a, const void *b);
		float	(*ap_axis_squared_dist)(const void *a, const void *b,
						unsigned dim);
	} _single;
	struct {
		uint64_t	(*ap_squared_dist)(const void *a, const void *b);
		uint64_t	(*ap_axis_squared_dist)(const void *a,
							const void *b,
							unsigned dim);
	} _uint64;
	struct {
		uint32_t	(*ap_squared_dist)(const void *a, const void *b);
		uint32_t	(*ap_axis_squared_dist)(const void *a,
							const void *b,
							unsigned dim);
	} _uint32;
	} _u;
};

struct akd_tree;

/*
 * akd_create - Create a balanced KD-tree.
 *
 * 'items' is an array of K-dimensional items in space.
 * 'nmemb' is the number of elements in 'items'.
 * 'size' is the size, in bytes, of each item.
 * 'pb' is the KD parameters for this tree. They are copied and the memory
 *     backing this pointer is not needed after create() returns.
 *
 * On success, returns zero (no error), setting '*tree_out' to a KD-tree
 * handle. On error, a non-zero error code is returned.
 *
 * N.B., 'items' will be shuffled multiple times. It must be modifiable by
 * akd_create(). However, its contents will be copied in to the tree, so the
 * memory backing them can be released or reused after create() returns. If
 * your actual items are large and copying them is expensive or cannot be
 * represented with just memcpy(3), consider passing an array of pointers.
 *
 * Some potential sources of error:
 *   * EINVAL: Bad inputs, K of zero, or invalid flags set in ap_flags.
 *   * ENOMEM: Malloc failed.
 */
int akd_create(void *items, size_t nmemb, const struct akd_param_block *pb,
    struct akd_tree **tree_out);

void akd_free(struct akd_tree *tree);


/*
 * akd_get_param_block - Gets parameters used to create this KD-tree.
 */
const struct akd_param_block *akd_get_param_block(const struct akd_tree *tree);


/*
 * akd_find_nearest - Find the nearest item to the given item.
 *
 * Returns NULL if the tree is empty.
 */
const void *akd_find_nearest(const struct akd_tree *tree, const void *key);
