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
/// \file     fib.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/*
* fib.c
*
 *  Created on: 2009/10/27
 *      Author: carloshgv
 */
#include <cstdlib>
#include <cstdio>
#include <sys/time.h>

int p = 25;

size_t fib(int n) {
  
  if (n < 2) {
    return n;
  } else {
    return fib(n - 1) + fib(n - 2);
  }
  
}

int main(int argc, char** argv) {
  size_t n;
  struct timeval t0, t1, t;
  
  if (argc > 1)
    p = atoi(argv[1]);
  
  gettimeofday(&t0, NULL);
  n = fib(p);
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  printf("fib(%d) = %lu\n", p, n);
  printf("compute time: %f\n", (t.tv_sec * 1000000 + t.tv_usec) / 1000000.0);
  
  return 0;
}
