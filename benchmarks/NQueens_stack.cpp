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
/// \file     NQueens_stack.cpp
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///


#include <cstdlib>
#include <vector>
#include <utility>
#include <sys/time.h>
#include <iostream>
#include "dparallel_recursion/parallel_stack_recursion.h"

int nthreads = 8;
int limitParallel = 768;

class Board {
public:
	static const int MaxBoard = 20;
	int N;

//	static void setN(int n) {
//		if ( n > MaxBoard ) {
//			puts("Board too large");
//			exit(EXIT_FAILURE);
//		}
//
//		N = n;
//	}

	///Entry method to play N Queens
	static int play(int boardsize, int chunkSize, int partitioner, const dpr::AutomaticChunkOptions &opt, int test_boardsize);

	static std::vector<dpr::ResultChunkTest> test(int boardsize, int chunkSize, int partitioner, const dpr::AutomaticChunkOptions &opt);

	Board() { }

	Board(int n)  : N(n), row_(0), nchildren_(N) {
		for (int i = 0; i < N; i++)
			state_[i] = i;
	}

	Board(const Board& other, int nchild)  : N(other.N), row_(other.row_) {
		for (int i = 0; i < row_; i++)
			state_[i] = other.state_[i];

		state_[row_] = other.state_[row_ + nchild];


		while ( (row_ == other.row_) || (nchildren_ == 1 && row_ < N - 1) ) {
			row_++;
			nchildren_ = 0;

			for (int i = 0; i < N; i++) {
				int j;

				for (j = 0; j < row_; j++)
					if ( (state_[j] == i) || ((state_[j] + (row_ - j)) == i) || ((state_[j] - (row_ - j)) == i) )
						break;

				if (j == row_) {
					state_[row_ + nchildren_] = i;
					nchildren_++;
				}
			}
		}

	}

	int numChildren() const {
		return nchildren_;
	}

	int row() const {
		return row_;
	}
  
private:
	int row_;       ///< row to fill next
	int nchildren_; ///< num. legal children at row_
	char state_[MaxBoard]; ///<state_[0..row-1] queens already put. state_[row_..row_+nchildren_-1] legal placements of a queen in row row_
};

/// Info on structure of N Queens problem
struct NQueensInfo : public dpr::Arity<0> {
	//NQueensInfo() {}

	bool is_base(const Board& b) const {
		return b.row() == (b.N - 1);
	}

	int num_children(const Board& b) const {
		return b.numChildren();
	}

	bool do_parallel(const Board& b) const {
		int ntasks = nthreads * limitParallel;
		int tasks = 1;
		for (int i=0; i < b.row(); i++) {
			tasks *= i ? (b.N - 1 - 2 * i) : b.N;
		}
		//printf("[row %d] %d > %d\n", b.row(), ntasks , tasks);
		return !is_base(b) && (ntasks > tasks);
	}

	Board child(int i, const Board& b) const {
		return Board(b, i);
	}

};

struct NQueensBody : public dpr::EmptyBody<Board, int> {
	int base(const Board& b) {
		return b.numChildren();
	}

	/*int post(const Board& b, int* results) {
		int nc = b.numChildren(); // In this problem it can be 0 sometimes
		int r = 0;

		while (nc) {
			r += results[--nc];
		}

		return r;
	}*/
	
	void post(const int& r, int& rr) {
		rr += r;
	}

};

int Board::play(int boardsize, int chunkSize, int partitioner, const dpr::AutomaticChunkOptions &opt = dpr::aco_default, int test_boardsize = -1) {
	//setN(boardsize);
	if ((chunkSize > 0) or (test_boardsize > 0)) {
		if (partitioner == 1) {
			//_partitioner = 1 => custom
			return dpr::parallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::custom());
		} else if (partitioner == 2) {
			//_partitioner = 2 => automatic
			return dpr::parallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::automatic());
		} else {
			//_partitioner = 0 => simple
			return dpr::parallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::simple());
		}
	} else {
		if (partitioner == 1) {
			//_partitioner = 1 => custom
			return dpr::parallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::custom(), opt, Board(test_boardsize));
		} else if (partitioner == 2) {
			//_partitioner = 2 => automatic
			return dpr::parallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::automatic(), opt, Board(test_boardsize));
		} else {
			//_partitioner = 0 => simple
			return dpr::parallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::simple(), opt, Board(test_boardsize));
		}
	}
}

std::vector<dpr::ResultChunkTest> Board::test(int boardsize, int chunkSize, int partitioner, const dpr::AutomaticChunkOptions &opt = dpr::aco_default) {
	//setN(boardsize);
	std::vector<dpr::ResultChunkTest> listChunks;
	if (partitioner == 1) {
		//_partitioner = 1 => custom
		listChunks = dpr::parallel_stack_recursion_test<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::custom(), opt);
	} else if (partitioner == 2) {
		//_partitioner = 2 => automatic
		listChunks = dpr::parallel_stack_recursion_test<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::automatic(), opt);
	} else {
		//_partitioner = 0 => simple
		listChunks = dpr::parallel_stack_recursion_test<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::simple(), opt);
	}
	return listChunks;
}

#ifndef NO_VALIDATE
int rec_nqueens(Board b) {
  int num_children = b.numChildren();
  if(b.row() == b.N - 1)
    return num_children;
  else {
    int result = 0;
    for(int i = 0; i < num_children; i++)
      result += rec_nqueens(Board(b, i));
    return result;
  }
}
#endif

int main(int argc, char** argv) {
	struct timeval t0, t1, t;
	int sols;
	int problem_sz = 15;
	int chunkSize = 8;	//10 is a great chunk size also
	int stackSize = 500000;
	int partitioner = 0;
	int test_boardsize = -1;
	bool runTests = false;
	std::vector<dpr::ResultChunkTest> listChunks;

	if (getenv("OMP_NUM_THREADS")) {
		nthreads = atoi(getenv("OMP_NUM_THREADS"));
	}
  
	if (argc > 1) {
		problem_sz = atoi(argv[1]);
		test_boardsize = problem_sz;
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
			test_boardsize = atoi(argv[6]);
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

	gettimeofday(&t0, NULL);
	if (!runTests) {
		sols = Board::play(problem_sz, chunkSize, partitioner, opt, test_boardsize);
	} else {
		listChunks = Board::test(problem_sz, chunkSize, partitioner, opt);
	}
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);
	
	std::cout << "Threads=" << nthreads;
	if ((chunkSize > 0) || (runTests)) {
		std::cout << " chunkSize=" << chunkSize;
	} else {
		std::cout << " chunkSize=" << dpr::getPsrLastRunExtraInfo().chunkSizeUsed << "(auto)";
	}
	std::cout << " stackSize=" << stackSize << " partitioner=";
	if (partitioner == 1) {
		std::cout << "custom limitParallel=" << limitParallel << std::endl;
	} else if (partitioner == 2) {
		std::cout << "automatic" << std::endl;
	} else {
		std::cout << "simple" << std::endl;
	}
	if (!runTests) {
		std::cout << "compute time: " << (t.tv_sec + t.tv_usec / 1000000.0) << std::endl;
		if (chunkSize <= 0) {
			std::cout << "  (autochunk test time): " << dpr::getPsrLastRunExtraInfo().testChunkTime << " (run time): " << dpr::getPsrLastRunExtraInfo().runTime << std::endl;
		}
		std::cout << "solutions: " << sols << std::endl;
#ifndef NO_VALIDATE
		std::cout << (sols == rec_nqueens(Board(problem_sz)) ? "*SUCCESS*" :" FAILURE!") << std::endl;
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
