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
/// \file     quicksort.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cstdio>
#include <algorithm>
#include <sys/time.h>
#include <mpi.h>

/*
 *  The initial input is only located in the root.
 *  The result is left distributed.
 */

size_t length = 100000000;
int nprocs;

struct range_t {
  
  int *begin;
  size_t size, i, j;
  
  range_t(int *begin_, size_t size_)
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
    //if (size <= (length / (4 * nprocs)))
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
  
};

void distributed_quicksort(int rank, int *& data, size_t length)
{ MPI_Status status;

  // Do recursive quicksort starting at processor 0 and spreading out recursive calls to other machines
  range_t r(data, length);

  for (int step = 1; step < nprocs; step *= 2) {
    if (rank < step) {
      if ((rank + step) < nprocs) {
        r.partition();
        
        //printf("[%d] partitions %d elems at point %d\n", rank, localDataSize, pivot);
        
        // Send everything after the pivot to processor rank + step and keep up to the pivot
        MPI_Send(data+r.i, r.size - r.i, MPI_INT, rank + step, 0, MPI_COMM_WORLD);
        r.size = r.i;
      }
    }
    else if (rank < (2 * step)) {
      int localDataSize;
      
      MPI_Probe(rank - step, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
      // How much data did we really get?
      MPI_Get_count(&status, MPI_INT, &localDataSize);
      
      data = (int *) malloc(localDataSize * sizeof(int));
      // Get data from processor rank - step
      MPI_Recv(data, localDataSize, MPI_INT, rank - step, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

      r.begin = data;
      r.size = localDataSize;
    }
  }
  
  //printf("[%d] sorts %lu elems\n", rank, r.size);
  
  // Perform local sort
  r.quicksort();

}

int main(int argc, char *argv[]) {
  struct timeval start, end, t;
  int rank;
  int *data;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  MPI_Comm_size (MPI_COMM_WORLD, &nprocs);
  
  if (argc > 1)
    length = (size_t) strtoull(argv[1], NULL, 0);

  if (!rank) {
    srand(1234);
    data = (int *) malloc(length * sizeof(int));
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
    printf("Nprocs=%d\n", nprocs);
    printf("Problem size: %lu\n", length);
    printf("[%d] compute time: %f\n", rank, t.tv_sec + t.tv_usec / 1000000.0);
  }
  MPI_Finalize();
  return 0;
}
