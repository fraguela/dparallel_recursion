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
/// \file     quicksort.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

/*
 * quicksort.cpp
 *
 *  Created on: 2009/10/07
 *      Author: carloshgv
 */
#include <cstdlib>
#include <sys/time.h>
#include <cstdio>
#include <algorithm>
#include <iostream>

using namespace std;

size_t LIMIT = 10000;

/*
 void print(int* a, int size) {
 int i;
 for (i = 0; i < size; i++) {
 printf("%d\n", a[i]);
 }
 printf("\n");
 }
*/

struct range_t {
  int* begin;
  size_t size;
  
  range_t(int* begin_, size_t size_)
  : begin(begin_), size(size_) {}
  
  range_t partition() {
    int* array = begin;
    int* key0 = begin;
    size_t m = size/2u;
    std::swap ( array[0], array[m] );
    
    size_t i=0;
    size_t j=size;
    // Partition interval [i+1,j-1] with key *key0.
    for(;;) {
      // Loop must terminate since array[l]==*key0.
      do {
	--j;
      } while( *key0 > array[j] );
      do {
	if( i==j ) goto partition;
        ++i;
      } while( array[i] > *key0 );
      if( i==j ) goto partition;
      std::swap( array[i], array[j] );
    }
  partition:
    // Put the partition key where it belongs
    std::swap( array[j], *key0 );
    // array[l..j) is less or equal to key.
    // array(j..r) is std::greater than or equal to key.
    // array[j] is equal to key
    i=j+1;
    //other.begin = array+i;
    //other.size = size-i;
    m = size - i;
    size = j;
    return range_t(array+i, m);
  }
  
};

void quickSort(range_t range) {
  if (range.size > LIMIT) {
    range_t other = range.partition();
    quickSort(range);
    quickSort(other);
  } else {
    std::sort(range.begin, range.begin + range.size, greater<int> ());
  }
}

int main(int argc, char** argv) {
  size_t n = 10;
  int *a;
  
  struct timeval t0, t1, t;
  
  if (argc > 1)
    n = (size_t) strtoull(argv[1], NULL, 0);
  if (argc > 2)
    LIMIT = atoi(argv[2]);
  
  a = (int*)malloc(sizeof(int) * n);
  srand(1234);
  for (size_t i = 0; i < n; i++)
    a[i] = rand();
  //print(a, n);
  
  gettimeofday(&t0, NULL);
  quickSort(range_t(a, n));
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  printf("compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
  //print(a, n);
  
  return 0;
}
