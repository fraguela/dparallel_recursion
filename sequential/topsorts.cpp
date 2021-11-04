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

int *A = nullptr; /* holds acyclic digraph compatible with 1,2,...,n */
size_t Nnodes{1};

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

  void process() {
    fill_nchildren();
    const size_t num_children = nchildren();
    if (num_children > 0) {
      Nnodes += num_children;
      for (int i = 0; i < num_children; i++) {
        child(i).process();
      }
    }
  }

  bool empty() const noexcept { return value_.empty(); }

  // Requires i in 0..(SZ-1)
  Problem adj(const int i) const noexcept {
    auto copy = value_;
    std::swap(copy[i], copy[i+1]);
    return copy;
  }

  bool reverse(const int s, const int i) const noexcept {
    return (i <= (s - 1)) || ( (i == (s + 1)) && (s <= (SZ-3)) && (value_[s] < value_[s+2]) );
  }
  
  void fill_nchildren() {
    if (!empty()) { //dismiss empty problems
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

  // Generate the i-th child problem, i in [0..(nchildren()-1)]
  // Does not test whether that child should exist!
  Problem child(const int i) const noexcept { return adj(children_[i]); }

  size_t nchildren() const noexcept { return children_.size(); }

};

int Problem::SZ = 9;

int main(int argc, char** argv) {
  int m;

  printf("\nEnter number of vertices and edges:");
  scanf("%d %d", &Problem::SZ, &m);
  printf("\nEnter %d edge(s) as pairs i j, i<j : \n", m);
  A = (int *)calloc(Problem::SZ * Problem::SZ, sizeof(int));

  for(int k=1; k<=m; k++) {
    int i, j;
    scanf("%d %d", &i, &j);
    if (i >= j) {
      printf ("\nEdge must have form i<j");
      return 0;
    }
    A[(i - 1) * Problem::SZ + (j - 1)] = 1;
  }
  
  const auto t0 = std::chrono::steady_clock::now();

  Problem input(true);
  input.process();
  
  const auto t1 = std::chrono::steady_clock::now();
  double time = std::chrono::duration<double>(t1 - t0).count();

  printf("Result: %zu\nTime: %lf\n", Nnodes, time);

  free(A);

  return 0;
}
