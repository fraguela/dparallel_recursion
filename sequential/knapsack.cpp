/*
 * program to solve the 0-1 knapsack problem using a branch-and-bound
 * technique. 
 *
 * Author: Matteo Frigo
 */
/*
 * Copyright (c) 2000 Massachusetts Institute of Technology
 * Copyright (c) 2000 Matteo Frigo
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <iostream>
#include <cstdio>
#include <sys/time.h>
#include <string>
#include <limits>

#define MAX_ITEMS 256

int best_so_far = std::numeric_limits<int>::min();

struct item {
	int value;
	int weight;
};

int compare(struct item *a, struct item *b) {
	double c = ((double) a->value / a->weight) - ((double) b->value / b->weight);

	if (c > 0) return -1;
	if (c < 0) return 1;
	return 0;
}

int read_input(std::string filename, item *items, int &capacity, int &n) {
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
	qsort(items, n, sizeof(item), (int (*)(const void *, const void *)) compare);

	return 0;;
}

/* 
 * return the optimal solution for n items (first is e) and
 * capacity c. Value so far is v.
 */
int knapsack(struct item *e, int c, int n, int v) {
	int with, without, best;
	//double ub;

	/* base case: full knapsack or no items */
	if (c < 0) {
		return std::numeric_limits<int>::min();
	}

	/* feasible solution, with value v */
	if (n == 0 || c == 0) {
		return  v;
	}

	//ub = (double) v + c * e->value / e->weight;

	//if (ub < best_so_far) {
	if ((v + c * e->value / e->weight) < best_so_far) {
		/* prune ! */
		return std::numeric_limits<int>::min();
	}
	/*
	* compute the best solution without the current item in the knapsack
	*/
	without = knapsack(e + 1, c, n - 1, v);

	/* compute the best solution with the current item in the knapsack */
	with = knapsack(e + 1, c - e->weight, n - 1, v + e->value);

	best = with > without ? with : without;

	/*
	* notice the race condition here. The program is still
	* correct, in the sense that the best solution so far
	* is at least best_so_far. Moreover best_so_far gets updated
	* when returning, so eventually it should get the right
	* value. The program is highly non-deterministic.
	*/
	if (best > best_so_far) best_so_far = best;

	return best;
}

int main(int argc, char** argv) {
	struct item items[MAX_ITEMS];
	int n, capacity;
	int sol = 0;
	std::string filename;
	struct timeval t0, t1, t;

	if (argc > 1) {
		filename = std::string(argv[1]);
	}

	read_input(filename, items, capacity, n);

	gettimeofday(&t0, NULL);
	sol = knapsack(items, capacity, n, 0);
	gettimeofday(&t1, NULL);
	timersub(&t1, &t0, &t);

	std::cout << "compute time: " <<( t.tv_sec + t.tv_usec / 1000000.0) << std::endl;
	std::cout << "knapsack(" << filename << "): " << sol << std::endl;

	return 0;
}
