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
/// \file     args.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/* For copyright information, see olden_v1.0/COPYRIGHT */
#include <stdlib.h>
#include "tree.h"

int level = 25, iters = 100;

int dealwithargs(int argc, char *argv[]) {
	//if (argc > 3)
	//	LIMIT = atoi(argv[3]);

	if (argc > 2)
		iters = atoi(argv[2]);

	if (argc > 1)
		level = atoi(argv[1]);

	return level;
}
