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
/// \file     test_distrAddVector.cpp
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

struct DVInfo : public DInfo<Local_t, 2> {
	DVInfo() {}

	bool is_base(const Local_t& r) const {
		return r.size() < BASE_SIZE;
	}

	Local_t child(int i, const Local_t& r) const {
		int sz = r.size() / 2;
		return Local_t(r.data() + (i ? sz: 0), sz);
	}

};

struct AdditionBody : public EmptyBody<Local_t, int> {
	int base(Local_t& t) const {
		int r =0, sz=t.size();

		for (int i=0; i<sz; ++i)
			r += t[i];

		return r;
	}

	int post(const Local_t& t, const int* r) const {
		return r[0] + r[1];
	}
};

bool isPowerOf2(int i) {
	return !(i & (i-1));
}

/// Actually holds the data
std::vector<int> replicated_vector(LOCAL_SIZE);

/// Allow to partition the work on replicated_vector without making copies
Local_t local_vector(replicated_vector);

int main(int argc, char** argv) {
	int rank, provided, nthreads, nprocs, ret = 0;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	assert(provided >= MPI_THREAD_SERIALIZED);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

	const char* nthreads_env_var = getenv("OMP_NUM_THREADS");
        nthreads = (nthreads_env_var != NULL)? atoi(nthreads_env_var) : 8; //tbb::task_scheduler_init::automatic;

	tbb::task_scheduler_init init(nthreads);

	for (int i=0; i<LOCAL_SIZE; ++i) {
		local_vector[i] = LOCAL_SIZE * rank + i;
	}

        if (!rank) {
                printf("Nprocs=%d threads=%d\n", nprocs, nthreads);
        }
  
	if (!isPowerOf2(nprocs)) {
                if (!rank) {
			std::cout << "This implementation only supports a number of processors that is a power of 2\n";
                }
                ret = -1;
	} else {
		int res = dparallel_recursion<int>(local_vector, DVInfo(), AdditionBody(), PR_PART(), DistributedInput);

		if (!rank) {
			std::cout << "R from " << rank << " = " << res << std::endl;

			int tmp = LOCAL_SIZE * nprocs - 1;
			tmp = tmp * (tmp+1) / 2;
			std::cout << "Theory = " << tmp << ' ' << ((tmp == res) ? "*SUCCESS*" : "FAILURE!") << std::endl;
                        ret = (tmp == res) ? 0 : -1;
		}
	}

	MPI_Finalize();
	return ret;
}
