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
/// \file     test_ranges
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdio>
#include <cstdlib>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/DRange.h"
#include "dparallel_recursion/dparallel_recursion.h"

using namespace dpr;

int n = 64;
int rank, nprocs;
int tasks_per_thread = 1;
int nthreads = 8;

template<bool EXCLUSIVE>
int seq_alg(Range r) {
  int res = 0;
  int end = EXCLUSIVE ? r.end - 1 : r.end;
  for (int k = r.start; k <= end; k++)
    res += k;
  return res;
}

template<bool EXCLUSIVE>
struct MyBody : public EmptyBody<Range, int> {
  
  int base(Range r) {
    return seq_alg<EXCLUSIVE>(r);
  }
  
  int post(const Range& r, const int* v) {
    int res = 0;
    for (int i  = 0; i < r.nchildren; i++) {
      res += v[i];
    }
    
    /*
    if (r.inclusiveSize() >= n / nprocs) {
      printf("[%d] Reducing PROC range [%d, %d%c (%d children)->%d\n", rank, r.start, r.end, EXCLUSIVE ? ')' : ']', r.nchildren, res);
    }
    */
    
    return res;
  }

};


int test(int n, int ntasks) {

  Range input {0, n};
  
  int r1 = dparallel_recursion<int> (input, ExclusiveRangeDInfo(input, nprocs, ntasks), MyBody<true>(), partitioner::automatic());
  
  int s = seq_alg<true>(input);
  
  input.end--;
  
  int r2 = dparallel_recursion<int> (input, InclusiveRangeDInfo(input, nprocs, ntasks), MyBody<false>(), partitioner::automatic());
  
  if (rank == 0) {
    printf("par_add_range(n=%d): %d (exclusive)\n", n, r1);
    printf("par_add_range(n=%d): %d (inclusive)\n", n, r2);
    printf("seq_add_range(n=%d): %d\n", n, s);
  }
  
  //to see that the output replication is effective
  if ( rank == 0 ) {
    int success = (r1 == s) && (r2 == s);
    printf("[%d] %d tasks/node: %s\n", rank, ntasks, success ? "*SUCCESS*" : " FAILURE!");
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
  
  int ret = 0;
  for (int i = -2; i <= 2; i++) {
    for (int ntasks=1; !ret && (ntasks < 22); ntasks++) {
      ret += test(100 * nprocs + i, ntasks);
    }
  
    for (int ntasks=1; !ret && (ntasks < 22); ntasks++) {
      ret += test(23 * nprocs + i , ntasks);
    }
  }
  
  
  MPI_Finalize();
  
  return ret;
}
