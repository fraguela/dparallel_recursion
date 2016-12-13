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
/// \file     test_gatherVector.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstring>
#include <tbb/task_scheduler_init.h>
#include <boost/serialization/array.hpp>
#include "dparallel_recursion/dparallel_recursion.h"
#include "dparallel_recursion/FillableAliasVector.h"

#define VECTOR_SIZE 8192
#define BASE_SIZE (VECTOR_SIZE/8)

using namespace dpr;

int rank, provided, nthreads, nprocs;

typedef FillableAliasVector<int> Local_t;

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

struct AdditionNegBody : public EmptyBody<Local_t, int> {

	int base(Local_t& t) const {
		int r = 0, sz = t.size();

		for (int i = 0; i < sz; ++i) {
			t[i] = -t[i];
			r += t[i];
		}

		return r;
	}

	int post(const Local_t& t, const int* r) const {
		return r[0] + r[1];
	}
};

int display_results(int res, const Local_t& v, int lastval) {
        bool success = true;
  
	if (!rank) {
		std::cout << "R from " << rank << " = " << res << std::endl;

		int tmp = VECTOR_SIZE - 1;
		tmp = -tmp * (tmp + 1) / 2;
		std::cout << "Theory = " << tmp << std::endl;
		std::cout << "Elem 1 = " << v[1] << " Last elem = " << v[VECTOR_SIZE - 1] << std::endl;
		success = (tmp == res) && (v[1] == -1) && (v[VECTOR_SIZE - 1] == lastval);
		std::cout << (success ? "*SUCCESS*" : " FAILURE!") << std::endl;
	}
  
        return success ? 0 : -1;
}

int main(int argc, char** argv) {
	Local_t local_vector(VECTOR_SIZE);
	int res;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	assert(provided >= MPI_THREAD_SERIALIZED);

	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

	const char* nthreads_env_var = getenv("OMP_NUM_THREADS");
	nthreads = (nthreads_env_var != NULL) ? atoi(nthreads_env_var) : 8; //tbb::task_scheduler_init::automatic;

	tbb::task_scheduler_init init(nthreads);

	if (!rank) {
                printf("Nprocs=%d threads=%d\n", nprocs, nthreads);
          
		for (int i = 0; i < VECTOR_SIZE; ++i) {
			local_vector[i] = i;
		}
	}

	Local_t local_vector2(local_vector.deepCopy()); //Deep copy, to retain original value

	res = dparallel_recursion<int>(local_vector, DVInfo(), AdditionNegBody(), PR_PART() /*, GatherInput */);

	int ret = display_results(res, local_vector, (nprocs > 1) ? (VECTOR_SIZE - 1) : -(VECTOR_SIZE - 1) );

	res = dparallel_recursion<int>(local_vector2, DVInfo(), AdditionNegBody(), PR_PART(), GatherInput);

	ret += display_results(res, local_vector2, -(VECTOR_SIZE - 1));

	MPI_Finalize();
  
	return ret;
}
