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
/// \file     fib.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/*
 *  Input is replicated (taken from argv).
 *  Result is only obtained in rank 0.
 */

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <sys/time.h>
#include <mpi.h>
#include <cilk/cilk_api.h>
#include <vector>
#include <algorithm>
#include <numeric>

typedef std::vector<int> Datatree_level_t;
static const float MaxImbalance = 0.2f;

int n = 25;
int tasks_per_thread = 1;
int nthreads = 8;
int nwanted_tasks, probl_per_node;

#ifndef NO_VALIDATE
size_t seq_fib(int n) {
	if (n < 2) {
		return n;
	} else {
		size_t v[2], r = 1;
		v[0] = 1;
		v[1] = 1;

		while (n > 2 ) {
			r = v[0] + v[1];
			v[0] = v[1];
			v[1] = r;
			n--;
		}

		return r;
	}
}
#endif


size_t fib(int n) {
  if (n < 2) {
    return n;
  } else {
    size_t n1, n2;
    n1 = _Cilk_spawn fib(n - 1);
    n2 = fib(n - 2);
    _Cilk_sync;
    return n1 + n2;
  }
}

float cost(int i)
{
  return powf(1.61803f, i);
}

bool rec_redistribute(float * const elem_costs, float * const process_costs, int * const elems_per_rank, const int nprocs_considered) noexcept
{ bool retry, sucess = false;
  
  if(nprocs_considered > 1) {
    do {
      int maxproc, ind;
      
      do {
        
        float * const maxp = std::max_element(process_costs, process_costs + nprocs_considered);
        maxproc = maxp - process_costs;
        if(elems_per_rank[maxproc] == 1) { //cannot rebalance a single element
          return sucess;
        }
        ind = std::accumulate(elems_per_rank, elems_per_rank + maxproc, 0);
        
        if(maxproc && ((process_costs[maxproc-1] + elem_costs[ind]) < *maxp)) {
          process_costs[maxproc-1] += elem_costs[ind];
          elems_per_rank[maxproc-1]++;
          process_costs[maxproc] -= elem_costs[ind];
          elems_per_rank[maxproc]--;
          sucess = true;
          continue;
        }
        
        ind += elems_per_rank[maxproc] - 1;
        if((maxproc < (nprocs_considered-1)) && ((process_costs[maxproc+1] + elem_costs[ind]) < *maxp)) {
          process_costs[maxproc+1] += elem_costs[ind];
          elems_per_rank[maxproc+1]++;
          process_costs[maxproc] -= elem_costs[ind];
          elems_per_rank[maxproc]--;
          sucess = true;
          continue;
        }
        
        break;
        
      } while(1);
      
      bool bal1 = rec_redistribute(elem_costs, process_costs, elems_per_rank, maxproc);
      bool bal2 = rec_redistribute(elem_costs + (ind + 1), process_costs + (maxproc + 1), elems_per_rank + (maxproc + 1), nprocs_considered - (maxproc + 1));
      retry = bal1 || bal2;
      
    } while(retry);
  }
  
  return sucess;
}

bool is_balanced(int nprocs_, int *elems_per_rank_, const Datatree_level_t& lv)
{
  const int nglobal_elems_ = static_cast<int> (lv.size());
  
  float elem_costs[nglobal_elems_], process_costs[nprocs_], *maxp;
  
  float avg = 0.f;
  for (int i = 0; i < nglobal_elems_; i++) {
    elem_costs[i] = cost(lv[i]);
    avg += elem_costs[i];
  }
  avg = avg / nprocs_; //Expected average cost per process
  
  const float upper_limit = avg + std::min(1.f / nglobal_elems_, MaxImbalance / 2.0f);
  //const float lower_limit = avg * (1.f - MaxImbalance / 2.0f);
  
  std::fill(elems_per_rank_, elems_per_rank_ + nprocs_, 0);
  std::fill(process_costs, process_costs + nprocs_, 0.f);
  
  int cur_proc = 0;
  for (int i = 0; i < nglobal_elems_; i++) {
    if ( elems_per_rank_[cur_proc] && ((process_costs[cur_proc] + elem_costs[i]) > upper_limit) && (cur_proc < (nprocs_ - 1))) {
      cur_proc++;
    }
    process_costs[cur_proc] += elem_costs[i];
    elems_per_rank_[cur_proc]++; // gets v[i]
  }
  
  rec_redistribute(elem_costs, process_costs, elems_per_rank_, nprocs_);
  
  // Compute balance based on costs
  const auto minmax_pair = std::minmax_element(process_costs, process_costs + nprocs_);
  const float min_cost = *(minmax_pair.first);
  float max_imbalance_found = (min_cost > 1e-3f) ? (*(minmax_pair.second) - min_cost) / min_cost : 1e3f;
  
  return (max_imbalance_found <= MaxImbalance) || (nglobal_elems_ >= (50 * nprocs_));
}

size_t mpi_fib(int n, int rank, int nprocs)
{ std::vector<Datatree_level_t> datatree;
  int elems_per_rank[nprocs];
  
  if(n <= 1)
    return n;
  
  datatree.emplace_back(1);
  datatree[0][0] = n;
  const Datatree_level_t *cur_level = &(datatree.back());
  
  while ((cur_level->size() < nprocs) || !is_balanced(nprocs, elems_per_rank, *cur_level)) {
    const auto cur_size = cur_level->size();
    Datatree_level_t next_level(cur_size * 2);
    
    for(int i = 0; i < cur_size; i++) {
      next_level[2*i]     = (*cur_level)[i] - 1;
      next_level[2*i + 1] = (*cur_level)[i] - 2;
    }
    
    datatree.emplace_back(std::move(next_level));
    cur_level = &(datatree.back());
  }
  
  probl_per_node = elems_per_rank[rank];
  size_t result_arr[probl_per_node], result;
  const int begin = std::accumulate(elems_per_rank, elems_per_rank + rank, 0);
  
  const int step = (nwanted_tasks >= probl_per_node) ? 1 : (probl_per_node / nwanted_tasks);
  const int limit = begin + probl_per_node;

  _Cilk_for(int i = begin; i < limit; i += step) {
    for (int j = i; j < std::min(i + step, limit); j++) {
      result_arr[j-begin] = fib((*cur_level)[j]);
    }
    
  }

  size_t local_result = std::accumulate(result_arr, result_arr + probl_per_node, (size_t)0);
  MPI_Reduce(&local_result, &result, 1, MPI_UINT64_T, MPI_SUM, 0, MPI_COMM_WORLD);
  return result;
}

int main(int argc, char** argv) {
        int rank, nprocs;
	size_t r1;
	struct timeval t0, t1, t;

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

        nthreads = __cilkrts_get_nworkers();
  
	if (argc > 1)
		n = atoi(argv[1]);

        if (argc > 2)
                tasks_per_thread = atoi(argv[2]);
  
        nwanted_tasks = nthreads * tasks_per_thread;

	gettimeofday(&t0, NULL);
	r1 = mpi_fib(n, rank, nprocs);
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

	if (rank == 0) {
		printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
		printf("compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
                printf("fib(%d): %lu\n", n, r1);
#ifndef NO_VALIDATE
		size_t sfib = seq_fib(n);
		printf("%s\n", (r1 == sfib) ? "*SUCCESS*" : " FAILURE!");
#endif
	}

	MPI_Finalize();
	return 0;
}
