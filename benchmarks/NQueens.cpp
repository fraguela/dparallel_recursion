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
/// \file     NQueens.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///


#include <cstdlib>
#include <dparallel_recursion/dparallel_recursion.h>
#include <sys/time.h>

/*
 * Initial input is replicated from argv.
 * Result is only obtained in rank 0.
 */

using namespace dpr;

int nthreads = 8;
int tasks_per_thread = 1;
int nprocs;

class Board {
public:
	static int N;
	static const int MaxBoard = 20;

	static void setN(int n) {
		if ( n > MaxBoard ) {
			puts("Board too large");
			exit(EXIT_FAILURE);
		}

		N = n;
	}

	///Entry method to play N Queens
	static int play(int boardsize);

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
  
private:
	int row_;       ///< row to fill next
	int nchildren_; ///< num. legal children at row_
	char state_[MaxBoard]; ///<state_[0..row-1] queens already put. state_[row_..row_+nchildren_-1] legal placements of a queen in row row_
};

BOOST_IS_BITWISE_SERIALIZABLE(Board);

/// Info on structure of N Queens problem
struct NQueensInfo : public DInfo<Board, 0> {
	//NQueensInfo() {}

	bool is_base(const Board& b) const {
		return b.row() == (Board::N - 1);
	}

	int num_children(const Board& b) const {
		return b.numChildren();
	}

	bool do_parallel(const Board& b) const {
                int ntasks = nthreads * nprocs * tasks_per_thread;
                int tasks = 1;
                for (int i=0; i < b.row(); i++)
                  tasks *= i ? (Board::N - 1 - 2 * i) : Board::N;
                // printf("[row %d] %d > %d\n", b.row(), ntasks , tasks);
		return !is_base(b) && (ntasks > tasks);
	}

	Board child(int i, const Board& b) const {
		return Board(b, i);
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

int Board::play(int boardsize) {
	setN(boardsize);
	return dparallel_recursion<int>(Board(), NQueensInfo(), NQueensBody(), partitioner::custom(), ReplicatedInput|Balance);
}

int Board::N;

#ifndef NO_VALIDATE
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
#endif

int main(int argc, char** argv) {
	struct timeval t0, t1, t;
	int provided, rank, sols;

	if (getenv("OMP_NUM_THREADS"))
		nthreads = atoi(getenv("OMP_NUM_THREADS"));

	pr_init(nthreads);
	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	assert(provided >= MPI_THREAD_SERIALIZED);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
	if (argc > 2)
		tasks_per_thread = atoi(argv[2]);

        int problem_sz = (argc == 1) ? 15 : atoi(argv[1]);

	gettimeofday(&t0, NULL);
	sols = Board::play(problem_sz);
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

	if(rank == 0) {
                printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
                printf("compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
		printf("solutions: %d\n", sols);
#ifndef NO_VALIDATE
                printf("%s\n", sols == rec_nqueens(Board()) ? "*SUCCESS*" :" FAILURE!");
#endif
	}

	MPI_Finalize();
	return 0;
}
