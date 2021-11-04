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
/// \file     NQueens_dstack.cpp
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///


#include <iostream>
#include <sys/time.h>
#include <cstdlib>
#include <vector>
#include <dparallel_recursion/dparallel_stack_recursion.h>

int nprocs;
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

	template<class Archive>
	void serialize(Archive & ar, const unsigned int version) {
		ar & N;
		ar & row_;
		ar & nchildren_;
		ar & state_;
		//ar & MaxBoard;

	}
  
private:
	int row_;       ///< row to fill next
	int nchildren_; ///< num. legal children at row_
	char state_[MaxBoard]; ///<state_[0..row-1] queens already put. state_[row_..row_+nchildren_-1] legal placements of a queen in row row_
};

BOOST_IS_BITWISE_SERIALIZABLE(Board);

/// Info on structure of N Queens problem
struct NQueensInfo : public dpr::Arity<0> {
	NQueensInfo() : Arity<0>(nthreads, nthreads)
	{}

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
		//std::cout << "[row " << b.row() << "] " << ntasks << " > " << tasks << std::endl;
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
	
	void post(const int& r, int& rr) {
		rr += r;
	}

};

int Board::play(int boardsize, int chunkSize, int partitioner, const dpr::AutomaticChunkOptions &opt = dpr::aco_default, int test_boardsize = -1) {
	//setN(boardsize);
	if ((chunkSize > 0) || (test_boardsize <= 0)) {
		if (partitioner == 1) {
			//_partitioner = 1 => custom
			return dpr::dparallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::custom(), dpr::ReplicatedInput);
		} else if (partitioner == 2) {
			//_partitioner = 2 => automatic
			return dpr::dparallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::automatic(), dpr::ReplicatedInput);
		} else {
			//_partitioner = 0 => simple
			return dpr::dparallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::simple(), dpr::ReplicatedInput);
		}
	} else {
		if (partitioner == 1) {
			//_partitioner = 1 => custom
			return dpr::dparallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::custom(), dpr::ReplicatedInput, dpr::dspar_config_info_default, opt, Board(test_boardsize));
		} else if (partitioner == 2) {
			//_partitioner = 2 => automatic
			return dpr::dparallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::automatic(), dpr::ReplicatedInput, dpr::dspar_config_info_default, opt, Board(test_boardsize));
		} else {
			//_partitioner = 0 => simple
			return dpr::dparallel_stack_recursion<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::simple(), dpr::ReplicatedInput, dpr::dspar_config_info_default, opt, Board(test_boardsize));
		}
	}
}

std::vector<dpr::ResultChunkTest> Board::test(int boardsize, int chunkSize, int partitioner, const dpr::AutomaticChunkOptions &opt) {
	//setN(boardsize);
	std::vector<dpr::ResultChunkTest> listChunks;
	if (partitioner == 1) {
		//_partitioner = 1 => custom
		listChunks = dpr::dparallel_stack_recursion_test<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::custom(), dpr::ReplicatedInput, dpr::dspar_config_info_default, opt);
	} else if (partitioner == 2) {
		//_partitioner = 2 => automatic
		listChunks = dpr::dparallel_stack_recursion_test<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::automatic(), dpr::ReplicatedInput, dpr::dspar_config_info_default, opt);
	} else {
		//_partitioner = 0 => simple
		listChunks = dpr::dparallel_stack_recursion_test<int>(Board(boardsize), NQueensInfo(), NQueensBody(), chunkSize, dpr::partitioner::simple(), dpr::ReplicatedInput, dpr::dspar_config_info_default, opt);
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
	int rank;
	struct timeval t0, t1, t;
	int sols = -1;
	int problem_sz = 15;
	int chunkSize = 8;	//10 is a great chunk size also
	int chunksToSteal = dpr::CHUNKS_TO_STEAL_DEFAULT;
	unsigned int pollingInterval = dpr::POLLING_INTERVAL_DEFAULT;
	int stackSize = 100;
	int partitioner = 0;
	int threads_request_policy = dpr::THREAD_MPI_STEAL_POLICY_DEFAULT;
	int mpi_workrequest_limits = dpr::MPI_WORKREQUEST_LIMITS_DEFAULT;
	int trp_predict_workcount = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;
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
		test_boardsize = atoi(argv[11]);
		if (test_boardsize <= 0) {
			test_boardsize = problem_sz;
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

	gettimeofday(&t0, NULL);
	if (!runTests) {
		sols = Board::play(problem_sz, chunkSize, partitioner, opt, test_boardsize);
	} else {
		listChunks = Board::test(problem_sz, chunkSize, partitioner, opt);
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
	}
	
	MPI_Finalize();

	return 0;
}
