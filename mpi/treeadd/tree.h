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
/// \file     tree.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/* For copyright information, see olden_v1.0/COPYRIGHT */
#include <iostream>

/* tree.h
 */
extern int level, iters;

class tree {
public:
	int val, level;
	tree *left, *right;

	tree() : level(0), left(NULL), right(NULL), val(0) {};
	tree(int lvl) : level(lvl), left(NULL), right(NULL), val(1) {};
};
typedef tree tree_t;

extern tree_t *TreeAlloc(int level, int rank, int nprocs);
extern tree_t **partition_row;
extern int myleaves;
