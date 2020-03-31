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
/// \file     test_rvalue_correctness.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdlib>
#include <iostream>
#include <numeric>
#include <algorithm>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"
#include "NoisyFillableAliasVector.h"

#define LOCAL_SIZE 1024
#define BASE_CASE  (LOCAL_SIZE / (2*nprocs))

int rank, nthreads, nprocs;

typedef NoisyFillableAliasVector<int> vector_handle_t;

struct DVInfo : public DInfo<vector_handle_t, 2> {
  
  static bool is_base(const vector_handle_t& v) {
    // printf("[%d] Distributed base test <=%d\n", rank, BASE_CASE);
    return v.size() <= BASE_CASE;
  }
  
  static vector_handle_t child(int i, const vector_handle_t& v) {
    const auto sz = v.size() / 2;
    const auto begin = i * sz;
    return v.range(begin, begin + sz);
  }
  
};


struct FillInBody : public EmptyBody<vector_handle_t, vector_handle_t> {
  
  vector_handle_t base(vector_handle_t& v) const {
    // printf("[%d] base FillInBody\n", rank);
    std::iota(v.begin(), v.end(), rank);
    return v;
  }
  
  vector_handle_t post(vector_handle_t& t, vector_handle_t* v) const {
    // printf("[%d] post FillInBody\n", rank);
    return t;
  }
  
};


struct AdditionBody : public EmptyBody<vector_handle_t, int> {
  
  int base(vector_handle_t& v) const {
    int tmp = std::accumulate(v.begin(), v.end(), 0);
    // printf("[%d] base AdditionBody=%d (on %lu)\n", rank, tmp, v.size());
    return tmp;
  }
  
  int post(const vector_handle_t& t, int* tmp) const {
    auto result = tmp[0] + tmp[1];
    // printf("[%d] post AdditionBody=%d (%d + %d)\n", rank, result, tmp[0], tmp[1]);
    return result;
  }
  
};

struct InPlaceNegateBody : public EmptyBody<vector_handle_t, void> {
  
  void base(vector_handle_t& v) const {
    // printf("[%d] base InPlaceNegateBody\n", rank);
    std::transform(v.begin(), v.end(), v.begin(), [](int n){ return -n;});
  }
  
};

/// Actually holds the data. Avoids noise from handle when creating and destroying the underlying data
std::vector<int> local_vector(LOCAL_SIZE);
// Allow to partition the work on replicated_vector without making copies
vector_handle_t handle(local_vector);
int * const local_vector_ptr = local_vector.data();
DVInfo Gdvi;
int Gsum;

constexpr bool isPowerOf2(int i) {
  return !(i & (i-1));
}

int verify(const char *test_name, const vector_handle_t& res, const vector_handle_t& handle, bool eq_in_0)
{ int ret = 0;
  
  int tmp = NoisyFillableAliasVector<int>::allocations;
  
  for (int i = 0; i < nprocs; i++) {
    if (i == rank) {
      printf("[%d] Allocations=%d\n", rank, tmp);
      printf("[%d] res(%p, %lu, off=%ld) handle(%p, %lu, off=%ld)\n", rank, res.data(), res.size(), res.data() - local_vector_ptr, handle.data(), handle.size(), handle.data() - local_vector_ptr);
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }
  
  if (handle.size() != LOCAL_SIZE) {
    printf("[%d] Wrong handle.size()", rank);
    ret = -2;
  }
  if (handle.data() != local_vector_ptr) {
    printf("[%d] Wrong handle.data()", rank);
    ret = -3;
  }
  if ((rank || !eq_in_0) && ((res.data() != nullptr) || res.size())) {
    printf("[%d] Wrong res (should be empty but it is not)", rank);
    ret = -4;
  }
  if (!rank && eq_in_0 && ((res.data() != handle.data()) || (res.size() != handle.size()))) {
    printf("[%d] Wrong res (should be =handle but it is not)", rank);
    ret = -5;
  }
  MPI_Allreduce(&ret, &tmp, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

  if (!rank) {
    std::cout << " TEST" << test_name << ": "<< ((!tmp) ? "*SUCCESS*" : "FAILURE!") << std::endl;
  }
  
  MPI_Barrier(MPI_COMM_WORLD);
  return tmp;
}

int test1()
{
  NoisyFillableAliasVector<int>::allocations = 0;
  
  std::fill(local_vector.begin(), local_vector.end(), 0);

  printf("[%d] handle(%p, %lu, off=%ld)\n", rank, handle.data(), handle.size(), handle.data() - local_vector_ptr);
  
  vector_handle_t res = dparallel_recursion<vector_handle_t>(handle, Gdvi, FillInBody(), PR_PART());

  return verify("1", res, handle, true);
}

int test2()
{ vector_handle_t res;

  NoisyFillableAliasVector<int>::allocations = 0;

  Gsum = dparallel_recursion<int>(handle, Gdvi, AdditionBody(), PR_PART(), ReplicateOutput);
  
  if (!rank) {
    printf("[%d] Gsum=%d\n", rank, Gsum);
  }
  
  return verify("2", res, handle, false);
}

int test3()
{ vector_handle_t res;
  
  NoisyFillableAliasVector<int>::allocations = 0;

  dparallel_recursion<void>(handle, Gdvi, InPlaceNegateBody(), PR_PART());

  int test = verify("3.1", res, handle, false);
  
  if (!test) {
    int tmp = dparallel_recursion<int>(handle, Gdvi, AdditionBody(), PR_PART(), ReplicateOutput);
    if (tmp != -Gsum) {
      test = -6;
    }
    if (!rank) {
      printf("[%d] Gsum=%d Expected=%d\n", rank, tmp, -Gsum);
      std::cout << " TEST3.2: "<< ((!test) ? "*SUCCESS*" : "FAILURE!") << std::endl;
    }
  }
  return test;
}

int main(int argc, char** argv)
{ int provided, ret = 0;
  
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  assert(provided >= MPI_THREAD_SERIALIZED);
  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
  vector_handle_t::rank = rank;
  
  const char* nthreads_env_var = getenv("OMP_NUM_THREADS");
  nthreads = (nthreads_env_var != NULL)? atoi(nthreads_env_var) : 8; //tbb::task_scheduler_init::automatic;
  
  tbb::task_scheduler_init init(nthreads);
  if (!isPowerOf2(nprocs) || (nprocs > 64)) {
    if (!rank) {
      puts("************************************************************************");
      puts("This test only supports a number of processors that is a power of 2 < 64");
      puts("************************************************************************");
    }
    ret = -1;
  } else {
    if (!rank) {
      printf("Nprocs=%d threads=%d\n", nprocs, nthreads);
    }
  
    MPI_Barrier(MPI_COMM_WORLD);
  
    ret = test1() || test2() || test3();
  }
  
  MPI_Finalize();
  return ret;
}

