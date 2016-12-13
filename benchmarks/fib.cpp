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
/// \file     fib.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/*
 *  Input is replicated (taken from argv).
 *  Result is only obtained in rank 0.
 */

/** In fib(n) on nprocs processors, the process with less work gets fib(n - 2 * log2(nprocs))
    and the one with more work fib(n - log2(nprocs)), setting the limit for performance.
    In general, if each process has nthreads threads and we want to have tasks_per_thread tasks
    for each one, the largest task will be fib(n - log2(nprocs * nthreads * tasks_per_thread))
  */

#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <dparallel_recursion/dparallel_recursion.h>
#include <cmath>

using namespace dpr;

int n = 25;
int nprocs;
int tasks_per_thread = 1;
int nthreads = 8;

struct FibInfo : public DInfo<int, 2> {

        /* To be used with partitioner::custom()
         
         int maxn_task;
         
	FibInfo() : DInfo<int, 2>() {
          maxn_task = n; //calculate n - log2(nprocs * nthreads * tasks_per_thread)
          for (int mynprocs = nprocs * nthreads * tasks_per_thread; mynprocs > 1; mynprocs >>= 1) {
            maxn_task--;
          }
        }
        */
  
        FibInfo() : DInfo<int, 2>(nthreads * tasks_per_thread)
        {}
  
	static int is_base(const int t) {
          return t < 2;
	}

	static int child(int i, const int c) {
          return c - i - 1;
	}

        /* To be used with partitioner::custom()
	bool do_parallel(const int t) const {
          //printf("%d>=%d\n", t , maxn_task);
          return t >= maxn_task;
	}
        */
  
        static float cost(int i) noexcept {
          return powf(1.61803f, i);
        }
};

struct Fib: public EmptyBody<int, size_t> {
	static size_t base(int n) {
          return n;
	}

	static size_t post(int i, size_t* r) {
          return r[0] + r[1];
	}
};

#ifndef NO_VALIDATE
size_t seq_fib(int n) {
	if (n < 2) {
		return n;
	} else {
		size_t v[2], r = 1;
		v[0] = 1;
		v[1] = 1;

		while (n > 2 ) {
			r = v[0] + v[1];
			v[0] = v[1];
			v[1] = r;
			n--;
		}

		return r;
	}
}
#endif

int main(int argc, char** argv) {
	int provided, rank;
	size_t r1;
	struct timeval t0, t1, t;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	assert(provided >= MPI_THREAD_SERIALIZED);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
	if (getenv("OMP_NUM_THREADS"))
		nthreads = atoi(getenv("OMP_NUM_THREADS"));

	pr_init(nthreads);

	if (argc > 1)
		n = atoi(argv[1]);

	if (argc > 2)
		tasks_per_thread = atoi(argv[2]);

	gettimeofday(&t0, NULL);
	r1 = dparallel_recursion<size_t> (n, FibInfo(), Fib(), partitioner::automatic(), ReplicatedInput|UseCost);
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

	if (rank == 0) {
		printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
		printf("compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
                printf("fib(%d): %lu\n", n, r1);
#ifndef NO_VALIDATE
                size_t sfib = seq_fib(n);
                printf("%s\n", (r1 == sfib) ? "*SUCCESS*" : " FAILURE!");
#endif
	}

	MPI_Finalize();
	return 0;
}
