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
/// \file     npb-C.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/*
  NAS Parallel Benchmarks 2.3 OpenMP C Versions
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#if defined(_OPENMP)
#include <omp.h>
#endif /* _OPENMP */

#ifdef __cplusplus
extern "C" {
#endif

typedef int boolean;
typedef struct { double real; double imag; } dcomplex;

#define TRUE	1
#define FALSE	0
/*
#define max(a,b) (((a) > (b)) ? (a) : (b))
#define min(a,b) (((a) < (b)) ? (a) : (b))
*/
#define	pow2(a) ((a)*(a))

#define get_real(c) c.real
#define get_imag(c) c.imag
#define cadd(c,a,b) (c.real = a.real + b.real, c.imag = a.imag + b.imag)
#define csub(c,a,b) (c.real = a.real - b.real, c.imag = a.imag - b.imag)
#define cmul(c,a,b) (c.real = a.real * b.real - a.imag * b.imag, \
                     c.imag = a.real * b.imag + a.imag * b.real)
#define crmul(c,a,b) (c.real = a.real * b, c.imag = a.imag * b)

extern double randlc(double *, double);
extern void vranlc(int, double *, double, double *);
extern void timer_clear(int);
extern void timer_start(int);
extern void timer_stop(int);
extern double timer_read(int);

extern void c_print_results(char *name, char cls, int n1, int n2,
			    int n3, int niter, int nthreads, double t,
			    double mops, char *optype, int passed_verification,
			    char *npbversion, char *compiletime, char *cc,
			    char *clink, char *c_lib, char *c_inc,
			    char *cflags, char *clinkflags, char *rand);
#ifdef __cplusplus
}
#endif
