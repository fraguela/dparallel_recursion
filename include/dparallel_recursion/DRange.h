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
/// \file     DRange.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_DRANGE_H_
#define DPR_DRANGE_H_

#include "dparallel_recursion/Range.h"
#include "dparallel_recursion/BufferedDInfo.h"

// This is because of the non-macro template functions dpfor and dpfor_reduce
#ifndef DPR_PARALLEL_RECURSION_MPI_H_
#include "dparallel_recursion/dparallel_recursion.h"
#endif

namespace dpr {
  
/* Problem :static member from Arity repeated in both base classes
struct InclusiveRangeDInfo : public DInfo<Range, 0>, InclusiveRangeInfo {
  
  const int nprocs_;
  int procSize_, procPartPoint_;
  
  /// \pre r.inclusiveSize() >= ntasks
  InclusiveRangeDInfo(const Range& r, int ntasks) :
  InclusiveRangeInfo(r, ntasks), nprocs_(0), procSize_(0), procPartPoint_(0)
  { }
  
  /// \pre r.inclusiveSize() >= (nprocs * ntasks)
  InclusiveRangeDInfo(const Range& r, int nprocs, int ntasks) :
  InclusiveRangeInfo(r, ntasks, false), nprocs_(nprocs < 2 ? 0 : nprocs)
  {
    int subrange;
    
    if(nprocs_) {
      procSize_ = r.inclusiveMaxChunk(nprocs);
      procPartPoint_ = r.inclusiveSize() % nprocs;
      subrange = procSize_;
    } else {
      procSize_ = procPartPoint_ = 0;
      subrange = initRange_;
    }
    
    Range tmp {0, subrange}; //notice this is a exclusive range [0, subrange).
    // (*) and processors >= procPartPoint_ can get procSize_-1 iterations
    baseSize_ = tmp.exclusiveMaxChunk(ntasks_);
    taskPartPoint_ = subrange % ntasks_;
  }
  
  int num_children(Range& r) const {
    r.nchildren = (nprocs_ && (r.inclusiveSize() == initRange_)) ? nprocs_ : ntasks_;
    return r.nchildren;
  }

  Range child(int i, const Range& r) const {
    return (nprocs_ && (r.inclusiveSize() == initRange_)) ? sub_child(i, r, procSize_, procPartPoint_) : sub_child(i, r, baseSize_, taskPartPoint_);
  }
  
};
  
struct ExclusiveRangeDInfo : public DInfo<Range, 0>, ExclusiveRangeInfo {
  
  const int nprocs_;
  int procSize_, procPartPoint_;
  
  /// \pre r.inclusiveSize() >= ntasks
  ExclusiveRangeDInfo(const Range& r, int ntasks) :
  ExclusiveRangeInfo(r, ntasks), nprocs_(0), procSize_(0), procPartPoint_(0)
  { }
  
  /// \pre r.inclusiveSize() >= (nprocs * ntasks)
  ExclusiveRangeDInfo(const Range& r, int nprocs, int ntasks) :
  ExclusiveRangeInfo(r, ntasks, false), nprocs_(nprocs < 2 ? 0 : nprocs)
  {
    int subrange;
    
    if(nprocs_) {
      procSize_ = r.exclusiveMaxChunk(nprocs);
      procPartPoint_ = r.exclusiveSize() % nprocs;
      subrange = procSize_;
    } else {
      procSize_ = procPartPoint_ = 0;
      subrange = initRange_;
    }
    
    Range tmp {0, subrange}; //notice this is a exclusive range [0, subrange).
    // (*) and processors >= procPartPoint_ can get procSize_-1 iterations
    baseSize_ = tmp.exclusiveMaxChunk(ntasks_);
    taskPartPoint_ = subrange % ntasks_;
  }
  
  int num_children(Range& r) const {
    r.nchildren = (nprocs_ && (r.exclusiveSize() == initRange_)) ? nprocs_ : ntasks_;
    return r.nchildren;
  }
  
  Range child(int i, const Range& r) const {
    return (nprocs_ && (r.exclusiveSize() == initRange_)) ? sub_child(i, r, procSize_, procPartPoint_) : sub_child(i, r, baseSize_, taskPartPoint_);
  }
  
};
*/

/// DInfo object for inclusive Ranges suited for dparallel_recursion invocations
struct InclusiveRangeDInfo : public DInfo<Range, 0> {
  
  const int initRange_, nprocs_, ntasks_;
  int procSize_, procPartPoint_;
  int baseSize_, taskPartPoint_;
  
  /// Build Info object for a Range \c range to be split in \c ntasks tasks in a single node
  /// @param r      range to be split
  /// @param ntasks number of parallel tasks
  /// @pre r.inclusiveSize() >= ntasks
  InclusiveRangeDInfo(const Range& r, int ntasks) :
  initRange_(r.inclusiveSize()), nprocs_(0), ntasks_(ntasks),
  procSize_(0), procPartPoint_(0),
  baseSize_(r.inclusiveMaxChunk(ntasks)), taskPartPoint_(r.inclusiveSize() % ntasks)
  { }
  
  /// Build Info object for a Range \c range to be split distributedly
  /// @param r      range to be split
  /// @param nprocs number of processes
  /// @param ntasks number of parallel tasks in each process
  /// \pre r.inclusiveSize() >= (nprocs * ntasks)
  InclusiveRangeDInfo(const Range& r, int nprocs, int ntasks) :
  initRange_(r.inclusiveSize()), nprocs_(nprocs < 2 ? 0 : nprocs), ntasks_(ntasks)
  {
    int subrange;
    
    if(nprocs_) {
      procSize_ = r.inclusiveMaxChunk(nprocs);
      procPartPoint_ = r.inclusiveSize() % nprocs;
      subrange = procSize_;
    } else {
      procSize_ = procPartPoint_ = 0;
      subrange = initRange_;
    }
    
    Range tmp {0, subrange}; //notice this is a exclusive range [0, subrange).
    // (*) and processors >= procPartPoint_ can get procSize_-1 iterations
    baseSize_ = tmp.exclusiveMaxChunk(ntasks_);
    taskPartPoint_ = subrange % ntasks_;
  }
  
  /// Number of children in which a non-base Range \c r can be subdivided
  int num_children(Range& r) const {
    r.nchildren = (nprocs_ && (r.inclusiveSize() == initRange_)) ? nprocs_ : ntasks_;
    return r.nchildren;
  }
  
  /// Returns whether the input Range \c r is a base case
  bool is_base(const Range& r) const {
    return r.inclusiveSize() <= baseSize_;
  }
  
  /// Returns the i-th child of Range \c r
  Range child(int i, const Range& r) const {
    return (nprocs_ && (r.inclusiveSize() == initRange_)) ?
            InclusiveRangeInfo::sub_child(i, r, procSize_, procPartPoint_) :
            InclusiveRangeInfo::sub_child(i, r, baseSize_, taskPartPoint_);
  }
  
};

/// DInfo object for exclusive Ranges suited for dparallel_recursion invocations
struct ExclusiveRangeDInfo : public DInfo<Range, 0> {
  
  const int initRange_, nprocs_, ntasks_;
  int procSize_, procPartPoint_;
  int baseSize_, taskPartPoint_;
  
  /// Build Info object for a Range \c range to be split in \c ntasks tasks in a single node
  /// @param r      range to be split
  /// @param ntasks number of parallel tasks
  /// @pre r.exclusiveSize() >= ntasks
  ExclusiveRangeDInfo(const Range& r, int ntasks) :
  initRange_(r.exclusiveSize()), nprocs_(0), ntasks_(ntasks),
  procSize_(0), procPartPoint_(0),
  baseSize_(r.exclusiveMaxChunk(ntasks)), taskPartPoint_(r.exclusiveSize() % ntasks)
  { }
  
  /// Build Info object for a Range \c range to be split distributedly
  /// @param r      range to be split
  /// @param nprocs number of processes
  /// @param ntasks number of parallel tasks in each process
  /// @pre r.enclusiveSize() >= (nprocs * ntasks)
  ExclusiveRangeDInfo(const Range& r, int nprocs, int ntasks) :
  initRange_(r.exclusiveSize()), nprocs_(nprocs < 2 ? 0 : nprocs), ntasks_(ntasks)
  {
    int subrange;
    
    if(nprocs_) {
      procSize_ = r.exclusiveMaxChunk(nprocs);
      procPartPoint_ = r.exclusiveSize() % nprocs;
      subrange = procSize_;
    } else {
      procSize_ = procPartPoint_ = 0;
      subrange = initRange_;
    }
    
    Range tmp {0, subrange}; //notice this is a exclusive range [0, subrange).
    // (*) and processors >= procPartPoint_ can get procSize_-1 iterations
    baseSize_ = tmp.exclusiveMaxChunk(ntasks_);
    taskPartPoint_ = subrange % ntasks_;
  }
  
  /// Number of children in which a non-base Range \c r can be subdivided
  int num_children(Range& r) const {
    r.nchildren = (nprocs_ && (r.exclusiveSize() == initRange_)) ? nprocs_ : ntasks_;
    return r.nchildren;
  }
  
  /// Returns whether the input Range \c r is a base case
  bool is_base(const Range& r) const {
    return r.exclusiveSize() <= baseSize_;
  }
  
  /// Returns the i-th child of Range \c r
  Range child(int i, const Range& r) const {
    return (nprocs_ && (r.exclusiveSize() == initRange_)) ?
          ExclusiveRangeInfo::sub_child(i, r, procSize_, procPartPoint_) :
          ExclusiveRangeInfo::sub_child(i, r, baseSize_, taskPartPoint_);
  }
  
};

  
template<typename T>
struct RangedProblem {
  
  T data_;
  Range r_;
  
  T& data()             { return data_; }
  const T& data() const { return data_; }
  
  Range& range()             { return r_; }
  const Range& range() const { return r_; }

  template<class Archive>
  void serialize(Archive& ar,  unsigned int file_version)
  {
    ar & data_ & r_;
  }
  
  //template<T DIVIDER_F(const T&, const Range&), bool RELATIVE_RANGES>
  /// Generic DInfo for a RangedProblem
  template<bool RELATIVE_RANGES, typename F>
  struct DInfo : public dpr::DInfo<RangedProblem<T>, 0> {

    ExclusiveRangeDInfo rinfo_;
    F f_;
    
    DInfo(RangedProblem<T> &p, const F& f, int nprocs, int ntasks) :
    rinfo_(p.r_, nprocs, ntasks), f_(f)
    {}
    
    bool is_base(const RangedProblem<T>& p) const {
      return rinfo_.is_base(p.r_);
    }
    
    int num_children(RangedProblem<T>& p) const {
      return rinfo_.num_children(p.r_);
    }
    
    RangedProblem<T> child(int i, const RangedProblem<T>& p) const {

      Range tmp = rinfo_.child(i, p.r_);
      if(RELATIVE_RANGES) {
        tmp = tmp << p.r_.start;
      }
      
      //return RangedProblem<T> { DIVIDER_F(p.data_, tmp), tmp };
      return RangedProblem<T> { f_(p.data_, tmp), tmp };
    }
    
  };
  
  /*
  template<T DIVIDER_F(const T&, const Range&), bool RELATIVE_RANGES>
  inline const DInfo<DIVIDER_F, RELATIVE_RANGES> makeDInfo(int nprocs, int ntasks)
  {
    return DInfo<DIVIDER_F, RELATIVE_RANGES>(*this, nprocs, ntasks);
  }
  */
    
  template<bool RELATIVE_RANGES, typename F>
  inline const DInfo<RELATIVE_RANGES, F> makeDInfo(const F& f, int nprocs, int ntasks)
  {
    return DInfo<RELATIVE_RANGES, F>(*this, f, nprocs, ntasks);
  }
  
  template<bool RELATIVE_RANGES>
  inline auto makeDInfo(T (T::* f) (const Range&) const, int nprocs, int ntasks) -> const DInfo<RELATIVE_RANGES, decltype(std::mem_fn(f))>
  {
    return DInfo<RELATIVE_RANGES, decltype(std::mem_fn(f))>(*this, std::mem_fn(f), nprocs, ntasks);
  }
  
  template<bool RELATIVE_RANGES>
  inline auto makeDInfo(T (T::* f) (const Range&), int nprocs, int ntasks) -> const DInfo<RELATIVE_RANGES, decltype(std::mem_fn(f))>
  {
    return DInfo<RELATIVE_RANGES, decltype(std::mem_fn(f))>(*this, std::mem_fn(f), nprocs, ntasks);
  }
  
  /// Buffered version
  template<bool RELATIVE_RANGES, typename F, typename Result>
  struct BufferedDInfo : public dpr::BufferedDInfo<RangedProblem<T>, 0, Result> {
    
    ExclusiveRangeDInfo rinfo_;
    F f_;
    
    BufferedDInfo(RangedProblem<T> &p, const F& f, int nprocs, int ntasks, Result * const dest) :
    dpr::BufferedDInfo<RangedProblem<T>, 0, Result>::dest_(dest), rinfo_(p.r_, nprocs, ntasks), f_(f)
    { }
    
    bool is_base(const RangedProblem<T>& p) const {
      return rinfo_.is_base(p.r_);
    }
    
    int num_children(RangedProblem<T>& p) const {
      return rinfo_.num_children(p.r_);
    }
    
    RangedProblem<T> child(int i, const RangedProblem<T>& p) const {
      
      Range tmp = rinfo_.child(i, p.r_);
      if(RELATIVE_RANGES) {
        tmp = tmp << p.r_.start;
      }
      
      //return RangedProblem<T> { DIVIDER_F(p.data_, tmp), tmp };
      return RangedProblem<T> { f_(p.data_, tmp), tmp };
    }
    
  };
  
  template<bool RELATIVE_RANGES, typename F, typename Result>
  inline const BufferedDInfo<RELATIVE_RANGES, F, Result> makeBufferedDInfo(const F& f, int nprocs, int ntasks, Result * const dest = nullptr)
  {
    return BufferedDInfo<RELATIVE_RANGES, F, Result>(*this, f, nprocs, ntasks, dest);
  }
  
  template<bool RELATIVE_RANGES, typename Result>
  inline auto makeBufferedDInfo(T (T::* f) (const Range&) const, int nprocs, int ntasks, Result * const dest = nullptr) -> const BufferedDInfo<RELATIVE_RANGES, decltype(std::mem_fn(f)), Result>
  {
    return BufferedDInfo<RELATIVE_RANGES, decltype(std::mem_fn(f)), Result>(*this, std::mem_fn(f), nprocs, ntasks, dest);
  }
  
  template<bool RELATIVE_RANGES, typename Result>
  inline auto makeBufferedDInfo(T (T::* f) (const Range&), int nprocs, int ntasks, Result * const dest = nullptr) -> const BufferedDInfo<RELATIVE_RANGES, decltype(std::mem_fn(f)), Result>
  {
    return BufferedDInfo<RELATIVE_RANGES, decltype(std::mem_fn(f)), Result>(*this, std::mem_fn(f), nprocs, ntasks, dest);
  }

  /// Shared memory version
  template<bool RELATIVE_RANGES, typename F>
  struct Info {
    
    ExclusiveRangeInfo rinfo_;
    F f_;
    
    Info(RangedProblem<T> &p, const F& f, int ntasks) :
    rinfo_(p.r_, ntasks), f_(f)
    {}
    
    bool is_base(const RangedProblem<T>& p) const {
      return rinfo_.is_base(p.r_);
    }
    
    int num_children(RangedProblem<T>& p) const {
      return rinfo_.num_children(p.r_);
    }
    
    RangedProblem<T> child(int i, const RangedProblem<T>& p) const {
      
      Range tmp = rinfo_.child(i, p.r_);
      if(RELATIVE_RANGES) {
        tmp = tmp << p.r_.start;
      }
      
      //return RangedProblem<T> { DIVIDER_F(p.data_, tmp), tmp };
      return RangedProblem<T> { f_(p.data_, tmp), tmp };
    }
  };
  
  template<bool RELATIVE_RANGES, typename F>
  inline const Info<RELATIVE_RANGES, F> makeInfo(const F& f, int ntasks)
  {
    return Info<RELATIVE_RANGES, F>(*this, f, ntasks);
  }
  
  template<bool RELATIVE_RANGES>
  inline auto makeInfo(T (T::* f) (const Range&) const, int ntasks) -> const Info<RELATIVE_RANGES, decltype(std::mem_fn(f))>
  {
    return Info<RELATIVE_RANGES, decltype(std::mem_fn(f))>(*this, std::mem_fn(f), ntasks);
  }
  
  template<bool RELATIVE_RANGES>
  inline auto makeInfo(T (T::* f) (const Range&), int ntasks) -> const Info<RELATIVE_RANGES, decltype(std::mem_fn(f))>
  {
    return Info<RELATIVE_RANGES, decltype(std::mem_fn(f))>(*this, std::mem_fn(f), ntasks);
  }

};

template<typename T>
inline const RangedProblem<T> make_RangedProblem(const T& data, const Range& r) {
  return RangedProblem<T>{ data, r };
}

/// Template function to parallelize a for loop on top of dparallel_recursion
///
/// @param init     starting value of the loop
/// @param end      limit of the loop
/// @param ntasks   number of tasks in which the loop must be split in each process
/// @param fn       function that receives the iteration number and executes the associated iteration
/// @param behavior (optional) behavior flags for dparallel_recursion
template<typename T, typename F>
void dpfor(const T& init, const T& end, int ntasks, const F& fn, const Behavior behavior = ReplicatedInput | DistributedOutput) {
  dpr::Range pr_tmp_range {init, end};
  auto f = typename std::decay<F>::type(fn);
  auto pr_tmp_func = [f](T var, const T& pr_pfor_end) {
    while (var < pr_pfor_end) {
      f(var);
      ++var;
    }
  };
  dpr::dparallel_recursion<void>(pr_tmp_range,
                                 dpr::ExclusiveRangeDInfo(pr_tmp_range, dpr::ExclusiveRangeDInfo::defaultNProcs(), ntasks),
                                 dpr::internal::make_generic_pr_for_body(pr_tmp_func),
                                 dpr::partitioner::simple(),
                                 behavior);
}

/// Template function to parallelize a for loop on top of dparallel_recursion
///
/// @param init    starting value of the loop
/// @param end     limit of the loop
/// @param ntasks  number of tasks in which the loop must be split in each process
/// @param freducer object such that <tt>a=freducer(a,b)</tt> reduces \c a and \c b into \c a
/// @param fn       function that receives the iteration number and executes the associated iteration, returning the value to reduce
/// @param behavior (optional) behavior flags for dparallel_recursion
template<typename T, typename R, typename F>
auto dpfor_reduce(const T& init, const T& end, int ntasks, const R&& freducer, const F&& fn, const Behavior behavior = ReplicatedInput | ReplicateOutput) -> typename std::result_of<F(T)>::type {
  
  using Result = typename std::result_of<F(T)>::type;

  dpr::Range pr_tmp_range {init, end};
  auto reducer = typename std::decay<R>::type(freducer);
  auto f = typename std::decay<F>::type(fn);
  
  auto pr_tmp_func = [reducer, f](T var, const T& pr_pfor_end) {
    Result tmp {};
    if(var < pr_pfor_end) {
      tmp = f(var);
      ++var;
      while (var < pr_pfor_end) {
        tmp = reducer(tmp, f(var));
        ++var;
      }
    }
    return tmp;
  };
  return dpr::dparallel_recursion<Result>(pr_tmp_range,
                                          dpr::ExclusiveRangeDInfo(pr_tmp_range, dpr::ExclusiveRangeDInfo::defaultNProcs(), ntasks),
                                          dpr::internal::make_generic_pr_for_reduce_body<Result>(pr_tmp_func, reducer),
                                          dpr::partitioner::simple(),
                                          behavior);
}

/// Macro to parallelize a for loop on top of dparallel_recursion
///
/// @param var    loop variable
/// @param init   starting value of the loop variable
/// @param end    limit of the loop variable
/// @param ntasks number of tasks in which the loop must be split in each process
/// @param ...    loop body. Must rely on \c var to know the iteration number
#define dpr_pfor(var, init, end, ntasks, ...) {                                           \
        dpr::Range _pr_tmp_range_ {(init), (end)};                                        \
        auto _pr_tmp_func_ = [&](decltype(var) var, const decltype(var) _pr_pfor_end_) {  \
                                 while(var < _pr_pfor_end_) { __VA_ARGS__ ; ++var; }      \
                             };                                                           \
        dpr::dparallel_recursion<void>(_pr_tmp_range_,                                    \
                                  dpr::ExclusiveRangeDInfo(_pr_tmp_range_, dpr::ExclusiveRangeDInfo::defaultNProcs(), (ntasks)), \
                                  dpr::internal::make_generic_pr_for_body(_pr_tmp_func_), \
                                  dpr::partitioner::simple(),                             \
                                  ReplicatedInput | DistributedOutput);                   \
       }
  
/// Macro to parallelize a for loop with reduction on top of dparallel_recursion
///
/// @param var         loop variable
/// @param init        starting value of the loop variable
/// @param end         limit of the loop variable
/// @param ntasks      number of tasks in which the loop must be split in each process
/// @param var_reduce  variable in which the reduction takes place
/// @param neutrum     neutrum value for the reduction operation
/// @param reducer     object such that <tt>a=reducer(a,b)</tt> reduces \c a and \c b into \c a
/// @param ...         loop body. Must reduce the result of one iteration given by \c var in \c var_reduce
#define dpr_pfor_reduce(var, init, end, ntasks, var_reduce, neutrum, reducer, ...) {     \
        dpr::Range _pr_tmp_range_ {(init), (end)};                                       \
        auto _pr_tmp_func_ = [&](decltype(var) var, const decltype(var) _pr_pfor_end_) { \
                                 decltype(var_reduce) var_reduce {neutrum};              \
                                 while(var < _pr_pfor_end_) { __VA_ARGS__ ; ++var; }     \
                                 return var_reduce;                                      \
                             };                                                          \
        var_reduce = dpr::dparallel_recursion<decltype(var_reduce)>(_pr_tmp_range_,      \
                             dpr::ExclusiveRangeDInfo(_pr_tmp_range_, dpr::ExclusiveRangeDInfo::defaultNProcs(), (ntasks)),         \
                             dpr::internal::make_generic_pr_for_reduce_body<decltype(var_reduce)>(_pr_tmp_func_, (reducer)), \
                             dpr::partitioner::simple(),                                 \
                             ReplicatedInput | ReplicateOutput);                         \
        }

} // namespace dpr

#endif /* DPR_DRANGE_H_ */
