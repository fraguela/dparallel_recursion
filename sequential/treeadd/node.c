/* For copyright information, see olden_v1.0/COPYRIGHT */

/* node.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "tree.h"

#define chatting printf

int dealwithargs(int argc, char *argv[]);
int TreeAdd(/* tree_t *t */);
tree_t *TreeAlloc(/* int level */);


int main(int argc, char *argv[]) {
  tree_t *root;
  int i, result = 0;
  struct timeval t0, t1, t;
  
  (void) dealwithargs(argc, argv);
  
  chatting("Treeadd with %d levels and %d iterations\n", level, iters);
  
  chatting("About to enter TreeAlloc\n");
  gettimeofday(&t0, NULL);
  root = TreeAlloc(level);
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  printf("  alloc time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
  chatting("About to enter TreeAdd\n");
  
  gettimeofday(&t0, NULL);
  for (i = 0; i < iters; i++) {
    /* fprintf(stdout, "Iteration %d...", i); */
    result = TreeAdd(root);
    /* fprintf(stdout, "done\n"); */
  }
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  printf("compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
  
  chatting("Received result of %d\n", result);
  exit(0);
}

/* TreeAdd:
 */
int TreeAdd(t)
register tree_t *t; {
  if (t == NULL) {
    return 0;
  } else {
    int leftval, rightval;

    leftval = TreeAdd(t->left);
    rightval = TreeAdd(t->right);

    return leftval + rightval + t->val;
  }
} /* end of TreeAdd */

