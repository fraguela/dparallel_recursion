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
/// \file     strassen.cpp
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <sys/time.h>
#include <cstdio>
#include <cstdlib>
#include <dparallel_recursion/dparallel_recursion.h>
#include "Matrix.h"

using namespace dpr;

typedef Matrix<double> MMatrix;

int n = 1024;           //DEFAULT_PBL_SZ
int base_case_sz = 512; //DEFAULT_BASE_CASE_SZ
int nthreads = 8;
int tasks_per_thread = 1;
int nprocs;

#include "../sequential/mxm_product.cpp"

struct Problem {
  
  MMatrix a, b, c;
  bool solved_;
  
  template<typename T>
  Problem(T&& ai, T&& bi, T&& ci) :
  a(std::forward<T>(ai)),
  b(std::forward<T>(bi)),
  c(std::forward<T>(ci)),
  solved_(false)
  { }
  
  /// c is left empty
  template<typename T1, typename T2>
  Problem(T1&& ai, T2&& bi) :
  a(std::forward<T1>(ai)),
  b(std::forward<T2>(bi)),
  solved_(false)
  { }
  
  Problem() :
  solved_(false)
  { }
  
  /// Copy constructor
  Problem(const Problem& other) :
  a(other.a),
  b(other.b),
  c(other.c),
  solved_(other.solved_)
  {
    /* fulfills: assert(other.a.shallow() && other.b.shallow() && other.c.shallow()); */
  }
  
  /// Move constructor
  Problem(Problem&& other) noexcept :
  a(std::move(other.a)),
  b(std::move(other.b)),
  c(std::move(other.c)),
  solved_(other.solved_)
  {}
  
  /// Move-assignment. Mostly useful to fill-in initially empty Problems
  Problem& operator=(Problem&& other)
  {
    a = std::move(other.a);
    b = std::move(other.b);
    c = std::move(other.c);
    solved_ = other.solved_;
    return *this;
  }
  
  Problem& operator=(const Problem& other)
  {
    assert(other.a.empty() && other.b.empty() && other.c.empty());
    return *this;
  }
  
  template<typename Archive>
  void serialize(Archive& ar,  unsigned int file_version)
  {
    ar & solved_;
    if (solved_) ar & c;
    else ar & a & b;
  }
  
  //BOOST_SERIALIZATION_SPLIT_MEMBER()
  
};

TRANSMIT_BY_CHUNKS(MMatrix);
TRANSMIT_BY_CHUNKS(Problem);

struct StrassenInfo : public DInfo<Problem, 7> {
  
  StrassenInfo() : DInfo<Problem, 7> (nthreads * tasks_per_thread)
  {}
  
  bool is_base(const Problem& p) const {
    return p.a.rows() <= base_case_sz;
  }
  
  Problem child(int i, const Problem& p) const {
    
    int n  = p.a.rows();
    int n2 = n / 2;

    MMatrix A11(p.a,  0, n2,  0, n2);
    MMatrix A12(p.a,  0, n2, n2,  n);
    MMatrix A21(p.a, n2,  n,  0, n2);
    MMatrix A22(p.a, n2,  n, n2,  n);
    
    MMatrix B11(p.b,  0, n2,  0, n2);
    MMatrix B12(p.b,  0, n2, n2,  n);
    MMatrix B21(p.b, n2,  n,  0, n2);
    MMatrix B22(p.b, n2,  n, n2,  n);
  
    switch (i) {
      case 0: return Problem(A11 + A22, B11 + B22);
      case 1: return Problem(A21 + A22, B11);
      case 2: return Problem(A11, B12 - B22);
      case 3: return Problem(A22, B21 - B11);
      case 4: return Problem(A11 + A12, B22);
      case 5: return Problem(A21 - A11, B11 + B12);
      case 6: return Problem(A12 - A22, B21 + B22);
    }
  }
  
};

struct StrassenBody : public EmptyBody<Problem, Problem> {
  
  Problem&& base(Problem& p) {
    
    if(p.c.empty())
      p.c = MMatrix(p.a.rows());
    
    mxm(p.a, p.b, p.c);
    
    p.a.deallocate();
    p.b.deallocate();
    p.solved_ = true;
    return std::move(p);
  }
  
  
  Problem&& post(Problem& p, Problem* M) {
    
    int nl = p.a.rows();
    int n2 =  nl / 2;
    
    if(p.c.empty())
      p.c = MMatrix(nl);
    
    MMatrix C11(p.c,  0, n2,  0, n2);
    MMatrix C12(p.c,  0, n2, n2, nl);
    MMatrix C21(p.c, n2, nl,  0, n2);
    MMatrix C22(p.c, n2, nl, n2, nl);
    
    int step = (nl >= n/4) && (n2 >= 512) ? (n2 / 16) : n2;
    tbb::parallel_for(0, n2, step, [&](int ii) {
      int limit = std::min(n2, ii + step);
      for (int i = ii; i < limit; i++) {
        for (int j = 0; j < n2; j++) {
        
          C11(i, j) = M[0].c(i,j) + M[3].c(i,j) - M[4].c(i,j) + M[6].c(i,j);
          C12(i, j) = M[2].c(i,j) + M[4].c(i,j);
          C21(i, j) = M[1].c(i,j) + M[3].c(i,j);
          C22(i, j) = M[0].c(i,j) - M[1].c(i,j) + M[2].c(i,j) + M[5].c(i,j);

        }
      }
    }  );
    
    p.a.deallocate();
    p.b.deallocate();
    p.solved_ = true;
    return std::move(p);
  }
  
};

int main(int argc, char **argv)
{ struct timeval t0, t1, t;
  int provided, rank, res = 1;
  
  MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
  assert(provided >= MPI_THREAD_SERIALIZED);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
  
  if (getenv("OMP_NUM_THREADS"))
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  
  pr_init(nthreads);
  
  if (argc > 1)
    n = atoi(argv[1]);
  
  if (argc > 2)
    base_case_sz = atoi(argv[2]);
  
  if (argc > 3)
    tasks_per_thread = atoi(argv[3]);
  
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
  
  MMatrix a(n), b(n), c(n);
  
  if (!rank) {
    for(int i =0 ; i < n; i++)
      for(int j = 0; j < n; j++) {
        c(i,j) = 0.;
        a(i,j) = (rand() & 127) / 64.0;
        b(i,j) = (rand() & 127) / 64.0;
      }
  }

  gettimeofday(&t0, NULL);
  Problem p(MMatrix(a, 0, n, 0, n), MMatrix(b, 0, n, 0, n), MMatrix(c, 0, n, 0, n)); //Shallow ref
  dparallel_recursion<Problem> (p, StrassenInfo(), StrassenBody(), partitioner::automatic(), PrioritizeDM|Balance);
  gettimeofday(&t1, NULL);
  timersub(&t1, &t0, &t);
  
  if (!rank) {
    printf("Mx sz: %d Base_case_sz: %d\n", n, base_case_sz);
    printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
    printf("compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);

#ifndef NO_VALIDATE
    MMatrix c_gold(n);
  
    mxm(a, b, c_gold);
  
    res = (c == c_gold);
    printf("%s\n", res ? "*SUCCESS*" : " FAILURE!");
#endif
  }
  
  MPI_Finalize();
  return !res;
}
