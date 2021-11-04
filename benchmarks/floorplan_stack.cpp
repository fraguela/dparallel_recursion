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
/// \file     floorplan_stack.cpp
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#include <iostream>
#include <sys/time.h>
#include <algorithm>
#include <string>
#include <mutex>
#include <vector>
#include <dparallel_recursion/parallel_stack_recursion.h>

#define ROWS 64
#define COLS 64
#define DMAX 64

#define MAX_NN 8
#define MAX_INPUT_SIZE 20

int solution = -1;
int t_solution = -1;

typedef int  coor[2];

struct cell {
  int   n;
  coor *alt;
  int   top;
  int   bot;
  int   lhs;
  int   rhs;
  int   left;
  int   above;
  int   next;
};

std::array<cell, MAX_INPUT_SIZE+1> gcells;
std::array<cell, MAX_INPUT_SIZE+1> t_gcells;

std::mutex lockvar;

/* compute all possible locations for nw corner for cell */
static int starts(const int id, const int shape, std::array<std::array<int, 2>, DMAX> * NWS, const std::array<cell, MAX_INPUT_SIZE+1>& cells) { //const cell * const cells) {
	int i, n, top, bot, lhs, rhs;
	int rows, cols, left, above;

	/* size of cell */
	rows  = cells[id].alt[shape][0];
	cols  = cells[id].alt[shape][1];

	/* the cells to the left and above */
	left  = cells[id].left;
	above = cells[id].above;

	/* if there is a vertical and horizontal dependence */
	if ((left >= 0) && (above >= 0)) {

		top = cells[above].bot + 1;
		lhs = cells[left].rhs + 1;
		bot = top + rows;
		rhs = lhs + cols;

		/* if footprint of cell touches the cells to the left and above */
		if ((top <= cells[left].bot) && (bot >= cells[left].top) &&
				(lhs <= cells[above].rhs) && (rhs >= cells[above].lhs)) {
			n = 1;
			NWS[shape][0][0] = top;
			NWS[shape][0][1] = lhs;
		} else {
			n = 0;
		}

	/* if there is only a horizontal dependence */
	} else if (left >= 0) {

		/* highest initial row is top of cell to the left - rows */
		top = std::max(cells[left].top - rows + 1, 0);
		/* lowest initial row is bottom of cell to the left */
		bot = std::min(cells[left].bot, ROWS);
		n   = bot - top + 1;

		for (i = 0; i < n; i++) {
			NWS[shape][i][0] = i + top;
			NWS[shape][i][1] = cells[left].rhs + 1;
		}

	} else {

		/* leftmost initial col is lhs of cell above - cols */
		lhs = std::max(cells[above].lhs - cols + 1, 0);
		/* rightmost initial col is rhs of cell above */
		rhs = std::min(cells[above].rhs, COLS);
		n   = rhs - lhs + 1;

		for (i = 0; i < n; i++) {
			NWS[shape][i][0] = cells[above].bot + 1;
			NWS[shape][i][1] = i + lhs;
		}
	}

	return (n);
}


/* lay the cell down on the board in the rectangular space defined
   by the cells top, bottom, left, and right edges. If the cell can
   not be layed down, return false; else true.
*/
static bool lay_down(const int id, std::array<char, ROWS*COLS>& board, const cell * const cells) {
	int  i, j, top, bot, lhs, rhs;

	top = cells[id].top;
	bot = cells[id].bot;
	lhs = cells[id].lhs;
	rhs = cells[id].rhs;

	for (i = top; i <= bot; i++) {
		for (j = lhs; j <= rhs; j++) {
			if (board[i*ROWS+j] == 0) {
				board[i*ROWS+j] = (char)id;
			} else {
				return false;
			}
		}
	}

	return true;
}

#define read_integer(file,var) \
	if ( fscanf(file, "%d", &var) == EOF ) {\
		std::cout << " Bogus input file" << std::endl;\
		exit(-1);\
	}

static int read_inputs(FILE * inputFile, std::array<cell, MAX_INPUT_SIZE+1>& cells, int& sol) {
	int i, j, n;

	read_integer(inputFile, n);

	if (n > MAX_INPUT_SIZE) {
		throw std::runtime_error("The input exceeds the maximum allowed input size");
	}

	cells[0].n     =  0;
	cells[0].alt   =  0;
	cells[0].top   =  0;
	cells[0].bot   =  0;
	cells[0].lhs   = -1;
	cells[0].rhs   = -1;
	cells[0].left  =  0;
	cells[0].above =  0;
	cells[0].next  =  0;

	for (i = 1; i < n + 1; i++) {

		read_integer(inputFile, cells[i].n);
		cells[i].alt = new coor[cells[i].n];

		for (j = 0; j < cells[i].n; j++) {
			read_integer(inputFile, cells[i].alt[j][0]);
			read_integer(inputFile, cells[i].alt[j][1]);
		}

		read_integer(inputFile, cells[i].left);
		read_integer(inputFile, cells[i].above);
		read_integer(inputFile, cells[i].next);
	}

	if (!feof(inputFile)) {
		read_integer(inputFile, sol);
	}
	return n;
}

int MIN_AREA = ROWS*COLS;
std::array<int, 2> MIN_FOOTPRINT;
std::array<char, ROWS*COLS> BEST_BOARD;

static void write_outputs() {
	int i, j;

	std::cout << "Minimum area = " << MIN_AREA << std::endl << std::endl;

	for (i = 0; i < MIN_FOOTPRINT[0]; i++) {
		for (j = 0; j < MIN_FOOTPRINT[1]; j++) {
			if (BEST_BOARD[i*ROWS+j] == 0) {
				std::cout << " ";
			} else {
				std::cout << char('A' + BEST_BOARD[i*ROWS+j] - 1);
			}
		}
		std::cout << std::endl;
	}
}

struct Shape {
	static int N;

	int id;
	int prevId;
	int area;
	int level;
	std::array<int, 2> FOOTPRINT;
	int nn[MAX_NN];

	std::array<std::array<int, 2>, DMAX> NWS[MAX_NN];

	alignas(64) std::array<char, ROWS*COLS> BOARD;
	alignas(64) std::array<cell, MAX_INPUT_SIZE+1> CELLS;

	Shape()
	{ }

	void deallocate() {
		MIN_AREA = ROWS*COLS;
	}

};
int Shape::N;

std::string _filename;
int _nthreads = 8;
int _limitParallel = 5;

/* compute all possible locations for nw corner for cell */
static int starts_seq(const int id, const int shape, coor * const NWS, const cell * const cells) {
	int i, n, top, bot, lhs, rhs;
	int rows, cols, left, above;

	/* size of cell */
	rows  = cells[id].alt[shape][0];
	cols  = cells[id].alt[shape][1];

	/* the cells to the left and above */
	left  = cells[id].left;
	above = cells[id].above;

	/* if there is a vertical and horizontal dependence */
	if ((left >= 0) && (above >= 0)) {

		top = cells[above].bot + 1;
		lhs = cells[left].rhs + 1;
		bot = top + rows;
		rhs = lhs + cols;

		/* if footprint of cell touches the cells to the left and above */
		if ((top <= cells[left].bot) && (bot >= cells[left].top) &&
				(lhs <= cells[above].rhs) && (rhs >= cells[above].lhs)) {
			n = 1;
			NWS[0][0] = top;
			NWS[0][1] = lhs;
		} else {
			n = 0;
		}

	/* if there is only a horizontal dependence */
	} else if (left >= 0) {

		/* highest initial row is top of cell to the left - rows */
		top = std::max(cells[left].top - rows + 1, 0);
		/* lowest initial row is bottom of cell to the left */
		bot = std::min(cells[left].bot, ROWS);
		n   = bot - top + 1;

		for (i = 0; i < n; i++) {
			NWS[i][0] = i + top;
			NWS[i][1] = cells[left].rhs + 1;
		}

	} else {

		/* leftmost initial col is lhs of cell above - cols */
		lhs = std::max(cells[above].lhs - cols + 1, 0);
		/* rightmost initial col is rhs of cell above */
		rhs = std::min(cells[above].rhs, COLS);
		n   = rhs - lhs + 1;

		for (i = 0; i < n; i++) {
			NWS[i][0] = cells[above].bot + 1;
			NWS[i][1] = i + lhs;
		}
	}

	return (n);
}

static void add_cell_ser(const int id, const std::array<int, 2>& FOOTPRINT, const std::array<char, ROWS*COLS>& BOARD, cell * const CELLS) {
	coor NWS[DMAX];

	/* for each possible shape */
	for (int i = 0; i < CELLS[id].n; i++) {
		/* compute all possible locations for nw corner */
		int nn = starts_seq(id, i, NWS, CELLS);
		/* for all possible locations */
		for (int j = 0; j < nn; j++) {
			int area;
			std::array<char, ROWS*COLS> board;
			std::array<int, 2> footprint;
			cell *cells = CELLS;
			/* extent of shape */
			cells[id].top = NWS[j][0];
			cells[id].bot = cells[id].top + cells[id].alt[i][0] - 1;
			cells[id].lhs = NWS[j][1];
			cells[id].rhs = cells[id].lhs + cells[id].alt[i][1] - 1;

			std::copy(BOARD.begin(), BOARD.end(), board.begin());

			/* if the cell cannot be layed down, prune search */
			if (lay_down(id, board, cells)) {

				/* calculate new footprint of board and area of footprint */
				footprint[0] = std::max(FOOTPRINT[0], cells[id].bot+1);
				footprint[1] = std::max(FOOTPRINT[1], cells[id].rhs+1);
				area         = footprint[0] * footprint[1];

				/* if last cell */
				if (cells[id].next == 0) {

					/* if area is minimum, update global values */
					if (area < MIN_AREA) {
						std::lock_guard<std::mutex> lck(lockvar);
						if (area < MIN_AREA) {
							MIN_AREA         = area;
							MIN_FOOTPRINT[0] = footprint[0];
							MIN_FOOTPRINT[1] = footprint[1];
							std::copy(board.begin(), board.end(), BEST_BOARD.begin());
						}
					}
				/* if area is less than best area */
				} else if (area < MIN_AREA) {
					add_cell_ser(cells[id].next, footprint, board, cells);
				/* if area is greater than or equal to best area, prune search */
				}
			}
		}
	}
}


struct FloorplanBody: public dpr::EmptyBody<Shape, void> {

	static void base(const Shape& sh) {
		if (sh.area < MIN_AREA) {
			std::lock_guard<std::mutex> lck(lockvar);
			if (sh.area < MIN_AREA) {
				MIN_AREA         = sh.area;
				MIN_FOOTPRINT[0] = sh.FOOTPRINT[0];
				MIN_FOOTPRINT[1] = sh.FOOTPRINT[1];
				std::copy(sh.BOARD.begin(), sh.BOARD.end(), BEST_BOARD.begin());
			}
		}
	}

};

struct FloorplanInfo : public dpr::Arity<dpr::UNKNOWN> {

	FloorplanInfo() : dpr::Arity<dpr::UNKNOWN>(_nthreads, _nthreads)
	{}

	static bool is_base(const Shape& sh) {
		return ((sh.area > -1) && (sh.CELLS[sh.prevId].next == 0));
	}

	static int num_children(Shape& sh) {
		if ((sh.area == -99) || (sh.area >= MIN_AREA)) {
			return 0;
		} else {
			int nchild = 0;
			for (int i = 0; i < sh.CELLS[sh.id].n; i++) {
				sh.nn[i] = starts(sh.id, i, sh.NWS, sh.CELLS);
				nchild += sh.nn[i];
			}
			return nchild;
		}
	}

	static Shape child(int j, const Shape& sh) {
		int indexI = 0;
		int indexJ = 0;
		int counter = 0;
		int nI = 0;
		for (int i = 0; i < sh.CELLS[sh.id].n; ++i) {
			int nJ = 0;
			for (int k = 0; k < sh.nn[i]; ++k) {
				if (counter == j) {
					indexI = nI;
					indexJ = nJ;
					goto _end_forindex;
				}
				counter++;
				nJ++;
			}
			nI++;
		}
_end_forindex:;

		Shape ret;
		ret.id = sh.CELLS[sh.id].next;
		ret.level = sh.level + 1;
		ret.prevId = sh.id;

		std::copy(sh.CELLS.begin(), sh.CELLS.end(), ret.CELLS.begin());

		/* extent of shape */
		ret.CELLS[sh.id].top = sh.NWS[indexI][indexJ][0];
		ret.CELLS[sh.id].bot = ret.CELLS[sh.id].top + ret.CELLS[sh.id].alt[indexI][0] - 1;
		ret.CELLS[sh.id].lhs = sh.NWS[indexI][indexJ][1];
		ret.CELLS[sh.id].rhs = ret.CELLS[sh.id].lhs + ret.CELLS[sh.id].alt[indexI][1] - 1;

		std::copy(sh.BOARD.begin(), sh.BOARD.end(), ret.BOARD.begin());

		/* if the cell cannot be layed down, prune search */
		if (lay_down(sh.id, ret.BOARD, ret.CELLS.data())) {
			/* calculate new footprint of board and area of footprint */
			ret.FOOTPRINT[0] = std::max(sh.FOOTPRINT[0], ret.CELLS[sh.id].bot+1);
			ret.FOOTPRINT[1] = std::max(sh.FOOTPRINT[1], ret.CELLS[sh.id].rhs+1);
			ret.area = ret.FOOTPRINT[0] * ret.FOOTPRINT[1];
//			if (!do_parallel(sh) && (ret.id != 0)) {
//				add_cell_ser(ret.id, ret.FOOTPRINT, ret.BOARD, ret.CELLS.data());
//				ret.area = -99;
//			}
		} else {
			ret.area = -99;
		}
		return ret;
	}

	static bool do_parallel(const Shape& sh) noexcept {
		return sh.level < _limitParallel;
	}

	static void do_serial_func(Shape& ret) noexcept {
		add_cell_ser(ret.id, ret.FOOTPRINT, ret.BOARD, ret.CELLS.data());
	}

};


int main(int argc, char** argv) {
	struct timeval t0, t1, t;
	int chunkSize = 9;
	int stackSize = 1000;
	int _partitioner = 1;		//0 = simple, 1 = custom, 2 = automatic. Default: simple
	std::string test_filename;
	bool runTests = false;
	std::vector<dpr::ResultChunkTest> listChunks;
  
	if (getenv("OMP_NUM_THREADS"))
		_nthreads = atoi(getenv("OMP_NUM_THREADS"));

	if (argc > 1) {
		_filename = argv[1];
		test_filename = _filename;
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
		test_filename = argv[6];
		if (test_filename == "0" or test_filename == "") {
			test_filename = _filename;
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

	std::array<char, ROWS*COLS> board;

	FILE * inputFile;
	inputFile = fopen(_filename.c_str(), "r");

	if (NULL == inputFile) {
		std::cout << "Couldn't open " << _filename << " file for reading" << std::endl;
		std::exit(1);
	}

	/* read input file and initialize global minimum area */
	Shape::N = read_inputs(inputFile, gcells, solution);

	/* initialize board is empty */
	for (int i = 0; i < ROWS; i++)
	for (int j = 0; j < COLS; j++) board[i*ROWS+j] = 0;
	fclose(inputFile);

	//MIN_AREA.store(ROWS*COLS, std::memory_order_relaxed);
	MIN_AREA = ROWS*COLS;

	std::array<int, 2> footprint;
	footprint[0] = 0;
	footprint[1] = 0;

	Shape root;
	root.id = 1;
	root.level = 0;
	root.FOOTPRINT = footprint;
	std::copy(gcells.begin(), gcells.end(), root.CELLS.begin());
	std::copy(board.begin(), board.end(), root.BOARD.begin());
	root.area = -2;

	gettimeofday(&t0, NULL);
	if (!runTests) {
		if (chunkSize > 0) {
			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				dpr::parallel_stack_recursion<void>(root, FloorplanInfo(), FloorplanBody(), chunkSize, dpr::partitioner::custom());
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				dpr::parallel_stack_recursion<void>(root, FloorplanInfo(), FloorplanBody(), chunkSize, dpr::partitioner::automatic());
			} else {
				//_partitioner = 0 => simple
				dpr::parallel_stack_recursion<void>(root, FloorplanInfo(), FloorplanBody(), chunkSize, dpr::partitioner::simple());
			}
		} else {
			std::array<char, ROWS*COLS> t_board;

			FILE * t_inputFile;
			t_inputFile = fopen(test_filename.c_str(), "r");

			if (NULL == t_inputFile) {
				std::cout << "Couldn't open " << test_filename << " file for reading" << std::endl;
				std::exit(1);
			}

			/* read input file and initialize global minimum area */
			int t_N = read_inputs(t_inputFile, t_gcells, t_solution);
			if (t_N > Shape::N) {
				std::cout << "The size of the test problem must be less than or equal to the main problem" << std::endl;
				std::exit(1);
			}

			/* initialize board is empty */
			for (int i = 0; i < ROWS; i++)
			for (int j = 0; j < COLS; j++) t_board[i*ROWS+j] = 0;
			fclose(t_inputFile);

			std::array<int, 2> t_footprint;
			t_footprint[0] = 0;
			t_footprint[1] = 0;

			Shape test_root;
			test_root.id = 1;
			test_root.level = 0;
			test_root.FOOTPRINT = t_footprint;
			std::copy(t_gcells.begin(), t_gcells.end(), test_root.CELLS.begin());
			std::copy(t_board.begin(), t_board.end(), test_root.BOARD.begin());
			test_root.area = -2;

			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				dpr::parallel_stack_recursion<void>(root, FloorplanInfo(), FloorplanBody(), chunkSize, dpr::partitioner::custom(), opt, test_root);
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				dpr::parallel_stack_recursion<void>(root, FloorplanInfo(), FloorplanBody(), chunkSize, dpr::partitioner::automatic(), opt, test_root);
			} else {
				//_partitioner = 0 => simple
				dpr::parallel_stack_recursion<void>(root, FloorplanInfo(), FloorplanBody(), chunkSize, dpr::partitioner::simple(), opt, test_root);
			}
		}
	} else {
		if (_partitioner == 1) {
			//_partitioner = 1 => custom
			listChunks = dpr::parallel_stack_recursion_test<void>(root, FloorplanInfo(), FloorplanBody(), chunkSize, dpr::partitioner::custom(), opt);
		} else if (_partitioner == 2) {
			//_partitioner = 2 => automatic
			listChunks = dpr::parallel_stack_recursion_test<void>(root, FloorplanInfo(), FloorplanBody(), chunkSize, dpr::partitioner::automatic(), opt);
		} else {
			//_partitioner = 0 => simple
			listChunks = dpr::parallel_stack_recursion_test<void>(root, FloorplanInfo(), FloorplanBody(), chunkSize, dpr::partitioner::simple(), opt);
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
		std::cout << "floorplan(" << _filename << "): " << MIN_AREA << std::endl;
#ifndef NO_VALIDATE
		std::cout << ((MIN_AREA == solution) ? ("*SUCCESS*") : (" FAILURE! (" + std::to_string(MIN_AREA) + " != " + std::to_string(solution) + ")")) << std::endl;
#endif
		write_outputs();
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
