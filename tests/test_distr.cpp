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
/// \file     test_distr.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdlib>
#include <algorithm>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"
#include "NoisyFillableAliasVector.h"

#define LOCAL_SIZE 1024

constexpr bool WITH_PFOR_MACROS = false;

int rank, nthreads, nprocs;

typedef NoisyFillableAliasVector<int> vector_handle_t;


// Info object for an already distributed input so that in each node the local element
// will be process as base case
struct DVInfo : public DInfo<vector_handle_t, 2> {
  
  static bool is_base(const vector_handle_t& v) {
    printf("[%d] Distributed base test\n", rank);
    return true;
  }

  static vector_handle_t child(int i, const vector_handle_t& v) {
    std::cerr << "This should never be invoked\n";
    exit(EXIT_FAILURE);
  }

};

struct BufferedDVInfo : public BufferedDInfo<vector_handle_t, 2, vector_handle_t> {
  
  BufferedDVInfo()
  {}
  
  static bool is_base(const vector_handle_t& v) {
    return DVInfo::is_base(v);
  }
  
  static vector_handle_t child(int i, const vector_handle_t& v) {
    return DVInfo::child(i, v);
  }
  
};

template<bool USE_MACROS = false>
struct AdditionBody : public EmptyBody<vector_handle_t, vector_handle_t> {
  
  vector_handle_t base(vector_handle_t& v) const {
    printf("[%d] Base processing\n", rank);
    
    if(USE_MACROS) {
      int i;
      pr_pfor(i, 0, static_cast<int>(v.size()), nthreads * nprocs, v[i] = rank + i );
    } else {
      pfor(0, static_cast<int>(v.size()), nthreads * nprocs, [&](int i) { v[i] = rank + i; });
    }
    
    //for (int i = 0; i < v.size(); i++) {
    //  v[i] = rank + i;
    //}
    
    return v;
  }
  
  vector_handle_t post(const vector_handle_t& t, vector_handle_t* v) const {
    printf("[%d] Global reduction (for 0: %d+=%d)\n", rank, v[0][0], v[1][0]);
    
    if(USE_MACROS) {
      int i;
      pr_pfor(i, 0, static_cast<int>(v->size()), nthreads * nprocs, v[0][i] += v[1][i] );
    } else {
      pfor(0, static_cast<int>(v->size()), nthreads * nprocs, [&](int i) { v[0][i] += v[1][i]; });
    }
    
    //for (int i = 0; i < v->size(); i++) {
    //  v[0][i] += v[1][i];
    //}
    
    return *v;
  }
  
};

/// Actually holds the data. Avoids noise from handle when creating and destroying the underlying data
std::vector<int> local_vector(LOCAL_SIZE);

/// Allow to partition the work on replicated_vector without making copies
vector_handle_t handle(local_vector);

constexpr bool isPowerOf2(int i) {
  return !(i & (i-1));
}

void test_init()
{
  NoisyFillableAliasVector<int>::allocations = 0;
  
  std::fill(local_vector.begin(), local_vector.end(), 0);
}

int verify(const char *test_name, const vector_handle_t& res, int expected_allocations = -1)
{ int ret = 0, num_allocations;
  
  int tmp = NoisyFillableAliasVector<int>::allocations;
  
  MPI_Allreduce(&tmp, &num_allocations, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  
  std::cout.flush();
  MPI_Barrier(MPI_COMM_WORLD);
  
  if (!rank) {
    if (res.size() != LOCAL_SIZE) {
      std::cout << "Wrong vector size\n";
      ret = -1;
    } else {
      int tmp = nprocs * (nprocs - 1) / 2;
      for (int i = 0; i < LOCAL_SIZE; i++) {
        int expected = i * nprocs +  tmp;
        if(res[i] != expected) {
          std::cout << "Wrong value in position " << i << ' ';
          std::cout << "Read " << res[i] << " != expected " << expected << std::endl;
          ret = -2;
          break;
        }
      }
    }
    if (res.data() != local_vector.data()) {
      std::cout << "Wrong destination address\n";
      ret = -3;
    }
    if ( (expected_allocations != -1) && (num_allocations != expected_allocations) ) {
      std::cout << "Wrong number of allocations. Expected " << expected_allocations << std::endl;
      ret = -4;
    }
    std::cout << " TEST" << test_name << ": "<< ((!ret) ? "*SUCCESS*" : "FAILURE!") << std::endl;
    std::cout << "      : " << num_allocations << " vector allocations performed\n";
  }
  
  std::cout.flush();
  MPI_Barrier(MPI_COMM_WORLD);

  return ret;
}

/// Each process works indepently in its vector, all the vectors are reduced in rank 0.
/// Reductions take place on the first argument and return it, so res == handle in rank 0.
/// It should allocate nprocs-1 buffers, to receive the communications from the other processors.
int test1()
{ DVInfo dvi;
  
  test_init();

  vector_handle_t res = dparallel_recursion<vector_handle_t>(handle, dvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput);
  
  int ret = verify("1.1", res, nprocs-1);
  
  if (!ret) {
    test_init();
    res = dparallel_recursion<vector_handle_t>(handle, dvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput);
    ret = verify("1.2", res, nprocs-1);
  }

  return ret;
}

/// test1 + the result is replicated
/// It should allocate nprocs-1 additional buffers to receive the result in the ranks > 0
/// totalling 2*(nprocs-1) allocations
int test2()
{ DVInfo dvi;
  
  test_init();
  
  vector_handle_t res = dparallel_recursion<vector_handle_t>(handle, dvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput | ReplicateOutput);
  
  int ret = verify("2.1", res, 2*(nprocs-1));
  
  if (!ret) {
    test_init();
    vector_handle_t res = dparallel_recursion<vector_handle_t>(handle, dvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput | ReplicateOutput);
    ret = verify("2.2", res, 2*(nprocs-1));
  }
  
  return ret;
}

/// test1 + PrioritizeDM -> reduction takes place in tree among the processes
/// The are the same as in test1
int test3()
{ DVInfo dvi;
  
  test_init();
  
  vector_handle_t res = dparallel_recursion<vector_handle_t>(handle, dvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput | PrioritizeDM);
  
  int ret = verify("3.1", res, nprocs-1);
  
  if (!ret) {
    test_init();
    res = dparallel_recursion<vector_handle_t>(handle, dvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput | PrioritizeDM);
    ret = verify("3.2", res, nprocs-1);
  }
  
  return ret;
}

/// Like test1, showing that BufferedDVInfo avoids the creation of buffers in the second invocation
int test4()
{ BufferedDVInfo bdvi;
  
  test_init();
  
  vector_handle_t res = dparallel_recursion<vector_handle_t>(handle, bdvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput);
  
  int ret = verify("4.1", res, nprocs-1);
  
  if (!ret) {
    test_init();
    res = dparallel_recursion<vector_handle_t>(handle, bdvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput);
    ret = verify("4.2", res, 0);
  }
  
  return ret;
}

/// test4 + the result is replicated
/// showing that BufferedDVInfo avoids the creation of all the buffers in the second invocation
int test5()
{ BufferedDVInfo bdvi; //(handle);
  
  test_init();
  
  dparallel_recursion<vector_handle_t>(handle, bdvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput | ReplicateOutput, handle);
  
  vector_handle_t res = handle;
  
  int ret = verify("5.1", res, nprocs-1);
  
  if (!ret) {
    test_init();
    dparallel_recursion<vector_handle_t>(handle, bdvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput | ReplicateOutput, handle);
    ret = verify("5.2", res, 0);
  }
  
  return ret;
}

/// test1 + PrioritizeDM -> reduction takes place in tree among the processes
int test6()
{ BufferedDVInfo bdvi;
  
  test_init();
  
  vector_handle_t res = dparallel_recursion<vector_handle_t>(handle, bdvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput | PrioritizeDM);
  
  int ret = verify("6.1", res);
  
  if (!ret) {
    test_init();
    res = dparallel_recursion<vector_handle_t>(handle, bdvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput | PrioritizeDM);
    ret = verify("6.2", res);
  }
  
  return ret;
}

int main(int argc, char** argv)
{ int provided, ret = 0;
  
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  assert(provided >= MPI_THREAD_SERIALIZED);
  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
  NoisyFillableAliasVector<int>::rank = rank;
  
  const char* nthreads_env_var = getenv("OMP_NUM_THREADS");
  nthreads = (nthreads_env_var != NULL)? atoi(nthreads_env_var) : 8; //tbb::task_scheduler_init::automatic;
  
  tbb::task_scheduler_init init(nthreads);
  
  if (!isPowerOf2(nprocs)) {
    if (!rank) {
      puts("*******************************************************************");
      puts("This test only supports a number of processors that is a power of 2");
      puts("*******************************************************************");
    }
    ret = -1;
  } else {
    if (!rank) {
      printf("Nprocs=%d threads=%d\n", nprocs, nthreads);
    }
    
    MPI_Barrier(MPI_COMM_WORLD);
    
    ret = test1() || test2() || test3() || test4() || test5() || test6();
  }
  
  MPI_Finalize();
  return ret;
}
