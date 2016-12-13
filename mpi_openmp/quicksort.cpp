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
/// \file     quicksort2.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdio>
#include <algorithm>
#include <sys/time.h>
#include <mpi.h>
#include <omp.h>
#include <vector>
#include <algorithm>
#include <numeric>
#include <chrono>

static const float MaxImbalance = 0.2f;
using profile_clock_t = std::chrono::high_resolution_clock;
int nglobal_elems_;

/*
 *  The initial input is only located in the root.
 *  The result is left distributed.
 */

size_t length = 100000000;
int nprocs;
int nthreads;
int tasks_per_thread = 1;
int nwanted_tasks, probl_per_node;

struct range_t {
  
  int *begin;
  size_t size, i, j;
  
  range_t(int *begin_ = NULL, size_t size_ = 0)
  : begin(begin_), size(size_) {}
  
  void partition() {
    int *array = begin;
    int *key0 = begin;
    size_t m = size / 2u;
    std::swap ( array[0], array[m] );
    
    i = 0;
    j = size;
    // Partition interval [i+1,j-1] with key *key0.
    for (;;) {
      // Loop must terminate since array[l]==*key0.
      do {
        --j;
      } while ( *key0 > array[j] );
      do {
        if ( i == j ) goto partition;
        ++i;
      } while ( array[i] > *key0 );
      if ( i == j ) goto partition;
      std::swap( array[i], array[j] );
    }
  partition:
    // Put the partition key where it belongs
    std::swap( array[j], *key0 );
    // array[l..j) is less or equal to key.
    // array(j..r) is std::greater than or equal to key.
    // array[j] is equal to key
    i = j + 1;
  }

  void quicksort() {
    if (size < 2) return;
    //if (size <= (length / (4 * nthreads * nprocs * tasks_per_thread)))
    if (size <= 10000) {
      std::sort(begin, begin + size, std::greater<int> ());
    } else {
      partition();
      range_t r2(begin + i, size - i);
      r2.quicksort();
      size = i;
      quicksort();
    }
  }
  
  void quicksort_up(int l) {
    if (size < 2) return;
    //const size_t was_size = size;
    //printf("pB%d %lu (%d)\n", l, was_size, omp_get_num_threads());
    if ( ((1 << l) * probl_per_node) >= nwanted_tasks ) {
      //printf("[%lu]\n", size );
      quicksort();
    } else {
      partition();
      //printf("%lu,%lu\n", i, size -i );
      range_t r2(begin + i, size - i);
#pragma omp task default(shared) firstprivate(r2,l) untied
      r2.quicksort_up(l + 1);
      size = i;
      quicksort_up(l + 1);
    }
    //printf("pE%d %lu (%d)\n", l, was_size, omp_get_num_threads());
  }

};

typedef std::vector<range_t> Datatree_level_t;

bool heuristicRebalanceMustStop() noexcept
{ static std::chrono::time_point<profile_clock_t> tp;
  static float prevPeriod_ = 0.f;
  
  std::chrono::time_point<profile_clock_t> cur_tp = profile_clock_t::now();
  
  bool must_stop = (nglobal_elems_ >= (50 * nprocs));
  
  if(prevPeriod_ == 0.f) {
    prevPeriod_ = -1.f; // Only take tp. Skip timing tests
  } else {
    const float period = std::chrono::duration<float>(cur_tp - tp).count();
    if(prevPeriod_ > 0.f) { // Non-first measurement
      if(!must_stop) {
        const float time_limit = std::max(prevPeriod_ * 1.4f, 0.25f);
        must_stop = must_stop || (period > time_limit);
      }
    } else {
      prevPeriod_ = period;
    }
  }
  
  // printf("glbl=%d time=%f\n", nglobal_elems_, prevPeriod_);

  tp = cur_tp;
  
  return must_stop;
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
  nglobal_elems_ = static_cast<int> (lv.size());
  
  float elem_costs[nglobal_elems_], process_costs[nprocs_], *maxp;
  
  float avg = 0.f;
  for (int i = 0; i < nglobal_elems_; i++) {
    elem_costs[i] = lv[i].size; //cost
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
  
  return (max_imbalance_found <= MaxImbalance) || heuristicRebalanceMustStop();
}

void distributed_quicksort(int rank, int *data, size_t length)
{ int my_items = 1, *my_items_sz, *all_items_sz, scalar_my_items_sz;
  int all_items_displ[nprocs], elems_per_rank[nprocs];
  
  if (nprocs > 1) {
    std::vector<Datatree_level_t> datatree;

    if (!rank) {
      datatree.emplace_back(1);
      datatree[0][0] = range_t (data, length);
      Datatree_level_t *cur_level = &(datatree.back());
      
      while ((cur_level->size() < nprocs) || !is_balanced(nprocs, elems_per_rank, *cur_level)) {
        const auto cur_size = cur_level->size();
        Datatree_level_t next_level(cur_size * 2);
        
#pragma omp parallel for schedule(dynamic, 1) default(shared) if(cur_size > 1)
        for(int i = 0; i < cur_size; i++) {
          auto& tmp = (*cur_level)[i];
          if (tmp.size < 2) {
            tmp.i = tmp.size;
          } else {
            tmp.partition();
          }
          //printf("[%d] partitions %lu elems at point %lu. Sending %d\n", rank, tmp.size, tmp.i, tmp.begin[tmp.i]);
          next_level[2*i]      = tmp;
          next_level[2*i].size = tmp.i;
          next_level[2*i + 1]  = range_t(tmp.begin + tmp.i, tmp.size - tmp.i);
        }
        
        datatree.emplace_back(std::move(next_level));
        cur_level = &(datatree.back());
      }

      all_items_sz    = new int [cur_level->size()];
      for (int i = 0; i < cur_level->size(); i++) {
        all_items_sz[i] = (*cur_level)[i].size;
      }
      all_items_displ[0] = 0;
      std::partial_sum(elems_per_rank, elems_per_rank + (nprocs - 1), all_items_displ + 1);
    }
    
    MPI_Scatter(elems_per_rank, 1, MPI_INT, &my_items, 1, MPI_INT, 0, MPI_COMM_WORLD);
    my_items_sz = new int [my_items];
    MPI_Scatterv(all_items_sz, elems_per_rank, all_items_displ, MPI_INT, my_items_sz, my_items, MPI_INT, 0, MPI_COMM_WORLD);
    
    scalar_my_items_sz = std::accumulate(my_items_sz, my_items_sz + my_items, 0);
  
    if (!rank) {
      for (int i = 0; i < nprocs; i++) {
        const auto begin = all_items_sz + all_items_displ[i];
        all_items_sz[i] = std::accumulate(begin, begin + elems_per_rank[i], 0);
        //printf("Proc %3d : %d elems %d comp\n", i, elems_per_rank[i], all_items_sz[i]);
      }
      all_items_displ[0] = 0;
      std::partial_sum(all_items_sz, all_items_sz + (nprocs - 1), all_items_displ + 1);
    }
    
    MPI_Scatterv(data, all_items_sz, all_items_displ, MPI_INT, rank ? data : MPI_IN_PLACE, scalar_my_items_sz, MPI_INT, 0, MPI_COMM_WORLD);
    
    //printf("[%d] sorts %d elems (First: %d)\n", rank, scalar_my_items_sz, data[0]);
    if (!rank) {
      delete [] all_items_sz;
    }
    
  } else {
    my_items_sz = new int [1];
    *my_items_sz = length;
  }

  /* DEBUG
  for (int p = 0; p < nprocs; p++) {
    if (p == rank) {
      printf("[%d] recv %d items with %d elems\n", rank, my_items, scalar_my_items_sz);
      for (int i = 0; i < my_items; i++)
        printf("  it %d b:%d l:%d\n", i, std::accumulate(my_items_sz, my_items_sz + i, 0), my_items_sz[i]);
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }
  */
  
  // Perform local sort

  probl_per_node = my_items;
  
  const int step = (nwanted_tasks >= probl_per_node) ? 1 : (probl_per_node / nwanted_tasks);
  
#pragma omp parallel
#pragma omp single
  for (int i = 0; i < probl_per_node; i += step) {
#pragma omp task default(shared) firstprivate(i, probl_per_node, step) untied
    for (int j = i; j < std::min(i + step, probl_per_node); j++) {
    // { printf("%dS%d %d\n", rank, i, my_items_sz[i]);
      range_t(data + std::accumulate(my_items_sz, my_items_sz + j, 0), my_items_sz[j]).quicksort_up(0);
    //  printf("%dE%d %d\n", rank,i, my_items_sz[i]); }
    }
  }

/*
#pragma omp parallel for schedule(dynamic, 1) default(shared) if(my_items > 1)
  for (int i = 0; i < my_items; i++) {
    printf("%dS%d %d\n", rank, i, my_items_sz[i]);
    range_t(data + std::accumulate(my_items_sz, my_items_sz + i, 0), my_items_sz[i]).quicksort_up(0);
    printf("%dE%d %d\n", rank,i, my_items_sz[i]);
  }
*/
  delete [] my_items_sz;
}

int main(int argc, char *argv[]) {
  struct timeval start, end, t;
  int rank;
  
  MPI_Init(&argc, &argv);
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  MPI_Comm_size (MPI_COMM_WORLD, &nprocs);
  
  nthreads = omp_get_max_threads();

  if (argc > 1)
    length = (size_t) strtoull(argv[1], NULL, 0);
  if (argc > 2)
    tasks_per_thread = atoi(argv[2]);
  
  nwanted_tasks = nthreads * tasks_per_thread;

  int *data = (int *) malloc(length * sizeof(int));	// Big enough to hold it all

  if (!rank) {
    srand(1234);
    for (size_t i=0; i<length; i++)
      data[i] = rand();
  }
  

  MPI_Barrier(MPI_COMM_WORLD); //So that we all measure from the same point

  // Time everything after exchange of data until sorting is complete
  gettimeofday(&start, 0);
  
  distributed_quicksort(rank, data, length);

  MPI_Barrier(MPI_COMM_WORLD); //So that we all measure up to the same point
  // Measure elapsed time
  gettimeofday(&end, 0);
  timersub(&end, &start, &t);

  if (rank == 0) {
    printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
    printf("Problem size: %lu\n", length);
    printf("[%d] compute time: %f\n", rank, t.tv_sec + t.tv_usec / 1000000.0);
  }
  MPI_Finalize();
  return 0;
}
