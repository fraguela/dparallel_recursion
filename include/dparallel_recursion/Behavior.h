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
/// \file     Behavior.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_BEHAVIOR_H_
#define DPR_BEHAVIOR_H_

namespace dpr {

constexpr int DefaultBehavior =   0,
  GatherInput =       0x01, ///< return the input to the master
  ReplicatedInput =   0x02, ///< the input data is located in all the nodes
  ReplicateInput =    0x04, ///< replicate starting input from the root node
  ReplicateOutput =   0x08, ///< replicate result
  DistributedInput =  0x10, ///< the input is distributed, each node receiving its portion
  DistributedOutput = 0x20, ///< do not gather/reduce results in root
  PrioritizeDM =      0x40, ///< distribute/gather among distributed memory nodes ASAP
  Scatter =           0x80, ///< Can use MPI_Scatterv with scatter protocol to divide work
  Gather =           0x100, ///< Can use MPI_Gatherv to gather the results
  Balance  =         0x200, ///< balance load per node within a maxImbalance_ deviation
  ReusableGather =   0x400, ///< the information for gathering the result can be reused
  UseCost  =         0x800, ///< Use user-defined costs for the balancing
  SafeLoad =        0x1000, ///< it is safe to load/deserialize a result on a non-empty object
  KeepInput =       0x2000  ///< do not deallocate the input after the processing
  //GreedyParallel =  0x4000, ///< Process subproblems in parallel (Breadth-first rather than Depth-first)
  ;

/// Controls the behabior of the dparallel_recursion template
class Behavior {

  int flags_;  ///< Bitmask that controls the algorithm
  
public:
  
  Behavior(const int flags) noexcept :
  flags_(flags)
  {}

  Behavior(const Behavior&) = default;
  
  int operator| (int other_flags) const noexcept { return flags_ | other_flags; }

  int operator& (int other_flags) const noexcept { return flags_ & other_flags; }

  /// Only modifies the flags
  Behavior& operator|=(int some_flags) noexcept {
    flags_ |= some_flags;
    return *this;
  }

  /// Only modifies the flags
  Behavior& operator&=(int some_flags) noexcept {
    flags_ &= some_flags;
    return *this;
  }
  
  /// Assignment from an integer only modifies the flags
  Behavior& operator=(int some_flags) noexcept {
    flags_ = some_flags;
    return *this;
  }
  
  int flags() const noexcept {
    return flags_;
  }
  
  bool enableResultAllgatherv() const noexcept {
    return (flags_ & Gather) && (flags_ & ReplicateOutput) && (flags_ & ReplicatedInput) && !(flags_ & GatherInput);
  }
};


} // namespace dpr

#endif /* DPR_BEHAVIOR_H_ */

