/* For copyright information, see olden_v1.0/COPYRIGHT */

/* tree-alloc.c
 */

#include <stdlib.h>
#include "tree.h"

#ifdef SCALABLE_ALLOCATION
#include <tbb/scalable_allocator.h>
#define NODEALLOCATION scalable_malloc
#else
#define NODEALLOCATION malloc
#endif

tree_t *TreeAlloc(level)
	int level; {

	if (level == 0) {
		return NULL;
	} else {
		struct tree *new, *right, *left;

		new = (struct tree *) NODEALLOCATION(sizeof(tree_t));
		left = TreeAlloc(level - 1);
		right = TreeAlloc(level - 1);
		new->val = 1;
		new->level = level;
		new->right = (struct tree *) right;
		new->left = (struct tree *) left;
		return new;
	}
}
