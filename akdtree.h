#pragma once

/*
 * A KD-tree implementation. MIT-licensed, see LICENSE.
 *
 * Somewhat derived from the also-MIT-licensed OfflineReverseGeocode by Daniel
 * Glasson.
 */

/*
 * Opaque type for userdata (produces warnings where casts to/from void would
 * not
 */
typedef uint8_t akd_userdata_t;

struct akd_param_block {
	unsigned	  ap_k;			/* K dimensions */
	unsigned	  ap_flags;		/* Flags */

	size_t		  ap_size;		/* Item size (total) */

	/* Compare 'a' to 'b' in dimension 'dim'. 'dim' in [0, K) */
	int		(*ap_cmp)(unsigned dim, const akd_userdata_t *a,
				  const akd_userdata_t *b);

	/*
	 * Compare 'n1' to 'n2' (by squared distance from node to 'key').
	 * E.g.
	 *   dist1 = squared_distance(n1, key);
	 *   dist2 = squared_distance(n2, key);
	 *   if (dist1 > dist2)
	 *     return (1);
	 *   else if (dist1 == dist2)
	 *     return (0);
	 *   else
	 *     return (-1);
	 */
	int		(*ap_dist_cmp)(const akd_userdata_t *key,
				       const akd_userdata_t *n1,
				       const akd_userdata_t *n2);

	/*
	 * Compare 'n1' to 'n2' asymmetrically (by axis squared distance from
	 * n1 to key, and total squared distance from n2 to key).  E.g.
	 *
	 *   dist1 = axis_squared_distance(n1, key, dim);
	 *   dist2 = squared_distance(n2, key);
	 *   if (dist1 > dist2)
	 *     return (1);
	 *   else if (dist1 == dist2)
	 *     return (0);
	 *   else
	 *     return (-1);
	 */
	int		(*ap_dist_acmp)(const akd_userdata_t *key,
					const akd_userdata_t *n1,
					unsigned dim,
					const akd_userdata_t *n2);
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
int akd_create(akd_userdata_t *items, size_t nmemb,
    const struct akd_param_block *pb, struct akd_tree **tree_out);

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
const akd_userdata_t *akd_find_nearest(const struct akd_tree *tree,
    const akd_userdata_t *key);


/*
 * akd_find_nearest_ex - Find the nearest item to the given item, with flags.
 *
 * Returns NULL if the tree is empty.
 *
 * Returns NULL if AKD_NOT_EQUAL is passed and the tree only contains one node
 * equal to key.
 */
#define	AKD_NOT_EQUAL	0x1u
const akd_userdata_t *akd_find_nearest_ex(const struct akd_tree *tree,
    const akd_userdata_t *key, unsigned flags);


/*
 * Tree traversal - Walk the KD tree in infix order, invoking cb on each piece
 * of data.
 *
 * If a callback returns a non-zero value, halt the traversal and return the
 * error from akd_tree_walk.
 *
 * Returns EINVAL if given an invalid tree object.
 * Returns other errors if user-supplied callback does.
 * Otherwise, returns zero.
 */
typedef int (*akd_node_cb)(unsigned level, const akd_userdata_t *data);
int akd_tree_walk(const struct akd_tree *, akd_node_cb, unsigned flags);
