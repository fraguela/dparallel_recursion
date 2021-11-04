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
/// \file     fib_dstack.cpp
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

/*
 *  Input is replicated (taken from argv).
 *  Result is only obtained in rank 0.
 */

#include <iostream>
#include <cstdlib>
#include <sys/time.h>
#include <vector>
#include <dparallel_recursion/dparallel_stack_recursion.h>

int _n = 25;
int nprocs;
int _nthreads = 8;
int _limitParallel = 7;

struct FibInfo : public dpr::Arity<2> {
  
	FibInfo() : dpr::Arity<2>(_nthreads, _nthreads)
	{}
  
	static bool is_base(const int t) {
		return t < 2;
	}

	static int child(int i, const int c) {
		return c - i - 1;
	}

	static bool do_parallel(const int n) noexcept {
		return n > _limitParallel;
	}
  
        /*static float cost(int i) noexcept {
          return powf(1.61803f, i);
        }*/
};

struct Fib: public dpr::EmptyBody<int, size_t> {
	static size_t base(int n) {
          return n;
	}
	
	static void post(const size_t& r, size_t& rr) {
          rr += r;
	}
};

#ifndef NO_VALIDATE
size_t seq_fib_validate(int n) {
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

int main(int argc, char** argv) {
	int rank;
	size_t r1;
	struct timeval t0, t1, t;
	int chunkSize = 4;
	int chunksToSteal = dpr::CHUNKS_TO_STEAL_DEFAULT;
	unsigned int pollingInterval = dpr::POLLING_INTERVAL_DEFAULT;
	int stackSize = 100;
	int _partitioner = 1;		//0 = simple, 1 = custom, 2 = automatic. Default: custom
	int threads_request_policy = dpr::THREAD_MPI_STEAL_POLICY_DEFAULT;
	int mpi_workrequest_limits = dpr::MPI_WORKREQUEST_LIMITS_DEFAULT;
	int trp_predict_workcount = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;
	int test_n = -1;
	bool runTests = false;
	std::vector<dpr::ResultChunkTest> listChunks;
  
	if (getenv("OMP_NUM_THREADS")) {
		_nthreads = atoi(getenv("OMP_NUM_THREADS"));
	}

	if (argc > 1) {
		_n = atoi(argv[1]);
		test_n = _n;
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
		_partitioner = atoi(argv[7]);
	}

	if (_partitioner == 1) {	//_partitioner = 1 => custom
		if (argc > 8) {
			_limitParallel = atoi(argv[8]);
		}
	}

	if (argc > 9) {
		mpi_workrequest_limits = atoi(argv[9]);
	}

	if (argc > 10) {
		trp_predict_workcount = atoi(argv[10]);
	}

	if (argc > 11) {
		test_n = atoi(argv[11]);
		if (test_n <= 0) {
			test_n = _n;
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
	
	dpr_stack_mpi_init(argc, argv, nprocs, rank);
	dpr::DSParConfigInfo dsparConfigInfo(_nthreads, chunksToSteal, pollingInterval, stackSize, threads_request_policy, mpi_workrequest_limits, trp_predict_workcount);
	gettimeofday(&t0, NULL);
	if (!runTests) {
		if (chunkSize > 0) {
			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				r1 = dpr::dparallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::custom(), dpr::ReplicatedInput, dsparConfigInfo);
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				r1 = dpr::dparallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::automatic(), dpr::ReplicatedInput, dsparConfigInfo);
			} else {
				//_partitioner = 0 => simple
				r1 = dpr::dparallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::simple(), dpr::ReplicatedInput, dsparConfigInfo);
			}
		} else {
			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				r1 = dpr::dparallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::custom(), dpr::ReplicatedInput, dsparConfigInfo, opt, test_n);
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				r1 = dpr::dparallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::automatic(), dpr::ReplicatedInput, dsparConfigInfo, opt, test_n);
			} else {
				//_partitioner = 0 => simple
				r1 = dpr::dparallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::simple(), dpr::ReplicatedInput, dsparConfigInfo, opt, test_n);
			}
		}
	} else {
		if (_partitioner == 1) {
			//_partitioner = 1 => custom
			listChunks = dpr::dparallel_stack_recursion_test<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::custom(), dpr::ReplicatedInput, dsparConfigInfo, opt);
		} else if (_partitioner == 2) {
			//_partitioner = 2 => automatic
			listChunks = dpr::dparallel_stack_recursion_test<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::automatic(), dpr::ReplicatedInput, dsparConfigInfo, opt);
		} else {
			//_partitioner = 0 => simple
			listChunks = dpr::dparallel_stack_recursion_test<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::simple(), dpr::ReplicatedInput, dsparConfigInfo, opt);
		}
	}
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

	if (rank == 0) {
		std::cout << "NProcs=" << nprocs << " Threads=" << _nthreads;
		if ((chunkSize > 0) || (runTests)) {
			std::cout << " chunkSize=" << chunkSize;
		} else {
			std::cout << " chunkSize=" << dpr::getDpsrLastRunExtraInfo().chunkSizeUsed << "(auto)";
		}
		std::cout << " stackSize=" << stackSize << " chunksToSteal=" << chunksToSteal << " pollingI=" << pollingInterval << " trp=" << threads_request_policy << " partitioner=";
		if (_partitioner == 1) {
			std::cout << "custom limitParallel=" << _limitParallel;
		} else if (_partitioner == 2) {
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
			std::cout << "fib(" << _n << "): " << r1 << std::endl;
#ifndef NO_VALIDATE
		gettimeofday(&t0, NULL);
		size_t sfib = seq_fib_validate(_n);
		gettimeofday(&t1, NULL);
		timersub(&t1, &t0, &t);
		std::cout << ((r1 == sfib) ? "*SUCCESS*" : " FAILURE!") << " -> seq_time = " << (t.tv_sec + t.tv_usec / 1000000.0) << std::endl;
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
	}
	
	MPI_Finalize();

	return 0;
}
