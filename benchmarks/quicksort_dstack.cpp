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
/// \file     quicksort_dstack.cpp
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

/*
 *  The initial input is only located in the root.
 *  The result is left distributed.
 */

#include <iostream>
#include <cstdlib>
#include <algorithm>
#include <sys/time.h>
#include <vector>
#include <dparallel_recursion/dparallel_stack_recursion.h>

size_t n = 100000000;
int nprocs;
int nthreads = 8;
size_t limitParallel = 200000;
int *glData;
int *oldGlData;

struct range_t {

	int *begin;
	size_t size, i, x;

	range_t(int *begin_ = nullptr, size_t size_ = 0, int x_ = 0)
		: begin(begin_), size(size_), x(x_)
                { }

	range_t clone() {
		oldGlData = glData;
		int *newbegin = new int[size];
		glData = newbegin;
		std::copy_n(begin, size, newbegin);
		return range_t(newbegin, size, x);
	}

	void deallocate() {
		delete[] begin;
		glData = oldGlData;
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
		return nchild ? range_t(begin + i, size - i, x + i) : range_t(begin, i, x);
	}
  
	template<typename Archive>
	void save(Archive &ar, const unsigned int) const {
		ar & size & x & static_cast<std::ptrdiff_t>(begin-glData);
		ar & boost::serialization::make_array(begin, size);
	}

	template<typename Archive>
	void load(Archive &ar, const unsigned int) {
		std::ptrdiff_t diffptr;
		ar & size & x & diffptr;
		begin = glData + diffptr;
		ar & boost::serialization::make_array(begin, size);
	}

	BOOST_SERIALIZATION_SPLIT_MEMBER()

};

struct QSinfo: public dpr::Arity<2> {
    
  QSinfo() : dpr::Arity<2>(nthreads, nthreads)
  {}
  
  static bool is_base(const range_t &r) {
    //printf("%lu <= %lu (%d %d %d) = %d\n", r.size, (n / (4 * nthreads * nprocs * tasks_per_thread)), nthreads, nprocs, tasks_per_thread, (r.size <= (n / (4 * nthreads * nprocs * tasks_per_thread))));
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
  
//  void gather_input_post(const range_t& r, int n, range_t& root) {
//	  std::copy_n(r.begin, r.size, root.begin+r.x);
//  }

};

int main(int argc, char **argv) {
	int rank;
	struct timeval t0, t1, t;

	int chunkSize = 1;
	int chunksToSteal = dpr::CHUNKS_TO_STEAL_DEFAULT;
	unsigned int pollingInterval = dpr::POLLING_INTERVAL_DEFAULT;
	int stackSize = 1000;
	int partitioner = 0;
	size_t test_n = 0;
	int threads_request_policy = dpr::THREAD_MPI_STEAL_POLICY_DEFAULT;
	int mpi_workrequest_limits = dpr::MPI_WORKREQUEST_LIMITS_DEFAULT;
	int trp_predict_workcount = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;
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
		chunksToSteal = atoi(argv[3]);
	}

	if (argc > 4) {
		pollingInterval = atoi(argv[4]);
	}

	if (argc > 5) {
		threads_request_policy = atoi(argv[5]);
	}

	if (argc > 6) {
		stackSize = atoi(argv[6]);
	}

	if (argc > 7) {
		partitioner = atoi(argv[7]);
	}

	if (partitioner == 1) {	//_partitioner = 1 => custom
		if (argc > 8) {
			limitParallel = atoi(argv[8]);
		}
	}

	if (argc > 9) {
		mpi_workrequest_limits = atoi(argv[9]);
	}

	if (argc > 10) {
		trp_predict_workcount = atoi(argv[10]);
	}

	if (argc > 11) {
		if (atoi(argv[11]) > 0) {
			test_n = atoi(argv[11]);
		}
	}

	dpr::AutomaticChunkOptions opt = dpr::aco_test_default;

	if (argc > 12) {
		if (chunkSize > 0) {
			runTests = true;
		} else {
			runTests = false;
			opt = dpr::aco_default;
		}
		const int testSize = atoi(argv[12]);
		if (testSize < 0) {
			opt.limitTimeOfEachTest = false;
		} else {
			opt.limitTimeOfEachTest = true;
			opt.testSize = testSize;
		}
	} else {
		opt = dpr::aco_default;
	}

	if (argc > 13) {
		opt.targetTimePerTest = atof(argv[13]);
	}

	if (argc > 14) {
		opt.maxTime = atof(argv[14]);
	}

	if (argc > 15) {
		opt.initTolerance = atoi(argv[15]);
	}

	if (argc > 16) {
		opt.finalTolerance = atoi(argv[16]);
	}

	if (argc > 17) {
		opt.maxNumChunksTestAllowed = atoi(argv[17]);
	}

	if (argc > 18) {
		opt.mode = atoi(argv[18]);
	}

	if (argc > 19) {
		opt.subMode = atoi(argv[19]);
	}

	if (argc > 20) {
		opt.calcMode = atoi(argv[20]);
	}

	if (argc > 21) {
		opt.verbose = atoi(argv[21]);
	} else {
		opt.verbose = 4;
	}
	
	dpr_stack_init(argc, argv, nprocs, rank, nthreads, chunksToSteal, pollingInterval, stackSize, threads_request_policy, mpi_workrequest_limits, trp_predict_workcount);
	
	int *data = new int[n];
	glData = data;
	if (!rank) {	 // The root data is only in the root process
		srand(1234);
		for (size_t i=0; i<n; i++) {
			data[i] = rand();
		}
	}

	range_t tmp(data, n);
	QSinfo qsinfo;
	if (!runTests) {
		if (chunkSize > 0) {
			gettimeofday(&t0, NULL);
			if (partitioner == 1) {
				//_partitioner = 1 => custom
				dpr::dparallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::custom(), dpr::DistributedOutput);
			} else if (partitioner == 2) {
				//_partitioner = 2 => automatic
				dpr::dparallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::automatic(), dpr::DistributedOutput);
			} else {
				//_partitioner = 0 => simple
				dpr::dparallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::simple(), dpr::DistributedOutput);
			}
		} else {
			srand(1234);
			int *test_data = new int[test_n];
			if (!rank) {	 // The root data is only in the root process
				for (size_t i=0; i<test_n; i++) {
					test_data[i] = rand();
				}
			}
			range_t tmp_test(test_data, test_n);
			gettimeofday(&t0, NULL);
			if (partitioner == 1) {
				//_partitioner = 1 => custom
				dpr::dparallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::custom(), dpr::DistributedOutput, dpr::dspar_config_info_default, opt, tmp_test);
			} else if (partitioner == 2) {
				//_partitioner = 2 => automatic
				dpr::dparallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::automatic(), dpr::DistributedOutput, dpr::dspar_config_info_default, opt, tmp_test);
			} else {
				//_partitioner = 0 => simple
				dpr::dparallel_stack_recursion<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::simple(), dpr::DistributedOutput, dpr::dspar_config_info_default, opt, tmp_test);
			}
		}
	} else {
		gettimeofday(&t0, NULL);
		if (partitioner == 1) {
			//_partitioner = 1 => custom
			listChunks = dpr::dparallel_stack_recursion_test<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::custom(), dpr::DistributedOutput, dpr::dspar_config_info_default, opt);
		} else if (partitioner == 2) {
			//_partitioner = 2 => automatic
			listChunks = dpr::dparallel_stack_recursion_test<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::automatic(), dpr::DistributedOutput, dpr::dspar_config_info_default, opt);
		} else {
			//_partitioner = 0 => simple
			listChunks = dpr::dparallel_stack_recursion_test<void> (tmp, qsinfo, QS(), chunkSize, dpr::partitioner::simple(), dpr::DistributedOutput, dpr::dspar_config_info_default, opt);
		}
	}

	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

	if (rank == 0) {
		std::cout << "NProcs=" << nprocs << " Threads=" << nthreads;
		if ((chunkSize > 0) || (runTests)) {
			std::cout << " chunkSize=" << chunkSize;
		} else {
			std::cout << " chunkSize=" << dpr::getDpsrLastRunExtraInfo().chunkSizeUsed << "(auto)";
		}
		std::cout << " stackSize=" << stackSize << " chunksToSteal=" << chunksToSteal << " pollingI=" << pollingInterval << " trp=" << threads_request_policy << " partitioner=";
		if (partitioner == 1) {
			std::cout << "custom limitParallel=" << limitParallel;
		} else if (partitioner == 2) {
			std::cout << "automatic";
		} else {
			std::cout << "simple";
		}
		std::cout << " mpiWrLimits=" << mpi_workrequest_limits;
		if (threads_request_policy == dpr::THREAD_MPI_STEAL_POLICY_PREDICTIVE) {
			std::cout << " trpPredWc=" << trp_predict_workcount;
		}
		std::cout << std::endl;
		if (!runTests) {
			std::cout << "compute time: " << (t.tv_sec + t.tv_usec / 1000000.0) << std::endl;
			if (chunkSize <= 0) {
				std::cout << "  (autochunk test time): " << dpr::getDpsrLastRunExtraInfo().testChunkTime << " (run time): " << dpr::getDpsrLastRunExtraInfo().runTime << std::endl;
			}
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
	}
	MPI_Finalize();

	return 0;
}
