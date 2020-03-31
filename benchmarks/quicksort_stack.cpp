/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015-2020 Millan A. Martinez, Basilio B. Fraguela, Jose C. Cabaleiro. Universidade da Coruna
 
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
/// \file     quicksort_stack.cpp
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

/*
 *  The initial input is only located in the root.
 *  The result is left distributed.
 */

#include <cstdlib>
#include <algorithm>
#include <sys/time.h>
#include <vector>
#include <utility>
#include <iostream>
#include "dparallel_recursion/parallel_stack_recursion.h"

size_t n = 100000000;
int nthreads = 8;
int limitParallel = 200000;

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

	range_t clone() {
		int *newbegin = new int[size];
		for (size_t i=0; i<size; i++) {
			newbegin[i] = begin[i];
		}
		return range_t(newbegin, size);
	}

	void deallocate() {
		delete[] begin;
		begin = nullptr;
	}

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

	range_t child(const int nchild) const {
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

struct QSinfo: public dpr::Arity<2> {
    
  QSinfo() : dpr::Arity<2>(nthreads, nthreads)
  {}
  
  static bool is_base(const range_t &r) {
    //printf("%lu <= %lu (%d %d %d) = %d\n", r.size, (n / (4 * nthreads * nprocs * tasks_per_thread)), nthreads, nprocs, tasks_per_thread, (r.size <= (n / (4 * nthreads * nprocs * tasks_per_thread))));
    //return r.size <= (n / (4 * nthreads * nprocs * tasks_per_thread));
    return r.size <= 10000;
  }

  static range_t child(int i, range_t &r) {
    return r.child(i);
  }
  
  static bool do_parallel(const range_t &r) noexcept {
    return r.size > limitParallel;
  }

  /*static float cost(const range_t &r) noexcept {
    return (float)r.size;
  }*/
  
};

struct QS : public dpr::EmptyBody<range_t, void> {
  
  static void base(const range_t &r) {
    //printf(" sorts %lu elems\n", r.size);
    std::sort(r.begin, r.begin + r.size, std::greater<int> ());
  }

  static void pre_rec(range_t &r) {
    r.partition();
  }
  
};

int main(int argc, char **argv) {
	struct timeval t0, t1, t;

	int chunkSize = 1;
    int stackSize = 500000;
    int partitioner = 0;
    size_t test_n = 0;
    bool runTests = false;
	std::vector<dpr::ResultChunkTest> listChunks;
	
	if (getenv("OMP_NUM_THREADS")) {
		nthreads = atoi(getenv("OMP_NUM_THREADS"));
	}
  
	if (argc > 1) {
		n = (size_t) strtoull(argv[1], NULL, 0);
		test_n = n;
	}

	if (argc > 2) {
        chunkSize = atoi(argv[2]);
    }
    
    if (argc > 3) {
        stackSize = atoi(argv[3]);
    }

	if (argc > 4) {
		partitioner = atoi(argv[4]);
	}

	if (partitioner == 1) {	//_partitioner = 1 => custom
		if (argc > 5) {
			limitParallel = atoi(argv[5]);
		}
	}

	if (argc > 6) {
		if (argv[6] != nullptr) {
			test_n = atoi(argv[6]);
		}
	}

	dpr::AutomaticChunkOptions opt = dpr::aco_test_default;

	if (argc > 7) {
		if (chunkSize > 0) {
			runTests = true;
		} else {
			runTests = false;
			opt = dpr::aco_default;
		}
		const int testSize = atoi(argv[7]);
		if (testSize < 0) {
			opt.limitTimeOfEachTest = false;
		} else {
			opt.limitTimeOfEachTest = true;
			opt.testSize = testSize;
		}
	} else {
		opt = dpr::aco_default;
	}

	if (argc > 8) {
		opt.targetTimePerTest = atof(argv[8]);
	}

	if (argc > 9) {
		opt.maxTime = atof(argv[9]);
	}

	if (argc > 10) {
		opt.initTolerance = atoi(argv[10]);
	}

	if (argc > 11) {
		opt.finalTolerance = atoi(argv[11]);
	}

	if (argc > 12) {
		opt.maxNumChunksTestAllowed = atoi(argv[12]);
	}

	if (argc > 13) {
		opt.mode = atoi(argv[13]);
	}

	if (argc > 14) {
		opt.subMode = atoi(argv[14]);
	}

	if (argc > 15) {
		opt.calcMode = atoi(argv[15]);
	}

	if (argc > 16) {
		opt.verbose = atoi(argv[16]);
	} else {
		opt.verbose = 4;
	}
	
	dpr::prs_init(nthreads, stackSize);
	
	int *data = new int[n]; //BBF: Actually only needed in root
	srand(1234);
	for (size_t i=0; i<n; i++) {
		data[i] = rand();
	}

	range_t tmp(data, n);
	QSinfo qsinfo;
	//dparallel_recursion<void> (tmp, qsinfo, QS(), partitioner::automatic(), DistributedOutput|Scatter|UseCost);
	if (!runTests) {
		if (chunkSize > 0) {
			gettimeofday(&t0, NULL);
			if (partitioner == 1) {
				//_partitioner = 1 => custom
				dpr::parallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::custom());
			} else if (partitioner == 2) {
				//_partitioner = 2 => automatic
				dpr::parallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::automatic());
			} else {
				//_partitioner = 0 => simple
				dpr::parallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::simple());
			}
		} else {
			int *test_data = new int[test_n]; //BBF: Actually only needed in root
			for (size_t i=0; i<test_n; i++) {
				test_data[i] = data[i];
			}
			range_t tmp_test(test_data, test_n);
			gettimeofday(&t0, NULL);
			if (partitioner == 1) {
				//_partitioner = 1 => custom
				dpr::parallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::custom(), opt, tmp_test);
			} else if (partitioner == 2) {
				//_partitioner = 2 => automatic
				dpr::parallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::automatic(), opt, tmp_test);
			} else {
				//_partitioner = 0 => simple
				dpr::parallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::simple(), opt, tmp_test);
			}
		}
	} else {
		gettimeofday(&t0, NULL);
		if (partitioner == 1) {
			//_partitioner = 1 => custom
			listChunks = dpr::parallel_stack_recursion_test<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::custom(), opt);
		} else if (partitioner == 2) {
			//_partitioner = 2 => automatic
			listChunks = dpr::parallel_stack_recursion_test<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::automatic(), opt);
		} else {
			//_partitioner = 0 => simple
			listChunks = dpr::parallel_stack_recursion_test<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::simple(), opt);
		}
	}

	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

	std::cout << "Threads=" << nthreads;
	if ((chunkSize > 0) || (runTests)) {
		std::cout << " chunkSize=" << chunkSize;
	} else {
		std::cout << " chunkSize=" << dpr::getPsrLastRunExtraInfo().chunkSizeUsed << "(auto)";
	}
	std::cout << " stackSize=" << stackSize << std::endl;
	if (!runTests) {
		std::cout << "compute time: " << (t.tv_sec + t.tv_usec / 1000000.0) << std::endl;
		if (chunkSize <= 0) {
			std::cout << "  (autochunk test time): " << dpr::getPsrLastRunExtraInfo().testChunkTime << " (run time): " << dpr::getPsrLastRunExtraInfo().runTime << std::endl;
		}
#ifndef NO_VALIDATE
		bool checkOrder = true;
		for (size_t i=0; i<(n-1); i++) {
			if (data[i] < data[i+1]) {
				checkOrder = false;
			}
		}
		if (checkOrder) {
			std::cout << "check results: *SUCCESS*" << std::endl;
		} else {
			std::cout << "check results: FAILURE!" << std::endl;
		}
#endif
	} else {
		std::cout << "test time: " << (t.tv_sec + t.tv_usec / 1000000.0) << std::endl;
		std::cout << "test parameters: " << "limitTimeOfEachTest=" << opt.limitTimeOfEachTest << " testSize=" << opt.testSize << " targetTimePerTest=" << opt.targetTimePerTest
		<< " maxTime=" << opt.maxTime << " initTolerance=" << opt.initTolerance << " finalTolerance=" << opt.finalTolerance << " maxNumChunksTestAllowed=" << opt.maxNumChunksTestAllowed
		<< " mode=" << opt.mode	<< " subMode=" << opt.subMode << " calcMode=" << opt.calcMode << " verbose=" << opt.verbose << std::endl;
		std::cout << "test results table:" << std::endl;
		std::cout << "--------------" << std::endl;
		std::cout << "chunkId\tscore" << std::endl;
		for (const dpr::ResultChunkTest iChunkInfo : listChunks) {
			std::cout << iChunkInfo.chunkId << "\t" << iChunkInfo.score << std::endl;
		}
		std::cout << "--------------" << std::endl;
	}
	return 0;
}
