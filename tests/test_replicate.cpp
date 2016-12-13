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
/// \file     test_replicate.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstring>
#include <functional>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"
#include "dparallel_recursion/AliasVector.h"
#include "dparallel_recursion/DRange.h"

#define LOCAL_SIZE 8192
#define BASE_SIZE (LOCAL_SIZE/8)

using namespace dpr;

int rank, provided, nthreads, nprocs;

std::vector<int> local_vector(LOCAL_SIZE);

template<typename VT>
struct DVInfo : public DInfo<VT, 2> {


	bool is_base(const VT& r) const {
		return r.size() < BASE_SIZE;
	}

	VT child(int i, const VT& r) const {
		int sz = r.size() / 2;
		return VT(r.data() + (i ? sz: 0), sz);
	}
};

template<typename VT>
struct AdditionBody : public EmptyBody<VT, int> {
	int base(VT& t) const {
		int r =0, sz=t.size();

		for (int i=0; i<sz; ++i)
			r += t[i];

		return r;
	}

	int post(const VT& t, const int* r) const {
		return r[0] + r[1];
	}
};

/// Tests the replication of the input from rank 0. Then hand-made Info and Body objects process the local vector.
/// The usage of AliasVector avoids the creation of new vectors.
int test1()
{       int ret;
  
        for (int i=0; i<LOCAL_SIZE; ++i) {
                local_vector[i] = rank ? 0 : i; //ONLY rank=0 puts non-zeros
        }
  
        //This also works
        //int res = dparallel_recursion<int>(local_vector, DVInfo<AliasVector<int>>(), AdditionBody<AliasVector<int>>(), PR_PART(), ReplicateInput|ReplicateOutput);
  
        AliasVector<int> myv(local_vector);
        int res = dparallel_recursion<int>(myv, DVInfo<AliasVector<int>>(), AdditionBody<AliasVector<int>>(), PR_PART(), ReplicateInput|ReplicateOutput);
  
        std::cout << '[' << rank << "] R= " << res << std::endl;
        
        int tmp = LOCAL_SIZE - 1;
        tmp = tmp * (tmp+1) / 2;
        int local_ret = (tmp == res) ? 0 : -1;
  
        MPI_Allreduce(&local_ret, &ret, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
        
        if (!rank) {
                std::cout.flush();
                std::cout << "Theory = " << tmp << " TEST 1: " << (!ret ? "*SUCCESS*" : "FAILURE!") << std::endl;
                std::cout << "===============================================\n";
        }
        
        return ret;
}

struct Problem {
  
  AliasVector<int> av_;
  Range r_;
  
  AliasVector<int>& data()             { return av_; }
  const AliasVector<int>& data() const { return av_; }
  
  Range& range()             { return r_; }
  const Range& range() const { return r_; }

  template<class Archive>
  void serialize(Archive& ar,  unsigned int file_version)
  {
    ar & av_ & r_;
  }
  
};

TRANSMIT_BY_CHUNKS(AliasVector<int>);
TRANSMIT_BY_CHUNKS(Problem);

template<typename PROBLEM>
struct ChangeBody : public EmptyBody<PROBLEM, int> {
  
  int base(PROBLEM& p) {
    int r = 0;
    
    for (int i=0; i < p.data().size(); ++i) {
      r += p.data()[i];
      p.data()[i] = -rank;
    }
    
    return r;
  }
  
  int post(PROBLEM& p, int *res) {
    int r = 0;
    for (int i=0; i < p.range().nchildren; ++i) {
      r += res[i];
    }
    return r;
  }
  
};

/// Mimics a RangedProblem::DInfo<true>
struct ProblemInfo : public DInfo<Problem, 0> {
  
  ExclusiveRangeDInfo rinfo_;
  
  ProblemInfo(Problem &p, int nprocs, int ntasks) :
  rinfo_(p.r_, nprocs, ntasks)
  {}
  
  bool is_base(const Problem& p) const {
    return rinfo_.is_base(p.r_);
  }
  
  int num_children(Problem& p) const {
    return rinfo_.num_children(p.r_);
  }
  
  Problem child(int i, const Problem& p) const {
    // The ranges become relative within the existing range, like the av_
    Range tmp = rinfo_.child(i, p.r_) << p.r_.start;
    return Problem { p.av_.range(tmp), tmp };
  }

};

/// The input is replicated, but each task works on a different portion. The result is gathered and replicated.
int test2()
{ int ret;

  Problem p { AliasVector<int> (local_vector), Range { 0, LOCAL_SIZE } };
  ProblemInfo info(p, nprocs, nthreads);
  
  int res = dparallel_recursion<int>(p, info, ChangeBody<Problem>(), PR_PART(), ReplicatedInput|GatherInput|ReplicateOutput );
  
  std::cout << '[' << rank << "] R= " << res << " and sz=" << p.av_.size() << std::endl;
  std::cout << local_vector.data() << " should be == " << p.av_.data() << std::endl;
  
  int tmp = LOCAL_SIZE - 1;
  tmp = tmp * (tmp+1) / 2;
  int local_ret = ((tmp == res) && (local_vector.data() == p.av_.data()))? 0 : -1;


  // test vector correctness
  int nc = info.num_children(p);
  for (int i = 0; (i < nc) && !local_ret; i++) {
    Range tmp = info.child(i, p).r_;
    const int correct_value = nprocs > 1 ? -i : 0;
    std::cout << "[" << rank << "] Testing R[" << tmp.start << ", " << tmp.end << ")==" << correct_value << std::endl;
    for (int j = tmp.start; j < tmp.end; j++) {
      if (local_vector[j] != correct_value) {
        local_ret = -2;
        std::cout<< '[' << rank << "] Subproc[" << i << "] position is " << local_vector[j] << " instead of " << correct_value << " range=[" << tmp.start << ", " << tmp.end << ')' << std::endl;
        break;
      }
    }
  }
  
  MPI_Allreduce(&local_ret, &ret, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);

  if (!rank) {
    std::cout.flush();
    std::cout << "Theory = " << tmp << " TEST 2: " << (!ret ? "*SUCCESS*" : "FAILURE!") << std::endl;
    std::cout << "===============================================\n";
  }

  return ret;
}

TRANSMIT_BY_CHUNKS(RangedProblem<AliasVector<int>>);

/// The same as test2, but using the library facilities RangedProblem and RangedProblem::DInfo<true>
int test3()
{ int ret = 0;

  for (int i=0; i<LOCAL_SIZE; ++i) {
    local_vector[i] = i;
  }
  
  RangedProblem<AliasVector<int>> p { local_vector, Range { 0, LOCAL_SIZE } };
  
  //auto f = std::mem_fn(static_cast<AliasVector<int> (AliasVector<int>::*)(const Range&) const>(&AliasVector<int>::range));
  //auto f = std::mem_fn(&AliasVector<int>::range);  //works if there is only one "range" method
  
  auto info = p.makeDInfo<true>(&AliasVector<int>::range, nprocs, nthreads);

  int res = dparallel_recursion<int>(p, info, ChangeBody<RangedProblem<AliasVector<int>>>(), PR_PART(), ReplicatedInput|GatherInput|ReplicateOutput );
  
  std::cout << '[' << rank << "] R= " << res << " and sz=" << p.data().size() << std::endl;
  std::cout << local_vector.data() << " should be == " << p.data().data() << std::endl;
  
  int tmp = LOCAL_SIZE - 1;
  tmp = tmp * (tmp+1) / 2;
  int local_ret = ((tmp == res) && (local_vector.data() == p.data().data()))? 0 : -1;
  
  
  // test vector correctness
  int nc = info.num_children(p);
  for (int i = 0; (i < nc) && !local_ret; i++) {
    Range tmp = info.child(i, p).r_;
    const int correct_value = nprocs > 1 ? -i : 0;
    std::cout << "[" << rank << "] Testing R[" << tmp.start << ", " << tmp.end << ")==" << correct_value << std::endl;
    for (int j = tmp.start; j < tmp.end; j++) {
      if (local_vector[j] != correct_value) {
        local_ret = -2;
        std::cout<< '[' << rank << "] Subproc[" << i << "] position is " << local_vector[j] << " instead of " << correct_value << " range=[" << tmp.start << ", " << tmp.end << ')' << std::endl;
        break;
      }
    }
  }
  
  MPI_Allreduce(&local_ret, &ret, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  
  if (!rank) {
    std::cout.flush();
    std::cout << "Theory = " << tmp << " TEST 3: " << (!ret ? "*SUCCESS*" : "FAILURE!") << std::endl;
    std::cout << "===============================================\n";
  }

  return ret;
}

int main(int argc, char** argv) {
        int ret = 0;
  
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
  
        ret = test1();
        if(!ret) ret += test2();
        if(!ret) ret += test3();
  
	MPI_Finalize();

	return ret;
}
