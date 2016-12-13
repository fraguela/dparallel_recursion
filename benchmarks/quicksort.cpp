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

/*
 *  The initial input is only located in the root.
 *  The result is left distributed.
 */

#include <cstdlib>
#include <algorithm>
#include <sys/time.h>
#include <dparallel_recursion/dparallel_recursion.h>

//#include <boost/serialization/split_member.hpp>

using namespace dpr;

size_t n = 100000000;
int nthreads = 8;
int nprocs;
int tasks_per_thread = 1;

/*
#include <iostream>
 void print(int* a, int size) {
 for (int i = 0; i < size; i++) {
   std::cout << a[i] << std::endl;
 }
   std::cout << std::endl;
 }
*/

//namespace ser = boost::serialization;

struct range_t {

	int *begin;
	size_t size, i;

	range_t(int *begin_ = nullptr, size_t size_ = 0)
		: begin(begin_), size(size_)
                { }

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
	}

	range_t child(int nchild) {
		//return range_t(begin + nchild * i, nchild ? (size - i) : j);
		return nchild ? range_t(begin + i, size - i) : range_t(begin, i);
	}
  
        /*
	template<typename Archive>
	void save(Archive &ar, const unsigned int) const {
		ar & size;
		ar & ser::make_array(begin, size);
	}

	template<typename Archive>
	void load(Archive &ar, const unsigned int) {
		ar & size;

		if (!begin) {
			begin = (int *)malloc(sizeof(int) * size);
		}

		ar & ser::make_array(begin, size);
	}

	BOOST_SERIALIZATION_SPLIT_MEMBER()
        */
  
       template<class Archive>
       void serialize(Archive & ar, const unsigned int version)
       { }
  
       template<typename Archive>
       void gather_scatter(Archive &ar) {
                ar & begin & size;
       }
};

TRANSMIT_BY_CHUNKS(range_t);

struct QSinfo: public DInfo<range_t, 2> {
    
  QSinfo() : DInfo<range_t, 2>(nthreads * tasks_per_thread)
  {}
  
  static bool is_base(range_t &r) {
    //printf("%lu <= %lu (%d %d %d) = %d\n", r.size, (n / (4 * nthreads * nprocs * tasks_per_thread)), nthreads, nprocs, tasks_per_thread, (r.size <= (n / (4 * nthreads * nprocs * tasks_per_thread))));
    //return r.size <= (n / (4 * nthreads * nprocs * tasks_per_thread));
    return r.size <= 10000;
  }

  static range_t child(int i, range_t &r) {
    return r.child(i);
  }
  
  static float cost(const range_t &r) noexcept {
    return (float)r.size;
  }
  
};

struct QS : public EmptyBody<range_t, void> {
  
  static void base(range_t &r) {
    //printf(" sorts %lu elems\n", r.size);
    std::sort(r.begin, r.begin + r.size, std::greater<int> ());
  }

  static void pre_rec(range_t &r) {
    r.partition();
  }
  
};

int main(int argc, char **argv) {
	int rank, provided;
        struct timeval t0, t1, t;
  
        MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
        assert(provided >= MPI_THREAD_SERIALIZED);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

	if (getenv("OMP_NUM_THREADS"))
		nthreads = atoi(getenv("OMP_NUM_THREADS"));

	pr_init(nthreads);
  
	if (argc > 1)
		n = (size_t) strtoull(argv[1], NULL, 0);
	if (argc > 2)
		tasks_per_thread = atoi(argv[2]);

	int *data = (int *) malloc(n * sizeof(int)); //BBF: Actually only needed in root
        if (!rank) {
          srand(1234);
          for (size_t i=0; i<n; i++)
            data[i] = rand();
        }

        MPI_Barrier(MPI_COMM_WORLD); //So that we all measure from the same point
	gettimeofday(&t0, NULL);
  
        range_t tmp(data, n);
        QSinfo qsinfo;
	dparallel_recursion<void> (tmp, qsinfo, QS(), partitioner::automatic(), DistributedOutput|Scatter|UseCost);
  
        MPI_Barrier(MPI_COMM_WORLD); //So that we all measure up to the same point
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

        if(rank==0) {
                printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
                printf("Problem size: %lu\n", n);
		printf("compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
        }
	MPI_Finalize();
	return 0;
}
