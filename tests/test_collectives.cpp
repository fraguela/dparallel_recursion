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
/// \file     test_collectives.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdlib>
#include <numeric>
#include <algorithm>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"
#include "dparallel_recursion/DRange.h"
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

struct BDVInfo : public BufferedDInfo<vector_handle_t, 2, vector_handle_t> {

  BDVInfo()
  {}
  
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

template<bool NEGATE>
struct AdditionBody : public EmptyBody<vector_handle_t, int> {
  
  int base(vector_handle_t& v) const {
    int tmp = std::accumulate(v.begin(), v.end(), 0);
    if (NEGATE) {
      std::transform(v.begin(), v.end(), v.begin(), [](int i) { return -i; });
    }
    // printf("[%d] base AdditionBody=%d (on %lu)\n", rank, tmp, v.size());
    return tmp;
  }
  
  int post(const vector_handle_t& t, int* tmp) const {
    auto result = tmp[0] + tmp[1];
    // printf("[%d] post AdditionBody=%d (%d + %d)\n", rank, result, tmp[0], tmp[1]);
    return result;
  }
  
};

/// Notice that this operation almost always generates *new* storage for the vectors,
///both in the base and in the post operations. But if the vectors to reduce in the
///post came from a Gather process, they would have been placed consecutive,
///the second being shallow for sure. This is exploited in the post operation.
struct NegateBody : public EmptyBody<vector_handle_t, vector_handle_t> {
  
  vector_handle_t base(vector_handle_t& v) const {
    vector_handle_t tmp(v.size());
    std::transform(v.begin(), v.end(), tmp.begin(), [](int i) { return -i; });
    // printf("[%d] base AdditionBody=%d (on %lu)\n", rank, tmp, v.size());
    return tmp;
  }
  
  vector_handle_t post(const vector_handle_t& t, vector_handle_t* res) const {
    const auto new_size = res[0].size() + res[1].size();
    if ((res[1].data() == (res[0].data() + res[0].size())) && !res[1].owned()) {
      res[0].shallowResize(new_size);
      return res[0];
    } else {
      vector_handle_t tmp(new_size);
      std::copy(res[0].begin(), res[0].end(), tmp.begin());
      std::copy(res[1].begin(), res[1].end(), tmp.begin() +  res[0].size());
      return tmp;
    }
  }
  
};

/*
template<typename Result>
struct MyBufferedExclusiveRangeDInfo : public ExclusiveRangeDInfo {
  
  //Result* dest_;
  
  MyBufferedExclusiveRangeDInfo(const Range& r, int nprocs, int ntasks, Result* dest=nullptr)
  : ExclusiveRangeDInfo(r, nprocs, ntasks)
  {
    setDestination(dest);
  }
  
  // void* resultBufferPtr() {
  //return dest_;
  // }
};
*/

struct Test5Body : public EmptyBody<Range, vector_handle_t> {
  
  static std::vector<int> ReplIndexes;
  
  vector_handle_t& input;
  vector_handle_t& output;

  Test5Body(vector_handle_t &iinput, vector_handle_t& ioutput) :
  input(iinput), output(ioutput)
  {}
  
  vector_handle_t base(const Range& r) const {
    for (int i = r.start; i < r.end; i++) {
      output[i] = -input[ReplIndexes[i]];
    }
    //std::cout << '[' << r.start << ", " << r.end << "]:" << output[r.start] << std::endl;
    //sleep(1);
    return output.range(r.start, r.end);
  }
  
  vector_handle_t post(const Range& r, vector_handle_t* res) const {
    std::size_t sz0 = res[0].size();
    std::size_t sz1 = res[1].size();
    std::cout << 'R' << rank << "(W" << res[0].owned() << 'W' << res[1].owned() << ") J" << res[0][0] << '-' << res[1][sz1-1] << std::endl;
    std::size_t new_size = sz0 + sz1;
    if ((res[1].data() == (res[0].data() + sz0)) && !res[1].owned()) {
      std::cout << "match at R" << rank << " :)\n";
      res[0].shallowResize(new_size);
      return res[0];
    } else {
      std::cout << "no match at R" << rank << std::endl;
      vector_handle_t tmp(new_size);
      std::copy(res[0].begin(), res[0].end(), tmp.begin());
      std::copy(res[1].begin(), res[1].end(), tmp.begin() +  res[0].size());
      return tmp;
    }
    
  }
};

/// Indexes for the shuffling
std::vector<int> Test5Body::ReplIndexes(LOCAL_SIZE);

/// Actually holds the data. Avoids noise from handle when creating and destroying the underlying data
std::vector<int> local_vector(LOCAL_SIZE);
// Allow to partition the work on replicated_vector without making copies
vector_handle_t handle(local_vector);

/// Used as destination in Buffered Info tests (BDVInfo)
std::vector<int> result_vector(LOCAL_SIZE);
vector_handle_t result_handle(result_vector);

constexpr bool isPowerOf2(int i) {
  return !(i & (i-1));
}

int verify(const char * const test_name, int sum, int correct_result)
{ int ret = 0, tmp;
  
  if (correct_result > 0) {
    if (!rank) {
      printf("Res=%d (%d)\n", sum, correct_result);
    }
    
    if(sum != correct_result) {
      ret = -1;
      printf("[%d] Wrong sum", rank);
    }
  } else {
    ret = correct_result;
  }

  MPI_Allreduce(&ret, &tmp, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  
  if (!rank) {
    std::cout << " TEST" << test_name << ": " << ((!tmp) ? "*SUCCESS*" : "FAILURE!") << std::endl;
  }
  
  MPI_Barrier(MPI_COMM_WORLD);
  return tmp;
}

static auto Pos_i_is_minus_i = [](const vector_handle_t& v, int i) { return v[i] == -i; };

int negate_verify(const char * const test_name, const bool only0, const vector_handle_t& res, bool farg(const vector_handle_t& v, int i), const bool other_conditions)
{ int i, ret, tmp_ret = 0;

  if (!only0 || !rank) {
    for (i = 0; (i < LOCAL_SIZE) && farg(res, i) /*(res[i]==-i)*/; i++);
    
    if (i < LOCAL_SIZE) {
      printf("[%d] Res not valid", rank);
      tmp_ret = -2;
    } else {
      tmp_ret = 0;
    }
  }
  
  if (!other_conditions) {
    tmp_ret--;
  }

  MPI_Allreduce(&tmp_ret, &ret, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  
  if (!rank) {
    std::cout << " TEST" << test_name << ": " << ((!ret) ? "*SUCCESS*" : "FAILURE!") << std::endl;
  }
  
  MPI_Barrier(MPI_COMM_WORLD);

  return ret;
}

int test1()
{
  vector_handle_t::allocations = 0;
  if (!rank) {
    std::iota(handle.begin(), handle.end(), 0);
  }
  
  int sum = dparallel_recursion<int>(handle, DVInfo(), AdditionBody<false>(), PR_PART(), Scatter|ReplicateOutput);
  
  return verify("1", sum, (LOCAL_SIZE * (LOCAL_SIZE - 1)) / 2);
}

int test2()
{ int res = (LOCAL_SIZE * (LOCAL_SIZE - 1)) / 2;
  
  vector_handle_t::allocations = 0;
  
  int sum = dparallel_recursion<int>(handle, DVInfo(), AdditionBody<true>(), PR_PART(), Scatter|GatherInput|ReplicateOutput);

  // Notice that both sum and the input handle are replicated because of the GatherInput
  int i;
  for (i = 0; (i < LOCAL_SIZE) && (handle[i]==-i); i++);
  if (i < LOCAL_SIZE) {
    printf("[%d] Input not negated", rank);
    res = -2;
  }

  return verify("2", sum, res);
}

/* GatherInput does not make sense for DistributedInput because 
   1) we do not know how to combine several inputs a single input.
   2) it should destroy the existing local input.
 
int test3()
{
  vector_handle_t tmp((rank + 1) * 10);
  std::iota(tmp.begin(), tmp.end(), 0);
  
  int sum = dparallel_recursion<int>(tmp, DVInfo(), AdditionBody<false>(), PR_PART(), DistributedInput|GatherInput);
  
  int res = 0;
  for (int i = 0; i < nprocs; i++) {
    int lim = (i + 1) * 10;
    res += (lim * (lim-1)) / 2;
  }
  for (int i = 0; i < nprocs; i++) {
    if (i ==rank) {
      printf("[%d] Sum=%d res=%d tmp.size()=%lu\n", rank, sum, res, tmp.size());
    }
   MPI_Barrier(MPI_COMM_WORLD);
  }
  return 0;
}
*/

int test3()
{
  if (!isPowerOf2(nprocs)) {
    if (!rank) {
      puts("***************************************************************");
      puts("TEST3 only supports a number of processors that is a power of 2");
      puts("***************************************************************");
      /* The problem is that otherwise rank 0 has > 1 subresult, and since each subresult
       is an independent vector_handle_t, the memory, even if it may be consecutive, is not
       allocated from a single chunk, since each vector_handle_t subresult is independently
       allocated. Thus it does not fulfil the conditions for Gather. */
    }
    return -1;
  }
    
  vector_handle_t::allocations = 0;
  if (!rank) {
    std::iota(handle.begin(), handle.end(), 0);
  }
  
  vector_handle_t res = dparallel_recursion<vector_handle_t>(handle, DVInfo(), NegateBody(), PR_PART(), Scatter|Gather);

  return negate_verify("3", true, res, Pos_i_is_minus_i, true);
}

int test4()
{ BDVInfo bdvi;
  
  int * const initial_rh_ptr = result_handle.data();
  int * const initial_h_ptr = handle.data();
  
  std::fill(result_handle.begin(), result_handle.end(), 0);
  vector_handle_t::allocations = 0;
  if (!rank) {
    std::iota(handle.begin(), handle.end(), 0);
  }
  
  dparallel_recursion<vector_handle_t>(handle, bdvi, NegateBody(), PR_PART(), Scatter|Gather, result_handle);
  
  int * const final_rh_ptr = result_handle.data();
  int * const final_h_ptr = handle.data();
  
  const bool ptrs_match = (initial_h_ptr == final_h_ptr) && (initial_rh_ptr == final_rh_ptr);
  
  if (!rank) {
    std::cout << "       handle.data(): " <<initial_h_ptr << " vs " << final_h_ptr << ((initial_h_ptr == final_h_ptr) ? " match" : " do not match") << std::endl;
    std::cout << "result_handle.data(): " << initial_rh_ptr << " vs " << final_rh_ptr << ((initial_rh_ptr == final_rh_ptr) ? " match" : " do not match") << std::endl;
  }
  
  return negate_verify("4", true, result_handle, Pos_i_is_minus_i, ptrs_match);
}

int test5()
{
  int n = LOCAL_SIZE - 1;
  std::generate(Test5Body::ReplIndexes.begin(), Test5Body::ReplIndexes.end(), [&n] () { return n--; }); //reshuffle indexes
  std::iota(handle.begin(), handle.end(), 0); // data to be reshuffled
  
  vector_handle_t::allocations = 0;
  
  Range input {0, LOCAL_SIZE};
  ExclusiveRangeDInfo info_obj(input, nprocs, 2);
  //info_obj.setDestination(&result_handle);

  // It does not make sense to store the result in a vector_handle_t r
  //because we are using a Buffered Info object with instructions to store the result
  dparallel_recursion<vector_handle_t>(input, info_obj, Test5Body(handle, result_handle), partitioner::automatic(), ReplicatedInput | Gather | ReplicateOutput, result_handle );

  if (!rank) {
    std::cout << 'R' << rank << " i[0]= " << handle[0] << " i[1023]=" << handle[1023] << std::endl;
    std::cout << 'R' << rank << " R[0]= " << result_handle[0] << " R[1023]=" << result_handle[1023] << std::endl;
    //std::cout << result_handle.data() << " VS " << r.data() << std::endl;
    //std::cout << 'R' << rank << "res[0]= " << r[0] << " res[1023]=" << r[1023] << std::endl;
    std::cout << "Allocations=" << vector_handle_t::allocations << std::endl;
  }
  
  //return 0;
  return negate_verify("5", false, result_handle, [](const vector_handle_t& v, int i) { return v[i] == -(LOCAL_SIZE - 1 - i); }, !vector_handle_t::allocations);
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
 
  if (!rank) {
    printf("Nprocs=%d threads=%d\n", nprocs, nthreads);
  }
  
  MPI_Barrier(MPI_COMM_WORLD);
    
  ret = test1() || test2() || test3() || test4() || test5();

  MPI_Finalize();
  return ret;
}

