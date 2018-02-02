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
/// \file     test_PrioritizeDM.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <tbb/task_scheduler_init.h>
#include <cstdio>
#include <cstdlib>
#include "dparallel_recursion/Range.h"
#include "dparallel_recursion/dparallel_recursion.h"

using namespace dpr;

int n = 64;
int rank, nprocs;
int tasks_per_thread = 1;
int nthreads = 8;

int seq_alg(Range r) {
  int res = 0;
  for (int k = r.start; k <= r.end; k++)
    res += k;
  return res;
}

struct MyInfo : public DInfo<Range, 2> {

  
  MyInfo() : DInfo<Range, 2>(nthreads * tasks_per_thread)
  {}
  
  bool is_base(const Range& r) const {
    return r.inclusiveSize() < (n / nprocs);
  }
  
  Range child(int i, const Range& r) const {
    if (!i && (r.inclusiveSize() >= n / nprocs)) {
      printf("[%d] Splitting range (%d, %d)\n", rank, r.start, r.end);
    }
    int mid = r.start + (r.end - r.start) / 2;
    return i ? Range { mid+1, r.end } : Range { r.start, mid };
  }

};


struct MyBody : public EmptyBody<Range, int> {
  
  int base(Range r) {
    return seq_alg(r);
  }
  
  int post(const Range& r, const int* v) {
    
    if (r.inclusiveSize() >= n / nprocs) {
      printf("[%d] Reducing range (%d, %d)\n", rank, r.start, r.end);
    }
    
    return v[0] + v[1];
  }

};


int test(int n, const int flags, const char * const flag_c) {

  if (rank && (flags & ReplicateInput)) {
    n = 0; //to see that the input replication is effective
  }

  Range input {0, n};
  
  int r1 = dparallel_recursion<int> (input, MyInfo(), MyBody(), partitioner::automatic(), flags);
  
  
  int s = seq_alg(input);
  
  if (rank == 0) {
    printf("flags=%s\n", flag_c);
    printf("par_add_range(%d): %d (automatic)\nseq_add_range(%d): %d\n", n, r1, n, s);
  }
  
  //to see that the output replication is effective
  if ( (rank == 0) || (flags & ReplicateOutput) ) {
    int success = (r1 == s);
    printf("[%d] %s\n", rank, success ? "*SUCCESS*" : " FAILURE!");
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
  
  int ret = test(n, DefaultBehavior, "DefaultBehavior");
  ret += test(n, PrioritizeDM, "PrioritizeDM");
  
  MPI_Finalize();
  
  return ret;
}
