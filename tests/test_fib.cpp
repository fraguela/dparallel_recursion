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
/// \file     test_fib.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/** In fib(n) on nprocs processors, the process with less work gets fib(n - 2 * log2(nprocs))
 and the one with more work fib(n - log2(nprocs)), setting the limit for performance.
 In general, if each process has nthreads threads and we want to have tasks_per_thread tasks
 for each one, the largest task will be fib(n - log2(nprocs * nthreads * tasks_per_thread))
 */

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"

using namespace dpr;

int n = 26;
int rank, nprocs;
int tasks_per_thread = 1;
int nthreads = 8;

size_t sfib;

struct FibInfo : public DInfo<int, 2> {
  
  FibInfo() : DInfo<int, 2>(nthreads * tasks_per_thread)
  {}
  
  static int is_base(int t) noexcept {
    return t < 2;
  }
  
  static int child(int i, int c) noexcept {
    return c - i - 1;
  }

  static float cost(int i) noexcept {
    const float c = pow(1.61803, i); // static_cast<float>(1 << std::max(0, i - 18));
    // printf("C Cost %d = %f\n", i, c);
    return c;
  }

};

//To be used with partitioner::custom()
struct FibInfoCustom : public DInfo<int, 2> {

   int maxn_task;
   
   FibInfoCustom() : DInfo<int, 2>() {
     maxn_task = n; //calculate n - log2(nprocs * nthreads * tasks_per_thread)
     for (int mynprocs = nprocs * nthreads * tasks_per_thread; mynprocs > 1; mynprocs >>= 1) {
       maxn_task--;
     }
   }
  
  static int is_base(int t) {
    return t < 2;
  }
  
  static int child(int i, int c) {
    return c - i - 1;
  }
  
  static float cost(int i) noexcept {
    const float c = (i < 15) ? 377.f : pow(1.61803, i-1); // static_cast<float>(1 << std::max(0, i - 18));
    // printf("C Cost %d = %f\n", i, c);
    return c;
  }

  bool do_parallel(const int t) const {
    //printf("%d>=%d\n", t , maxn_task);
    return t >= maxn_task;
  }

};

struct Fib: public EmptyBody<int, size_t> {
  size_t base(int n) {
    return n;
  }
  
  size_t post(int i, size_t* r) {
    return r[0] + r[1];
  }
};


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

int test(int n, const int flags, const char * const flag_c)
{ static int NumTest = 0;

  using Chrono_t = std::chrono::high_resolution_clock;

  NumTest++;

  if (rank && (flags & ReplicateInput)) {
    n = 0; //to see that the input replication is effective
  }

  Chrono_t::time_point t0 = Chrono_t::now();
  
  size_t r1 = dparallel_recursion<size_t> (n, FibInfo(), Fib(), partitioner::automatic(), flags);
  
  Chrono_t::time_point t1 = Chrono_t::now();
  
  size_t r2 = dparallel_recursion<size_t> (n, FibInfoCustom(), Fib(), partitioner::custom(), flags);
  
  Chrono_t::time_point t2 = Chrono_t::now();
  
  std::chrono::duration<float> automatic_time_s = t1 - t0;
  std::chrono::duration<float> custom_time_s = t2 - t1;
  
  if (rank == 0) {
    printf("flags=%s\n", flag_c);
    printf("par_fib(%d): %lu (automatic) (t=%f)\npar_fib(%d): %lu (  custom )  (t=%f)\nseq_fib(%d): %lu\n", n, r1, automatic_time_s.count(), n, r2, custom_time_s.count(), n, sfib);
  }

  //to see that the output replication is effective
  if ( (rank == 0) || (flags & ReplicateOutput) ) {
    int success = (r1 == sfib) && (r2 == sfib);
    printf("[%d] TEST%d: %s\n", rank, NumTest, success ? "*SUCCESS*" : " FAILURE!");
    return success ? 0 : -1;
  }
  
  return 0;
}

int main(int argc, char** argv) {
  int provided;
  
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  assert(provided >= MPI_THREAD_SERIALIZED);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
  if (getenv("OMP_NUM_THREADS"))
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  
  tbb::task_scheduler_init init(nthreads);
  
  if (argc > 1)
    n = atoi(argv[1]);
  
  if (argc > 2)
    tasks_per_thread = atoi(argv[2]);
  
  if (rank == 0) {
    printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
  }
  
  sfib = seq_fib(n);

  int ret = test(n, DefaultBehavior, "DefaultBehavior")
  // || test(n, GreedyParallel, "GreedyParallel")
  || test(n, ReplicatedInput, "ReplicatedInput")
  || test(n, ReplicatedInput, "ReplicateInput")
  || test(n, ReplicateOutput, "ReplicateOutput")
  || test(n, Balance, "Balance")
  || test(n, Balance|UseCost, "Balance|UseCost")
  // || test(n, Balance|UseCost|GreedyParallel, "Balance|UseCost|GreedyParallel")
  ;
  
  MPI_Finalize();
  
  return ret;
}
