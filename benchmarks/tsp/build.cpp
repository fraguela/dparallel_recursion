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


