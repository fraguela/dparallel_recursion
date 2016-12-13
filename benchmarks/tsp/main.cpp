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
/// \file     main.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/* For copyright information, see olden_v1.0/COPYRIGHT */

#include "tsp.h"
#include <cstdlib>
#include <cstdio>
#include <sys/time.h>

using namespace dpr;

#define chatting printf


Tree tsp(Tree t, int sz);
int TreeInfo::sz = 0;

int nprocs, nthreads=8;

void print_tree(Tree t) {
	Tree left, right;
	double x, y;

	if (!t)
		return;
	x = t->x;
	y = t->y;
	chatting("x=%f,y=%f\n", x, y);
	left = t->left;
	right = t->right;
	print_tree(left);
	print_tree(right);
}

void print_list(Tree t) {
	Tree tmp;
	double x, y;

	if (!t)
		return;
	x = t->x;
	y = t->y;
	chatting("%f %f\n", x, y);
	for (tmp = t->next; tmp != t; tmp = tmp->next) {
		x = tmp->x;
		y = tmp->y;
		chatting("%f %f\n", x, y);
	}
}


int main(int argc, char *argv[]) {
	int provided, rank;
	int num;
	struct timeval t0, t1, tt;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	assert(provided >= MPI_THREAD_SERIALIZED);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

	if (getenv("OMP_NUM_THREADS"))
		nthreads = atoi(getenv("OMP_NUM_THREADS"));

        pr_init(nthreads);
  
	flag = 0;
	num = dealwithargs(argc, argv);
	
	if(rank == 0) {
		chatting("Building tree of size %d\n", num);
		//t = (Tree)malloc(sizeof(*t));
		//build_tree(t, num, 0, 0.0, 1.0, 0.0, 1.0);
	}
  
        MPI_Barrier(MPI_COMM_WORLD); //So that we all measure from the same point
        gettimeofday(&t0, NULL);
  
	Tree t = new tree(num, 0, 0.0, 1.0, 0.0, 1.0);
	TreeInfo info;
	TreeInfo::sz = 1;
	dparallel_recursion<void>(t, info, TSPBuildBody(), partitioner::automatic(), ReplicatedInput | DistributedOutput);

        MPI_Barrier(MPI_COMM_WORLD); //So that we all measure up to the same point
	
        if(rank == 0) {
		gettimeofday(&t1, NULL);
		timersub(&t1, &t0, &tt);
		printf("  alloc time: %f\n", (tt.tv_sec * 1000000 + tt.tv_usec) / 1000000.0);

		if (!flag)
			chatting("Past build\n");
		if (flag)
			chatting("newgraph\n");
		if (flag)
			chatting("newcurve pts\n");
	}

        MPI_Barrier(MPI_COMM_WORLD); //So that we all measure from the same point
	gettimeofday(&t0, NULL);
	TreeInfo::sz = 150;
	dparallel_recursion<Tree>(t, info, TSPBody(), partitioner::automatic());
	//tsp(t, 150);
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &tt);

	if(rank == 0) {
		if (flag)
			print_list(t);
		if (flag)
			chatting("linetype solid\n");
                printf("Nprocs=%d threads=%d tasks_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
		printf("compute time: %f\n", (tt.tv_sec * 1000000 + tt.tv_usec) / 1000000.0);
	}
	
	MPI_Finalize();
	return 0;
}
