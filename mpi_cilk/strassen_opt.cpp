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
#include <utility>
#include <cilk/cilk_api.h>
#include "Matrix.h"
#include <vector>
#include <chrono>

typedef Matrix<double> MMatrix;

int n = 1024;           //DEFAULT_PBL_SZ
int base_case_sz = 512; //DEFAULT_BASE_CASE_SZ
int rank, nprocs, nwanted_tasks, probl_per_node;
int tasks_per_thread = 1;
int nthreads = 8;

#include "../sequential/mxm_product.cpp"

void Strassen_seq(const MMatrix& a, const MMatrix& b, MMatrix& c, int ntasks);

MMatrix Strassen(const MMatrix& a, const MMatrix& b, int ntasks = 131072)
{
  MMatrix result(a.rows(), b.cols());
  
  Strassen_seq(a, b, result, ntasks);
  
  return result;
}

void Strassen_seq(const MMatrix& a, const MMatrix& b, MMatrix& c, int ntasks)
{
  assert( a.cols() == b.rows() );
  
  int nl = a.rows();
  if (nl <= base_case_sz) {
    mxm(a, b, c);
  } else {
    
    int n2 =  nl / 2;
    
    MMatrix A11(a,  0, n2,  0, n2);
    MMatrix A12(a,  0, n2, n2, nl);
    MMatrix A21(a, n2, nl,  0, n2);
    MMatrix A22(a, n2, nl, n2, nl);
    
    MMatrix B11(b,  0, n2,  0, n2);
    MMatrix B12(b,  0, n2, n2, nl);
    MMatrix B21(b, n2, nl,  0, n2);
    MMatrix B22(b, n2, nl, n2, nl);
    
    MMatrix C11(c,  0, n2,  0, n2);
    MMatrix C12(c,  0, n2, n2, nl);
    MMatrix C21(c, n2, nl,  0, n2);
    MMatrix C22(c, n2, nl, n2, nl);
    
    MMatrix M[7];
    
    if (ntasks >= nwanted_tasks) {
      M[0] = Strassen(A11 + A22, B11 + B22);
      M[1] = Strassen(A21 + A22, B11);
      M[2] = Strassen(A11, B12 - B22);
      M[3] = Strassen(A22, B21 - B11);
      M[4] = Strassen(A11 + A12, B22);
      M[5] = Strassen(A21 - A11, B11 + B12);
      M[6] = Strassen(A12 - A22, B21 + B22);
    } else {
      //printf("cur %d<%d -> 7 of size %d\n", ntasks, nwanted_tasks, n2);
      ntasks *= 7;

      _Cilk_spawn [&]{ M[0] =  Strassen(A11 + A22, B11 + B22, ntasks); }();
      _Cilk_spawn [&]{ M[1] =  Strassen(A21 + A22, B11, ntasks); }();
      _Cilk_spawn [&]{ M[2] =  Strassen(A11, B12 - B22, ntasks); }();
      _Cilk_spawn [&]{ M[3] =  Strassen(A22, B21 - B11, ntasks); }();
      _Cilk_spawn [&]{ M[4] =  Strassen(A11 + A12, B22, ntasks); }();
      _Cilk_spawn [&]{ M[5] =  Strassen(A21 - A11, B11 + B12, ntasks); }();
      // this one is run by this thread
      // #pragma omp task default(shared) firstprivate(ntasks) untied
      M[6] = Strassen(A12 - A22, B21 + B22, ntasks);
      _Cilk_sync;
    }
    
    /* We are inside a task, not a parallel region, so a worksharing construct is not in order
     #pragma omp for firstprivate(n2) \
     schedule(static, (ntasks < nwanted_tasks) && (n2 >= 1024) ? (int)ceilf(n2/ceilf(nwanted_tasks/(float)ntasks)) : n2)
     */
    int step = (nl >= n/4) && (n2 >= 512) ? (n2 / 16) : n2;
    _Cilk_for(int ii = 0; ii < n2; ii += step){
      int limit = std::min(n2, ii + step);
      for (int i = 0; i < limit; i++) {
        for (int j = 0; j < n2; j++) {
          C11(i, j) = M[0](i,j) + M[3](i,j) - M[4](i,j) + M[6](i,j); //7 OUT
          C12(i, j) = M[2](i,j) + M[4](i,j);                         //5 OUT
          C21(i, j) = M[1](i,j) + M[3](i,j);                         //4 OUT
          C22(i, j) = M[0](i,j) - M[1](i,j) + M[2](i,j) + M[5](i,j);
        }
      }
    }
  }
  
}



struct Problem {
  
  MMatrix a, b, c;
  
  template<typename T>
  Problem(T&& ai, T&& bi, T&& ci) :
  a(std::forward<T>(ai)),
  b(std::forward<T>(bi)),
  c(std::forward<T>(ci))
  { }
  
  /// c is left empty
  template<typename T1, typename T2>
  Problem(T1&& ai, T2&& bi) :
  a(std::forward<T1>(ai)),
  b(std::forward<T2>(bi))
  { }
  
  Problem()
  { }
  
  /// Copy constructor
  Problem(const Problem& other) :
  a(other.a),
  b(other.b),
  c(other.c)
  {}
  
  /// Move constructor
  Problem(Problem&& other) noexcept :
  a(std::move(other.a)),
  b(std::move(other.b)),
  c(std::move(other.c))
  {}
  
  /// Move-assignment. Mostly useful to fill-in initially empty Problems
  Problem& operator=(Problem&& other)
  {
    a = std::move(other.a);
    b = std::move(other.b);
    c = std::move(other.c);
    return *this;
  }
  
  void send(int rank) {
    a.send(rank);
    b.send(rank);
  }
  
  void recv(int rank) {
    a.recv(rank);
    b.recv(rank);
    //c = MMatrix(a.rows());
  }

  void Strassen()
  {
    if(c.empty())
      c = MMatrix(a.rows());
    
    Strassen_seq(a, b, c, probl_per_node);
    
    a.deallocate();
    b.deallocate();
  }

  void Strassen_reduce(Problem *M)
  {
    int n = a.rows();
    int n2 =  n / 2;
    
    if(c.empty())
      c = MMatrix(n);
    
    MMatrix C11(c,  0, n2,  0, n2);
    MMatrix C12(c,  0, n2, n2,  n);
    MMatrix C21(c, n2,  n,  0, n2);
    MMatrix C22(c, n2,  n, n2,  n);

    auto myf = [&, n2](int i) {
      for (int j = 0; j < n2; j++) {
        C11(i, j) = M[0].c(i,j) + M[3].c(i,j) - M[4].c(i,j) + M[6].c(i,j);
        C12(i, j) = M[2].c(i,j) + M[4].c(i,j);
        C21(i, j) = M[1].c(i,j) + M[3].c(i,j);
        C22(i, j) = M[0].c(i,j) - M[1].c(i,j) + M[2].c(i,j) + M[5].c(i,j);
      }
    };
    
    if(n2 >= 512) {
      _Cilk_for(int i = 0; i < n2; i++) {
        myf(i);
      }
    } else {
      for (int i = 0; i < n2; i++) {
        myf(i);
      }
    }

    for (int i = 0; i < 7; i++)
      M[i].c.deallocate();
  }

  void Strassen_partition(Problem *M)
  {
    int n  = a.rows();
    int n2 = n / 2;
    
    MMatrix A11(a,  0, n2,  0, n2);
    MMatrix A12(a,  0, n2, n2,  n);
    MMatrix A21(a, n2,  n,  0, n2);
    MMatrix A22(a, n2,  n, n2,  n);
    
    MMatrix B11(b,  0, n2,  0, n2);
    MMatrix B12(b,  0, n2, n2,  n);
    MMatrix B21(b, n2,  n,  0, n2);
    MMatrix B22(b, n2,  n, n2,  n);

    _Cilk_spawn [&]{ M[0] = Problem(A11 + A22, B11 + B22); }();
    _Cilk_spawn [&]{ M[1] = Problem(A21 + A22, B11); }();
    _Cilk_spawn [&]{ M[2] = Problem(A11, B12 - B22); }();
    _Cilk_spawn [&]{ M[3] = Problem(A22, B21 - B11); }();
    _Cilk_spawn [&]{ M[4] = Problem(A11 + A12, B22); }();
    _Cilk_spawn [&]{ M[5] = Problem(A21 - A11, B11 + B12); }();
    M[6] = Problem(A12 - A22, B21 + B22);
    _Cilk_sync;
  }

};

typedef std::vector<Problem> Datatree_level_t;
static const float MaxImbalance = 0.2f;
using profile_clock_t = std::chrono::high_resolution_clock;


/// Keeps the information on the distribution of data
struct Distribution {
  
  int rank_, nprocs_, nglobal_elems_;
  int * elems_per_rank_;
  int * next_elems_per_rank_;
  bool isBalanced_;
  

  Distribution(int rank) :
  rank_(rank), nprocs_(nprocs), nglobal_elems_(1)
  {
    elems_per_rank_ = new int [nprocs];
    elems_per_rank_[0] = 1;
    std::fill(elems_per_rank_ + 1, elems_per_rank_ + nprocs_, 0);
    
    next_elems_per_rank_ = new int [nprocs];
    //next_elems_per_rank_[0] = 0; // This informs later of whether this is the initial distribution
    
    isBalanced_ = (nprocs_ == 1);
  }
  
  /// Copy constructor
  Distribution(const Distribution& other) :
  rank_(other.rank_), nprocs_(other.nprocs_), nglobal_elems_(other.nglobal_elems_),
  isBalanced_(other.isBalanced_)
  {
    elems_per_rank_ = new int [nprocs_];
    std::copy(other.elems_per_rank_, other.elems_per_rank_ + nprocs_, elems_per_rank_);
    
    next_elems_per_rank_ = new int [nprocs_];
    std::copy(other.next_elems_per_rank_, other.next_elems_per_rank_ + nprocs_, next_elems_per_rank_);
  }
  

  /// \brief Returns the number of subproblems assigned to this rank
  int nLocalElems() const noexcept {
    return elems_per_rank_[rank_];
  }
  
  
  ~Distribution()
  {
    delete [] elems_per_rank_;
    delete [] next_elems_per_rank_;
  }

  /// \brief Returns the number of subproblems assigned to the process \c rank
  /// \param rank Id of the process whose number of subproblems is to be computed
  /// \param actual Whether we ask for the defult value (false) or the actual one (true) when using balancing
  int nLocalElems(int rank, bool actual) const {
    return actual ? elems_per_rank_[rank] : ((nglobal_elems_ / nprocs_) + (rank < (nglobal_elems_ % nprocs_)));
  }
  
  /// \brief whether the current distribution is within the maximum deviation
  bool isBalanced() const noexcept { return isBalanced_; }
  
  bool heuristicRebalanceMustStop() noexcept
  { static std::chrono::time_point<profile_clock_t> tp;
    static float prevPeriod_ = 0.f;
    
    std::chrono::time_point<profile_clock_t> cur_tp = profile_clock_t::now();
    
    bool must_stop = (nglobal_elems_ >= (50 * nprocs_));
    
    if(prevPeriod_ == 0.f) {
      prevPeriod_ = -1.f; // Only take tp. Skip timing tests
    } else {
      const float period = std::chrono::duration<float>(cur_tp - tp).count();
      if(prevPeriod_ > 0.f) { // Non-first measurement
        if(!must_stop) {
          const float time_limit = std::max(prevPeriod_ * 1.4f, 0.25f);
          int decision = (period > time_limit);
          MPI_Bcast(&decision, 1, MPI_INT, 0, MPI_COMM_WORLD);
          must_stop = must_stop || decision;
        }
      } else {
        prevPeriod_ = period;
      }
    }
    
    tp = cur_tp;
    
    return must_stop;
  }

  /// Compute balance based on number of items
  void computeStdBalance() noexcept
  {
    const auto minmax_pair = std::minmax_element(elems_per_rank_, elems_per_rank_ + nprocs_);
    const int min_elems = *(minmax_pair.first);
    float max_imbalance_found = min_elems ? (*(minmax_pair.second) - min_elems) / (float)min_elems : 1e3f;
    
    isBalanced_ = (max_imbalance_found <= MaxImbalance) || heuristicRebalanceMustStop();
  }

  /// Gets to the state in which the existing items have been partitioned into NCHILDREN
  ///children each one of them, but the data has not been yet redistributed to match next_elems_per_rank_
  int progress_DMdistr()
  {
    nglobal_elems_ = nglobal_elems_ * 7;
    
    for (int i = 0; i < nprocs_; i++) {
      elems_per_rank_[i] *= 7;
      next_elems_per_rank_[i] = nLocalElems(i, false);
    }
    
    return next_elems_per_rank_[rank_];
  }
  
  void commit_progress_DMdistr() noexcept
  {
    std::copy(next_elems_per_rank_, next_elems_per_rank_ + nprocs_, elems_per_rank_);
    computeStdBalance();
  }
  
  int progress_DMred()
  {
    nglobal_elems_ = nglobal_elems_ / 7;
    
    for (int i = 0; i < nprocs_; i++) {
      next_elems_per_rank_[i] = elems_per_rank_[i];    // Current (balanced) distribution
      elems_per_rank_[i] = nLocalElems(i, false) * 7;  // Distr. after redistribution but before reduction
    }
    
    return elems_per_rank_[rank_];
  }
  
  void commit_progress_DMred() noexcept
  {
    std::for_each(elems_per_rank_, elems_per_rank_ + nprocs_, [](int &n){ n /= 7; });
  }
  
  /// Whether this rank obtained all the data it needed after nreceives receives
  bool finished_receives(const int rank, const int nreceives) const noexcept
  {
    return (elems_per_rank_[rank] + nreceives) >= next_elems_per_rank_[rank];
  }
  
  /// Whether this rank sent all the data it had to after nsends sends
  bool finished_sends(const int rank, const int nsends) const noexcept
  {
    return (elems_per_rank_[rank] - nsends) <= next_elems_per_rank_[rank];
  }
  
  /// Called by sender in distribution creation tree to know where to send its first externalizable problem
  /// and how many problems has that receiver already received
  int destination_first_external_problem(int& previous_excess) const noexcept
  { int i;
    
    previous_excess = 0;
    
    // Counts how many elements send the ranks that precede this one
    for (i = 0; i < rank_; i++) {
      previous_excess += elems_per_rank_[i] - next_elems_per_rank_[i];
    }
    
    // skip other senders
    for(i++; elems_per_rank_[i] >= next_elems_per_rank_[i]; i++);
    
    // Skips receivers until it reaches the first receiver that needs items from this rank
    do {
      int items_to_receive = next_elems_per_rank_[i] - elems_per_rank_[i];
      if(previous_excess >= items_to_receive) {
        previous_excess -= items_to_receive;
        i++;
      } else {
        return i;
      }
    } while(true);
    
    return -1;
  }
  
  /// Called by receiver in distribution creation tree to know from where will it receive its first problem
  /// and how many problems has that sender already sent
  int source_first_external_problem(int& previously_sent) const noexcept
  { int i;
    
    previously_sent = 0;
    
    // skip senders
    for(i = 0; elems_per_rank_[i] >= next_elems_per_rank_[i]; i++);
    
    // Count how many elements were received by the destinations that precede this one
    while(i < rank_) {
      previously_sent += next_elems_per_rank_[i] - elems_per_rank_[i];
      i++;
    }
    
    
    // Skips senders until it reaches the first sender that will send items to this rank
    i = 0;
    do {
      int items_to_send = elems_per_rank_[i] - next_elems_per_rank_[i];
      if(previously_sent >= items_to_send) {
        previously_sent -= items_to_send;
        i++;
      } else {
        return i;
      }
    } while(true);
    
    return -1;
  }
  
};

void Strassen_mpi(Problem& p)
{ std::vector<Datatree_level_t> datatree;

  Distribution dist(rank);
  
  if(!rank) {
    datatree.emplace_back(1);
    datatree[0][0] = std::move(p);
  }

  while ( (dist.nglobal_elems_ < nprocs) || !dist.isBalanced() ) {
    const int pre_nlocal_elems = dist.nLocalElems(); //now (before re-partitioning)
    const int next_nlocal_elems = dist.progress_DMdistr(); //after redistributing
    const int post_nlocal_elems = dist.nLocalElems();     //after re-partitioning and before redistributing

    if (pre_nlocal_elems) {
      datatree.emplace_back(std::max(post_nlocal_elems, next_nlocal_elems));
      Datatree_level_t* const lv = &(datatree.back());
      Datatree_level_t& last_level = *(lv - 1);
      for (int i = 0; i < pre_nlocal_elems; i++) {
        last_level[i].Strassen_partition(lv->data() + (i * 7));
      }
    } else {
      if (next_nlocal_elems) { // Create space to receive the data
        datatree.emplace_back(next_nlocal_elems);
      }
    }
    
    if (next_nlocal_elems < post_nlocal_elems) {
      // send excess items
      int previous_excess;
      int receiver = dist.destination_first_external_problem(previous_excess);
      Datatree_level_t& last_level = datatree.back();
      for (int i = next_nlocal_elems; i < post_nlocal_elems; i++) {
        last_level[i].send(receiver);
        previous_excess++;
        if (dist.finished_receives(receiver, previous_excess)) {
          previous_excess = 0;
          receiver++;
        }
      }
    } else if (next_nlocal_elems > post_nlocal_elems) {
      // receive items
      int previously_sent;
      int sender = dist.source_first_external_problem(previously_sent);
      Datatree_level_t& last_level = datatree.back();
      for (int i = post_nlocal_elems; i < next_nlocal_elems; i++) {
        last_level[i].recv(sender);
        previously_sent++;
        if (dist.finished_sends(sender, previously_sent)) {
          previously_sent = 0;
          sender++;
        }
      }
    }
    
    dist.commit_progress_DMdistr();
  }

  //printf("Rank %d Solving %d problems\n", rank, dist.nLocalElems());
  probl_per_node = dist.nLocalElems();
  
  const int step = (nwanted_tasks >= probl_per_node) ? 1 : (probl_per_node / nwanted_tasks);
  
  _Cilk_for(int i = 0; i < probl_per_node; i += step) {
    for (int j = i; j < std::min(i + step, probl_per_node); j++) {
      datatree.back()[j].Strassen(); //Deallocates a and b
    }
  }

  Distribution reduce_tree_distr(dist);
  
  int cur_level_depth = datatree.size() - 1;
  
  while(reduce_tree_distr.nglobal_elems_ > 1) {
    const int pre_nlocal_elems = reduce_tree_distr.nLocalElems();     //now
    const int next_nlocal_elems = reduce_tree_distr.progress_DMred(); //after redistributing and before reducing
    
    Datatree_level_t& cur_level = datatree[cur_level_depth];
    
    if (next_nlocal_elems > pre_nlocal_elems) {

      // receive excess items
      int previous_excess;
      int receiver = reduce_tree_distr.destination_first_external_problem(previous_excess);
      for (int i = pre_nlocal_elems; i < next_nlocal_elems; i++) {
        cur_level[i].c.recv(receiver);
        previous_excess++;
        if (reduce_tree_distr.finished_receives(receiver, previous_excess)) {
          previous_excess = 0;
          receiver++;
        }
      }
      
    } else if (next_nlocal_elems < pre_nlocal_elems) {
      // send excess items
      int previously_sent;
      int sender = reduce_tree_distr.source_first_external_problem(previously_sent);
      for (int i = next_nlocal_elems;  i < pre_nlocal_elems; i++) {
        cur_level[i].c.send(sender);
        previously_sent++;
        if (reduce_tree_distr.finished_sends(sender, previously_sent)) {
          previously_sent = 0;
          sender++;
        }
      }
    }
    
    reduce_tree_distr.commit_progress_DMred();
    
    if(next_nlocal_elems) {
      cur_level_depth--;
      for (int i = 0; i < reduce_tree_distr.nLocalElems(); i++) {
        datatree[cur_level_depth][i].Strassen_reduce(datatree[cur_level_depth + 1].data() + (i * 7));
      }
      datatree.pop_back();
    }
    
  }
  
  if(!rank)
    p = std::move(datatree[0][0]);

}

int main(int argc, char **argv)
{ struct timeval t0, t1, t;
  int res = 1;
  
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

  nthreads = __cilkrts_get_nworkers();
  
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

  nwanted_tasks = nthreads * tasks_per_thread;

  MMatrix a(n), b(n), c(n);
  
  if (!rank) {
    for(int i =0 ; i < n; i++)
      for(int j = 0; j < n; j++) {
        c(i,j) = 0.;
        a(i,j) = (rand() & 127) / 64.0;
        b(i,j) = (rand() & 127) / 64.0;
      }
  }

  if (!rank) {
    
    printf("Mx sz: %d Base_case_sz: %d\n", n, base_case_sz);

    gettimeofday(&t0, NULL);
    Problem p(MMatrix(a, 0, n, 0, n), MMatrix(b, 0, n, 0, n), MMatrix(c, 0, n, 0, n)); //Shallow ref
    Strassen_mpi(p);
    gettimeofday(&t1, NULL);
    timersub(&t1, &t0, &t);
    
    printf("Nprocs=%d threads=%d task_per_thread=%d\n", nprocs, nthreads, tasks_per_thread);
    printf("compute time: %f\n", t.tv_sec + t.tv_usec / 1000000.0);
  
#ifndef NO_VALIDATE
    MMatrix c_gold(n);
  
    mxm(a, b, c_gold);
    
    res = (c == c_gold);

    printf("%s\n", res ? "*SUCCESS*" : " FAILURE!");
#endif
    
  } else {
    Problem p;
    Strassen_mpi(p);
  }
  
  MPI_Finalize();
  return !res;
}
