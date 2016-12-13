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
/// \file     Range.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_RANGE_H_
#define DPR_RANGE_H_


#include <algorithm>
#include "dparallel_recursion/EmptyBody.h"
#include "dparallel_recursion/Arity.h"

// This is because of the non-macro template functions pfor and pfor_reduce
#ifndef DPR_PARALLEL_RECURSION_H_
#include "dparallel_recursion/parallel_recursion.h"
#endif

namespace dpr {

/// Represents a range of integers
struct Range {

  int start;     ///< First element of the Range
  int end;       ///< Last or last plus one element of the Range, depending on whether it is inclusive or exlusive
  int nchildren; ///< Number of children problems in which this Range has been partitioned
  
  /// Size of the Range interpreted as an exclusive Range, i.e., [)
  constexpr int exclusiveSize() const { return end - start; }
  
  /// Size of the Range interpreted as an inclusie Range, i.e., []
  constexpr int inclusiveSize() const { return end - start + 1; }
  
  /// Returns a Range shifted \c offset positions in the positive direction
  constexpr Range operator>> (int offset) const {
    return Range { start + offset, end + offset, nchildren };
  }
  
  /// Returns a Range shifted \c offset positions in the negative direction
  constexpr Range operator<< (int offset) const {
    return Range { start - offset, end - offset, nchildren };
  }
  
  /// Size of the biggest chunk if this exclusive range is partitioned in \c nchunk chunks
  int exclusiveMaxChunk (int nchunks) const {
    int tmp = exclusiveSize();
    return (tmp / nchunks) + ((tmp % nchunks) ? 1 : 0);
  }
  
  /// Size of the biggest chunk if this inclusive range is partitioned in \c nchunk chunks
  int inclusiveMaxChunk (int nchunks) const {
    int tmp = inclusiveSize();
    return (tmp / nchunks) + ((tmp % nchunks) ? 1 : 0);
  }
  
  /*
  template<class Archive>
  void serialize(Archive& ar,  unsigned int file_version)
  {
    ar & start & end & nchildren;
  }
  */
  
  friend void swap(Range& a, Range& b) {
    using std::swap; // bring in swap for built-in types
    
    swap(a.start, b.start);
    swap(a.end, b.end);
    swap(a.nchildren, b.nchildren);
  }

};

/*
 struct InclusiveRangeInfo2 : public DInfo<Range, 2> {
 
 int baseSize_;
 
 InclusiveRangeInfo2(int minimum_size=1) :
 baseSize_(minimum_size)
 {}
 
 InclusiveRangeInfo2(const Range& r, int ntasks) :
 baseSize_(r.inclusiveSize() / ntasks)
 {}
 
 bool is_base(const Range& r) const {
 return r.inclusiveSize() <= baseSize_;
 }
 
 Range child(int i, const Range& r) const {
 const int mid = r.start + (r.end - r.start) / 2;
 return i ? Range { mid+1, r.end } : Range { r.start, mid };
 }
 };
 
 struct ExclusiveRangeInfo2 : public DInfo<Range, 2> {
 
 int baseSize_;
 
 ExclusiveRangeInfo2(int minimum_size=1) :
 baseSize_(minimum_size)
 {}
 
 ExclusiveRangeInfo2(const Range& r, int ntasks) :
 baseSize_(r.exclusiveSize() / ntasks)
 {}
 
 bool is_base(const Range& r) const {
 return r.exclusiveSize() <= baseSize_;
 }
 
 Range child(int i, const Range& r) const {
 const int mid = r.start + (r.end - r.start) / 2;
 return i ? Range { mid, r.end } : Range { r.start, mid };
 }
 };
 */

/// Info object for inclusive Ranges suited for parallel_recursion invocations
struct InclusiveRangeInfo : public Arity<0> {
  
  const int initRange_, ntasks_;
  int baseSize_, taskPartPoint_;
  
  /// Build Info object for a Range \c range to be split in \c ntasks tasks
  /// @param r      range to be split
  /// @param ntasks number of parallel tasks
  /// @pre r.inclusiveSize() >= ntasks
  InclusiveRangeInfo(const Range& r, int ntasks) :
  initRange_(r.inclusiveSize()), ntasks_(ntasks),
  baseSize_(r.inclusiveMaxChunk(ntasks)), taskPartPoint_(r.inclusiveSize() % ntasks)
  { }

  /// Number of children in which a non-base Range \c r can be subdivided
  int num_children(Range& r) const {
    r.nchildren = ntasks_;
    return ntasks_;
  }
  
  /// Returns whether the input Range \c r is a base case
  bool is_base(const Range& r) const {
    return r.inclusiveSize() <= baseSize_;
  }
  
  /// Returns the i-th child of Range \c r
  Range child(int i, const Range& r) const {
    return sub_child(i, r, baseSize_, taskPartPoint_);
  }

  static Range sub_child(int i, const Range& r, int base_size, int part_point) {
    int begin = r.start + base_size * i;
    int sz = base_size;
    
    if (part_point && (i >= part_point)) {
      begin -= (i - part_point);
      sz--;
    }
    
    int end = std::min(r.end, begin + sz - 1); //Required because of (*)
    
    //printf("R[%d, %d] ->(%d) -> [%d, %d]\n", r.start, r.end, i, begin, end);
    return Range { begin, end };
  }

protected:
  
  // Used by descendants, because they can calculate a different baseSize_ and taskPartPoint_
  //InclusiveRangeInfo(const Range& r, int ntasks, bool) :
  //initRange_(r.inclusiveSize()), ntasks_(ntasks)
  //{}
  
};

/// Info object for exclusive Ranges suited for parallel_recursion invocations
struct ExclusiveRangeInfo : public Arity<0> {
  
  const int initRange_, ntasks_;
  int baseSize_, taskPartPoint_;
  
  /// Build Info object for a Range \c range to be split in \c ntasks tasks
  /// @param r      range to be split
  /// @param ntasks number of parallel tasks
  /// @pre r.exclusiveSize() >= ntasks
  ExclusiveRangeInfo(const Range& r, int ntasks) :
  initRange_(r.exclusiveSize()), ntasks_(ntasks),
  baseSize_(r.exclusiveMaxChunk(ntasks)), taskPartPoint_(r.exclusiveSize() % ntasks)
  { }
  
  /// Number of children in which a non-base Range \c r can be subdivided
  int num_children(Range& r) const {
    r.nchildren = ntasks_;
    return ntasks_;
  }
  
  /// Returns whether the input Range \c r is a base case
  bool is_base(const Range& r) const {
    return r.exclusiveSize() <= baseSize_;
  }
  
  /// Returns the i-th child of Range \c r
  Range child(int i, const Range& r) const {
    return sub_child(i, r, baseSize_, taskPartPoint_);
  }
  
  static Range sub_child(int i, const Range& r, int base_size, int part_point) {
    int begin = r.start + base_size * i;
    int sz = base_size;
    
    if (part_point && (i >= part_point)) {
      begin -= (i - part_point);
      sz--;
    }
    
    int end = std::min(r.end, begin + sz); //Required because of (*)
    
    //printf("R[%d, %d) ->(%d) -> [%d, %d)\n", r.start, r.end, i, begin, end);
    return Range { begin, end };
  }
  
protected:
  
  // Used by descendants, because they can calculate a different baseSize_ and taskPartPoint_
  //ExclusiveRangeInfo(const Range& r, int ntasks, bool) :
  //initRange_(r.exclusiveSize()), ntasks_(ntasks)
  //{}
  
};

namespace internal {
  
  template<typename F>
  struct generic_pr_for_body : EmptyBody<Range, void> {
    const F& f_;
    
    generic_pr_for_body(const F& f) : f_(f) {}
    
    generic_pr_for_body(const generic_pr_for_body& other) : f_(other.f_) {}
    
    generic_pr_for_body(generic_pr_for_body&& other) : f_(other.f_) {}
    
    void base(const Range& range) {
      f_(range.start, range.end);
    }
  };
  
  template<typename F>
  inline const generic_pr_for_body<F> make_generic_pr_for_body(F&& f) {
    return generic_pr_for_body<F>(f);
  }
  
  template<typename Return, typename F, typename ReduceF>
  struct generic_pr_for_reduce_body : EmptyBody<Range, Return> {
    const F& f_;
    const ReduceF& rf_;
    
    generic_pr_for_reduce_body(const F& f, const ReduceF& rf) :
    f_(f), rf_(rf)
    {}
    
    generic_pr_for_reduce_body(const generic_pr_for_reduce_body& other) :
    f_(other.f_), rf_(other.rf_)
    {}
    
    generic_pr_for_reduce_body(generic_pr_for_reduce_body&& other) :
    f_(other.f_), rf_(other.rf_)
    {}
    
    Return base(const Range& range) {
      return f_(range.start, range.end);
    }
    
    Return post(const Range& range, Return* r) {
      for (int i  = 1; i < range.nchildren; i++) {
        *r = rf_(*r, r[i]);
      }
      return *r;
    }
    
  };
  
  template<typename Return, typename F, typename ReduceF>
  inline const  generic_pr_for_reduce_body<Return, F, ReduceF> make_generic_pr_for_reduce_body(const F& f, const ReduceF& rf) {
    return generic_pr_for_reduce_body<Return, F, ReduceF>(f, rf);
  }

} // namespace internal

/// Template function to parallelize a for loop on top of parallel_recursion
///
/// @param init   starting value of the loop
/// @param end    limit of the loop
/// @param ntasks number of tasks in which the loop must be split
/// @param fn     function that receives the iteration number and executes the associated iteration
template<typename T, typename F>
void pfor(const T& init, const T& end, int ntasks, const F&& fn) {
  dpr::Range pr_tmp_range {init, end};
  auto f = typename std::decay<F>::type(fn);
  auto pr_tmp_func = [f](T var, const T& pr_pfor_end) {
    while (var < pr_pfor_end) {
      f(var);
      ++var;
    }
  };
  dpr::parallel_recursion<void>(pr_tmp_range,
                                dpr::ExclusiveRangeInfo(pr_tmp_range, ntasks),
                                dpr::internal::make_generic_pr_for_body(pr_tmp_func));
}

/// Template function to parallelize a for loop on top of parallel_recursion
///
/// @param init    starting value of the loop
/// @param end     limit of the loop
/// @param ntasks  number of tasks in which the loop must be split
/// @param freducer object such that <tt>a=freducer(a,b)</tt> reduces \c a and \c b into \c a
/// @param fn       function that receives the iteration number and executes the associated iteration, returning the value to reduce
template<typename T, typename R, typename F>
auto pfor_reduce(const T& init, const T& end, int ntasks, const R&& freducer, const F&& fn) -> typename std::result_of<F(T)>::type {
  
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

  return dpr::parallel_recursion<Result>(pr_tmp_range,
                                         dpr::ExclusiveRangeInfo(pr_tmp_range, ntasks),
                                         dpr::internal::make_generic_pr_for_reduce_body<Result>(pr_tmp_func, reducer));
}

/// Macro to parallelize a for loop on top of parallel_recursion
///
/// @param var    loop variable
/// @param init   starting value of the loop variable
/// @param end    limit of the loop variable
/// @param ntasks number of tasks in which the loop must be split
/// @param ...    loop body. Must rely on \c var to know the iteration number
#define pr_pfor(var, init, end, ntasks, ...) {                                              \
     dpr::Range _pr_tmp_range_ {(init), (end)};                                             \
     auto _pr_tmp_func_ = [&](decltype(var) var, const decltype(var) _pr_pfor_end_) {       \
                              while(var < _pr_pfor_end_) { __VA_ARGS__ ; ++var; }           \
                          };                                                                \
     dpr::parallel_recursion<void>(_pr_tmp_range_,                                          \
                                   dpr::ExclusiveRangeInfo(_pr_tmp_range_, (ntasks)),       \
                                   dpr::internal::make_generic_pr_for_body(_pr_tmp_func_)); \
   }

/// Macro to parallelize a for loop with reduction on top of parallel_recursion
///
/// @param var         loop variable
/// @param init        starting value of the loop variable
/// @param end         limit of the loop variable
/// @param ntasks      number of tasks in which the loop must be split
/// @param var_reduce  variable in which the reduction takes place
/// @param neutrum     neutrum value for the reduction operation
/// @param reducer     object such that <tt>a=reducer(a,b)</tt> reduces \c a and \c b into \c a
/// @param ...         loop body. Must reduce the result of one iteration given by \c var in \c var_reduce
#define pr_pfor_reduce(var, init, end, ntasks, var_reduce, neutrum, reducer, ...) {   \
     dpr::Range _pr_tmp_range_ {(init), (end)};                                       \
     auto _pr_tmp_func_ = [&](decltype(var) var, const decltype(var) _pr_pfor_end_) { \
                              decltype(var_reduce) var_reduce {neutrum};              \
                              while(var < _pr_pfor_end_) { __VA_ARGS__ ; ++var; }     \
                              return var_reduce;                                      \
                          };                                                          \
     var_reduce = dpr::parallel_recursion<decltype(var_reduce)>(_pr_tmp_range_,       \
                          dpr::ExclusiveRangeInfo(_pr_tmp_range_, (ntasks)),          \
                          dpr::internal::make_generic_pr_for_reduce_body<decltype(var_reduce)>(_pr_tmp_func_, (reducer))); \
  }

} // namespace dpr

#ifdef DPR_PARALLEL_RECURSION_MPI_H_
BOOST_IS_BITWISE_SERIALIZABLE(dpr::Range);
#endif

#endif /* DPR_RANGE_H_ */
