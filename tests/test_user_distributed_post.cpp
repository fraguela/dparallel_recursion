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
/// \file     test_user_distributed_post.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdlib>
#include <algorithm>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"
#include "NoisyFillableAliasVector.h"

#define LOCAL_SIZE 1024

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

template<bool USE_MACROS = false>
struct AdditionBody : public EmptyBody<vector_handle_t, vector_handle_t> {
  
  vector_handle_t base(vector_handle_t& v) const {
    printf("[%d] Base processing\n", rank);
    int i;

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
  
  void distributed_post(std::vector<vector_handle_t *>& inputs, std::vector<vector_handle_t>& results, int rank, int nprocs, vector_handle_t& global_input, vector_handle_t& global_result) const {
    assert(results[0].size() == LOCAL_SIZE);
    std::cout << " Using user-defined distributed_post\n";
    
    MPI_Allreduce(MPI_IN_PLACE, results[0].data(), LOCAL_SIZE, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
    
    const auto tmp_sz = global_result.size();
    if(!tmp_sz) {
      std::cout << " The global result was empty\n";
      global_result = std::move(results[0]);
    } else {
      std::cout << " The global result was not empty and points to " << tmp_sz << " elems\n";
      if(global_result.data() == results[0].data()) {
        std::cout << " Already points to correct place\n";
      } else {
        exit(EXIT_FAILURE);
      }
    }
  }
};

/// Actually holds the data. Avoids noise from handle when creating and destroying the underlying data
std::vector<int> local_vector(LOCAL_SIZE);

/// Allow to partition the work on replicated_vector without making copies
vector_handle_t handle(local_vector);

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
    std::cout << "Wrong number of allocations. Expected " << expected_allocations << " but found " << num_allocations << std::endl;
    ret = -4;
  }
  
  if (!rank) {
    std::cout << " TEST" << test_name << ": "<< ((!ret) ? "*SUCCESS*" : "FAILURE!") << std::endl;
    std::cout << "      : " << num_allocations << " vector allocations performed\n";
  }
  
  std::cout.flush();
  MPI_Barrier(MPI_COMM_WORLD);

  return ret;
}

/// Each process works indepently in its vector, all the vectors are reduced using a MPI_Allreduce.
/// Reductions take place on the first argument and return it, so res == handle in rank 0.
/// It should allocate no buffers to receive the communications from the other processors.
template<bool WITH_PFOR_MACROS>
int test1()
{ DVInfo dvi;
  static char test_name_string[] {'1', '.', WITH_PFOR_MACROS ? '1' : '3'};

  test_init();

  vector_handle_t res = dparallel_recursion<vector_handle_t>(handle, dvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput);
  
  int ret = verify(test_name_string, res, 0);
  test_name_string[2]++;
  
  // Same test but now providing in advance the destination
  if (!ret) {
    test_init();
    dparallel_recursion<vector_handle_t>(handle, dvi, AdditionBody<WITH_PFOR_MACROS>(), PR_PART(), DistributedInput, res);
    ret = verify(test_name_string, res, 0);
    test_name_string[2]++;
  }

  return ret;
}


int main(int argc, char** argv)
{ int provided;
  
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  assert(provided >= MPI_THREAD_SERIALIZED);
  
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
  NoisyFillableAliasVector<int>::rank = rank;
  
  const char* nthreads_env_var = getenv("OMP_NUM_THREADS");
  nthreads = (nthreads_env_var != NULL)? atoi(nthreads_env_var) : 8; //tbb::task_scheduler_init::automatic;
  
  tbb::task_scheduler_init init(nthreads);
  
  if (!rank) {
    printf("Nprocs=%d threads=%d\n", nprocs, nthreads);
  }
  
  MPI_Barrier(MPI_COMM_WORLD);
    
  int ret = test1<true>() || test1<false>();

  MPI_Finalize();
  return ret;
}
