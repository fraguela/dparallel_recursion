/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015 Carlos H. Gonzalez, Basilio B. Fraguela. Universidade da Coruna
 
 This file is part of dparallel_recursion.
 
 dparallel_recursion is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software  Foundation; either version 3, or (at your option) any later version.
 
 dparallel_recursion is distributed in the  hope that  it will  be  useful, 
 but  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
 or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for 
 more details.
 
 You should have received a copy of  the GNU General  Public License along with
 dparallel_recursion. If not, see <http://www.gnu.org/licenses/>.
*/

///
/// \file     ep_mpi.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/*--------------------------------------------------------------------
  
  NAS Parallel Benchmarks 2.3 OpenMP C versions - EP

  This benchmark is an OpenMP C version of the NPB EP code.
  
  The OpenMP C versions are developed by RWCP and derived from the serial
  Fortran versions in "NPB 2.3-serial" developed by NAS.

  Permission to use, copy, distribute and modify this software for any
  purpose with or without fee is hereby granted.
  This software is provided "as is" without express or implied warranty.
  
  Send comments on the OpenMP C versions to pdp-openmp@rwcp.or.jp

  Information on OpenMP activities at RWCP is available at:

           http://pdplab.trc.rwcp.or.jp/pdperf/Omni/
  
  Information on NAS Parallel Benchmarks 2.3 is available at:
  
           http://www.nas.nasa.gov/NAS/NPB/

--------------------------------------------------------------------*/
/*--------------------------------------------------------------------

  Author: P. O. Frederickson 
          D. H. Bailey
          A. C. Woo

  OpenMP C version: S. Satoh
  
--------------------------------------------------------------------*/

#include <algorithm>
#include "npb-C.h"
#include "npbparams.h"
#include <mpi.h>

/* parameters */
#define	MK		16
#define	MM		(M - MK)
#define	NN		(1 << MM)
#define	NK		(1 << MK)
#define	NQ		10
#define EPSILON		1.0e-8
#define	A		1220703125.0
#define	S		271828183.0
#define	TIMERS_ENABLED	FALSE

/* global variables */
/* common /storage/ */
static double x[2*NK];
// #pragma omp threadprivate(x)
static double q[NQ];

/*--------------------------------------------------------------------
      program EMBAR
c-------------------------------------------------------------------*/
/*
c   This is the serial version of the APP Benchmark 1,
c   the "embarassingly parallel" benchmark.
c
c   M is the Log_2 of the number of complex pairs of uniform (0, 1) random
c   numbers.  MK is the Log_2 of the size of each batch of uniform random
c   numbers.  MK can be set for convenience on a given system, since it does
c   not affect the results.
*/
int main(int argc, char **argv) {
    int rank, nprocs;
    double Mops, t1, t2, t3, t4, x1, x2, sx, sy, tm, an, tt, gc;
    double gsx, gsy;
    double dum[3] = { 1.0, 1.0, 1.0 };
    int np, i, ik, kk, l, k, nit, k_offset, j;
    //int nthreads = 1;
    boolean verified;
    char size[13+1];	/* character*13 */

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
/*
c   Because the size of the problem is too large to store in a 32-bit
c   integer for some classes, we put it into a string (for printing).
c   Have to strip off the decimal point put in there by the floating
c   point print statement (internal file)
*/

  if (!rank) {
    printf("\n\n NAS Parallel Benchmarks 2.3 OpenMP C version"
           " - EP Benchmark\n");
    sprintf(size, "%12.0f", pow(2.0, M+1));
    for (j = 13; j >= 1; j--) {
      if (size[j] == '.') size[j] = ' ';
    }
    printf(" Number of random numbers generated: %13s\n", size);
  }
  
    verified = FALSE;

/*
c   Compute the number of "batches" of random number pairs generated 
c   per processor. Adjust if the number of processors does not evenly 
c   divide the total number
*/
    np = NN;

/*
c   Call the random number generator functions and initialize
c   the x-array to reduce the effects of paging on the timings.
c   Also, call all mathematical functions that are used. Make
c   sure these initializations cannot be eliminated as dead code.
*/
    vranlc(0, &(dum[0]), dum[1], &(dum[2]));
    dum[0] = randlc(&(dum[1]), dum[2]);
    //for (i = 0; i < 2*NK; i++) x[i] = -1.0e99;
    Mops = log(sqrt(fabs(std::max(1.0, 1.0))));

    timer_clear(1);
    timer_clear(2);
    timer_clear(3);
    timer_start(1);

    //vranlc(0, &t1, A, x);

/*   Compute AN = A ^ (2 * NK) (mod 2^46). */

    t1 = A;

    for ( i = 1; i <= MK+1; i++) {
	t2 = randlc(&t1, t1);
    }

    an = t1;
    tt = S;
    gc = 0.0;
    sx = 0.0;
    sy = 0.0;

    for ( i = 0; i <= NQ - 1; i++) {
	q[i] = 0.0;
    }
      
/*
c   Each instance of this loop may be performed independently. We compute
c   the k offsets separately to take into account the fact that some nodes
c   have more numbers to generate than others
*/
    k_offset = -1;

//#pragma omp parallel copyin(x)
//{
    //double t1, t2, t3, t4, x1, x2;
    //int kk, i, ik, l;
    double qq[NQ];		/* private copy of q[0:NQ-1] */

    for (i = 0; i < NQ; i++) qq[i] = 0.0;

    int iters_per_proc = (np + nprocs -1) / nprocs;
    int end_iter = std::min(np,(rank + 1) * iters_per_proc);

    for (k = rank * iters_per_proc + 1; k <= end_iter; k++) {
	kk = k_offset + k;
	t1 = S;
	t2 = an;

/*      Find starting seed t1 for this kk. */

	for (i = 1; i <= 100; i++) {
            ik = kk / 2;
            if (2 * ik != kk) t3 = randlc(&t1, t2);
            if (ik == 0) break;
            t3 = randlc(&t2, t2);
            kk = ik;
	}

/*      Compute uniform pseudorandom numbers. */

	if (TIMERS_ENABLED == TRUE) timer_start(3);
	vranlc(2*NK, &t1, A, x-1);
	if (TIMERS_ENABLED == TRUE) timer_stop(3);

/*
c       Compute Gaussian deviates by acceptance-rejection method and 
c       tally counts in concentric square annuli.  This loop is not 
c       vectorizable.
*/
	if (TIMERS_ENABLED == TRUE) timer_start(2);

	for ( i = 0; i < NK; i++) {
            x1 = 2.0 * x[2*i] - 1.0;
            x2 = 2.0 * x[2*i+1] - 1.0;
            t1 = pow2(x1) + pow2(x2);
            if (t1 <= 1.0) {
		t2 = sqrt(-2.0 * log(t1) / t1);
		t3 = (x1 * t2);				/* Xi */
		t4 = (x2 * t2);				/* Yi */
                l = std::max(fabs(t3), fabs(t4));
		qq[l] += 1.0;				/* counts */
		sx = sx + t3;				/* sum of Xi */
		sy = sy + t4;				/* sum of Yi */
            }
	}
	if (TIMERS_ENABLED == TRUE) timer_stop(2);
    }
  

    MPI_Reduce(&sx, &gsx, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&sy, &gsy, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(qq,    q, NQ, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
//#pragma omp critical
//    {
//      for (i = 0; i <= NQ - 1; i++) q[i] += qq[i];
//    }
//#if defined(_OPENMP)
//#pragma omp master
//    nthreads = omp_get_num_threads();
//#endif /* _OPENMP */
/* end of parallel region */

    for (i = 0; i < NQ; i++) {
        gc = gc + q[i];
    }

    timer_stop(1);
    tm = timer_read(1);

    nit = 0;
    if (M == 24) {
	if((fabs((gsx- (-3.247834652034740e3))/gsx) <= EPSILON) &&
	   (fabs((gsy- (-6.958407078382297e3))/gsy) <= EPSILON)) {
	    verified = TRUE;
	}
    } else if (M == 25) {
	if ((fabs((gsx- (-2.863319731645753e3))/gsx) <= EPSILON) &&
	    (fabs((gsy- (-6.320053679109499e3))/gsy) <= EPSILON)) {
	    verified = TRUE;
	}
    } else if (M == 28) {
	if ((fabs((gsx- (-4.295875165629892e3))/gsx) <= EPSILON) &&
	    (fabs((gsy- (-1.580732573678431e4))/gsy) <= EPSILON)) {
	    verified = TRUE;
	}
    } else if (M == 30) {
	if ((fabs((gsx- (4.033815542441498e4))/gsx) <= EPSILON) &&
	    (fabs((gsy- (-2.660669192809235e4))/gsy) <= EPSILON)) {
	    verified = TRUE;
	}
    } else if (M == 32) {
	if ((fabs((gsx- (4.764367927995374e4))/gsx) <= EPSILON) &&
	    (fabs((gsy- (-8.084072988043731e4))/gsy) <= EPSILON)) {
	    verified = TRUE;
	}
    } else if (M == 36) {
        if ((fabs((gsx - (1.982481200946593e5)) / gsx) <= EPSILON) &&
            (fabs((gsy - (-1.020596636361769e5)) / gsy) <= EPSILON)) {
            verified = TRUE;
        }
    } else if (M == 40) {
        if ((fabs((gsx - (-5.319717441530e5)) / gsx) <= EPSILON) &&
            (fabs((gsy - (-3.688834557731e5)) / gsy) <= EPSILON)) {
            verified = TRUE;
        }
    }

    Mops = pow(2.0, M+1)/tm/1000000.0;

  if (!rank) {

    printf("EP Benchmark Results: \n"
	   "CPU Time = %10.4f\n"
	   "N = 2^%5d\n"
	   "No. Gaussian Pairs = %15.0f\n"
	   "Sums = %25.15e %25.15e\n"
	   "Counts:\n",
	   tm, M, gc, gsx, gsy);
    printf("compute time: %f\n", tm);

    for (i = 0; i  <= NQ-1; i++) {
	printf("%3d %15.0f\n", i, q[i]);
    }
	  
    c_print_results("EP", CLASS, M+1, 0, 0, nit, nprocs,
		  tm, Mops, 	
		  "Random numbers generated",
		  verified, NPBVERSION, COMPILETIME,
		  CS1, CS2, CS3, CS4, CS5, CS6, CS7);

    if (TIMERS_ENABLED == TRUE) {
	printf("Total time:     %f", timer_read(1));
	printf("Gaussian pairs: %f", timer_read(2));
	printf("Random numbers: %f", timer_read(3));
    }
  }
  
  MPI_Finalize();
  return 0;
}
