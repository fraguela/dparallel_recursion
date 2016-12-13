/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015-2016 Carlos H. Gonzalez, Basilio B. Fraguela. Universidade da Coruna
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

///
/// \file     par-alloc.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/* For copyright information, see olden_v1.0/COPYRIGHT */

/* tree-alloc.c
 */
#include <cstdlib>
#include "tree.h"

#ifdef SCALABLE_ALLOCATION
#include <tbb/scalable_allocator.h>
#define NODEALLOCATION (scalable_malloc(sizeof(tree_t)))
#else
#define NODEALLOCATION
#endif

tree_t **partition_row;
int myleaves, par_level;

static int above_level = 1;

tree_t *TreeAlloc_sec(int level) {
  struct tree *new_t = new NODEALLOCATION tree_t(level);
  
  if (level > above_level) {
    new_t->left = TreeAlloc_sec(level - 1);
    new_t->right = TreeAlloc_sec(level - 1);
  }
  return new_t;
}

tree_t *TreeAlloc(int level) {
	struct tree *new_t = new NODEALLOCATION tree_t(level);

	if (level > above_level) {
          if (level > par_level) {
            new_t->left = _Cilk_spawn TreeAlloc(level - 1);
            new_t->right = TreeAlloc(level - 1);
            _Cilk_sync;
          } else {
            new_t->left = TreeAlloc_sec(level - 1);
            new_t->right = TreeAlloc_sec(level - 1);
          }
	}
	return new_t;
}

/// calculates topmost level with parallelism in a tree with level \c level so that there are
/// nthreads * tasks_per_thread tasks
void set_par_level(int level) {
  for (par_level = level; (1 << (level - par_level)) < (nthreads * tasks_per_thread); par_level--);
}

tree_t *TreeAlloc_par(int level) {
  tree_t *t;
  set_par_level(level);

  t = TreeAlloc(level);
  
  return t;
}

tree_t *TreeAlloc(int level, int rank, int nprocs) {
  tree_t * root = NULL;
  int nleaves;
  int max_level_task = level;

  for (nleaves = 1; max_level_task && (nleaves < nprocs); nleaves *=2) {
    max_level_task--;
  }
  
  myleaves = nleaves / nprocs;
  if (rank < (nleaves % nprocs)) {
    myleaves++;
  }

  partition_row = new tree_t * [myleaves];
  
  for (int i = 0; i < myleaves; i++) {
    partition_row[i] = TreeAlloc_par(max_level_task);
  }
  
  if (!rank) {
    above_level = max_level_task + 1;
    
    tree_t * root = TreeAlloc_par(level);
    
    above_level = 1;
  }
  
  return root;
}