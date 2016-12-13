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
/// \file     node.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/* For copyright information, see olden_v1.0/COPYRIGHT */

/*
 * Initial input is replicated from argv.
 * The tree is built distributed.
 * All the nodes get the addition result.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include "tree.h"
#include <dparallel_recursion/dparallel_recursion.h>

#ifdef SCALABLE_ALLOCATION
#include <tbb/scalable_allocator.h>
#define NODEALLOCATION (scalable_malloc(sizeof(tree_t)))
#else
#define NODEALLOCATION
#endif

using namespace dpr;

#define chatting printf

int dealwithargs(int argc, char *argv[]);

int nprocs, nthreads = 8;

struct TreeInfo: public DInfo<tree_t *, 2> {
  
        /* To be used with partitioner::custom()
         
         int max_level_task;
         
	TreeInfo() : DInfo<tree_t *, 2>() {
          max_level_task = level;
          for (int mynprocs = nprocs * nthreads * tasks_per_thread; mynprocs > 1; mynprocs >>= 1) {
            max_level_task--;
          }
        }
        */
  
        TreeInfo() : DInfo<tree_t *, 2>(nthreads * tasks_per_thread)
        {}
  
	bool is_base(const tree_t *t) const {
		return t->level == 1;
	}

        /* To be used with partitioner::custom()
	bool do_parallel(const tree_t *t) const {
                //printf("%d> %d\n", t->level , max_level_task);
		return t->level > max_level_task;
	}
        */
  
	tree_t *child(int i, tree_t *t) const {
		return (i == 0) ? t->left : t->right;
	}

};

struct ParAllocBody : EmptyBody<tree_t *, void> {
	void pre_rec(tree_t *t) {
            t->right = new NODEALLOCATION tree_t(t->level-1);
            t->left = new NODEALLOCATION tree_t(t->level-1);
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

int main(int argc, char *argv[]) {
	int provided, rank;
	tree_t *root;
	int i, result = 0;
	struct timeval t0, t1, t;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	assert(provided >= MPI_THREAD_SERIALIZED);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
	if (getenv("OMP_NUM_THREADS"))
		nthreads = atoi(getenv("OMP_NUM_THREADS"));
  
	pr_init(nthreads);

	(void) dealwithargs(argc, argv);
	if (rank == 0) {
		chatting("Treeadd with %d levels and %d iterations\n", level, iters);
		chatting("About to enter TreeAlloc\n");
	}
  
        MPI_Barrier(MPI_COMM_WORLD); //So that we all measure from the same point
        gettimeofday(&t0, NULL);
  
	root = new tree_t(level);
	TreeInfo ti;
	dparallel_recursion<void>(root, ti, ParAllocBody(), partitioner::automatic(), ReplicatedInput|DistributedOutput);
  
        MPI_Barrier(MPI_COMM_WORLD); //So that we all measure up to the same point
	//printf("[%d] root = %p\n", rank, root);

	if(rank == 0) {
		gettimeofday(&t1, NULL);
		timersub(&t1, &t0, &t);
		printf("[%d]  alloc time: %f\n", rank, (t.tv_sec * 1000000 + t.tv_usec) / 1000000.0);
                chatting("About to enter TreeAdd\n");
	}
  
	MPI_Barrier(MPI_COMM_WORLD); //So that we all measure from the same point
	gettimeofday(&t0, NULL);
	for (i = 0; i < iters; i++) {
		//if (rank == 0)
		//	printf("Iteration %d...\n", i);
		result = dparallel_recursion<int> (root, ti, TreeAddBody(), partitioner::automatic(), ReplicateOutput);
        }
        //MPI_Barrier(MPI_COMM_WORLD); //Not necessary thanks to ReplicateOutput
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

	if (rank == 0) {
		printf("Nprocs=%d threads=%d tasks_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
	        chatting("[%d]Received result: %d\n", rank, result);
	        chatting("[%d]compute time: %f\n", rank, (t.tv_sec * 1000000 + t.tv_usec) / 1000000.0);
        }
  
	MPI_Finalize();
	return 0;
}
