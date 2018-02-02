/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015-2018 Carlos H. Gonzalez, Basilio B. Fraguela. Universidade da Coruna
 
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
/// \file     test_treeadd.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <stdio.h>
#include <stdlib.h>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"
#include "../benchmarks/treeadd/tree.h"


#ifdef SCALABLE_ALLOCATION
#include <tbb/scalable_allocator.h>
#define NODEALLOCATION (scalable_malloc(sizeof(tree_t)))
#else
#define NODEALLOCATION
#endif

using namespace dpr;
using namespace tbb;

int rank, nprocs, nthreads = 8;

int level = 20, iters = 10, tasks_per_thread = 1;

int dealwithargs(int argc, char *argv[]) {
  if (argc > 3)
    tasks_per_thread = atoi(argv[3]);
  
  if (argc > 2)
    iters = atoi(argv[2]);
  
  if (argc > 1)
    level = atoi(argv[1]);
  
  return level;
}

struct TreeInfo: public DInfo<tree_t *, 2> {
  
  TreeInfo() : DInfo<tree_t *, 2>(nthreads * tasks_per_thread)
  {}
  
  bool is_base(const tree_t *t) const {
    return t->level == 1;
  }
  
  tree_t *child(int i, tree_t *t) const {
    return (i == 0) ? t->left : t->right;
  }
  
};

/// To be used with partitioner::custom()
struct TreeInfoCustom: public DInfo<tree_t *, 2> {
  
  
   int max_level_task;
   
   TreeInfoCustom() : DInfo<tree_t *, 2>() {
     max_level_task = level;
     for (int mynprocs = nprocs * nthreads * tasks_per_thread; mynprocs > 1; mynprocs >>= 1) {
       max_level_task--;
     }
   }
  
  bool is_base(const tree_t *t) const {
    return t->level == 1;
  }
  
   bool do_parallel(const tree_t *t) const {
     //printf("%d> %d\n", t->level , max_level_task);
     return t->level > max_level_task;
   }
  
  tree_t *child(int i, tree_t *t) const {
    return (i == 0) ? t->left : t->right;
  }
  
};

struct ParAllocBody : EmptyBody<tree_t *, void> {
  void pre(tree_t *t) {
    if (t->level > 1) {
      t->right = new NODEALLOCATION tree_t(t->level-1);
      t->left = new NODEALLOCATION tree_t(t->level-1);
    }
  }
};

struct TreeAddBody : EmptyBody<tree_t *, int> {
  int base(tree_t *t)  {
    return t->val;
  }
  
  int post(tree_t *t, int *r)  {
    return t->val + r[0] + r[1];
  }
};

template<typename INFO, typename PARTITIONER>
int test() {
  int result = 0;
  INFO ti;
  
  tree_t *root = new tree_t(level);

  dparallel_recursion<void>(root, ti, ParAllocBody(), PARTITIONER(), ReplicatedInput|DistributedOutput);
  
  for (int i = 0; i < iters; i++) {
    result = dparallel_recursion<int> (root, ti, TreeAddBody(), PARTITIONER(), ReplicateOutput);
  }
  
  int success = (result == ( (1 << level) - 1));
  
  if (rank == 0) {
    printf("[%d]Received result: %d\n", rank, result);
    printf("[%d] %s\n", rank, success ? "*SUCCESS*" : " FAILURE!");
  }
  
  return success ? 0 : -1;
}

int main(int argc, char *argv[]) {
  int provided;

  
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  assert(provided >= MPI_THREAD_SERIALIZED);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
  if (getenv("OMP_NUM_THREADS"))
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  task_scheduler_init init(nthreads);
  
  (void) dealwithargs(argc, argv);
  if (rank == 0) {
    printf("Treeadd with %d levels and %d iterations\n", level, iters);
    printf("Nprocs=%d threads=%d tasks_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
    printf("Running with automatic partitioning\n");
  }

  int ret = test<TreeInfo, partitioner::automatic>();

  if (rank == 0) {
    printf("Running with custom partitioning\n");
  }
  
  ret += test<TreeInfoCustom, partitioner::custom>();
  
  MPI_Finalize();
  
  return ret;
}
