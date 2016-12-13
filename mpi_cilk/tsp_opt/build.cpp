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
/// \file     build.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/* For copyright information, see olden_v1.0/COPYRIGHT */

/* build.c
 *
 * By:  Martin C. Carlisle
 *      Princeton University
 *      6/24/94
 *
 * builds a two-dimensional tree for TSP
 *
 * distribution of median is given by modification of exponential to
 * be [-1,1]
 */

#include <cstdlib>
#include <cmath>

#ifdef M_E
#undef M_E
#endif

#define M_E        2.7182818284590452354
#define M_E2       7.3890560989306502274
#define M_E3      20.08553692318766774179
#define M_E6     403.42879349273512264299
#define M_E12 162754.79141900392083592475

#include "tsp.h"

#ifdef SCALABLE_ALLOCATION
#include <tbb/scalable_allocator.h>
#define NODEALLOCATION scalable_malloc
#else
#define NODEALLOCATION malloc
#endif

/* Return an estimate of median of n values distributed in [min,max) */
double median(double min, double max, int n, double fseed) {
	double t;
	double retval;

        unsigned seed = min * max * 1000. * fseed;
	t = (double(rand_r(&seed)) / RAND_MAX); //drand48(); /* in [0.0,1.0) */
	if (t > 0.5) {
		retval = log(1.0 - (2.0 * (M_E12 - 1) * (t - 0.5) / M_E12)) / 12.0;
	} else {
		retval = -log(1.0 - (2.0 * (M_E12 - 1) * t / M_E12)) / 12.0;
	}
	/* We now have something distributed on (-1.0,1.0) */
	retval = (retval + 1.0) * (max - min) / 2.0;
	retval = retval + min;
	return retval;
}

/* Get double uniformly distributed over [min,max) */
double uniform(double min, double max) {
	double retval;

        unsigned seed = min * max * 1000.;
	retval = (double(rand_r(&seed)) / RAND_MAX); //drand48(); /* in [0.0,1.0) */
	retval = retval * (max - min);
	return retval + min;
}


int nleaves, bottom_n, orig_bottom_n, myleaves, myfirst_leaf, partition_row_count = 0, par_level;
Tree *partition_row;

//BBF Test: int reserved =0;

void set_par_level(int top_n) {
  par_level = top_n;
  for (int nleaves = 1; (par_level > 1) && nleaves < (nthreads * tasks_per_thread); nleaves *=2) {
    par_level /= 2;
  }
}

/* Builds a 2D tree of n nodes in specified range with dir as primary
 axis (0 for x, 1 for y) */
Tree build_tree(int n, int dir, double min_x, double max_x, double min_y, double max_y) {
  double med;
  Tree t;
  
  if (n == bottom_n) {
    if (bottom_n) {
      if((partition_row_count >= myfirst_leaf) && (partition_row_count < (myfirst_leaf + myleaves))) {
        bottom_n = 0;
        t = build_tree(n, dir, min_x, max_x, min_y, max_y);
        bottom_n = orig_bottom_n;
      } else {
        t = NULL;
      }
      partition_row[partition_row_count++] = t;
      return t;
    }
    return NULL;
  }

  t = (Tree) NODEALLOCATION(sizeof(*t));

  if (dir) {
    dir = !dir;
    med = median(min_x, max_x, n, min_y * max_y);
    
    t->x = med;
    t->y = uniform(min_y, max_y);
    
    if (!bottom_n && (n > par_level)) {
      t->left = _Cilk_spawn build_tree(n / 2, dir, min_x, med, min_y, max_y);
      t->right = build_tree(n / 2, dir, med, max_x, min_y, max_y);
      _Cilk_sync;
    } else {
      t->left = build_tree(n / 2, dir, min_x, med, min_y, max_y);
      t->right = build_tree(n / 2, dir, med, max_x, min_y, max_y);
    }
  } else {
    dir = !dir;
    med = median(min_y, max_y, n, min_x * max_x);
    
    t->y = med;
    t->x = uniform(min_x, max_x);
    
    if (!bottom_n && (n > par_level)) {
      t->left = _Cilk_spawn build_tree(n / 2, dir, min_x, max_x, min_y, med);
      t->right = build_tree(n / 2, dir, min_x, max_x, med, max_y);
      _Cilk_sync;
    } else {
      t->left = build_tree(n / 2, dir, min_x, max_x, min_y, med);
      t->right = build_tree(n / 2, dir, min_x, max_x, med, max_y);
    }
  }
  t->sz = n;
  t->next = NULL;
  t->prev = NULL;
  //printf("%d %d %f %f %f %f %f %f\n", n, dir, min_x, max_x, min_y, max_y, t->x, t->y);
  return t;
}

Tree build_tree(int rank, int nprocs, int n, int dir, double min_x, double max_x, double min_y, double max_y) {
  Tree ret;
  
  bottom_n = n;
  for (nleaves = 1; (bottom_n > 1) && nleaves < nprocs; nleaves *=2) {
    bottom_n /= 2;
  }
  
  orig_bottom_n = bottom_n;
  partition_row = new Tree [nleaves];
  //assert(partition_row != NULL);

  myleaves = nleaves / nprocs;
  int tmp2 = nleaves % nprocs;
  if (rank < tmp2) {
    myleaves++;
  }
  
  myfirst_leaf = (nleaves / nprocs) * rank;
  myfirst_leaf += (rank >= tmp2) ? tmp2 : rank;

  set_par_level(bottom_n);

  ret = build_tree(n, dir, min_x, max_x, min_y, max_y);

  return ret;
}
