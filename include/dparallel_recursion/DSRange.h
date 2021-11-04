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
/// \file     DSRange.h
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#ifndef DPR_DSRANGE_H_
#define DPR_DSRANGE_H_

#include <type_traits>
#include <vector>
#ifndef DPR_SRANGE_COMM_H_
#include "dparallel_recursion/SRange_comm.h"
#endif
#ifndef DPR_SRANGE_H_
#include "dparallel_recursion/SRange.h"
#endif
#ifndef DPR_PARALLEL_STACK_RECURSION_MPI_H_
#include "dparallel_recursion/dparallel_stack_recursion.h"
#endif

#include <boost/serialization/is_bitwise_serializable.hpp>
BOOST_IS_BITWISE_SERIALIZABLE(dpr::SRange);

namespace dpr {

/// Template function to parallelize a for loop on top of dparallel_stack_recursion
///
/// @tparam DCHUNK   number of childs for each range division
/// @param init      starting value of the loop
/// @param end       limit of the loop
/// @param chunkSize the chunkSize parameter value
/// @param fn        function that receives the iteration number and executes the associated iteration
/// @param behavior  distributed behavior flags
/// @param dspInfObj dsparallel configuration info object
template<int DCHUNK = 2, typename T, typename F>
void dpsfor(const T& init, const T& end, const int chunkSize, const F&& fn, const Behavior behavior = ReplicatedInput | DistributedOutput, const DSParConfigInfo& dspInfObj = dspar_config_info_default) {
	dpr::SRange psr_tmp_range {init, end};
	auto f = typename std::decay<F>::type(fn);
	auto psr_tmp_func = [f](T var, const T& psr_pfor_end) {
		while (var < psr_pfor_end) {
			f(var);
			++var;
		}
	};
	dpr::dparallel_stack_recursion<void>(psr_tmp_range, dpr::SRangeInfo<DCHUNK>(), dpr::internal::make_generic_psr_for_body(psr_tmp_func), chunkSize, dpr::partitioner::simple(), behavior, dspInfObj);
}

/// Template function to parallelize a for loop on top of dparallel_stack_recursion
///
/// @param init      starting value of the loop
/// @param end       limit of the loop
/// @param chunkSize the chunkSize parameter value
/// @param dchunk    number of childs for each range division
/// @param fn        function that receives the iteration number and executes the associated iteration+
/// @param behavior  distributed behavior flags
/// @param dspInfObj dsparallel configuration info object
template<typename T, typename F>
void dpsfor(const T& init, const T& end, const int chunkSize, const int dchunk, const F&& fn, const Behavior behavior = ReplicatedInput | DistributedOutput, const DSParConfigInfo& dspInfObj = dspar_config_info_default) {
	dpr::SRange psr_tmp_range {init, end};
	auto f = typename std::decay<F>::type(fn);
	auto psr_tmp_func = [f](T var, const T& psr_pfor_end) {
		while (var < psr_pfor_end) {
			f(var);
			++var;
		}
	};
	dpr::dparallel_stack_recursion<void>(psr_tmp_range, dpr::SRangeInfo<0>(dchunk), dpr::internal::make_generic_psr_for_body(psr_tmp_func), chunkSize, dpr::partitioner::simple(), behavior, dspInfObj);
}

/// Template function to parallelize a for loop on top of dparallel_stack_recursion
///
/// @tparam DCHUNK   number of childs for each range division
/// @param init      starting value of the loop
/// @param end       limit of the loop
/// @param chunkSize the chunkSize parameter value
/// @param freducer  object such that <tt>a=freducer(a,b)</tt> reduces \c a and \c b into \c a
/// @param fn        function that receives the iteration number and executes the associated iteration, returning the value to reduce
/// @param behavior  distributed behavior flags
/// @param dspInfObj dsparallel configuration info object
template<int DCHUNK = 2, typename T, typename R, typename F>
auto dpsfor_reduce(const T& init, const T& end, const int chunkSize, const R&& freducer, const F&& fn, const Behavior behavior = ReplicatedInput | ReplicateOutput, const DSParConfigInfo& dspInfObj = dspar_config_info_default) -> typename std::result_of<F(T)>::type {

  using Result = typename std::result_of<F(T)>::type;

  dpr::SRange psr_tmp_range {init, end};
  auto reducer = typename std::decay<R>::type(freducer);
  auto f = typename std::decay<F>::type(fn);

  auto psr_tmp_func = [reducer, f](T var, const T& psr_pfor_end) {
    Result tmp {};
    if(var < psr_pfor_end) {
      tmp = f(var);
      ++var;
      while (var < psr_pfor_end) {
        tmp = reducer(tmp, f(var));
        ++var;
      }
    }
    return tmp;
  };

  return dpr::dparallel_stack_recursion<Result>(psr_tmp_range, dpr::SRangeInfo<DCHUNK>(), dpr::internal::make_generic_psr_for_reduce_body<Result>(psr_tmp_func, reducer), chunkSize, dpr::partitioner::simple(), behavior, dspInfObj);
}

/// Template function to parallelize a for loop on top of dparallel_stack_recursion
///
/// @param init      starting value of the loop
/// @param end       limit of the loop
/// @param chunkSize the chunkSize parameter value
/// @param dchunk    number of childs for each range division
/// @param freducer  object such that <tt>a=freducer(a,b)</tt> reduces \c a and \c b into \c a
/// @param fn        function that receives the iteration number and executes the associated iteration, returning the value to reduce
/// @param behavior  distributed behavior flags
/// @param dspInfObj dsparallel configuration info object
template<typename T, typename R, typename F>
auto dpsfor_reduce(const T& init, const T& end, const int chunkSize, const int dchunk, const R&& freducer, const F&& fn, const Behavior behavior = ReplicatedInput | ReplicateOutput, const DSParConfigInfo& dspInfObj = dspar_config_info_default) -> typename std::result_of<F(T)>::type {

  using Result = typename std::result_of<F(T)>::type;

  dpr::SRange psr_tmp_range {init, end};
  auto reducer = typename std::decay<R>::type(freducer);
  auto f = typename std::decay<F>::type(fn);

  auto psr_tmp_func = [reducer, f](T var, const T& psr_pfor_end) {
    Result tmp {};
    if(var < psr_pfor_end) {
      tmp = f(var);
      ++var;
      while (var < psr_pfor_end) {
        tmp = reducer(tmp, f(var));
        ++var;
      }
    }
    return tmp;
  };

  return dpr::dparallel_stack_recursion<Result>(psr_tmp_range, dpr::SRangeInfo<0>(dchunk), dpr::internal::make_generic_psr_for_reduce_body<Result>(psr_tmp_func, reducer), chunkSize, dpr::partitioner::simple(), behavior, dspInfObj);
}

/// Macro to parallelize a for loop on top of parallel_recursion
///
/// @param var       loop variable
/// @param init      starting value of the loop variable
/// @param end       limit of the loop variable
/// @param chunkSize the chunkSize parameter value
/// @param dchunk    number of childs for each range division
/// @param ...       loop body. Must rely on \c var to know the iteration number
#define dpr_psfor(var, init, end, chunkSize, dchunk, ...) {												\
	dpr::SRange _psr_tmp_range_ {(init), (end)};														\
	auto _psr_tmp_func_ = [&](decltype(var) var, const decltype(var) _psr_pfor_end_) {					\
								while(var < _psr_pfor_end_) { __VA_ARGS__ ; ++var; }					\
							};																			\
	dpr::dparallel_stack_recursion<void>(_psr_tmp_range_,												\
									dpr::SRangeInfo<0>(dchunk),											\
									dpr::internal::make_generic_psr_for_body(_psr_tmp_func_),			\
									chunkSize,															\
									dpr::partitioner::simple(), ReplicatedInput | DistributedOutput);	\
}

/// Macro to parallelize a for loop with reduction on top of parallel_recursion
///
/// @param var         loop variable
/// @param init        starting value of the loop variable
/// @param end         limit of the loop variable
/// @param chunkSize   the chunkSize parameter value
/// @param dchunk      number of childs for each range division
/// @param var_reduce  variable in which the reduction takes place
/// @param neutrum     neutrum value for the reduction operation
/// @param reducer     object such that <tt>a=reducer(a,b)</tt> reduces \c a and \c b into \c a
/// @param ...         loop body. Must reduce the result of one iteration given by \c var in \c var_reduce
#define dpr_psfor_reduce(var, init, end, chunkSize, dchunk, var_reduce, neutrum, reducer, ...) {						\
	dpr::SRange _psr_tmp_range_ {(init), (end)};																		\
	auto _psr_tmp_func_ = [&](decltype(var) var, const decltype(var) _psr_pfor_end_) {									\
								decltype(var_reduce) var_reduce {neutrum};												\
								while(var < _psr_pfor_end_) { __VA_ARGS__ ; ++var; }									\
								return var_reduce;																		\
							};																							\
	var_reduce = dpr::dparallel_stack_recursion<decltype(var_reduce)>(_psr_tmp_range_,									\
					dpr::SRangeInfo<0>(dchunk),																			\
					dpr::internal::make_generic_psr_for_reduce_body<decltype(var_reduce)>(_psr_tmp_func_, (reducer)),	\
					chunkSize,																							\
					dpr::partitioner::simple(), ReplicatedInput | ReplicateOutput);										\
}

/// Template function to test the parallelizaton of a for loop on top of dparallel_stack_recursion
///
/// @tparam DCHUNK   number of childs for each range division
/// @param init      starting value of the loop
/// @param end       limit of the loop
/// @param chunkSize the chunkSize parameter value
/// @param fn        function that receives the iteration number and executes the associated iteration
/// @param behavior  distributed behavior flags
/// @param dspInfObj dsparallel configuration info object
/// @param auto_opt  AutomaticChunkOptions object with the test options
template<int DCHUNK = 2, typename T, typename F>
std::vector<dpr::ResultChunkTest> dpsfor_test(const T& init, const T& end, const int chunkSize, const F&& fn, const Behavior behavior = ReplicatedInput | DistributedOutput, const DSParConfigInfo& dspInfObj = dspar_config_info_default, const AutomaticChunkOptions& auto_opt = dpr::aco_test_default) {
	dpr::SRange psr_tmp_range {init, end};
	auto f = typename std::decay<F>::type(fn);
	auto psr_tmp_func = [f](T var, const T& psr_pfor_end) {
		while (var < psr_pfor_end) {
			f(var);
			++var;
		}
	};
	return dpr::dparallel_stack_recursion_test<void>(psr_tmp_range, dpr::SRangeInfo<DCHUNK>(), dpr::internal::make_generic_psr_for_body(psr_tmp_func), chunkSize, dpr::partitioner::simple(), behavior, dspInfObj, auto_opt);
}

/// Template function to test the parallelizaton of a for loop on top of dparallel_stack_recursion
///
/// @param init      starting value of the loop
/// @param end       limit of the loop
/// @param chunkSize the chunkSize parameter value
/// @param dchunk    number of childs for each range division
/// @param fn        function that receives the iteration number and executes the associated iteration+
/// @param behavior  distributed behavior flags
/// @param dspInfObj dsparallel configuration info object
/// @param auto_opt  AutomaticChunkOptions object with the test options
template<typename T, typename F>
std::vector<dpr::ResultChunkTest> dpsfor_test(const T& init, const T& end, const int chunkSize, const int dchunk, const F&& fn, const Behavior behavior = ReplicatedInput | DistributedOutput, const DSParConfigInfo& dspInfObj = dspar_config_info_default, const AutomaticChunkOptions& auto_opt = dpr::aco_test_default) {
	dpr::SRange psr_tmp_range {init, end};
	auto f = typename std::decay<F>::type(fn);
	auto psr_tmp_func = [f](T var, const T& psr_pfor_end) {
		while (var < psr_pfor_end) {
			f(var);
			++var;
		}
	};
	return dpr::dparallel_stack_recursion_test<void>(psr_tmp_range, dpr::SRangeInfo<0>(dchunk), dpr::internal::make_generic_psr_for_body(psr_tmp_func), chunkSize, dpr::partitioner::simple(), behavior, dspInfObj, auto_opt);
}

/// Template function to test the parallelizaton of a for loop on top of dparallel_stack_recursion
///
/// @tparam DCHUNK   number of childs for each range division
/// @param init      starting value of the loop
/// @param end       limit of the loop
/// @param chunkSize the chunkSize parameter value
/// @param freducer  object such that <tt>a=freducer(a,b)</tt> reduces \c a and \c b into \c a
/// @param fn        function that receives the iteration number and executes the associated iteration, returning the value to reduce
/// @param behavior  distributed behavior flags
/// @param dspInfObj dsparallel configuration info object
/// @param auto_opt  AutomaticChunkOptions object with the test options
template<int DCHUNK = 2, typename T, typename R, typename F>
std::vector<dpr::ResultChunkTest> dpsfor_reduce_test(const T& init, const T& end, const int chunkSize, const R&& freducer, const F&& fn, const Behavior behavior = ReplicatedInput | ReplicateOutput, const DSParConfigInfo& dspInfObj = dspar_config_info_default, const AutomaticChunkOptions& auto_opt = dpr::aco_test_default) {

  using Result = typename std::result_of<F(T)>::type;

  dpr::SRange psr_tmp_range {init, end};
  auto reducer = typename std::decay<R>::type(freducer);
  auto f = typename std::decay<F>::type(fn);

  auto psr_tmp_func = [reducer, f](T var, const T& psr_pfor_end) {
    Result tmp {};
    if(var < psr_pfor_end) {
      tmp = f(var);
      ++var;
      while (var < psr_pfor_end) {
        tmp = reducer(tmp, f(var));
        ++var;
      }
    }
    return tmp;
  };

  return dpr::dparallel_stack_recursion_test<Result>(psr_tmp_range, dpr::SRangeInfo<DCHUNK>(), dpr::internal::make_generic_psr_for_reduce_body<Result>(psr_tmp_func, reducer), chunkSize, dpr::partitioner::simple(), behavior, dspInfObj, auto_opt);
}

/// Template function to test the parallelizaton of a for loop on top of dparallel_stack_recursion
///
/// @param init      starting value of the loop
/// @param end       limit of the loop
/// @param chunkSize the chunkSize parameter value
/// @param dchunk    number of childs for each range division
/// @param freducer  object such that <tt>a=freducer(a,b)</tt> reduces \c a and \c b into \c a
/// @param fn        function that receives the iteration number and executes the associated iteration, returning the value to reduce
/// @param behavior  distributed behavior flags
/// @param dspInfObj dsparallel configuration info object
/// @param auto_opt  AutomaticChunkOptions object with the test options
template<typename T, typename R, typename F>
std::vector<dpr::ResultChunkTest> dpsfor_reduce_test(const T& init, const T& end, const int chunkSize, const int dchunk, const R&& freducer, const F&& fn, const Behavior behavior = ReplicatedInput | ReplicateOutput, const DSParConfigInfo& dspInfObj = dspar_config_info_default, const AutomaticChunkOptions& auto_opt = dpr::aco_test_default) {

  using Result = typename std::result_of<F(T)>::type;

  dpr::SRange psr_tmp_range {init, end};
  auto reducer = typename std::decay<R>::type(freducer);
  auto f = typename std::decay<F>::type(fn);

  auto psr_tmp_func = [reducer, f](T var, const T& psr_pfor_end) {
    Result tmp {};
    if(var < psr_pfor_end) {
      tmp = f(var);
      ++var;
      while (var < psr_pfor_end) {
        tmp = reducer(tmp, f(var));
        ++var;
      }
    }
    return tmp;
  };

  return dpr::dparallel_stack_recursion_test<Result>(psr_tmp_range, dpr::SRangeInfo<0>(dchunk), dpr::internal::make_generic_psr_for_reduce_body<Result>(psr_tmp_func, reducer), chunkSize, dpr::partitioner::simple(), behavior, dspInfObj, auto_opt);
}

/// Macro to test the parallelizaton of a for loop on top of parallel_recursion
///
/// @param var       loop variable
/// @param init      starting value of the loop variable
/// @param end       limit of the loop variable
/// @param chunkSize the chunkSize parameter value
/// @param dchunk    number of childs for each range division
/// @param res_vec   variable in witch the vector with the ordered list of the best chunks takes place
/// @param auto_opt  AutomaticChunkOptions object with the test options
/// @param ...       loop body. Must rely on \c var to know the iteration number
#define dpr_psfor_test(var, init, end, chunkSize, dchunk, res_vec, auto_opt, ...) {								\
	dpr::SRange _psr_tmp_range_ {(init), (end)};																\
	auto _psr_tmp_func_ = [&](decltype(var) var, const decltype(var) _psr_pfor_end_) {							\
								while(var < _psr_pfor_end_) { __VA_ARGS__ ; ++var; }							\
							};																					\
	res_vec = dpr::dparallel_stack_recursion_test<void>(_psr_tmp_range_,										\
									dpr::SRangeInfo<0>(dchunk),													\
									dpr::internal::make_generic_psr_for_body(_psr_tmp_func_),					\
									chunkSize,																	\
									dpr::partitioner::simple(),													\
									ReplicatedInput | DistributedOutput,										\
									dpr::dspar_config_info_default,												\
									auto_opt);																	\
}

/// Macro to test the parallelizaton of a for loop with reduction on top of parallel_recursion
///
/// @param var         loop variable
/// @param init        starting value of the loop variable
/// @param end         limit of the loop variable
/// @param chunkSize   the chunkSize parameter value
/// @param dchunk      number of childs for each range division
/// @param neutrum     neutrum value for the reduction operation
/// @param reducer     object such that <tt>a=reducer(a,b)</tt> reduces \c a and \c b into \c a
/// @param res_vec     variable in witch the vector with the ordered list of the best chunks takes place
/// @param auto_opt    AutomaticChunkOptions object with the test options
/// @param ...         loop body. Must reduce the result of one iteration given by \c var in \c var_reduce
#define dpr_psfor_reduce_test(var, init, end, chunkSize, dchunk, neutrum, reducer, res_vec, auto_opt, ...) {	\
	dpr::SRange _psr_tmp_range_ {(init), (end)};																\
    auto tmp_result_func = __VA_ARGS__;																			\
    using Result = decltype(tmp_result_func);																	\
	auto _psr_tmp_func_ = [&](decltype(var) var, const decltype(var) _psr_pfor_end_) {							\
								Result var_reduce {neutrum};													\
								while(var < _psr_pfor_end_) { __VA_ARGS__ ; ++var; }							\
								return var_reduce;																\
							};																					\
	res_vec = dpr::dparallel_stack_recursion_test<Result>(_psr_tmp_range_,										\
					dpr::SRangeInfo<0>(dchunk),																	\
					dpr::internal::make_generic_psr_for_reduce_body<Result>(_psr_tmp_func_, (reducer)),			\
					chunkSize,																					\
					dpr::partitioner::simple(),																	\
					ReplicatedInput | ReplicateOutput,															\
					dpr::dspar_config_info_default,																\
					auto_opt);																					\
}

} // namespace dpr

#endif /* DPR_SRANGE_H_ */
