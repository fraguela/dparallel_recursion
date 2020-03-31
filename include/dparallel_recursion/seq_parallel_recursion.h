/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015-2018 Carlos H. Gonzalez, Basilio B. Fraguela. Universidade da Coruna
 
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
/// \file     seq_parallel_recursion.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_SEQ_PARALLEL_RECURSION_H_
#define DPR_SEQ_PARALLEL_RECURSION_H_

#include <tbb/task.h>
#include <tbb/task_scheduler_init.h>
#include <utility>

#include "dparallel_recursion/dpr_utils.h"
#include "dparallel_recursion/general_reference_wrapper.h"
#include "dparallel_recursion/EmptyBody.h"
#include "dparallel_recursion/Arity.h"

namespace dpr {

  /*! \namespace internal
   *
   * \brief Contains the non-public implementation of the library
   *
   */
namespace internal {

/// Sequential implementation of a ::parallel_recursion task
template<typename Return, typename T, typename Info, typename Body, int I>
struct do_it_serial_struct {
	static const Info* info;

	static void do_it_serial(Return& ret, T&& data, const Info& info_in, Body& body) { do_it_serial(ret, data, info_in, body); }
	static void do_it_serial(Return& ret, T& data, const Info& info_in, Body& body) {
		if (sizeof(Return) > 8)
			do_it_serial(ret, data, body);
		else
			ret = do_it_serial(data, body);
	}

	static void do_it_serial(Return& ret, T&& data, Body& body) { do_it_serial(ret, data, body); }
	static void do_it_serial(Return& ret, T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			ret = body.base(data);
			return;
		}
          
                body.pre_rec(data);
          
		Return results[I];

		for (int i = 0; i < I; ++i) {
			do_it_serial(results[i], info->child(i, data), body);
		}

		ret = body.post(data, results);
	}

	static Return do_it_serial(T&& data, Body& body) { return do_it_serial(data, body); }
	static Return do_it_serial(T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			return body.base(data);
		}
          
                body.pre_rec(data);
          
		Return results[I];

		for (int i = 0; i < I; ++i) {
			results[i] = do_it_serial(info->child(i, data), body);
		}

		return body.post(data, results);
	}

};
template<typename Return, typename T, typename Info, typename Body, int I>
const Info* dpr::internal::do_it_serial_struct<Return, T, Info, Body, I>::info;

///Specialization of do_it_serial_struct for Return==void
template<typename T, typename Info, typename Body, int I>
struct do_it_serial_struct<void, T, Info, Body, I> {
	static const Info* info;

	static void do_it_serial(T&& data, const Info& info_in, Body& body) { do_it_serial(data, info_in, body); }
	static void do_it_serial(T& data, const Info& info_in, Body& body) {
		do_it_serial(data, body);
	}

	static void do_it_serial(T&& data, Body& body) { do_it_serial(data, body); }
	static void do_it_serial(T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			body.base(data);
			return;
		}
          
                body.pre_rec(data);
          
		for (int i = 0; i < I; ++i) {
			do_it_serial(info->child(i, data), body);
		}
	}
};

///Specialization for Return==void
template<typename T, typename Info, typename Body, int I>
const Info* dpr::internal::do_it_serial_struct<void, T, Info, Body, I>::info;

///Specialization of do_it_serial_struct for I==0
template<typename Return, typename T, typename Info, typename Body>
struct do_it_serial_struct<Return, T, Info, Body, 0> {
	static const Info* info;

	static void do_it_serial(Return& ret, T&& data, const Info& info_in, Body& body) { do_it_serial(ret, data, info_in, body); }
	static void do_it_serial(Return& ret, T& data, const Info& info_in, Body& body) {
		if (sizeof(Return) > 8)
			do_it_serial(ret, data, body);
		else
			ret = do_it_serial(data, body);
	}

	static void do_it_serial(Return& ret, T&& data, Body& body) { do_it_serial(ret, data, body); }
	static void do_it_serial(Return& ret, T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			ret = body.base(data);
			return;
		}

                body.pre_rec(data);
          
		const int c = info->num_children(data);
#ifdef __clang__
                Return * const results = new Return[c]; //clang does not support variable length arrays of non-POD types
#else
                Return results[c];
#endif
		for (int i = 0; i < c; ++i) {
			do_it_serial(results[i], info->child(i, data), body);
		}

		ret = body.post(data, results);
          
#ifdef __clang__
                delete [] results;
#endif
	}

	static Return do_it_serial(T&& data, Body& body) { return do_it_serial(data, body); }
	static Return do_it_serial(T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			return body.base(data);
		}
          
                body.pre_rec(data);
          
		const int c = info->num_children(data);
#ifdef __clang__
                Return * const results = new Return[c]; //clang does not support variable length arrays of non-POD types
#else
                Return results[c];
#endif

		for (int i = 0; i < c; ++i) {
			results[i] = do_it_serial(info->child(i, data), body);
		}

		
                Return ret = body.post(data, results);
#ifdef __clang__
                delete [] results;
#endif
                return ret;
	}
};

///Specialization for I==0
template<typename Return, typename T, typename Info, typename Body>
const Info* dpr::internal::do_it_serial_struct<Return, T, Info, Body, 0>::info;

///Specialization of do_it_serial_struct for Return==void, I==0
template<typename T, typename Info, typename Body>
struct do_it_serial_struct<void, T, Info, Body, 0> {
	static const Info* info;

	static void do_it_serial(T&& data, const Info& info_in, Body& body) { do_it_serial(data, info_in, body); }
	static void do_it_serial(T& data, const Info& info_in, Body& body) {
		do_it_serial(data, body);
	}

	static void do_it_serial(T&& data, Body& body) { do_it_serial(data, body); }
	static void do_it_serial(T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			body.base(data);
			return;
		}
          
                body.pre_rec(data);
          
		const int c = info->num_children(data);

		for (int i = 0; i < c; ++i) {
			do_it_serial(info->child(i, data), body);
		}

	}
};

///Specialization for Return==void, I==0
template<typename T, typename Info, typename Body>
const Info* dpr::internal::do_it_serial_struct<void, T, Info, Body, 0>::info;

///Specialization of do_it_serial_struct for I==2
template<typename Return, typename T, typename Info, typename Body>
struct do_it_serial_struct<Return, T, Info, Body, 2> {
	static const Info* info;

	static void do_it_serial(Return& ret, T&& data, const Info& info_in, Body& body) { do_it_serial(ret, data, info_in, body); }
	static void do_it_serial(Return& ret, T& data, const Info& info_in, Body& body) {
		if (sizeof(Return) > 8)
			do_it_serial(ret, data, body);
		else
			ret = do_it_serial(data, body);
	}

	static void do_it_serial(Return& ret, T&& data, Body& body) { do_it_serial(ret, data, body); }
	static void do_it_serial(Return& ret, T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			ret = body.base(data);
			return;
		}

                body.pre_rec(data);
          
		Return results[2];
		do_it_serial(results[0], info->child(0, data), body);
		do_it_serial(results[1], info->child(1, data), body);

		ret = body.post(data, results);
	}

	static Return do_it_serial(T&& data, Body& body) { return do_it_serial(data, body); }
	static Return do_it_serial(T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			return body.base(data);
		}
          
                body.pre_rec(data);
          
		Return results[2];
		results[0] = do_it_serial(info->child(0, data), body);
		results[1] = do_it_serial(info->child(1, data), body);

		return body.post(data, results);
	}
};

///Specialization for I==2
template<typename Return, typename T, typename Info, typename Body>
const Info* dpr::internal::do_it_serial_struct<Return, T, Info, Body, 2>::info;

///Specialization of do_it_serial_struct for Return==void, I==2
template<typename T, typename Info, typename Body>
struct do_it_serial_struct<void, T, Info, Body, 2> {
	static const Info* info;

	static void do_it_serial(T&& data, const Info& info_in, Body& body) { do_it_serial(data, info_in, body); }
	static void do_it_serial(T& data, const Info& info_in, Body& body) {
		do_it_serial(data, body);
	}

	static void do_it_serial(T&& data, Body& body) { do_it_serial(data, body); }
	static void do_it_serial(T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			body.base(data);
			return;
		}
          
                body.pre_rec(data);
          
		do_it_serial(info->child(0, data), body);
		do_it_serial(info->child(1, data), body);

	}
};

///Specialization for Return==void, I==2
template<typename T, typename Info, typename Body>
const Info* dpr::internal::do_it_serial_struct<void, T, Info, Body, 2>::info;

template<typename Return, typename T, typename Info, typename Body>
class simple_partitioner;

template<typename Return, typename T, typename Info, typename Body>
class auto_partitioner;

template<typename Return, typename T, typename Info, typename Body>
class custom_partitioner;
  
} // namespace internal

#ifndef DPR_SEQ_PARALLEL_STACK_RECURSION_H_
/// Provides the public interface to the partitioner implementations
namespace partitioner {

/// Request parallelization with granularity automatically controlled by the library
struct automatic {  };

/// Request simple parallelization, that is, each base case is a parallel task
struct simple {  };

/// Request custom parallelization, controlled by the Info::do_parallel method
struct custom {  };
}
#endif

/// Facilitates the initialization of the parallel_recursion TBB environment
template<typename T>
inline void pr_init(T nthreads) {
  static_assert(std::is_integral<T>::value, "Integer required.");
  new tbb::task_scheduler_init(nthreads);
}
  
} // namespace dpr


#endif /* DPR_SEQ_PARALLEL_RECURSION_H_ */

