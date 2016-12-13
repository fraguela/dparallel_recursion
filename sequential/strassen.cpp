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

#include <sys/time.h>
#include <cstdio>
#include <cstdlib>
#include "Matrix.h"

typedef Matrix<double> MMatrix;

int n = 1024;           //DEFAULT_PBL_SZ
int base_case_sz = 512; //DEFAULT_BASE_CASE_SZ

#include "mxm_product.cpp"

void Strassen(const MMatrix& a, const MMatrix& b, MMatrix& c);

MMatrix Strassen(const MMatrix& a, const MMatrix& b)
{
  MMatrix result(a.rows(), b.cols());
  
  Strassen(a, b, result);
  
  return result;
}

void Strassen(const MMatrix& a, const MMatrix& b, MMatrix& c)
{
  assert( a.cols() == b.rows() );
  
  int n = a.rows();
  if (n <= base_case_sz) {
    mxm(a, b, c);
  } else {
    
    int n2 =  n / 2;
    
    MMatrix A11(a,  0, n2,  0, n2);
    MMatrix A12(a,  0, n2, n2,  n);
    MMatrix A21(a, n2,  n,  0, n2);
    MMatrix A22(a, n2,  n, n2,  n);
    
    MMatrix B11(b,  0, n2,  0, n2);
    MMatrix B12(b,  0, n2, n2,  n);
    MMatrix B21(b, n2,  n,  0, n2);
    MMatrix B22(b, n2,  n, n2,  n);
    
    MMatrix C11(c,  0, n2,  0, n2);
    MMatrix C12(c,  0, n2, n2,  n);
    MMatrix C21(c, n2,  n,  0, n2);
    MMatrix C22(c, n2,  n, n2,  n);

  
    MMatrix M1 = Strassen(A11 + A22, B11 + B22);
    MMatrix M2 = Strassen(A21 + A22, B11);
    MMatrix M3 = Strassen(A11, B12 - B22);
    MMatrix M4 = Strassen(A22, B21 - B11);
    MMatrix M5 = Strassen(A11 + A12, B22);
    MMatrix M6 = Strassen(A21 - A11, B11 + B12);
    MMatrix M7 = Strassen(A12 - A22, B21 + B22);
    
    for (int i = 0; i < n2; i++) {
      for (int j = 0; j < n2; j++) {
        C11(i, j) = M1(i,j) + M4(i,j) - M5(i,j) + M7(i,j); //7 OUT
        C12(i, j) = M3(i,j) + M5(i,j);                     //5 OUT
        C21(i, j) = M2(i,j) + M4(i,j);                     //4 OUT
        C22(i, j) = M1(i,j) - M2(i,j) + M3(i,j) + M6(i,j);
      }
    }
  }
  
}

int main(int argc, char **argv)
{ struct timeval t0, t1, t;
  int res = 1;
  
  if (argc > 1)
    n = atoi(argv[1]);
  
  if (argc > 2)
    base_case_sz = atoi(argv[2]);
  
  if (base_case_sz >= n)
    base_case_sz = n /2;
  
  if ( (n <= 0) && (base_case_sz <= 0) ) {
    puts("The input and base case should be > 0");
    return -1;
  }
  
  if ( (n & (n - 1)) || (base_case_sz & (base_case_sz - 1)) ) {
    puts("The input and base case size should be powers of 2");
    return -1;
  }

  printf("Mx sz: %d Base_case_sz: %d\n", n, base_case_sz);
  
  MMatrix a(n), b(n), c(n);
  
  for(int i =0 ; i < n; i++)
    for(int j = 0; j < n; j++) {
      c(i,j) = 0.;
      a(i,j) = (rand() & 127) / 64.0;
      b(i,j) = (rand() & 127) / 64.0;
    }
  
  gettimeofday(&t0, NULL);
  Strassen(a, b, c);
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  
  printf("compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
  
#ifndef NO_VALIDATE
  MMatrix c_gold(n);
  
  mxm(a, b, c_gold);

  res = (c == c_gold);
  printf("%s\n", res ? "*SUCCESS*" : " FAILURE!");
#endif
  
  return !res;
}
