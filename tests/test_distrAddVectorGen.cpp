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
/// \file     test_distrAddVectorGen.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstring>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"
#include "dparallel_recursion/AliasVector.h"

#define LOCAL_SIZE 1024
#define BASE_SIZE (LOCAL_SIZE/4)

using namespace dpr;

typedef AliasVector<int> Local_t;

int rank, provided, nthreads, nprocs;

/// Actually holds the data
std::vector<int> replicated_vector(LOCAL_SIZE);

/// Allow to partition the work on replicated_vector without making copies
Local_t local_vector(replicated_vector);

struct DVInfo : public DInfo<Local_t, UNKNOWN> {
	DVInfo() {}

	int num_children(const Local_t& t) const {
		return t.size() ? 2 : nNodes();
	}

	bool is_base(const Local_t& r) const {
		return r.size() < BASE_SIZE;
	}

	Local_t child(int i, const Local_t& r) const {
		int sz = r.size() / 2;
		return Local_t(r.data() + (i ? sz: 0), sz);
	}
};

template<bool NEGATE>
struct AdditionBody : public EmptyBody<Local_t, int> {
	int base(Local_t& t) const {
		int r =0, sz=t.size();

                for (int i=0; i<sz; ++i) {
                        if (NEGATE) {
                                t[i] = -2 * t[i];
                        }
			r += t[i];
                }
          
		return r;
	}

	int post(const Local_t& t, const int* r) const {
		if (t.size()) {
			return r[0] + r[1];
		} else {
			int sum = 0;

			for (int i = 0; i < nprocs; ++i) {
				//std::cout << r[i] << "\n";
				sum += r[i];
			}

			return sum;
		}
	}

};

int main(int argc, char** argv) {
        int ret = 0, i, tmp;
  
	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	assert(provided >= MPI_THREAD_SERIALIZED);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

	const char* nthreads_env_var = getenv("OMP_NUM_THREADS");
	nthreads = (nthreads_env_var != NULL)? atoi(nthreads_env_var) : 8; //tbb::task_scheduler_init::automatic;

	tbb::task_scheduler_init init(nthreads);

	for (i=0; i<LOCAL_SIZE; ++i) {
		local_vector[i] = LOCAL_SIZE * rank + i;
	}

	int res = dparallel_recursion<int>(local_vector, DVInfo(), AdditionBody<false>(), PR_PART(), DistributedInput);

	if (!rank) {
                printf("Nprocs=%d threads=%d\n", nprocs, nthreads);
                tmp = LOCAL_SIZE * nprocs - 1;
                tmp = tmp * (tmp+1) / 2;
                printf("Result = %d\nTheory = %d\n%s\n", res, tmp, (tmp == res) ? "*SUCCESS*" : "FAILURE!");
                ret = (tmp == res) ? 0 : -1;
	}

        // Reuses same local vector with a different info object
        DVInfo dvi;
        res = dparallel_recursion<int>(local_vector, dvi, AdditionBody<true>(), PR_PART(), DistributedInput);

        for (i=0; i<LOCAL_SIZE; ++i) {
          if (local_vector[i] != (-2 * (LOCAL_SIZE * rank + i))) {
            printf("map operation FAILURE in rank %d!\n", rank);
            break;
          }
        }
  
        ret += (i == LOCAL_SIZE) ? 0 : -1;
  
        if (!rank) {
          tmp = -2 * tmp;
          printf("Result = %d\nTheory = %d\n%s\n", res, tmp, (tmp == res) ? "*SUCCESS*" : "FAILURE!");
          ret += (tmp == res) ? 0 : -1;
        }

       // Reuses the local vectors from the same info object. No need for DistributedInput flag
       res = dparallel_recursion<int>(Local_t(), dvi, AdditionBody<true>(), PR_PART());
  
       for (i=0; i<LOCAL_SIZE; ++i) {
          if (local_vector[i] != (4 * (LOCAL_SIZE * rank + i))) {
            printf("map operation FAILURE in rank %d!\n", rank);
            break;
          }
        }
        
        ret += (i == LOCAL_SIZE) ? 0 : -1;
        
        if (!rank) {
          tmp = -2 * tmp;
          printf("Result = %d\nTheory = %d\n%s\n", res, tmp, (tmp == res) ? "*SUCCESS*" : "FAILURE!");
          ret += (tmp == res) ? 0 : -1;
        }
  
	MPI_Finalize();
	return ret;
}
