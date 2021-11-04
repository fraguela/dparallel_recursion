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
/// \file     knapsack_stack.cpp
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#include <iostream>
#include <cstdio>
#include <sys/time.h>
#include <string>
#include <limits>
#include <vector>
#include <dparallel_recursion/parallel_stack_recursion.h>

#define MAX_ITEMS 256
int _nthreads = 8;
int _limitParallel = 8;

int best_so_far = std::numeric_limits<int>::min();

struct Item {
	int value;
	int weight;
};

int knapsack_seq(Item *e, int c, int n, int v);

int knapsack_seq_without(Item *e, int c, int n, int v) {
	int with, without, best;

	if (n == 0) {
		return v;
	}

	if ((v + c * e->value / e->weight) < best_so_far) {
		return std::numeric_limits<int>::min();
	}

	without = knapsack_seq_without(e + 1, c, n - 1, v);

	with = knapsack_seq(e + 1, c - e->weight, n - 1, v + e->value);

	best = with > without ? with : without;

	if (best > best_so_far) {
		best_so_far = best;
	}

	return best;
}

int knapsack_seq(Item *e, int c, int n, int v) {
	int with, without, best;
	//long long int ub;

	/* base case: full knapsack or no items */
	if (c < 0) {
		return std::numeric_limits<int>::min();
	}

	/* feasible solution, with value v */
	if (n == 0 || c == 0) {
		return v;
	}

	//ub = v + c * e->value / e->weight;

	if ((v + c * e->value / e->weight) < best_so_far) {
		/* prune ! */
		return std::numeric_limits<int>::min();
	}
	/*
	* compute the best solution without the current item in the knapsack
	*/
	without = knapsack_seq_without(e + 1, c, n - 1, v);

	/* compute the best solution with the current item in the knapsack */
	with = knapsack_seq(e + 1, c - e->weight, n - 1, v + e->value);

	best = with > without ? with : without;

	/*
	* notice the race condition here. The program is still
	* correct, in the sense that the best solution so far
	* is at least best_so_far. Moreover best_so_far gets updated
	* when returning, so eventually it should get the right
	* value. The program is highly non-deterministic.
	*/
	if (best > best_so_far) {
		best_so_far = best;
	}

	return best;
}

struct KnapsackConfig {
	int capacity;
	int nitems;
	int value;
	Item *fitem;
	int level;

	KnapsackConfig() {

	}

	KnapsackConfig(int _capacity, int _nitems, int _value, Item *_fitem, int _level) : capacity(_capacity), nitems(_nitems), value(_value), fitem(_fitem), level(_level) {

	}

	void deallocate() {
		best_so_far = std::numeric_limits<int>::min();
	}
};

struct KnapsackInfo : public dpr::Arity<2> {

	KnapsackInfo() : dpr::Arity<2>(_nthreads, _nthreads)
	{}

	static bool is_base(const KnapsackConfig& t) {
		/* base case: full knapsack or no items */
		if (t.capacity < 0) {
			return true;
		} else if (t.nitems == 0 || t.capacity == 0) {
			if (t.value > best_so_far) {
				best_so_far = t.value;
			}
			return true;
		} else {
			return ((t.value + t.capacity * t.fitem->value / t.fitem->weight) < best_so_far);
			//return false;
		}
	}

	static KnapsackConfig child(int i, const KnapsackConfig& c) {
		if (i == 0) {
			return KnapsackConfig(c.capacity, c.nitems - 1, c.value, c.fitem+1, c.level+1);
		} else {
			return KnapsackConfig(c.capacity - c.fitem->weight, c.nitems - 1, c.value + c.fitem->value, c.fitem+1, c.level+1);
		}
	}

	static bool do_parallel(const KnapsackConfig& t) noexcept {
		return t.level < _limitParallel;
	}

	static void do_serial_func(const KnapsackConfig& c) noexcept {
		knapsack_seq(c.fitem, c.capacity, c.nitems, c.value);
	}

};

struct KnapsackBody: public dpr::EmptyBody<KnapsackConfig, void> {

};

int compare(struct Item *a, struct Item *b) {
	double c = ((double) a->value / a->weight) - ((double) b->value / b->weight);

	if (c > 0) return -1;
	if (c < 0) return 1;
	return 0;
}

int read_input(std::string filename, Item *items, int &capacity, int &n) {
	FILE *f;

	f = fopen(filename.c_str(), "r");
	if (f == NULL) {
		fprintf(stderr, "open_input(\"%s\") failed\n", filename.c_str());
		return -1;
	}
	/* format of the input: #items capacity value1 weight1 ... */
	fscanf(f, "%d", &n);
	fscanf(f, "%d", &capacity);

	for (int i = 0; i < n; ++i) {
		fscanf(f, "%d %d", &items[i].value, &items[i].weight);
	}

	fclose(f);

	/* sort the items on decreasing order of value/weight */
	/* cilk2c is fascist in dealing with pointers, whence the ugly cast */
	qsort(items, n, sizeof(Item), (int (*)(const void *, const void *)) compare);

	return 0;
}

int main(int argc, char** argv) {
	struct Item items[MAX_ITEMS];
	int capacity, nitems;
	std::string filename;
	struct timeval t0, t1, t;
	int chunkSize = 6;
	int stackSize = 1000;
	int _partitioner = 0;		//0 = simple, 1 = custom, 2 = automatic. Default: simple
	std::string test_filename;
	struct Item test_items[MAX_ITEMS];
	bool runTests = false;
	std::vector<dpr::ResultChunkTest> listChunks;
  
	if (getenv("OMP_NUM_THREADS"))
		_nthreads = atoi(getenv("OMP_NUM_THREADS"));

	if (argc > 1) {
		filename = std::string(argv[1]);
		test_filename = filename;
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
		test_filename = std::string(argv[6]);
		if (test_filename == "" || test_filename == "0") {
			test_filename = filename;
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

	read_input(filename, items, capacity, nitems);

	gettimeofday(&t0, NULL);
	KnapsackConfig initknapsack = KnapsackConfig(capacity, nitems, 0, items, 0);

	if (!runTests) {
		if (chunkSize > 0) {
			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				dpr::parallel_stack_recursion<void> (initknapsack, KnapsackInfo(), KnapsackBody(), chunkSize, dpr::partitioner::custom());
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				dpr::parallel_stack_recursion<void> (initknapsack, KnapsackInfo(), KnapsackBody(), chunkSize, dpr::partitioner::automatic());
			} else {
				//_partitioner = 0 => simple
				dpr::parallel_stack_recursion<void> (initknapsack, KnapsackInfo(), KnapsackBody(), chunkSize, dpr::partitioner::simple());
			}
		} else {
			KnapsackConfig test_initknapsack;
			read_input(test_filename, test_items, test_initknapsack.capacity, test_initknapsack.nitems);
			test_initknapsack.value = 0;
			test_initknapsack.fitem = test_items;
			if (_partitioner == 1) {
				//_partitioner = 1 => custom
				dpr::parallel_stack_recursion<void>(initknapsack, KnapsackInfo(), KnapsackBody(), chunkSize, dpr::partitioner::custom(), opt, test_initknapsack);
			} else if (_partitioner == 2) {
				//_partitioner = 2 => automatic
				dpr::parallel_stack_recursion<void>(initknapsack, KnapsackInfo(), KnapsackBody(), chunkSize, dpr::partitioner::automatic(), opt, test_initknapsack);
			} else {
				//_partitioner = 0 => simple
				dpr::parallel_stack_recursion<void>(initknapsack, KnapsackInfo(), KnapsackBody(), chunkSize, dpr::partitioner::simple(), opt, test_initknapsack);
			}
		}
	} else {
		if (_partitioner == 1) {
			//_partitioner = 1 => custom
			listChunks = dpr::parallel_stack_recursion_test<void>(initknapsack, KnapsackInfo(), KnapsackBody(), chunkSize, dpr::partitioner::custom(), opt);
		} else if (_partitioner == 2) {
			//_partitioner = 2 => automatic
			listChunks = dpr::parallel_stack_recursion_test<void>(initknapsack, KnapsackInfo(), KnapsackBody(), chunkSize, dpr::partitioner::automatic(), opt);
		} else {
			//_partitioner = 0 => simple
			listChunks = dpr::parallel_stack_recursion_test<void>(initknapsack, KnapsackInfo(), KnapsackBody(), chunkSize, dpr::partitioner::simple(), opt);
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
		std::cout << "knapsach(" << filename << "): " << best_so_far << std::endl;
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
