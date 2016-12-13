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

#include <stdio.h>
#include <stdlib.h>

//extern double exp(double x);
extern double log(double x);

#define  M_E       2.7182818284590452354
#define  M_E2      7.3890560989306502274
#define  M_E3     20.08553692318766774179
#define M_E6     403.42879349273512264299
#define M_E12 162754.79141900392083592475

#include "tsp.h"

#ifdef SCALABLE_ALLOCATION
#include <tbb/scalable_allocator.h>
#define NODEALLOCATION scalable_malloc
#else
#define NODEALLOCATION malloc
#endif

/*
static double median(double min, double max, int n, double fseed);
static double uniform(double min, double max);
*/

/* Return an estimate of median of n values distributed in [min,max) */
static double median(double min, double max, int n, double fseed) {
	double t;
	double retval;

        unsigned seed = min * max * 1000. * fseed;
	t = ((double)rand_r(&seed) / RAND_MAX);//drand48(); /* in [0.0,1.0) */
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
static double uniform(double min, double max) {
	double retval;

        unsigned seed = min * max * 1000.;
	retval = ((double)rand_r(&seed) / RAND_MAX);//drand48(); /* in [0.0,1.0) */
	retval = retval * (max - min);
	return retval + min;
}

/* Builds a 2D tree of n nodes in specified range with dir as primary
 axis (0 for x, 1 for y) */
Tree build_tree(int n, int dir, double min_x, double max_x, double min_y, double max_y) {
	double med;
	Tree t;

	if (n == 0)
		return NULL;

	t = (Tree) NODEALLOCATION(sizeof(*t));
	if (dir) {
		dir = !dir;
		med = median(min_x, max_x, n, min_y * max_y);

                t->x = med;
                t->y = uniform(min_y, max_y);
          
		t->left = build_tree(n / 2, dir, min_x, med, min_y, max_y);
		t->right = build_tree(n / 2, dir, med, max_x, min_y, max_y);
	} else {
		dir = !dir;
		med = median(min_y, max_y, n, min_x * max_x);

                t->y = med;
                t->x = uniform(min_x, max_x);
          
		t->left = build_tree(n / 2, dir, min_x, max_x, min_y, med);
		t->right = build_tree(n / 2, dir, min_x, max_x, med, max_y);
	}
	t->sz = n;
	t->next = NULL;
	t->prev = NULL;
  
   //printf("%d %d %f %f %f %f %f %f\n", n, !dir, min_x, max_x, min_y, max_y, t->x, t->y);

        return t;
}
