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
/// \file     test_quicksort.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/*
 *  The initial input is only located in the root.
 *  The result is left distributed.
 */

#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"
#include <boost/serialization/split_member.hpp>

using namespace dpr;

size_t n = 1000000;
int nthreads = 8;
int rank, nprocs;
int tasks_per_thread = 1;
int *a = nullptr, *seq;

using Chrono_t = std::chrono::high_resolution_clock;
Chrono_t::time_point t0;

namespace ser = boost::serialization;

struct range_t {
  
  int *begin;
  size_t size, i;
  
  range_t(int *begin_ = nullptr, size_t size_ = 0)
  : begin(begin_), size(size_) {}

  void partition() {
    int *array = begin;
    int *key0 = begin;
    size_t m = size / 2u;
    std::swap ( array[0], array[m] );
    
    i = 0;
    size_t j = size;
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
    
    // printf("[%d] P(%lu -> %lu + %lu)\n", rank, size, i, size-i);
  }
  
  range_t child(int nchild) {
    //return range_t(begin + nchild * i, nchild ? (size - i) : j);
    return nchild ? range_t(begin + i, size - i) : range_t(begin, i);
  }
  
  template<typename Archive>
  void save(Archive &ar, const unsigned int) const {
    ar & size;
    if(size)
      ar & ser::make_array(begin, size);
    printf("[%d] Saving %lu elements. First one is %d (%p)\n", rank, size, size ? *begin : 0, this);
  }
  
  template<typename Archive>
  void load(Archive &ar, const unsigned int) {
    ar & size;
    
    if (!begin && size) {
      begin = (int *)malloc(sizeof(int) * size);
    }
    
    if(size)
      ar & ser::make_array(begin, size);

    printf("[%d] Loading %lu elements. First one is %d (%p)\n", rank, size, size ? *begin : 0, this);
  }
  
  BOOST_SERIALIZATION_SPLIT_MEMBER()
  
  template<typename Archive>
  void gather_scatter(Archive &ar) {
    
    if(rank) {
      assert(begin == nullptr);
    } else {
      assert(begin != nullptr);
    }
    
    printf(" [%d] in %p %d\n", rank, begin, (int)size);
    
    ar & begin & size;
    
    printf(" [%d] out %p %d\n", rank, begin, (int)size);
  }
  
};

TRANSMIT_BY_CHUNKS(range_t);

struct QSinfo: public DInfo<range_t, 2> {
  
  QSinfo() : DInfo<range_t, 2>(nthreads * tasks_per_thread)
  {}
  
  static bool is_base(range_t &r) noexcept {
    //printf("%lu <= %lu (%d %d %d) = %d\n", r.size, (n / (4 * nthreads * nprocs * tasks_per_thread)), nthreads, nprocs, tasks_per_thread, (r.size <= (n / (4 * nthreads * nprocs * tasks_per_thread))));
    //return r.size <= (n / (4 * nthreads * nprocs * tasks_per_thread));
    return r.size <= (n / 100);
  }
  
  static range_t child(int i, range_t &r) {
    return r.child(i);
  }
  
  static float cost(const range_t &r) noexcept {
    return (float)r.size;
  }
    
};

struct QS : public EmptyBody<range_t, void> {
  void base(range_t &r) {
    //printf(" sorts %lu elems\n", r.size);
    std::sort(r.begin, r.begin + r.size, std::greater<int> ());
  }
  
  void pre_rec(range_t &r) {
    r.partition();
  }
};

void displayInfo(const QSinfo& qsinfo, const char *str = nullptr)
{  
  MPI_Barrier(MPI_COMM_WORLD);
  
  std::chrono::duration<float> time_s = Chrono_t::now() - t0;

  if (!rank && (str != nullptr) ) {
    printf("\nRun with %s (%f s.): \n", str, time_s.count());
  }

  for (int i=0; i< nprocs; i++) {
    MPI_Barrier(MPI_COMM_WORLD);
    if (rank == i) {
      printf(" [%d] global_root_ptr=%p  hasRoot=%c Nodes=%d\n", rank, qsinfo.globalRoot(), qsinfo.hasRoot() ? 'Y' : 'N', qsinfo.nNodes());
      if (qsinfo.globalRoot() != nullptr) {
        const int *p = qsinfo.globalRoot()->begin;
        size_t sz = qsinfo.globalRoot()->size;
        printf("     Root has (ptr to array=%p, length=%lu) *p=%d\n", p, sz, ((p != nullptr) && sz) ? p[0] : 0);
      }
      printf(" [%d] %d decomposition levels\n", rank, qsinfo.nLevels());
      printf(" [%d] %d elements globally at the decomposition level\n", rank, qsinfo.nGlobalElems());
      printf(" [%d] %d local elements (==%d)\n", rank, qsinfo.nLocalElems(), qsinfo.nLocalElems(i));
      puts("-----------------------------");
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);
}

void reinit(int *a, int *seq, int n)
{
  srand(1234);

  for (size_t i=0; i<n; i++)
    a[i] = rand();
  
  memcpy(seq, a, n * sizeof(int));
  std::sort(seq, seq + n, std::greater<int> ());
}

int verify(const int test_num)
{ int success;
  
  if(rank == 0) {
    success = !memcmp(seq, a, n * sizeof(int));
    printf("TEST%d: %s\n", test_num, success ? "*SUCCESS*" : " FAILURE!");
  }

  MPI_Bcast((void *)&success, 1, MPI_INT, 0, MPI_COMM_WORLD);

  return success ? 0 : -1;
}
    
int test(const int flags, const char * const flag_c)
{ static int NumTest = 0;

  NumTest++;
  
  QSinfo qsinfo;
  
  if (rank == 0) {
    reinit(a, seq, n);
  }

  t0 = Chrono_t::now();
  
  dparallel_recursion<void> (range_t(a, n), qsinfo, QS(), partitioner::automatic(), flags);

  displayInfo(qsinfo, flag_c);
  
  return verify(NumTest);
}
    
int main(int argc, char **argv) {
  int provided, ret = 0;
  
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  assert(provided >= MPI_THREAD_SERIALIZED);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
  if (getenv("OMP_NUM_THREADS"))
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  
  tbb::task_scheduler_init init(nthreads);
  
  if (argc > 1)
    n = (size_t) strtoull(argv[1], NULL, 0);
  if (argc > 2)
    tasks_per_thread = atoi(argv[2]);
  
  srand(1234);

  if (!rank) {
    printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
    printf("Problem size: %lu\n", n);
    
    a = (int *) malloc(n * sizeof(int));
    seq = (int *) malloc(n * sizeof(int));

    printf("Original Root ptr at 0 in %p\n", a);
  }

  ret = test(DefaultBehavior, "DefaultBehavior")
  || test(ReplicateInput|ReplicateOutput, "ReplicateInput|ReplicateOutput")
  || test(PrioritizeDM, "PrioritizeDM")
  || test(Scatter, "Scatter")
  // || test(GreedyParallel, "GreedyParallel")
  || test(Balance, "Balance")
  || test(Balance|UseCost, "Balance|UseCost")
  // || test(Balance|UseCost|GreedyParallel, "Balance|UseCost|GreedyParallel")
  // || test(Scatter|Balance|UseCost|GreedyParallel, "Scatter|Balance|UseCost|GreedyParallel")
  // || test(PrioritizeDM|Balance|GreedyParallel, "PrioritizeDM|Balance|GreedyParallel")
  ;
  
  MPI_Finalize();
  
  return ret;
}
