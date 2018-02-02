/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015-2018 Carlos H. Gonzalez, Basilio B. Fraguela. Universidade da Coruna
 
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
/// \file     test_nqueens.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <iostream>
#include <cstdlib>
#include <cmath>
#include <tbb/task_scheduler_init.h>
#include "dparallel_recursion/dparallel_recursion.h"

using namespace dpr;

int nthreads = 8;
int tasks_per_thread = 1;
int nprocs, rank, problem_size;

using Chrono_t = std::chrono::high_resolution_clock;
Chrono_t::time_point t0;

class Board {
public:
	static int N;
	static const int MaxBoard = 20;

	static void setN(int n) {
		if ( n > MaxBoard ) {
                        std::cerr << "Board too large\n";
			exit(EXIT_FAILURE);
		}

		N = n;
	}

	Board()  : row_(0), nchildren_(N) {
		for (int i = 0; i < N; i++)
			state_[i] = i;
	}

	Board(const Board& other, int nchild)  : row_(other.row_) {
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

        /*
	template<class Archive>
	void serialize(Archive& ar,  unsigned int file_version) {
                //puts("Serializing...");
		ar& row_ & nchildren_ & state_;
	}
        */
  
        float cost() const {
          float base = powf(std::max(1.f, N / 4.f), (N - row_));
          return base + (140 - (abs(state_[row_] - N/2) / (N/2)) * 40);
        }
  
private:
	int row_;       ///< row to fill next
	int nchildren_; ///< num. legal children at row_
	char state_[MaxBoard]; ///<state_[0..row-1] queens already put. state_[row_..row_+nchildren_-1] legal placements of a queen in row row_
};

BOOST_IS_BITWISE_SERIALIZABLE(Board);

/// Info on structure of N Queens problem
struct NQueensInfo : public DInfo<Board, 0> {
	NQueensInfo() {}

	static bool is_base(const Board& b) {
		return b.row() == (Board::N - 1);
	}

	static int num_children(const Board& b) {
		return b.numChildren();
	}

	static bool do_parallel(const Board& b) {
                int ntasks = nthreads * nprocs * tasks_per_thread;
                int tasks = 1;
                for (int i=0; i < b.row(); i++)
                  tasks *= i ? (Board::N - 1 - 2 * i) : Board::N;
                // printf("[row %d] %d > %d\n", b.row(), ntasks , tasks);
		return !is_base(b) && (ntasks > tasks);
	}

	static Board child(int i, const Board& b) {
		return Board(b, i);
	}
  
        static float cost(const Board &r) noexcept {
          return r.cost();
        }

};

struct NQueensBody : public EmptyBody<Board&, int> {
	int base(const Board& b) {
		return b.numChildren();
	}

	int post(const Board& b, int* results) {
		int nc = b.numChildren(); // In this problem it can be 0 sometimes
		int r = 0;

		while (nc) {
			r += results[--nc];
		}

		return r;
	}

};

int Board::N;


int rec_nqueens(Board b) {
  int num_children = b.numChildren();
  if(b.row() == Board::N - 1)
    return num_children;
  else {
    int result = 0;
    for(int i = 0; i < num_children; i++)
      result += rec_nqueens(Board(b, i));
    return result;
  }
}

int test(const int flags, const char * const flag_c) {

  if (rank && (flags & ReplicateInput)) {
    Board::setN(0); //to see that the input replication is effective
  } else {
    Board::setN(problem_size);
  }
  
  t0 = Chrono_t::now();
  
  int sols = dparallel_recursion<int>(Board(), NQueensInfo(), NQueensBody(), partitioner::custom(), flags);
  
  std::chrono::duration<float> time_s = Chrono_t::now() - t0;

  int seq_sols = rec_nqueens(Board());
  
  if(rank == 0) {
    printf("\nProblem size=%d Run with %s (%f s.): \n", problem_size, flag_c, time_s.count());
    printf("par_sols: %d\nseq_sols: %d\n", sols, seq_sols);
  }
  
  //to see that the output replication is effective
  if ( (rank == 0) || (flags & ReplicateOutput) ) {
    int success = (sols == seq_sols);
    
    printf("[%d] %s\n", rank, success ? "*SUCCESS*" : " FAILURE!");
    
    return success ? 0 : -1;
  }
  
  return 0;
}

int main(int argc, char** argv) {
  int provided;
  
  if (getenv("OMP_NUM_THREADS"))
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  
  tbb::task_scheduler_init init(nthreads);
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  assert(provided >= MPI_THREAD_SERIALIZED);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

  problem_size = (argc == 1) ? 12 : atoi(argv[1]);

  if (argc > 2)
    tasks_per_thread = atoi(argv[2]);
  
  if (rank == 0) {
    printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
  }
  
  int ret = test(DefaultBehavior, "DefaultBehavior")
  || test(ReplicatedInput, "ReplicatedInput")
  || test(ReplicatedInput, "ReplicateInput")
  || test(ReplicateOutput, "ReplicateOutput")
  // || test(GreedyParallel, "GreedyParallel")
  || test(Balance, "Balance")
  // || test(Balance|GreedyParallel, "Balance|GreedyParallel")
  || test(Balance|UseCost, "Balance|UseCost")
  // || test(Balance|UseCost|GreedyParallel, "Balance|UseCost|GreedyParallel")
  ;
  
  MPI_Finalize();
  
  return ret;
}
