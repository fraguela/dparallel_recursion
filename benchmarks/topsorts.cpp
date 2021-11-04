/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015-2020 Millan A. Martinez, Basilio B. Fraguela, Jose C. Cabaleiro. Universidade da Coruna
 
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
/// \file     topsorts.cpp
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <boost/serialization/vector.hpp>
#include <dparallel_recursion/dparallel_recursion.h>

using namespace dpr;

int nthreads = 4;
int tasks_per_thread = 8;
int *A = nullptr; /* holds acyclic digraph compatible with 1,2,...,n */

struct Problem {
  
  static int SZ;
  
  std::vector<int> value_;
  std::vector<int> children_;

  //If root is false, it generates the default empty problem []
  //If root is true, it generates the root problem 1 2 3 ... SZ
  Problem(const bool root = false) {
    if (root) {
      value_.resize(SZ);
      std::iota(value_.begin(), value_.end(), 1);
    }
  }

  // copy constructor
  Problem(const std::vector<int>& v) :
  value_(v)
  { }
  
  //move constructor
  Problem(std::vector<int>&& v) :
  value_(std::move(v))
  { }

  /*
  //print current problem
  void print() const noexcept {
    printf("[");
    for(const auto& v : value_) printf(" %d", v);
    printf(" ]\n");
  }
  */

  bool empty() const noexcept { return value_.empty(); }

  // Requires i in 0..(SZ-1)
  Problem adj(const int i) const noexcept {
    auto copy = value_;
    std::swap(copy[i], copy[i+1]);
    return copy;
  }

//  Problem f() const noexcept
//  { int i = 1;
//
//    while( (i < SZ) && (value_[i-1] < value_[i]) ) {
//      i++;
//    }
//
//    if (i == SZ) {
//      return Problem();
//    } else {
//      return adj(i - 1);
//    }
//  }

  bool reverse(const int s, const int i) const noexcept {
    return (i <= (s - 1)) || ( (i == (s + 1)) && (s <= (SZ-3)) && (value_[s] < value_[s+2]) );
  }
  
  void fill_nchildren() {
    if (!empty() && children_.empty()) { //dismiss empty problems
      // findindex
      int idx;
      for(idx = 0; (idx < (SZ-1)) && (value_[idx] < value_[idx+1]); idx++);
      for (int i = 0; i < (SZ-1); i++) {
        if(A[value_[i] * SZ + value_[i+1] - SZ - 1] != 1) {
          if (reverse(idx, i)) {
            children_.push_back(i);
          }
        }
      }
    }
  }

//  void fill_nchildren() {
//    if (!empty()) { //dismiss empty problems
//      for (int i = 0; i < (SZ-1); i++) { // implements "reverse"
//        if(A[value_[i] * SZ + value_[i+1] - SZ - 1] != 1) {
//          const auto w = adj(i); // This problem cannot have w == null/empty
//          if (value_ == w.f().value_) {
//            children_.push_back(i);
//          }
//        }
//      }
//    }
//  }

  // Generate the i-th child problem, i in [0..(nchildren()-1)]
  // Does not test whether that child should exist!
  Problem child(const int i) const noexcept { return adj(children_[i]); }

  size_t nchildren() const noexcept { return children_.size(); }

  template<typename Archive>
  void serialize(Archive &ar, const unsigned int) {
    ar & value_ & children_;
  }

};


int Problem::SZ = 9;

struct MyInfo : public DInfo<Problem, UNKNOWN> {
  
  MyInfo() : DInfo<Problem, UNKNOWN>(nthreads * tasks_per_thread)
  {}

  static bool is_base(const Problem& p) { return !p.nchildren(); }
  
  static int num_children(const Problem& p) { return p.nchildren(); }

  static Problem child(int i, const Problem& p) { return p.child(i); }

  //static bool do_parallel(const Problem& p) { ... }
};


struct MyBody : public EmptyBody<Problem, size_t> {
  
  static void pre(Problem& p) { p.fill_nchildren(); }

  static size_t base(const Problem& p) { return 1; }
  
  static size_t post(const Problem& p, const size_t* v) {
    return std::accumulate(v, v + p.nchildren(), (size_t)1);
  }
  
};

//BOOST_IS_BITWISE_SERIALIZABLE(Problem);

int main(int argc, char** argv)
{
  int rank;
  int nprocs;
  int partitioner = 2;
  int m;

  if (getenv("OMP_NUM_THREADS"))
    nthreads = atoi(getenv("OMP_NUM_THREADS"));

  if (argc > 1)
    tasks_per_thread = atoi(argv[1]);

  if (argc > 2)
    partitioner = atoi(argv[2]);

  dpr_init(argc, argv, nprocs, rank, nthreads);

  if (rank == 0) {
    printf("\nEnter number of vertices and edges:");

    scanf("%d %d", &Problem::SZ, &m);
    MPI_Bcast(&Problem::SZ, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&m, 1, MPI_INT, 0, MPI_COMM_WORLD);
    printf("\nEnter %d edge(s) as pairs i j, i<j : \n", m);
    //printf("%d %d\n", Problem::SZ, m);
    A = (int *)calloc(Problem::SZ * Problem::SZ, sizeof(int));

    for(int k=1; k<=m; k++) {
      int i, j;
      scanf("%d %d", &i, &j);
      MPI_Bcast(&i, 1, MPI_INT, 0, MPI_COMM_WORLD);
      MPI_Bcast(&j, 1, MPI_INT, 0, MPI_COMM_WORLD);
      if (i >= j) {
        printf ("\nEdge must have form i<j");
        return 0;
      }
      A[(i - 1) * Problem::SZ + (j - 1)] = 1;
      //printf("%d %d\n", i, j);
    }

    printf("Nprocs=%d threads=%d sz=%d tasks_per_thread=%d partitioner=%d\n", nprocs, nthreads, Problem::SZ, tasks_per_thread, partitioner);
  } else {
    MPI_Bcast(&Problem::SZ, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&m, 1, MPI_INT, 0, MPI_COMM_WORLD);

    A = (int *)calloc(Problem::SZ * Problem::SZ, sizeof(int));

    for(int k=1; k<=m; k++) {
      int i, j;
      MPI_Bcast(&i, 1, MPI_INT, 0, MPI_COMM_WORLD);
      MPI_Bcast(&j, 1, MPI_INT, 0, MPI_COMM_WORLD);
      if (i >= j) {
        return 0;
      }
      A[(i - 1) * Problem::SZ + (j - 1)] = 1;
      //printf("%d %d\n", i, j);
    }
  }

  Problem input(true);

  const auto t0 = std::chrono::steady_clock::now();

  size_t result;
  if (partitioner == 2) {
    result = dparallel_recursion<size_t> (input, MyInfo(), MyBody(), dpr::partitioner::automatic(), dpr::ReplicatedInput | dpr::Balance);
  } else {
    result = dparallel_recursion<size_t> (input, MyInfo(), MyBody(), dpr::partitioner::simple(), dpr::ReplicatedInput | dpr::Balance);
  }
  
  const auto t1 = std::chrono::steady_clock::now();
  double time = std::chrono::duration<double>(t1 - t0).count();

  if (rank == 0) {
    printf("Result: %zu\nTime: %lf\n", result, time);
  }

  free(A);

  MPI_Finalize();

  return 0;
}
