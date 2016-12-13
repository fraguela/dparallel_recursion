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
/// \file     DInfo.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_DINFO_H_
#define DPR_DINFO_H_

#include <cassert>
#include <vector>
#include <algorithm>
#include <numeric>
#include <boost/tti/has_member_function.hpp>
#include <tbb/parallel_for.h>
#include "dparallel_recursion/dpr_mpi_comm.h"
#include "dparallel_recursion/dpr_mpi_gather_scatter.h"
#include "dparallel_recursion/general_reference_wrapper.h"
#include "dparallel_recursion/Arity.h"
#include "dparallel_recursion/Behavior.h"

namespace dpr {

/// Specifies heuristics for the desired balancing
struct BalanceParams {

  float maxImbalance_;    ///< Ratio of maximum imbalance allowed
  float maxIncrPartTime_; ///< Ratio of maximum growth of the partitioning time
  int maxElemsPerRank_;   ///< Maximum average number of subproblems per rank

  BalanceParams(float maxImbalance = 0.2f,
                float maxIncrPartTime = 0.4f,
                int maxElemsPerRank = 50) :
  maxImbalance_(maxImbalance),
  maxIncrPartTime_(maxIncrPartTime),
  maxElemsPerRank_(maxElemsPerRank)
  {}
  
  BalanceParams(const BalanceParams& other) = default;

  BalanceParams& operator= (const BalanceParams& other) = default;

};

/**
 \brief Base class for Info objects used by the ::dparallel_recursion skeleton.

        It fulfills two roles: providing information on how to partition the problem
        and storing its decomposition so it can be reused in other executions of the skeleton.
 
 \tparam T           type of the input problem
 \tparam NCHILDREN   Number of children subproblems of a non-base problem, or
                     \c UNKNOWN (=0) when this is variable or not known in advance

 Conditions for the use of a distributed input:
 - if NCHILDREN are fixed (>0), nprocs_ must be a power of NCHILDREN and reduction is in tree.
   Otherwise there is a single level in which the T() root problem has nprocs_ children at once.
 - Body::post must not use the input problem T, except to detect whether it is T()
 */
template<typename T, int NCHILDREN>
class DInfo : public Arity<NCHILDREN> {
        /// \cond detailed
public:
	typedef std::vector<general_reference_wrapper<T>> Datatree_level_t;
	typedef std::vector<Datatree_level_t> Datatree_t;

	static int DefaultNprocs;
        static const int MaxGlobalElems = 0x10000000;
  
	BOOST_TTI_HAS_MEMBER_FUNCTION(gather_scatter);

	class EmptyClass {};
  
	template<typename TYPE>
	using MakeClass_t = typename boost::mpl::if_<std::is_class<TYPE>, TYPE, EmptyClass>::type;

private:
  
        /// Keeps the information on the distribution of data
        struct Distribution {

          int rank_, nprocs_, nglobal_elems_;
          int * elems_per_rank_;
          int * next_elems_per_rank_;
          BalanceParams balanceParams_;
          float prevPeriod_;
          int behavior_flags_;
          bool isBalanced_;
          
          Distribution(const BalanceParams& ibalanceParams = BalanceParams()) :
          rank_(0), nprocs_(0), nglobal_elems_(0),
          elems_per_rank_(nullptr), next_elems_per_rank_(nullptr),
          balanceParams_(ibalanceParams), prevPeriod_(0.f), behavior_flags_(DefaultBehavior),
          isBalanced_(true)
          { }
          
          /// Copy constructor
          ///
          /// \internal elems_per_rank_ and next_elems_per_rank_ are deeply copied
          Distribution(const Distribution& other) :
          rank_(other.rank_), nprocs_(other.nprocs_), nglobal_elems_(other.nglobal_elems_),
          elems_per_rank_(nullptr), next_elems_per_rank_(nullptr),
          balanceParams_(other.balanceParams_), prevPeriod_(other.prevPeriod_), behavior_flags_(other.behavior_flags_),
          isBalanced_(other.isBalanced_)
          {
            if(other.elems_per_rank_ != nullptr) {
              elems_per_rank_ = new int [nprocs_];
              std::copy(other.elems_per_rank_, other.elems_per_rank_ + nprocs_, elems_per_rank_);
            }
            
            if(other.next_elems_per_rank_ != nullptr) {
              next_elems_per_rank_ = new int [nprocs_];
              std::copy(other.next_elems_per_rank_, other.next_elems_per_rank_ + nprocs_, next_elems_per_rank_);
            }
          }
          
          void init(int rank, int nprocs, const Behavior& behavior)
          {
            assert(elems_per_rank_ == nullptr);

            rank_ = rank;
            nprocs_ = nprocs;
            behavior_flags_ = behavior.flags();
            isBalanced_ = (nprocs == 1);
            elems_per_rank_ = new int [nprocs];
            elems_per_rank_[0] = 0; // This informs setNGlobalElems of whether the Distribution has been balanced or not
          }
          
          void init_distr(const int rank, int nprocs, const Behavior& behavior)
          {
            init(rank, nprocs, behavior);

            // The data is supposed to be initially in proc. 0
            elems_per_rank_[0] = 1;
            std::fill(elems_per_rank_ + 1, elems_per_rank_ + nprocs_, 0);
            setNGlobalElems(1);

            next_elems_per_rank_ = new int [nprocs];
            //next_elems_per_rank_[0] = 0; // This informs later of whether this is the initial distribution
          }
          
          void prepareDistributedReduceTree(const Behavior& behavior)
          {
            assert(elems_per_rank_ != nullptr);
            
            for (int i = 0; i < nprocs_; i++) {
              assert(elems_per_rank_[i] == nLocalElems(i, false));
            }
            
            if(next_elems_per_rank_ == nullptr) {
              assert(behavior & (DistributedInput | ReplicatedInput));
              next_elems_per_rank_ = new int [nprocs_];
            }
          }
          
          /// \brief Returns the total/global number of subproblems at the decomposition level
          int nGlobalElems() const noexcept {
            return nglobal_elems_;
          }
          
          /// \brief Returns the number of subproblems assigned to this rank
          int nLocalElems() const noexcept {
            return elems_per_rank_[rank_];
          }
          
          /// \brief Returns the number of MPI processes
          int nNodes() const noexcept {
            return nprocs_;
          }
          
          /// \brief Set the parameters for the balacing heuristics
          void setBalanceParams(const BalanceParams& ibalanceParams) noexcept {
            balanceParams_ = ibalanceParams;
          }
          
          bool usesComputedCosts() const noexcept {
            return behavior_flags_ & UseCost;
          }

          void reset()
          {
            rank_ = nprocs_ = nglobal_elems_ = 0;
            balanceParams_ = BalanceParams();
            prevPeriod_ = 0.f;
            behavior_flags_ = DefaultBehavior;
            isBalanced_ = true;
            if(elems_per_rank_ != nullptr) {
              delete [] elems_per_rank_;
              elems_per_rank_ = nullptr;
            }
            if(next_elems_per_rank_ != nullptr) {
              delete [] next_elems_per_rank_;
              next_elems_per_rank_ = nullptr;
            }
          }
          
          /*
          void dump() const {
            std::cerr << "Rank " << rank_ << " of " << nprocs_ << " GlobalElems=" << nglobal_elems_ << std::endl;
            std::cerr << "Balanced=" << isBalanced_ << " required=" << (behavior_flags_ & Balance) << " based_on_costs=" << (behavior_flags_ & UseCost) << std::endl;
            std::cerr << "maxImbalance=" << balanceParams_.maxImbalance_ << " maxElemsPerRank=" << balanceParams_.maxElemsPerRank_ << " maxIncrPartTime=" << balanceParams_.maxIncrPartTime_ << std::endl;
            for(int i = 0; i < nprocs_; i++) {
              std::cerr << "Rank " << i << " has " << elems_per_rank_[i] << " elems\n";
            }
          }
          */
          
          ~Distribution()
          {
            reset();
          }
          
          bool heuristicRebalanceMustStop() noexcept
          { static std::chrono::time_point<profile_clock_t> tp;
            
            const std::chrono::time_point<profile_clock_t> cur_tp = profile_clock_t::now();
            
            bool must_stop = (nglobal_elems_ >= (balanceParams_.maxElemsPerRank_ * nprocs_));
            
            //if(!rank_ && must_stop) { std::cerr << " ***** MAX ELEMS REACHED! **** " << nglobal_elems_ << std::endl; }
            
            if(prevPeriod_ == 0.f) {
              prevPeriod_ = -1.f; // Only take tp. Skip timing tests
            } else {
              const float period = std::chrono::duration<float>(cur_tp - tp).count();
              if(prevPeriod_ > 0.f) { // Non-first measurement
                if(!must_stop) {
                  const float time_limit = std::max(prevPeriod_ * (1.f + balanceParams_.maxIncrPartTime_), 0.25f);
                  int decision = (period > time_limit);
                  if((behavior_flags_ & PrioritizeDM) || (behavior_flags_ & ReplicatedInput) ) {
                    MPI_Bcast(&decision, 1, MPI_INT, 0, MPI_COMM_WORLD);
                  }
                  must_stop = must_stop || decision;
                }
                //if(!rank_ && (period > time_limit) ) {
                //  std::cerr << " ***** MAX TIME REACHED! ****\n";
                //  std::cerr << period << ">?" << time_limit << std::endl;
                // }
              } else {
                //With this else it is only timed once. Otherwise it could eternally grow at a rate of balanceParams_.maxIncrPartTime_ %
                prevPeriod_ = period;
                //if(!rank_) std::cerr << "max time will be " << std::max(prevPeriod_ * (1.f + balanceParams_.maxIncrPartTime_), 0.2f) << std::endl;
              }
            }
            
            //if(!rank_) std::cerr << "glbl=" << nglobal_elems_ << " time=" << prevPeriod_ << std::endl;
            
            tp = cur_tp;
            
            return must_stop;
          }
    
          /// Sets the number of global elements distributed and returns the number associated to the indicated rank
          int setNGlobalElems(const int value)
          {
            assert(value < MaxGlobalElems);
            
            nglobal_elems_ = value;
            if(!elems_per_rank_[0]) {
              redistributeOnElems();
            }

            return nLocalElems();
          }
          
          /// \brief Returns the order of the first subproblem assigned to a process at the level where the decomposition took place
          /// \param rank Id of the process of interest
          int firstGlobalElem(const int rank) const noexcept {
            if (behavior_flags_ & UseCost) {
              return std::accumulate(elems_per_rank_, elems_per_rank_ + rank, 0);
            } else {
              int tmp = (nglobal_elems_ / nprocs_) * rank;
              int tmp2 = nglobal_elems_ % nprocs_;
              return (rank >= tmp2) ? (tmp + tmp2) : (tmp + rank); // tmp + std::min(tmp2, rank)
            }
          }
          
          /// \brief Returns the number of subproblems assigned to the process \c rank
          /// \param rank Id of the process whose number of subproblems is to be computed
          /// \param actual Whether we ask for the defult value (false) or the actual one (true) when using balancing
          int nLocalElems(int rank, bool actual) const {
            return actual ? elems_per_rank_[rank] : ((nglobal_elems_ / nprocs_) + (rank < (nglobal_elems_ % nprocs_)));
          }
          
          /// \brief whether the current distribution is within the maximum deviation
          bool isBalanced() const noexcept { return isBalanced_; }
          
          /// Compute balance based on number of items
          /// \internal used either from redistribute or from commit_progress_DMdistr (when PrioritizeDM)
          void computeBalanceOnElems() noexcept
          {
            assert(!(behavior_flags_ & UseCost));

            const auto minmax_pair = std::minmax_element(elems_per_rank_, elems_per_rank_ + nprocs_);
            const int min_elems = *(minmax_pair.first);
            float max_imbalance_found = min_elems ? (*(minmax_pair.second) - min_elems) / (float)min_elems : 1e3f;

            //if(!rank_) std::cerr << "maximbalance=" << max_imbalance_found << std::endl;

            isBalanced_ = (max_imbalance_found <= balanceParams_.maxImbalance_) ||
                          ( /* min_elems && */ heuristicRebalanceMustStop() );
          }
          
          bool rec_redistribute(float * const elem_costs, float * const process_costs, int * const elems_per_rank, const int nprocs_considered) const noexcept
          { bool retry, sucess = false;

            if(nprocs_considered > 1) {
              do {
                int maxproc, ind;
                
                do {

                  float * const maxp = std::max_element(process_costs, process_costs + nprocs_considered);
                  maxproc = maxp - process_costs;
                  if(elems_per_rank[maxproc] == 1) { //cannot rebalance a single element
                    return sucess;
                  }
                  ind = std::accumulate(elems_per_rank, elems_per_rank + maxproc, 0);
                
                  if(maxproc && ((process_costs[maxproc-1] + elem_costs[ind]) < *maxp)) {
                    process_costs[maxproc-1] += elem_costs[ind];
                    elems_per_rank[maxproc-1]++;
                    process_costs[maxproc] -= elem_costs[ind];
                    elems_per_rank[maxproc]--;
                    sucess = true;
                    continue;
                  }
                
                  ind += elems_per_rank[maxproc] - 1;
                  if((maxproc < (nprocs_considered-1)) && ((process_costs[maxproc+1] + elem_costs[ind]) < *maxp)) {
                    process_costs[maxproc+1] += elem_costs[ind];
                    elems_per_rank[maxproc+1]++;
                    process_costs[maxproc] -= elem_costs[ind];
                    elems_per_rank[maxproc]--;
                    sucess = true;
                    continue;
                  }
                  
                  break;
                  
                } while(1);
                
                bool bal1 = rec_redistribute(elem_costs, process_costs, elems_per_rank, maxproc);
                bool bal2 = rec_redistribute(elem_costs + (ind + 1), process_costs + (maxproc + 1), elems_per_rank + (maxproc + 1), nprocs_considered - (maxproc + 1));
                retry = bal1 || bal2;
                
              } while(retry);
            }

            return sucess;
          }
          
          /// \brief recomputes elems_per_rank_ to mimize the imbalance and computes whether the result is balanced
          /// \internal computations only begin once we reach nglobal_elems_ >= nprocs_, otherwise it returns false
          template<typename InfoTYPE>
          bool redistribute(const InfoTYPE& info, const Datatree_level_t& lv)
          {
            if(!isBalanced_) {

              // this is not the final value, as there may be more iterations
              nglobal_elems_ = static_cast<int> (lv.size());
              
              if(nglobal_elems_ < nprocs_) {
                return isBalanced(); // = false
              }
              
              if(!(behavior_flags_ & Balance)) {
                isBalanced_ = true; // because nglobal_elems_>= nprocs_
              } else {
                if (behavior_flags_ & UseCost) {
                  float elem_costs[nglobal_elems_], process_costs[nprocs_];
                  
                  float avg = 0.f;
                  for (int i = 0; i < nglobal_elems_; i++) {
                    elem_costs[i] = info.cost(lv[i].get());
                    avg += elem_costs[i];
                  }
                  avg = avg / nprocs_; //Expected average cost per process
                  
                  const float upper_limit = avg + std::min(1.f / nglobal_elems_, balanceParams_.maxImbalance_ / 2.0f);
                  //const float lower_limit = avg * (1.f - balanceParams_.maxImbalance_ / 2.0f);
                  
                  std::fill(elems_per_rank_, elems_per_rank_ + nprocs_, 0);
                  std::fill(process_costs, process_costs + nprocs_, 0.f);
                  
                  int cur_proc = 0;
                  for (int i = 0; i < nglobal_elems_; i++) {
                    if ( elems_per_rank_[cur_proc] && ((process_costs[cur_proc] + elem_costs[i]) > upper_limit) && (cur_proc < (nprocs_ - 1))) {
                      cur_proc++;
                    }
                    process_costs[cur_proc] += elem_costs[i];
                    elems_per_rank_[cur_proc]++; // gets v[i]
                  }
                  
                  rec_redistribute(elem_costs, process_costs, elems_per_rank_, nprocs_);
                  
                  /*
                   do {
                   
                   float * const maxp = std::max_element(process_costs, process_costs + nprocs_);
                   const int maxproc = maxp - process_costs;
                   int ind = std::accumulate(elems_per_rank_, elems_per_rank_ + maxproc, 0);
                   
                   if(maxproc && ((process_costs[maxproc-1] + elem_costs[ind]) < *maxp)) {
                   process_costs[maxproc-1] += elem_costs[ind];
                   elems_per_rank_[maxproc-1]++;
                   process_costs[maxproc] -= elem_costs[ind];
                   elems_per_rank_[maxproc]--;
                   continue;
                   }
                   
                   ind += elems_per_rank_[maxproc] - 1;
                   if((maxproc < (nprocs_-1)) && ((process_costs[maxproc+1] + elem_costs[ind]) < *maxp)) {
                   process_costs[maxproc+1] += elem_costs[ind];
                   elems_per_rank_[maxproc+1]++;
                   process_costs[maxproc] -= elem_costs[ind];
                   elems_per_rank_[maxproc]--;
                   continue;
                   }
                   
                   break;
                   
                   } while(1);
                   
                   // FOR DEBUGGING
                   
                   const int maxproc = std::max_element(process_costs, process_costs + nprocs_) - process_costs;
                   for (int j = 0; j < nprocs_; j++) {
                   if(j == maxproc) std::cerr << "======!! ";
                   std::cerr << "Rank " << j << " has " << elems_per_rank_[j] << " items\n";
                   int ind = std::accumulate(elems_per_rank_, elems_per_rank_ + j, 0);
                   if(elems_per_rank_[j]) {
                   std::cerr << "   0 : " << (unsigned long long)elem_costs[ind] << std::endl;
                   }
                   if(elems_per_rank_[j] > 1) {
                   ind += elems_per_rank_[j] - 1;
                   std::cerr << "   " << (elems_per_rank_[j]- 1) << " : " << (unsigned long long)elem_costs[ind] << std::endl;
                   }
                   std::cerr << " COST=" << (unsigned long long)process_costs[j] << std::endl;
                   }
                   */
                  
                  // Compute balance based on costs
                  const auto minmax_pair = std::minmax_element(process_costs, process_costs + nprocs_);
                  const float min_cost = *(minmax_pair.first);
                  float max_imbalance_found = (min_cost > 1e-3f) ? (*(minmax_pair.second) - min_cost) / min_cost : 1e3f;
                  //if(!rank_) std::cerr << "maximbalance=" << max_imbalance_found << " on " << nglobal_elems_ << std::endl;
                  isBalanced_ = (max_imbalance_found <= balanceParams_.maxImbalance_) ||
                  ( /*(min_cost > 0.0f) &&*/ heuristicRebalanceMustStop() );
                } else {
                  redistributeOnElems();
                  computeBalanceOnElems();
                }
              }
            }
            
            return isBalanced();
          }
          
          /// Applies default distribution based on elements, not costs
          void redistributeOnElems()
          {
            for (int i = 0; i < nprocs_; i++) {
              elems_per_rank_[i] = nLocalElems(i, false);
            }
          }

          /// Gets to the state in which the existing items have been partitioned into NCHILDREN
          ///children each one of them, but the data has not been yet redistributed to match next_elems_per_rank_
          int progress_DMdistr()
          {
            setNGlobalElems(nglobal_elems_ * NCHILDREN);

            for (int i = 0; i < nprocs_; i++) {
              elems_per_rank_[i] *= NCHILDREN;
              next_elems_per_rank_[i] = nLocalElems(i, false);
            }

            return next_elems_per_rank_[rank_];
          }

          void commit_progress_DMdistr() noexcept
          {
            std::copy(next_elems_per_rank_, next_elems_per_rank_ + nprocs_, elems_per_rank_);
            if(nglobal_elems_ >= nprocs_) {
              if(!(behavior_flags_ & Balance)) {
                isBalanced_ = true; // because nglobal_elems_>= nprocs_
              } else {
                computeBalanceOnElems();
              }
            }
          }

          int progress_DMred()
          {
            setNGlobalElems(nglobal_elems_ / NCHILDREN);
            
            for (int i = 0; i < nprocs_; i++) {
              next_elems_per_rank_[i] = elems_per_rank_[i];            // Current (balanced) distribution
              elems_per_rank_[i] = nLocalElems(i, false) * NCHILDREN;  // Distr. after redistribution but before reduction
            }
            
            return elems_per_rank_[rank_];
          }

          void commit_progress_DMred() noexcept
          {
            std::for_each(elems_per_rank_, elems_per_rank_ + nprocs_, [](int &n){ n /= NCHILDREN; });
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
              assert(elems_per_rank_[i] >= next_elems_per_rank_[i]);
              previous_excess += elems_per_rank_[i] - next_elems_per_rank_[i];
            }
            
            // skip other senders
            for(i++; elems_per_rank_[i] >= next_elems_per_rank_[i]; i++);
            
            // Skips receivers until it reaches the first receiver that needs items from this rank
            do {
              assert(elems_per_rank_[i] < next_elems_per_rank_[i]);
              int items_to_receive = next_elems_per_rank_[i] - elems_per_rank_[i];
              if(previous_excess >= items_to_receive) {
                previous_excess -= items_to_receive;
                i++;
              } else {
                return i;
              }
            } while(true);
            
            assert(false);
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
              assert(elems_per_rank_[i] < next_elems_per_rank_[i]);
              previously_sent += next_elems_per_rank_[i] - elems_per_rank_[i];
              i++;
            }
          
            
            // Skips senders until it reaches the first sender that will send items to this rank
            i = 0;
            do {
              assert(elems_per_rank_[i] >= next_elems_per_rank_[i]);
              int items_to_send = elems_per_rank_[i] - next_elems_per_rank_[i];
              if(previously_sent >= items_to_send) {
                previously_sent -= items_to_send;
                i++;
              } else {
                return i;
              }
            } while(true);
            
            assert(false);
            return -1;
          }

        };
  
	T* global_root;
	general_reference_wrapper<T>* local_root;

	Datatree_t datatree;
        Distribution distribution_;
        internal_mpi::ScatterHelper *scatter_;
        internal_mpi::GatherHelper *gather_;
  
        template<typename InfoTYPE, typename BodyTYPE>
        struct PreParallelHelper {
          
          DInfo &dinfo_;
          int * const ptr_startpos_vector_;
          const int level_;
          const InfoTYPE& info_;
          BodyTYPE& body_;
          
          PreParallelHelper(DInfo &dinfo, int *ptr_startpos_vector, int level, const InfoTYPE& info, BodyTYPE& body)
          : dinfo_(dinfo), ptr_startpos_vector_(ptr_startpos_vector), level_(level), info_(info), body_(body)
          {}
          
          void operator() (int i) const {
            T& data = dinfo_.datatree[level_][i];
            if(info_.is_base(data)) {
              ptr_startpos_vector_[i] = 1;
            } else {
              body_.pre(data);
              body_.pre_rec(data);
              ptr_startpos_vector_[i] = info_.num_children(data);
            }
          }
          
        };
  
        /// Helper class to parallelize a decomposition step in PrioritizeDM mode
        template<typename InfoTYPE, typename BodyTYPE>
        struct ParallelCreateTreeHelper {
          DInfo &dinfo_;
          const InfoTYPE& info_;
          BodyTYPE& body_;
          
          ParallelCreateTreeHelper(DInfo &dinfo, const InfoTYPE& info, BodyTYPE& body)
          : dinfo_(dinfo), info_(info), body_(body)
          {}
          
          void operator() (int i) const {
            Datatree_level_t* const lv = &(dinfo_.lastTreeLevel());
            Datatree_level_t& last_level = *(lv - 1);
            T& tmp_ref = last_level[i].get();
            if(!info_.is_base(tmp_ref)) {
              dinfo_.decompose<true>(tmp_ref, i * NCHILDREN, info_, body_, lv);
            } else { //base cases in the middle of the decomposition are currently supported by generating T()
              //printf("[%d] had a base at level %d\n", rank, this->nLevels());
              (*lv)[i * NCHILDREN]= std::move(last_level[i]); //this leaves a hole (empty()) in the tree
              // Notice that since they were generated by means of the emplace_back call,
              //the elements lv[i * NCHILDREN + 1] ... lv[i * NCHILDREN + (NCHILDREN-1)] are already T()
            }
          }
        };
  
        /// Helper class to parallelize a reduction step in PrioritizeDM mode
        template<typename BodyTYPE, typename ResultTYPE>
        struct ParallelReduceTreeHelper {
          DInfo &dinfo_;
          BodyTYPE& body_;
          const int behavior_flags_;
          Datatree_level_t& cur_level_;
          std::vector<ResultTYPE>& output_results_;
          std::vector<ResultTYPE>& local_results_;
          
          ParallelReduceTreeHelper(DInfo &dinfo, BodyTYPE& body, const Behavior& behavior, Datatree_level_t& cur_level, std::vector<ResultTYPE>& output_results, std::vector<ResultTYPE>& local_results)
          : dinfo_(dinfo), body_(body), behavior_flags_(behavior.flags()), cur_level_(cur_level), output_results_(output_results), local_results_(local_results)
          {}
          
          void operator() (int i) const {
            if (behavior_flags_ & DistributedInput) { // Problem: cannot distinguish from hole, with result in (*local_results)[i * NCHILDREN]!
              T tmp_value;
              output_results_[i] = body_.post(tmp_value, &(local_results_[i * NCHILDREN]));
            } else {
              general_reference_wrapper<T>& tmp = cur_level_[i];
              if (tmp.empty()) {
                //This happens when T was a base case at this level, leaving a hole in the tree and moving one level down.
                // (*local_results)[i * NCHILDREN] already contains the result for this item
                // We leave T at the bottom of the tree because there are no reductions at this level
                output_results_[i] = std::move(local_results_[i * NCHILDREN]);
              } else {
                output_results_[i] = body_.post(tmp.get(), &(local_results_[i * NCHILDREN]));
              }
            }
          }

        };
  
        template<bool RunPre, typename InfoTYPE, typename Body>
        void decompose(T& data, int startpos, const InfoTYPE& info, Body& body, Datatree_level_t* lv)
        {
          if (RunPre) {
            body.pre(data);
            body.pre_rec(data);
          }
          
          //general_reference_wrapper<T> * const ptr = &((*lv)[0]) + startpos;
          
          tbb::parallel_for(0, info.num_children(data), [&](int i) {
            (*lv)[startpos + i] = general_reference_wrapper<T>(info.child(i, data));
          });
        }
        
        template<bool RECV_RESULTS, typename BodyTYPE, typename ResultTYPE>
        inline std::enable_if_t<RECV_RESULTS> distributed_reduction(const int cur_level_depth, const int reduced_nlocal_elems, const Behavior& behavior, BodyTYPE& body, std::vector<ResultTYPE> * const local_results_ptr)
        { std::vector<ResultTYPE> tmp_results(reduced_nlocal_elems);
          
          Datatree_level_t& cur_level = this->treeLevel(cur_level_depth);
          std::vector<ResultTYPE>& local_results = *local_results_ptr;

          const ParallelReduceTreeHelper<BodyTYPE, ResultTYPE> tmp_parallel_reduce_tree_helper(*this, body, behavior, cur_level, tmp_results, local_results);
          
          if (reduced_nlocal_elems == 1) {
            tmp_parallel_reduce_tree_helper(0);
            local_results[0] = std::move(tmp_results[0]);
          } else {
            tbb::parallel_for(0, reduced_nlocal_elems, tmp_parallel_reduce_tree_helper);
            std::move(tmp_results.begin(), tmp_results.end(), local_results.begin());
          }
          
          if (!(behavior & SafeLoad)) {
            //This is done so that upcoming recvs on these positions find empty objects
            const auto cur_size = local_results.size();
            local_results.resize(reduced_nlocal_elems);
            local_results.resize(cur_size);
          }
        }
        
        template<bool RECV_RESULTS, typename BodyTYPE, typename ResultTYPE>
        inline std::enable_if_t<!RECV_RESULTS> distributed_reduction(const int cur_level, const int reduced_nlocal_elems, const Behavior& behavior, BodyTYPE& body, std::vector<ResultTYPE> * const local_results_ptr)
        { }
  
protected:

        /// Tries to fill in pointed vector with pre-saved buffers.
        /// \internal This virtual function allows to call the correct prepare_reception from a DInfo method
        ///           when the object is actually a BufferedDInfo
        /// \return Whether it was successful
        virtual bool prepare_reception(void *ptr, int required) {
          return false;
        }
  
        virtual void save_reception_buffer(void *ptr) {
        }
  
        /// \endcond
  
public:


        /// \brief Default constructor
	DInfo(const BalanceParams& ibalanceParams = BalanceParams()) :
		Arity<NCHILDREN>(),
		local_root(nullptr),
		global_root(nullptr),
                distribution_(ibalanceParams),
                scatter_(nullptr),
                gather_(nullptr)
        { }

        /// \brief Constructor that specifies the number of tasks to generate per process when using automatic partitioning
        /// \param n Number of tasks to generate per process when using automatic partitioning
        DInfo(int n, const BalanceParams& ibalanceParams = BalanceParams()) :
		Arity<NCHILDREN>(n),
		local_root(nullptr),
		global_root(nullptr),
                distribution_(ibalanceParams),
                scatter_(nullptr),
                gather_(nullptr)
        { }

        /// \brief Copy constructor
        DInfo(const DInfo<T, NCHILDREN>& other) :
		Arity<NCHILDREN>(other),
		local_root(nullptr),
		global_root(nullptr),
                distribution_(other.distribution_),
                scatter_(nullptr),
                gather_(nullptr)
        {
		//  The RecursionTask from parallel_recursion.h used to make a copy of the Info
		// object that broke these assertions. Now it has been turned into a const Info&
		assert(other.distribution_.rank_ == 0);
                assert(other.distribution_.nprocs_ == 0);
		assert(other.distribution_.nglobal_elems_ == 0);
		assert(other.local_root == 0);
		assert(other.global_root == 0);
		assert(other.datatree.empty());
	}

        /// \brief Set the parameters for the balacing heuristics
        void setBalanceParams(const BalanceParams& ibalanceParams) noexcept {
          distribution_.setBalanceParams(ibalanceParams);
        }
        
        /// Return the number of processors used by default
        static int defaultNProcs() {
          if (!DefaultNprocs) {
            MPI_Comm_size(MPI_COMM_WORLD, &DefaultNprocs);
          }
          return DefaultNprocs;
        }
        
        /// Destructor
        virtual ~DInfo() {
          reset();
        }

        /// \cond detailed
  
        /// \brief Returns a pointer to the initial (top) problem provided.
        ///        This can be the local input provided in the node or the global problem from the source if replicated
        /// \internal this method is currently never invoked
	T* globalRoot() const noexcept {
          return global_root;
	}

        /// \brief Returns a reference to the the i-th portion of the problem available in this object
        /// \param i Local index of the problem. Should be between 0 and \c nLocalElems(rank) - 1
        /// \deprecated The input problem should be accessed through a general_reference_wrapper for safety. Use rootRef
	T& root(int i = 0) const {
          return local_root[i];
	}

        /// \brief Returns the total/global number of subproblems at the decomposition level
        int nGlobalElems() const noexcept {
          return distribution_.nGlobalElems();
        }
  
        /// \brief Returns the number of subproblems assigned to the process \c rank
        /// \param rank Id of the process whose number of subproblems is to be computed
        int nLocalElems(int rank) const {
          return distribution_.nLocalElems(rank, true);
        }
  
        /// \brief Returns the number of subproblems assigned to this rank
        int nLocalElems() const noexcept {
          return distribution_.nLocalElems();
        }
  
        /// \brief Returns the number of MPI processes
        int nNodes() const noexcept {
          return distribution_.nNodes();
        }
  
        /// \brief Returns the order of the first subproblem assigned to a process at the level where the decomposition took place
        /// \param rank Id of the process of interest
        int firstGlobalElem(const int rank) const noexcept {
          return distribution_.firstGlobalElem(rank);
        }
  
        /// \brief Returns a reference to the the i-th portion of the problem available in this object
        /// \param i Local index of the problem. Should be between 0 and \c nLocalElems(rank)
        ///
        /// Users do not need to access this function. It is used internally by dparallel_recursion
	general_reference_wrapper<T> rootRef(int i = 0) {
          // The purpose of this get() is to generate a general_reference_wrapper
          // that is never a copy of local_root[i]. Instead it is a shallow pointer to its content
          return local_root[i].get();
	}

        /// \brief Indicates whether this object has a portion of the problem
	bool hasRoot() const noexcept {
          return local_root != nullptr;
	}

        /// \brief DistributedInput creation of the decomposition tree of the input problem
        ///
        /// The current implementation does not try to balance work; each rank just works on its data.
        template<typename InfoTYPE, typename Body>
        void distributed_input_create_tree(T& iglobal_root, const int inprocs, const int rank, const InfoTYPE& info, const Behavior& behavior, Body& body)
        {
          distribution_.init(rank, inprocs, behavior);
          datatree.emplace_back(1);
          global_root = nullptr;             //There is not an actual "global root"
          
          while (datatree.back().size() < inprocs) {
            const int next_level_size = (NCHILDREN > 0) ? ( NCHILDREN * this->lastTreeLevel().size() ) : inprocs;
            datatree.emplace_back(next_level_size);
          }
          
          distribution_.setNGlobalElems(static_cast<int> (this->lastTreeLevel().size()));
          
          assert(distribution_.nLocalElems(rank, false) == 1); //This requires nprocs_ to be a power of NCHILDREN, or NCHILDREN=0
          assert(distribution_.nLocalElems() == 1);

          local_root = new general_reference_wrapper<T>(iglobal_root);
        }

	/// \brief Master side or ReplicatedInput creation of the decomposition tree of the input problem
	template<typename InfoTYPE, typename Body>
        void create_tree(T& iglobal_root, const int inprocs, const int rank, const InfoTYPE& info, const Behavior& behavior, Body& body)
        {
          distribution_.init(rank, inprocs, behavior);

          int level = 0;
          
          global_root = &iglobal_root;
          datatree.emplace_back(1);
          datatree[0][0] = general_reference_wrapper<T>(iglobal_root);
          
          while ( !distribution_.redistribute(info, this->lastTreeLevel()) ) {

            if (!level) {
              if(info.is_base(*global_root)) {
                break;
              } else  {
                datatree.emplace_back(info.num_children(*global_root));
                decompose<true>(iglobal_root, 0, info, body, &(this->lastTreeLevel()));
              }
            } else {
              const int nelems_level = datatree[level].size();
              int startpos_vector[2 * nelems_level];
              int * const nelems_vector = startpos_vector + nelems_level;
              
              /*
              tbb::parallel_for(0, nelems_level, [&, ptr_startpos_vector](int i) {
                T& data = datatree[level][i];
                body.pre(data);
                body.pre_rec(data);
                nelems_vector[i] = info.num_children(data);
              });
              */
              tbb::parallel_for(0, nelems_level, PreParallelHelper<InfoTYPE,Body>(*this, nelems_vector, level, info, body));
              
              startpos_vector[0] = 0;
              std::partial_sum(nelems_vector, nelems_vector + (nelems_level - 1), startpos_vector + 1);
              
              const int next_level_size = startpos_vector[nelems_level - 1] + nelems_vector[nelems_level - 1];
              if (next_level_size == nelems_level) {
                break; // all the items would have a single child -> all of them would be base problems
              }
              datatree.emplace_back(next_level_size);
              Datatree_level_t* const lv = &(this->lastTreeLevel());

              /* The lambda breaks for g++ 4.7.2 in pluton
              tbb::parallel_for(0, nelems_level, [&, ptr_startpos_vector](int i) {
                decompose<false>(datatree[level][i], ptr_startpos_vector[i], info, body, lv);
              });
              */
              for (int i = 0; i < nelems_level; i++) {
                if (nelems_vector[i] == 1) { //base case
                  (*lv)[startpos_vector[i]] = std::move(datatree[level][i]); //leaves a hole (empty()) in the tree
                } else {
                  decompose<false>(datatree[level][i], startpos_vector[i], info, body, lv);
                }
              }
              
            }
            
            level++;
          }
          
          const int nlocal_elems = distribution_.setNGlobalElems(static_cast<int> (this->lastTreeLevel().size()));
          
          if (!(behavior & ReplicatedInput)) {
            if (distribution_.usesComputedCosts()) {
              MPI_Bcast((void *)distribution_.elems_per_rank_, inprocs, MPI_INT, 0, MPI_COMM_WORLD);
            } else {
              MPI_Bcast((void *)&distribution_.nglobal_elems_, 1, MPI_INT, 0, MPI_COMM_WORLD);
            }
          }

          local_root = (level && nlocal_elems) ? &(datatree.back()[distribution_.firstGlobalElem(rank)]) : &(datatree.back()[0]);

        }

	/// \brief  Slave side creation of the decomposition tree of the input problem
        void create_tree(T& iglobal_root, const int inprocs, const int rank, const Behavior& behavior)
        { int tmp;
          
          global_root = &iglobal_root;
          distribution_.init(rank, inprocs, behavior);

          if (distribution_.usesComputedCosts()) {
            MPI_Bcast((void *)distribution_.elems_per_rank_, inprocs, MPI_INT, 0, MPI_COMM_WORLD);
            tmp = std::accumulate(distribution_.elems_per_rank_, distribution_.elems_per_rank_ + inprocs, 0);
          } else {
            MPI_Bcast((void *)&tmp, 1, MPI_INT, 0, MPI_COMM_WORLD);
          }
          
          const int nlocal_elems = distribution_.setNGlobalElems(tmp);

          local_root = nlocal_elems ? new general_reference_wrapper<T>[nlocal_elems] : 0; // T must be default-constructible
	}

        template<typename T2>
        std::enable_if_t<! has_member_function_gather_scatter< MakeClass_t<T2>, void, boost::mpl::vector<internal_mpi::ScatterHelper&> >::value> scatter(const int rank)
        {
          static_assert(std::is_same<T, T2>::value, "T must be == T2");
          assert(false);
        }
  
        template<typename T2>
        std::enable_if_t<has_member_function_gather_scatter< MakeClass_t<T2>, void, boost::mpl::vector<internal_mpi::ScatterHelper&> >::value> scatter(const int rank)
        {
          static_assert(std::is_same<T, T2>::value, "T must be == T2");
          
          if (scatter_ == nullptr) {
            
            const int nglobal_elems = nGlobalElems();
            const int nprocs = nNodes();
            
            scatter_ = new internal_mpi::ScatterHelper(nprocs, rank, nglobal_elems);
            
            for (int i = 0; i < nprocs; i++) {
              scatter_->set_items_proc(i, distribution_.nLocalElems(i, true));
            }
            
            if (!rank) {
              for (int i = 0; i < nglobal_elems; i++) {
                lastTreeLevelElement(i).gather_scatter(*scatter_); scatter_->advance_item();
              }
            }
            
            scatter_->scatter_info();
          }
          
          scatter_->scatter_data();
          
          if (rank) {
            for (int i = 0; i < distribution_.nLocalElems(); i++) {
              root(i).gather_scatter(*scatter_); scatter_->advance_item();
            }
          }
          
        }
  
        void gather_input()
        {
          assert(scatter_ != nullptr);
          scatter_->gather_data();
        }

        template<typename Return>
        std::enable_if_t<! has_member_function_gather_scatter< MakeClass_t<Return>, void, boost::mpl::vector<internal_mpi::ScatterHelper&> >::value> gather_output(const int rank, std::vector<Return>& local_results, const Behavior& behavior, Return * const ret_ptr)
        {
          assert(false);
        }
  
        template<typename Return>
        std::enable_if_t<has_member_function_gather_scatter< MakeClass_t<Return>, void, boost::mpl::vector<internal_mpi::ScatterHelper&> >::value> gather_output(const int rank, std::vector<Return>& local_results, const Behavior& behavior, Return * const ret_ptr)
        {
          // currently also computed in ImplRun::enableResultAllgatherv()
          const bool replicate = behavior.enableResultAllgatherv();

          bool first_use = (gather_ == nullptr);

          if(!first_use && !(behavior & ReusableGather)) {
            delete gather_;
            gather_ = nullptr;
            first_use = true;
          }

          const int nglobal_elems = nGlobalElems();

          if (first_use) {

            const int nprocs = nNodes();
            
            gather_ = new internal_mpi::GatherHelper(nprocs, rank, nglobal_elems);
            
            for (int i = 0; i < nprocs; i++) {
              gather_->set_items_proc(i, distribution_.nLocalElems(i, true));
            }
            
            if (ret_ptr != nullptr) {
              gather_->get_resultBuffer(*ret_ptr);
            }
            
            for (int i = 0; i < nLocalElems(); i++) {
              local_results[i].gather_scatter(*gather_); gather_->advance_item();
            }
            
            gather_->gather_output(replicate);

          } else {
            gather_->quick_gather_output(replicate);
          }

          if (!rank || replicate) {
            if (rank) {
              local_results.resize(nglobal_elems);
            }
            for (int i = 0; i < nglobal_elems; i++) {
              local_results[i].gather_scatter(*gather_); gather_->advance_item();
            }
          }
        }
  
        /// \brief Creation of the decomposition tree of the input problem split among nodes ASAP
        template<typename InfoTYPE, typename Body>
        void distributed_create_tree(T& iglobal_root, const int inprocs, const int rank, const InfoTYPE& info, const Behavior& behavior, Body& body)
        {
          assert( NCHILDREN != 0 ); // "PrioritizeDM is only supported for problems with known arity"
          assert( !(behavior & DistributedInput) );
          assert( !(behavior & ReplicatedInput) );
          
          distribution_.init_distr(rank, inprocs, behavior);
          
          if (rank == 0) {
            global_root = &iglobal_root;
            datatree.emplace_back(1);
            datatree[0][0] = general_reference_wrapper<T>(iglobal_root);
          } else {
            global_root = nullptr;
          }
          
          while ( !distribution_.isBalanced() ) {
            
            const int pre_nlocal_elems = nLocalElems();  //now (before re-partitioning)
            const int next_nlocal_elems = distribution_.progress_DMdistr(); //after redistributing
            const int post_nlocal_elems = nLocalElems();     //after re-partitioning and before redistributing
            
            assert((!pre_nlocal_elems && datatree.empty()) || (pre_nlocal_elems <= this->lastTreeLevel().size()));

            if (pre_nlocal_elems) {
              datatree.emplace_back(std::max(post_nlocal_elems, next_nlocal_elems));

              const ParallelCreateTreeHelper<InfoTYPE,Body> tmp_parallel_create_tree_helper(*this, info, body);
              if(pre_nlocal_elems == 1) {
                tmp_parallel_create_tree_helper(0);
              } else {
                tbb::parallel_for(0, pre_nlocal_elems, tmp_parallel_create_tree_helper);
              }
            } else {
              if (next_nlocal_elems) { // Create space to receive the data
                datatree.emplace_back(next_nlocal_elems);
              }
            }
            
            if (next_nlocal_elems < post_nlocal_elems) {
              // send excess items
              int previous_excess;
              int receiver = distribution_.destination_first_external_problem(previous_excess);
              Datatree_level_t& last_level = this->lastTreeLevel();
              for (int i = next_nlocal_elems; i < post_nlocal_elems; i++) {
                internal_mpi::send(receiver, 0, last_level[i].get());
                previous_excess++;
                if (distribution_.finished_receives(receiver, previous_excess)) {
                  previous_excess = 0;
                  receiver++;
                }
              }
              if (!(behavior & GatherInput)) { // eliminate items in the tree that will not be used
                //last_level.resize(next_nlocal_elems);  //Requires operator =, which can be deleted
                Datatree_level_t tmp(next_nlocal_elems);
                std::move(last_level.data(), last_level.data() + next_nlocal_elems, tmp.data());
                last_level = std::move(tmp);
              }
            } else if (next_nlocal_elems > post_nlocal_elems) {
              // receive items
              int previously_sent;
              int sender = distribution_.source_first_external_problem(previously_sent);
              Datatree_level_t& last_level = this->lastTreeLevel();
              for (int i = post_nlocal_elems; i < next_nlocal_elems; i++) {
                internal_mpi::recv(sender, 0, last_level[i].get());
                previously_sent++;
                if (distribution_.finished_sends(sender, previously_sent)) {
                  previously_sent = 0;
                  sender++;
                }
              }
            }
            
            distribution_.commit_progress_DMdistr();
          }

          assert(nLocalElems() == distribution_.nLocalElems(rank, false));
          
          local_root = &(datatree.back()[0]);

        }
  
        /// \internal the weirdo NCHILDREN * sizeof(Result) instead of NCHILDREN != 0 is because enable_if
        ///           does not kick in unless one of the direct template parameters is used in the expression. See
        ///           http://stackoverflow.com/questions/13964447/why-compile-error-with-enable-if
        template<bool RECV_RESULTS, typename Body, typename Result>
        typename std::enable_if_t<!(NCHILDREN * sizeof(Result))>
        distributed_reduce_tree(const int rank, const Behavior& behavior, Body& body, std::vector<Result> * const local_results) {
          assert(false); //distributed_reduce_tree unsupported for NCHILDREN =0 
        }

        /// \brief Creation of the decomposition tree of the input problem split among nodes ASAP
        template<bool RECV_RESULTS, typename Body, typename Result>
        std::enable_if_t<(NCHILDREN * sizeof(Result)) != 0>
        distributed_reduce_tree(const int rank, const Behavior& behavior, Body& body, std::vector<Result> * const local_results)
        {
          static_assert( NCHILDREN != 0, "PrioritizeDM is only supported for problems with known arity");
          
          assert ( (RECV_RESULTS && (local_results != nullptr)) || (!RECV_RESULTS && (local_results == nullptr)) );
          
          const bool recv_inputs = !RECV_RESULTS || (behavior & GatherInput);
          bool needs_saving_buffer = RECV_RESULTS;
          Distribution reduce_tree_distr(distribution_);
          reduce_tree_distr.prepareDistributedReduceTree(behavior);
          
          int cur_level_depth = this->nLevels() - 1;

          while(reduce_tree_distr.nGlobalElems() > 1) {
            const int pre_nlocal_elems = reduce_tree_distr.nLocalElems();     //now
            const int next_nlocal_elems = reduce_tree_distr.progress_DMred(); //after redistributing and before reducing
            
            Datatree_level_t& cur_level = this->treeLevel(cur_level_depth);
            
            if (next_nlocal_elems > pre_nlocal_elems) {
              
              needs_saving_buffer = needs_saving_buffer && !prepare_reception(*local_results, next_nlocal_elems);
              
              // receive excess items
              int previous_excess;
              int receiver = reduce_tree_distr.destination_first_external_problem(previous_excess);
              for (int i = pre_nlocal_elems; i < next_nlocal_elems; i++) {
                if(RECV_RESULTS) {
                  internal_mpi::recv(receiver, 0, (*local_results)[i]);
                }
                if(recv_inputs) {
                  internal_mpi::recv(receiver, 0, cur_level[i].get());
                }
                previous_excess++;
                if (reduce_tree_distr.finished_receives(receiver, previous_excess)) {
                  previous_excess = 0;
                  receiver++;
                }
              }

              if (needs_saving_buffer) {
                save_reception_buffer(*local_results);
                needs_saving_buffer = false;
              }

            } else if (next_nlocal_elems < pre_nlocal_elems) {
              needs_saving_buffer = false;
              
              // send excess items
              int previously_sent;
              int sender = reduce_tree_distr.source_first_external_problem(previously_sent);
              for (int i = next_nlocal_elems;  i < pre_nlocal_elems; i++) {
                if(RECV_RESULTS) {
                  internal_mpi::send(sender, 0, (*local_results)[i]);
                }
                if(recv_inputs) {
                  // Remember : There could be a hole...
                  Datatree_level_t *ptr_level = &cur_level;
                  int level_pos = i;
                  while ((*ptr_level)[level_pos].empty()) {
                    level_pos *= NCHILDREN;
                    ptr_level++;
                  }
                  internal_mpi::send(sender, 0, (*ptr_level)[level_pos].get());
                }
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
              distributed_reduction<RECV_RESULTS>(cur_level_depth, reduce_tree_distr.nLocalElems(), behavior, body, local_results);
            }
            
          }
          
        }

        /// \brief Returns the depth of the distributed decomposition level only if this process participated in the decomposition
	int nLevels() const {
		return datatree.size();
	}

        /// \brief Returns the i-th level of the decomposition tree. Meant for internal use in dparallel_recursion
        /// \param i Level of decomposition requested. Should be between 0 and nLevels()-1
	Datatree_level_t& treeLevel(int i) {
		return datatree[i];
	}
  
        /// \brief Returns the last level of distributed decomposition tree. Meant for internal use in dparallel_recursion
	const Datatree_level_t& lastTreeLevel() const {
		return datatree.back();
	}
  
        /// \brief Returns the last level of distributed decomposition tree. Meant for internal use in dparallel_recursion
	Datatree_level_t& lastTreeLevel() {
		return datatree.back();
	}

        /// \brief Returns the i-th element of the last level of distributed decomposition tree.
        ///        Meant for internal use in dparallel_recursion
	const T& lastTreeLevelElement(int i) const {
		return datatree.back()[i].get();
	}

        /// \brief Returns the i-th element of the last level of distributed decomposition tree.
        ///        Meant for internal use in dparallel_recursion
	T& lastTreeLevelElement(int i) {
		return datatree.back()[i].get();
	}

        /// Resets the object to its initial state so it can store another problem decomposition
	void reset() {
          if ( (local_root != nullptr) && datatree.empty() ) {
            delete [] local_root;
          }
          
          distribution_.reset();
          
          if ( scatter_ != nullptr ) {
            delete scatter_;
            scatter_ = nullptr;
          }
          
          if ( gather_ != nullptr ) {
            delete gather_;
            gather_ = nullptr;
          }
          
          local_root = nullptr;
          global_root = nullptr;

          datatree.clear();
	}
  
        template<typename Result>
        bool prepare_reception(std::vector<Result>& local_results, int required) {
          const bool ret = prepare_reception(static_cast<void *>(&local_results), required);
          if (!ret) {
            local_results.resize(required);
          }
          return ret;
        }

        template<typename Result>
        void save_reception_buffer(std::vector<Result>& local_results) {
          save_reception_buffer(static_cast<void *>(&local_results));
        }

        /// \endcond
};

template<typename T, int NCHILDREN>
int DInfo<T, NCHILDREN>::DefaultNprocs = 0;

} // namespace dpr

#endif /* DPR_DINFO_H_ */
