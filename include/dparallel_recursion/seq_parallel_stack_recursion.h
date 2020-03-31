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
/// \file     seq_parallel_stack_recursion.h
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#ifndef DPR_SEQ_PARALLEL_STACK_RECURSION_H_
#define DPR_SEQ_PARALLEL_STACK_RECURSION_H_

#include <utility>

#include "dparallel_recursion/dpr_utils.h"
#include "dparallel_recursion/general_reference_wrapper.h"
#include "dparallel_recursion/EmptyBody.h"
#include "dparallel_recursion/Arity.h"

namespace dpr {

/// Class that allow store extra info of the last parallel_stack_recursion run (only for normal runs, not test runs)
struct RunExtraInfo {
	int nthreads;
	int chunkSize;
	int chunkSizeUsed;
	double runTime;
	double testChunkTime;

	RunExtraInfo(int _nthreads = 0, int _chunkSize = 0, int _chunkSizeUsed = 0, double _runTime = 0.0, double _testChunkTime = 0.0) :
		nthreads(_nthreads), chunkSize(_chunkSize), chunkSizeUsed(_chunkSizeUsed), runTime(_runTime), testChunkTime(_testChunkTime)
	{ }

};

/*! \namespace internal
*
* \brief Contains the non-public implementation of the library
*
*/
namespace internal {

static constexpr int defaultChunkSize = 10;

inline int& num_total_threads() {
   static int num_total_threads = std::thread::hardware_concurrency();
   return num_total_threads;
}

inline int& initialStackSize() {
#ifndef DPR_DENY_STACK_RESIZE
	static int initialStackSize = 1000;
#else
	static int initialStackSize = 500000;
#endif
	return initialStackSize;
}

inline dpr::RunExtraInfo& lastPsrRunExtraInfo() {
	static dpr::RunExtraInfo lastPsrRunExtraInfo;
	return lastPsrRunExtraInfo;
}

/// Sequential implementation of a ::parallel_recursion task
template<typename Return, typename T, typename Info, typename Body, int I>
struct do_it_serial_stack_struct {
	static const Info* info;

	static void do_it_serial(Return& ret, T&& data, const Info& info_in, Body& body) { do_it_serial(ret, data, info_in, body); }
	static void do_it_serial(Return& ret, T& data, const Info& info_in, Body& body) {
		do_it_serial(ret, data, body);
	}

	static void do_it_serial(Return& ret, T&& data, Body& body) { do_it_serial(ret, data, body); }
	static void do_it_serial(Return& ret, T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			body.post(body.base(data), ret);
			return;
		}
          
		body.pre_rec(data);

		for (int i = 0; i < I; ++i) {
			do_it_serial(ret, info->child(i, data), body);
		}

	}

	//static void do_it_serial(Return& ret, T*& data, const Info& info_in, Body& body) { do_it_serial(ret, data, info_in, body); }
	static void do_it_serial(Return& ret, T* data, const Info& info_in, Body& body) {
		do_it_serial(ret, data, body);
	}

	//static void do_it_serial(Return& ret, T*& data, Body& body) { do_it_serial(ret, data, body); }
	static void do_it_serial(Return& ret, T* data, Body& body) {
		body.pre(*data);

		if (info->is_base(*data)) {
			body.post(body.base(*data), ret);
			return;
		}

		body.pre_rec(*data);

		for (int i = 0; i < I; ++i) {
			T child;
			info->child(i, *data, child);
			do_it_serial(ret, &child, body);
		}

	}

};
template<typename Return, typename T, typename Info, typename Body, int I>
const Info* dpr::internal::do_it_serial_stack_struct<Return, T, Info, Body, I>::info;

///Specialization of do_it_serial_stack_struct for Return==void
template<typename T, typename Info, typename Body, int I>
struct do_it_serial_stack_struct<void, T, Info, Body, I> {
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

	//static void do_it_serial(T*& data, const Info& info_in, Body& body) { do_it_serial(data, info_in, body); }
	static void do_it_serial(T* data, const Info& info_in, Body& body) {
		do_it_serial(data, body);
	}

	//static void do_it_serial(T*& data, Body& body) { do_it_serial(data, body); }
	static void do_it_serial(T* data, Body& body) {
		body.pre(*data);

		if (info->is_base(*data)) {
			body.base(*data);
			return;
		}

		body.pre_rec(*data);

		for (int i = 0; i < I; ++i) {
			T child;
			info->child(i, *data, child);
			do_it_serial(&child, body);
		}
	}
};

///Specialization for Return==void
template<typename T, typename Info, typename Body, int I>
const Info* dpr::internal::do_it_serial_stack_struct<void, T, Info, Body, I>::info;

///Specialization of do_it_serial_stack_struct for I==0
template<typename Return, typename T, typename Info, typename Body>
struct do_it_serial_stack_struct<Return, T, Info, Body, 0> {
	static const Info* info;

	static void do_it_serial(Return& ret, T&& data, const Info& info_in, Body& body) { do_it_serial(ret, data, info_in, body); }
	static void do_it_serial(Return& ret, T& data, const Info& info_in, Body& body) {
		do_it_serial(ret, data, body);
	}

	static void do_it_serial(Return& ret, T&& data, Body& body) { do_it_serial(ret, data, body); }
	static void do_it_serial(Return& ret, T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			body.post(body.base(data), ret);
			return;
		}

		body.pre_rec(data);
          
		const int c = info->num_children(data);
		for (int i = 0; i < c; ++i) {
			do_it_serial(ret, info->child(i, data), body);
		}

	}

	//static void do_it_serial(Return& ret, T*& data, const Info& info_in, Body& body) { do_it_serial(ret, data, info_in, body); }
	static void do_it_serial(Return& ret, T* data, const Info& info_in, Body& body) {
		do_it_serial(ret, data, body);
	}

	//static void do_it_serial(Return& ret, T*& data, Body& body) { do_it_serial(ret, data, body); }
	static void do_it_serial(Return& ret, T* data, Body& body) {
		body.pre(*data);

		if (info->is_base(*data)) {
			body.post(body.base(*data), ret);
			return;
		}

		body.pre_rec(*data);

		const int c = info->num_children(*data);
		for (int i = 0; i < c; ++i) {
			T child;
			info->child(i, *data, child);
			do_it_serial(ret, &child, body);
		}

	}

};

///Specialization for I==0
template<typename Return, typename T, typename Info, typename Body>
const Info* dpr::internal::do_it_serial_stack_struct<Return, T, Info, Body, 0>::info;

///Specialization of do_it_serial_stack_struct for Return==void, I==0
template<typename T, typename Info, typename Body>
struct do_it_serial_stack_struct<void, T, Info, Body, 0> {
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

	//static void do_it_serial(T*& data, const Info& info_in, Body& body) { do_it_serial(data, info_in, body); }
	static void do_it_serial(T* data, const Info& info_in, Body& body) {
		do_it_serial(data, body);
	}

	//static void do_it_serial(T*& data, Body& body) { do_it_serial(data, body); }
	static void do_it_serial(T* data, Body& body) {
		body.pre(*data);

		if (info->is_base(*data)) {
			body.base(*data);
			return;
		}

		body.pre_rec(*data);

		const int c = info->num_children(*data);

		for (int i = 0; i < c; ++i) {
			T child;
			info->child(i, *data, child);
			do_it_serial(&child, body);
		}

	}
};

///Specialization for Return==void, I==0
template<typename T, typename Info, typename Body>
const Info* dpr::internal::do_it_serial_stack_struct<void, T, Info, Body, 0>::info;

///Specialization of do_it_serial_stack_struct for I==2
template<typename Return, typename T, typename Info, typename Body>
struct do_it_serial_stack_struct<Return, T, Info, Body, 2> {
	static const Info* info;

	static void do_it_serial(Return& ret, T&& data, const Info& info_in, Body& body) { do_it_serial(ret, data, info_in, body); }
	static void do_it_serial(Return& ret, T& data, const Info& info_in, Body& body) {
		do_it_serial(ret, data, body);
	}

	static void do_it_serial(Return& ret, T&& data, Body& body) { do_it_serial(ret, data, body); }
	static void do_it_serial(Return& ret, T& data, Body& body) {
		body.pre(data);

		if (info->is_base(data)) {
			body.post(body.base(data), ret);
			return;
		}

		body.pre_rec(data);
		do_it_serial(ret, info->child(0, data), body);
		do_it_serial(ret, info->child(1, data), body);

	}

	//static void do_it_serial(Return& ret, T*& data, const Info& info_in, Body& body) { do_it_serial(ret, data, info_in, body); }
	static void do_it_serial(Return& ret, T* data, const Info& info_in, Body& body) {
		do_it_serial(ret, data, body);
	}

	//static void do_it_serial(Return& ret, T*& data, Body& body) { do_it_serial(ret, data, body); }
	static void do_it_serial(Return& ret, T* data, Body& body) {
		body.pre(*data);

		if (info->is_base(*data)) {
			body.post(body.base(*data), ret);
			return;
		}

		body.pre_rec(*data);
		T child0;
		info->child(0, *data, child0);
		do_it_serial(ret, &child0, body);
		T child1;
		info->child(1, *data, child1);
		do_it_serial(ret, &child1, body);

	}

};

///Specialization for I==2
template<typename Return, typename T, typename Info, typename Body>
const Info* dpr::internal::do_it_serial_stack_struct<Return, T, Info, Body, 2>::info;

///Specialization of do_it_serial_stack_struct for Return==void, I==2
template<typename T, typename Info, typename Body>
struct do_it_serial_stack_struct<void, T, Info, Body, 2> {
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

	//static void do_it_serial(T*& data, const Info& info_in, Body& body) { do_it_serial(data, info_in, body); }
	static void do_it_serial(T* data, const Info& info_in, Body& body) {
		do_it_serial(data, body);
	}

	//static void do_it_serial(T*& data, Body& body) { do_it_serial(data, body); }
	static void do_it_serial(T* data, Body& body) {
		body.pre(*data);

		if (info->is_base(*data)) {
			body.base(*data);
			return;
		}

		body.pre_rec(*data);

		T child0;
		info->child(0, *data, child0);
		do_it_serial(&child0, body);
		T child1;
		info->child(1, *data, child1);
		do_it_serial(&child1, body);

	}
};

///Specialization for Return==void, I==2
template<typename T, typename Info, typename Body>
const Info* dpr::internal::do_it_serial_stack_struct<void, T, Info, Body, 2>::info;

template<typename Return, typename T, typename Info, typename Body>
class simple_stack_partitioner;

template<typename Return, typename T, typename Info, typename Body>
class auto_stack_partitioner;

template<typename Return, typename T, typename Info, typename Body>
class custom_stack_partitioner;

} // namespace internal

#ifndef DPR_SEQ_PARALLEL_RECURSION_H_
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

} // namespace dpr


#endif /* DPR_SEQ_PARALLEL_STACK_RECURSION_H_ */
