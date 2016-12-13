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

#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>
#include "tree.h"

#define chatting printf

int dealwithargs(int argc, char *argv[]);

int nthreads;

int treeadd_rec_sec(tree_t* root) {
  if(!root->left) {
    return root->val;
  } else {
    return treeadd_rec_sec(root->left) + treeadd_rec_sec(root->right) + root->val;
  }
}

int treeadd_rec(tree_t* root) {
  if(!root->left || (root->level <= par_level))
    return treeadd_rec_sec(root);
  else {
    int n1, n2;
#pragma omp task default(shared) untied
    n1 = treeadd_rec(root->left);
    n2 = treeadd_rec(root->right);
#pragma omp taskwait
    return n1 + n2 + root->val;
  }
}

int treeadd_rec_par(tree_t* root) {
  int n;
  set_par_level(root->level);
#pragma omp parallel
#pragma omp single
  n = treeadd_rec(root);
  return n;
}

int treeadd_mpi(tree_t* root, int rank, int nprocs) {
  int result, local_result = 0;
  
  if(root!= NULL) local_result = treeadd_rec_par(root);
  
  for (int i = 0; i < myleaves; i++) {
    local_result += treeadd_rec_par(partition_row[i]);
  }
  
  //std::cout << rank << ' ' << local_result << std::endl;
  MPI_Allreduce(&local_result, &result, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  return result;
}

int main(int argc, char *argv[]) {
	int rank, nprocs;
	tree_t *root;
	int i, result = 0;
	struct timeval t0, t1, t;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
        nthreads = omp_get_max_threads();

	(void) dealwithargs(argc, argv);
	if (rank == 0) {
		chatting("Treeadd with %d levels and %d iterations\n", level, iters);
		chatting("About to enter TreeAlloc\n");
	}
  
        MPI_Barrier(MPI_COMM_WORLD); //So that we all measure from the same point
        gettimeofday(&t0, NULL);
	root = TreeAlloc(level, rank, nprocs);
        MPI_Barrier(MPI_COMM_WORLD); //So that we all measure up to the same point

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
		result = treeadd_mpi(root, rank, nprocs);
	}
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

	if (rank == 0) {
		printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
                chatting("[%d]Received result: %d\n", rank, result);
                chatting("[%d]compute time: %f\n", rank, (t.tv_sec * 1000000 + t.tv_usec) / 1000000.0);
	}

	MPI_Finalize();
	return 0;
}
