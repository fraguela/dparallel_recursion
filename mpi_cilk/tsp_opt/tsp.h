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
/// \file     tsp.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <cassert>
#include <iostream>

/* For copyright information, see olden_v1.0/COPYRIGHT */

extern int flag;

struct tree {
	int sz;
	double x, y;

	struct tree *left, *right;
	struct tree *next, *prev;
  
	tree() /*: sz(0), left(NULL), right(NULL), next(NULL), prev(NULL), cd(NULL) */ { }
};

typedef tree* Tree;

extern Tree *partition_row;
extern int nleaves, bottom_n, myleaves, myfirst_leaf;
extern int tasks_per_thread, nthreads, par_level;

// Should optimize a bit
//BOOST_CLASS_IMPLEMENTATION(tree, boost::serialization::object_serializable);

/* Builds a 2D tree of n nodes in specified range with dir as primary
 axis (0 for x, 1 for y) */
//void build_tree(Tree root, int n, int dir, double min_x, double max_x, double min_y, double max_y);
/* Compute TSP for the tree t -- use conquer for problems <= sz */

Tree conquer(Tree t);
Tree merge(Tree a, Tree b, Tree t);
Tree makelist(Tree t);
void reverse(Tree t);
double distance(Tree a, Tree b);
Tree tsp(int rank, int nprocs, Tree t, int sz);
int dealwithargs(int argc, char** argv);

extern void set_par_level(int top_n);
