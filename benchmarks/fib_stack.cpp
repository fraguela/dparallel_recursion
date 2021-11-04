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
/// \file     fib_stack.cpp
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#include <iostream>
#include <cstdlib>
#include <sys/time.h>
#include <vector>
#include <dparallel_recursion/parallel_stack_recursion.h>

int _n = 25;
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
	size_t r1;
	struct timeval t0, t1, t;
	int chunkSize = 4;
	int stackSize = 100;
	int _partitioner = 1;		//0 = simple, 1 = custom, 2 = automatic. Default: custom
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
		stackSize = atoi(argv[3]);
	}
	
	if (argc > 4) {
		_partitioner = atoi(argv[4]);
	}

	if (_partitioner == 1) {	//_partitioner = 1 => custom
		if (argc > 5) {
			_limitParallel = atoi(argv[5]);
		}
	}

	if (argc > 6) {
		test_n = atoi(argv[6]);
		if (test_n <= 0) {
			test_n = _n;
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
	
	dpr::prs_init(_nthreads, stackSize);
	gettimeofday(&t0, NULL);
	if (!runTests) {
		if (chunkSize > 0) {
			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				r1 = dpr::parallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::custom());
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				r1 = dpr::parallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::automatic());
			} else {
				//_partitioner = 0 => simple
				r1 = dpr::parallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::simple());
			}
		} else {
			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				r1 = dpr::parallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::custom(), opt, test_n);
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				r1 = dpr::parallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::automatic(), opt, test_n);
			} else {
				//_partitioner = 0 => simple
				r1 = dpr::parallel_stack_recursion<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::simple(), opt, test_n);
			}
		}
	} else {
		if (_partitioner == 1) {
			//_partitioner = 1 => custom
			listChunks = dpr::parallel_stack_recursion_test<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::custom(), opt);
		} else if (_partitioner == 2) {
			//_partitioner = 2 => automatic
			listChunks = dpr::parallel_stack_recursion_test<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::automatic(), opt);
		} else {
			//_partitioner = 0 => simple
			listChunks = dpr::parallel_stack_recursion_test<size_t> (_n, FibInfo(), Fib(), chunkSize, dpr::partitioner::simple(), opt);
		}
	}
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

	std::cout << "Threads=" << _nthreads;
	if ((chunkSize > 0) || (runTests)) {
		std::cout << " chunkSize=" << chunkSize;
	} else {
		std::cout << " chunkSize=" << dpr::getPsrLastRunExtraInfo().chunkSizeUsed << "(auto)";
	}
	std::cout << " stackSize=" << stackSize << " partitioner=";
	if (_partitioner == 1) {
		std::cout << "custom limitParallel=" << _limitParallel << std::endl;
	} else if (_partitioner == 2) {
		std::cout << "automatic" << std::endl;
	} else {
		std::cout << "simple" << std::endl;
	}
	if (!runTests) {
		std::cout << "compute time: " << (t.tv_sec + t.tv_usec / 1000000.0) << std::endl;
		if (chunkSize <= 0) {
			std::cout << "  (autochunk test time): " << dpr::getPsrLastRunExtraInfo().testChunkTime << " (run time): " << dpr::getPsrLastRunExtraInfo().runTime << std::endl;
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
	
	return 0;
}
