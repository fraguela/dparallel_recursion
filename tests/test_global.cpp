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
/// \file     test_global.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstring>
#include <functional>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"
#include "dparallel_recursion/AliasVector.h"
#include "dparallel_recursion/DRange.h"

#define GLOBAL_SIZE 1024


using namespace dpr;

int rank, provided, nthreads, nprocs;

std::vector<int> global_vector(GLOBAL_SIZE);

TRANSMIT_BY_CHUNKS(AliasVector<int>);

TRANSMIT_BY_CHUNKS(RangedProblem<AliasVector<int>>);

template<typename T>
struct ChangeBody : public EmptyBody<RangedProblem<AliasVector<T>>, void> {
  
  void base(RangedProblem<AliasVector<T>>& p) {
    int r = p.data().offset();

    for (int i=0; i < p.data().size(); ++i) {
      p.data()[i] = r + i;
    }
  }
  
};

void zero_vector()
{
  const int sz = global_vector.size();
  
  for (int i = 0; i < sz; ++i) {
    global_vector[i] = 0;
  }
}

int verify(const RangedProblem<AliasVector<int>>& p)
{ static int ntest = 1;
  int ret;
  
  //std::cout << '[' << rank << "] sz=" << p.data().size() << std::endl;
  //std::cout << global_vector.data() << " should be == " << p.data().data() << std::endl;

  int local_ret = (global_vector.data() == p.data().data()) ? 0 : -1;
  
  const int sz = global_vector.size();
  
  for (int i = 0;  i < sz; i++) {
    if (global_vector[i] != i) {
      local_ret = -1;
      std::cout<< '[' << rank << "] pos[" << i << "] is " << global_vector[i] << " instead of " << i << std::endl;
      break;
    }
  }
  
  MPI_Allreduce(&local_ret, &ret, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  
  if (!rank) {
    std::cout.flush();
    std::cout << " TEST " << ntest << ": " << (!ret ? "*SUCCESS*" : "FAILURE!") << std::endl;
    std::cout << "===============================================\n";
  }
  
  ntest++;
  
  return ret;
}

///  A replicated vector is modified in parallel so that each task in each processor takes
/// care of a different portion thanks to the RangedProblem::DInfo.
/// The result is gathered and broadcasted
int test1()
{
  zero_vector();
  
  const int sz = global_vector.size();
  
  RangedProblem<AliasVector<int>> p { global_vector, Range { 0, sz } };

  auto info = p.makeDInfo<true>(&AliasVector<int>::range, nprocs, nthreads);

  dparallel_recursion<void>(p, info, ChangeBody<int>(), PR_PART(), ReplicatedInput|GatherInput|ReplicateOutput );

  return verify(p);
}

///  Achieves the same as test1, but just using dpr_pfor . The only difference is that
/// this parallel loop does not gather and broadcast the result, so we make a dparallel_recursion
/// invocation with an EmptyBody to achieve this.
template<bool WITH_MACRO>
int test2()
{ int i;

  zero_vector();
  
  const int sz = global_vector.size();

  if(WITH_MACRO) {
    dpr_pfor(i, 0, sz, nthreads, global_vector[i] = i );
  } else {
    dpfor(0, sz, nthreads, [&](int i) {global_vector[i] = i; } );
  }
  //This just allgathers a vector choosing the region owned by each processor according to ExclusiveRangeInfo
  RangedProblem<AliasVector<int>> p { global_vector, Range { 0, sz } };
  
  auto info = p.makeDInfo<true>(&AliasVector<int>::range, nprocs, nthreads);
  
  dparallel_recursion<void>(p, info, EmptyBody<RangedProblem<AliasVector<int>>, void>(), PR_PART(), ReplicatedInput|GatherInput|ReplicateOutput );

  return verify(p);
}

int noisy_zero()
{
#ifdef DEBUG
  char strbuf[128];
  sprintf(strbuf, "[%d]Init0", rank);
  std::cout << strbuf << std::endl;
#endif
  return 0;
}

int noisy_add(int a, int b)
{
#ifdef DEBUG
  char strbuf[128];
  sprintf(strbuf, "[%d]Add", rank);
  std::cout << strbuf << std::endl;
#endif
  return a + b;
}

/// Tests reductions as/in results of the dparallel_recursion algorithm using dpr_pfor_reduce macro
int test4()
{ int i, r, r2, r3, ret;

  const int sz = global_vector.size();
  
  if (!rank) {
    std::cout << " TEST 4.1: Noisy execution with one task per rank:\n";
  }
  dpr_pfor_reduce(i, 0, sz, 1, r2, noisy_zero(), noisy_add, r2 += global_vector[i] );
  MPI_Barrier(MPI_COMM_WORLD);
  
  if (!rank) {
    std::cout << " TEST 4.2: Noisy execution with one task per thread:\n";
  }
  
  dpr_pfor_reduce(i, 0, sz, nthreads, r3, noisy_zero(), noisy_add, r3 += global_vector[i] );
  MPI_Barrier(MPI_COMM_WORLD);
  
  if (!rank) {
    std::cout << " TEST 4.3: Silent execution with one task per rank\n";
  }
  
  dpr_pfor_reduce(i, 0, sz, nthreads, r, 0, std::plus<int>(), r += global_vector[i] );
  MPI_Barrier(MPI_COMM_WORLD);

  int tmp = GLOBAL_SIZE - 1;
  tmp = tmp * (tmp+1) / 2;
  int local_ret = (tmp == r) && (r == r2) && (r == r3) ? 0 : -1;

  
  MPI_Allreduce(&local_ret, &ret, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  
  if (!rank) {
    std::cout.flush();
    std::cout << " TEST 4: " << (!ret ? "*SUCCESS*" : "FAILURE!") << std::endl;
    std::cout << "===============================================\n";
  }

  return ret;
}

/// Tests reductions as/in results of the dparallel_recursion algorithm using pfor_reduce function
int test5()
{ int r, r2, r3, ret;
  
  const int sz = global_vector.size();
  
  if (!rank) {
    std::cout << " TEST 5.1: Noisy execution with one task per rank:\n";
  }
  r2 = dpfor_reduce(0, sz, 1, noisy_add, [&](int i) { return global_vector[i]; } );
  MPI_Barrier(MPI_COMM_WORLD);
  
  if (!rank) {
    std::cout << " TEST 5.2: Noisy execution with one task per thread:\n";
  }
  
  r3 = dpfor_reduce(0, sz, nthreads, noisy_add, [&](int i) { return global_vector[i]; } );
  MPI_Barrier(MPI_COMM_WORLD);
  
  if (!rank) {
    std::cout << " TEST 5.3: Silent execution with one task per rank\n";
  }
  
  r = dpfor_reduce(0, sz, nthreads, std::plus<int>(), [&](int i) { return global_vector[i]; } );
  MPI_Barrier(MPI_COMM_WORLD);
  
  int tmp = GLOBAL_SIZE - 1;
  tmp = tmp * (tmp+1) / 2;
  int local_ret = (tmp == r) && (r == r2) && (r == r3) ? 0 : -1;
  
  
  MPI_Allreduce(&local_ret, &ret, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  
  if (!rank) {
    std::cout.flush();
    std::cout << " TEST 5: " << (!ret ? "*SUCCESS*" : "FAILURE!") << std::endl;
    std::cout << "===============================================\n";
  }
  
  return ret;
}

int main(int argc, char** argv)
{ int ret = 0;
  
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  assert(provided >= MPI_THREAD_SERIALIZED);
  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
  const char* nthreads_env_var = getenv("OMP_NUM_THREADS");
  nthreads = (nthreads_env_var != NULL)? atoi(nthreads_env_var) : 8; //tbb::task_scheduler_init::automatic;
  
  tbb::task_scheduler_init init(nthreads);
  
  if (rank == 0) {
    printf("Nprocs=%d threads=%d\n", nprocs, nthreads);
  }
  
  ret = test1() || test2<true>() || test2<false>() || test4() || test5();
  
  MPI_Finalize();
  
  return ret;
}
