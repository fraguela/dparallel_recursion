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

#include <vector>
#include <cmath>
#include <cstdlib>
#include <sys/time.h>
#include <numeric>
#include <mpi.h>
#include <cilk/cilk_api.h>

/*
 * Initial input is replicated from argv.
 * Result is only obtained in rank 0.
 */

int tasks_per_thread = 1;
int nthreads, nwanted_tasks, probl_per_node;

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

int Board::N;

int rec_nqueens(const Board& b) {
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

int rec_nqueens_up(const Board& b) {
  int num_children = b.numChildren();
  if(b.row() == Board::N - 1)
    return num_children;
  else {
    int tasks = 1;
    for(int i = 0; i < b.row(); i++)
      tasks *= i ? (Board::N - 1 - 2 * i) : Board::N;
    if (tasks < nwanted_tasks) {
      int tmp[num_children];
      _Cilk_for(int i = 0; i < num_children; i++) {
        tmp[i] = rec_nqueens_up(Board(b, i));
      }
      return std::accumulate(tmp, tmp + num_children, 0);
    } else {
      return rec_nqueens(b);
      //for(int i = 0; i < num_children; i++)
      //  result += rec_nqueens(Board(b, i));
    }
  }
}

int mpi_nqueens(int rank, int nprocs) {
        std::vector<Board> boards;
        float inbalance = 0.f;
	int head_result = 0;
	int parent_start = 0, level_start = 1, nglobal_elems_ = 1;
	Board b;
	boards.push_back(b);
	bool new_boards = true;
  
	// Builds a tree of nqueens boards, flattened into an array
        while(new_boards && ((nglobal_elems_ < nprocs) ||
                             ((inbalance > 0.2f) &&
                              (nglobal_elems_ < (50 * nprocs))))) {
		int i, j = level_start;
		new_boards = false;
		for(i = parent_start; i < level_start; i++) {
			int number_of_children_boards = boards[i].numChildren();
			if(boards[i].row() == Board::N - 1)
				head_result += number_of_children_boards;
			for(int k = 0; k < number_of_children_boards; k++) {
				new_boards = true;
				boards.push_back(Board(boards[i], k));
			}
			j += number_of_children_boards;
		}
		if(new_boards) {
			parent_start = level_start;
			level_start = j;
		}
                nglobal_elems_ = level_start - parent_start;
                inbalance = nglobal_elems_ / (float)nprocs;
                inbalance = (ceilf(inbalance) - floorf(inbalance)) / floorf(inbalance);
	}
	// the first half of the tree has the already computed boards
	// the last level contains the boards for which more processing can be done
        /*
	#if 0
	for(int i = parent_start; i < level_start; i++) {
		result += rec_nqueens(boards[i]);
	}
	#endif
        */
  
	int local_result = 0, reduced_result;
  //printf("[%d] found %d boards on %d procs\n", rank, level_start - parent_start, nprocs);
        int block_size = (nglobal_elems_ + (nprocs -1))/nprocs; //+(nprocs-1) => roundup
        int start = parent_start + block_size * rank;
        int end = std::min(start + block_size, level_start);
  
        nwanted_tasks = nthreads * tasks_per_thread;

  //printf("[%d] will reduce %d boards (head_result=%d) (row=%d)\n", rank, end-start, head_result, (end-start) ? boards[start].row() : -1);
  
        probl_per_node = end - start;

        if (probl_per_node > 0) { //Can be even negative
          int tmp[probl_per_node];
          
          const int step = (nwanted_tasks >= probl_per_node) ? 1 : (probl_per_node / nwanted_tasks);
          
          nwanted_tasks *= nprocs;


          _Cilk_for(int i = start; i < end; i += step)
            for (int j = i; j < std::min(i + step, end); j++) {
              tmp[j-start] = rec_nqueens_up(boards[j]);
            }
          
          local_result = std::accumulate(tmp, tmp + probl_per_node, 0);
        }

	MPI_Reduce(&local_result, &reduced_result, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
	return head_result + reduced_result;
}

int main(int argc, char** argv) {
	struct timeval t0, t1, t;
        int rank, nprocs, sols;

	MPI_Init(&argc, &argv);
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

        nthreads = __cilkrts_get_nworkers();

        if (argc > 2)
          tasks_per_thread = atoi(argv[2]);
  
        int problem_sz = (argc == 1) ? 15 : atoi(argv[1]);

	gettimeofday(&t0, NULL);
	Board::setN(problem_sz);
	sols = mpi_nqueens(rank, nprocs);
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
