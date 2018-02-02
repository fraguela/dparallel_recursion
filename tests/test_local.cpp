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
/// \file     test_local.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdio>
#include <cstdlib>
#include <sys/time.h>
#include <vector>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/Range.h"
#include "dparallel_recursion/parallel_recursion.h"

using namespace dpr;

int n = 32;
int tasks_per_thread = 1, ntasks;
int nthreads = 8;

struct MRange {
  int start, end;
  
  int inclusiveSize() const { return end - start + 1; }
};

template<typename T>
int seq_alg(T r) {
  int res = 0;
  for (int k = r.start; k <= r.end; k++)
    res += k;
  return res;
}

struct MyInfo : public Arity<2> {

  
  MyInfo() : Arity<2> (ntasks)
  {}
  
  bool is_base(const MRange& r) const {
    return r.inclusiveSize() < (n / ntasks);
  }
  
  MRange child(int i, const MRange& r) const {
    if (!i) {
      printf(" Splitting MRange (%d, %d)\n", r.start, r.end);
    }
    int mid = r.start + (r.end - r.start) / 2;
    return i ? MRange { mid+1, r.end } : MRange { r.start, mid };
  }

};

template<typename T>
struct MyBody : public EmptyBody<T, int> {
  
  int base(T r) {
    return seq_alg(r);
  }
  
  int post(const MRange& r, const int* v) {
    
    printf(" Reducing MRange (%d, %d)\n", r.start, r.end);
    
    return v[0] + v[1];
  }

  int post(const Range& r, const int* v) {
    
    printf(" Reducing Range (%d, %d)\n", r.start, r.end);
    
    int res = 0;
    for (int i  = 0; i < r.nchildren; i++) {
      res += v[i];
    }
    return res;
  }
  
};


int test1(int n) {

  MRange input {0, n};
  
  int r1 = parallel_recursion<int> (input, MyInfo(), MyBody<MRange>());
  
  int s = seq_alg(input);

  printf("par_add_MRange(%d): %d\nseq_add_range(%d): %d\n", n, r1, n, s);
  
  int success = (r1 == s);
  printf(" %s\n", success ? "*SUCCESS*" : " FAILURE!");

  return success ? 0 : -1;
}

int test2(int n) {
  
  Range input {0, n};
  
  int r1 = parallel_recursion<int> (input, InclusiveRangeInfo(input, ntasks), MyBody<Range>());
  
  int s = seq_alg(input);
  
  printf("par_add_Range(%d): %d\nseq_add_range(%d): %d\n", n, r1, n, s);
  
  int success = (r1 == s);
  printf(" %s\n", success ? "*SUCCESS*" : " FAILURE!");

  return success ? 0 : -1;
}

int test_pr_pfor(int n, bool with_macro) {
  struct timeval t0, t1, t;
  std::vector<double> v1(n, 0.), v2(n);
  int i;

  for (i = 0; i < n; i++) {
    v2[i] = static_cast<double>(i);
  }
  
  gettimeofday(&t0, NULL);
  for (i = 0; i < n; i++) {
    v1[i] += 2 * v2[i];
  }
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  printf("  serial pfor time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
  
  Range pr_tmp_range {0, n};
  auto f = [&](int i, int j) { while(i<j){ v1[i] += 2 * v2[i]; i++;} };
  auto bf= internal::make_generic_pr_for_body(f); //not indented for users
  parallel_recursion<void>(pr_tmp_range, ExclusiveRangeInfo(pr_tmp_range, ntasks), bf);
  
  for (i = 0; i < n; i++) {
    if(v1[i] != 4 * v2[i]) {
      puts("  FAILURE!");
      return -1;
    }
  }
  
  gettimeofday(&t0, NULL);
  
  if (with_macro) {
    pr_pfor(i, 0, n, ntasks, v1[i] += 2 * v2[i] );
  } else {
    pfor(0, n, ntasks, [&](int i) { v1[i] += 2 * v2[i]; });
  }
  
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  printf("parallel pfor time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);

  for (int i = 0; i < n; i++) {
    if(v1[i] != 6 * v2[i]) {
      puts("  FAILURE!");
      return -1;
    }
    v1[i] = 0;
  }
  
  puts(" *SUCCESS*");
  return 0;
}

int test_pr_pfor_reduce(int n, bool with_macro) {
  struct timeval t0, t1, t;
  std::vector<int> v1(n);
  int i, s=0, r1;
  
  for (i = 0; i < n; i++) {
    v1[i] = i;
  }
  
  gettimeofday(&t0, NULL);
  for (i = 0; i < n; i++) {
    s += v1[i];
  }
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  printf("  serial pfor_reduce time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
  
  gettimeofday(&t0, NULL);
  
  if (with_macro) {
    pr_pfor_reduce(i, 0, n, ntasks, r1, 0, std::plus<int>(), r1 += v1[i] );
  } else {
    r1 = pfor_reduce(0, n, ntasks, std::plus<int>(), [&](int i) { return v1[i]; } );
  }
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  printf("parallel pfor_reduce time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
  
  int success = (r1 == s);
  printf(" %s\n", success ? "*SUCCESS*" : " FAILURE!");
  return success ? 0 : -1;
}


int main(int argc, char** argv) {

  if (getenv("OMP_NUM_THREADS"))
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  
  tbb::task_scheduler_init init(nthreads);
  
  if (argc > 1)
    n = atoi(argv[1]);
  
  if (argc > 2)
    tasks_per_thread = atoi(argv[2]);
  
  ntasks = nthreads * tasks_per_thread;

  printf("Nprocs=1 threads=%d task_per_thread=%d\n", nthreads, tasks_per_thread);

  
  int ret = test1(n) || test2(n);
  
  n *= 100000;
  
  ret = ret
  || test_pr_pfor(n, true)
  || test_pr_pfor(n, false)
  || test_pr_pfor_reduce(n, true)
  || test_pr_pfor_reduce(n, false);
  
  return ret;
}
