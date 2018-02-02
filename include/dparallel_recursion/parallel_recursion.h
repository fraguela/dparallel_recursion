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
/// \file     parallel_recursion.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_PARALLEL_RECURSION_H_
#define DPR_PARALLEL_RECURSION_H_

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

/// TBB parallel tasks that implements a ::parallel_recursion parallel task
template<typename Return, typename T, typename Info, typename Body, typename Partitioner>
class RecursionTask: public tbb::task {
	Return& _ret;
	general_reference_wrapper<T> _data;
	Body _body;
	int _level;

public:
	RecursionTask(Return& ret, general_reference_wrapper<T>&& data, const Body& body, int level) :
		_ret(ret), _data(std::move(data)), _body(body), _level(level) {
                  // printf("Task %s\n", Partitioner::do_parallel(*this) ? "Par" : "Seq");
	}

	tbb::task* execute() {
                const Info& _info = *do_it_serial_struct<Return, T, Info, Body, Info::NumChildren>::info;
		_body.pre(_data);

		if (_info.is_base(_data)) {
			_ret = _body.base(_data);
			return nullptr;
		}
          
                _body.pre_rec(_data);
          
		const int c = _info.num_children(_data);
#ifdef __clang__
                Return * const results = new Return[c]; //clang does not support variable length arrays of non-POD types
#else
		Return results[c];
#endif
          
		if (!Partitioner::do_parallel(*this)) { //BBF: Removed "&& 0"
                  
                        //PROFILEDEFINITION( const auto t0 = profile_clock_t::now() );

			for (int i = 0; i < c; ++i) {
				do_it_serial_struct<Return, T, Info, Body, Info::NumChildren>::do_it_serial(results[i], _info.child(i, _data), _info, _body);
			}

                        //PROFILEACTION(const auto t1 = profile_clock_t::now();
                        //            std::cerr << "Seq. Body run in " << profile_duration_t(t1 - t0).count() << " seconds\n";);

		} else {
			tbb::task_list list;

			for (int i = 0; i < c; ++i) {
				list.push_back(
					*new (tbb::task::allocate_child()) RecursionTask(results[i],
						general_reference_wrapper<T>(_info.child(i, _data)),
						_body, _level + 1));
			}

			set_ref_count(c + 1);
			spawn_and_wait_for_all(list);
		}

		_ret = _body.post(_data, results);
#ifdef __clang__
                delete [] results;
#endif
          
		return nullptr;
	}

	friend class simple_partitioner<Return, T, Info, Body>;

	friend class auto_partitioner<Return, T, Info, Body>;

	friend class custom_partitioner<Return, T, Info, Body>;
};

///Specialization of RecursionTask for Return==void
template<typename T, typename Info, typename Body, typename Partitioner>
class RecursionTask<void, T, Info, Body, Partitioner>: public tbb::task {
	general_reference_wrapper<T> _data;
	Body _body;
	int _level;

public:
	RecursionTask(general_reference_wrapper<T>&& data, const Body& body, int level) :
		_data(std::move(data)), _body(body), _level(level) {
                  // printf("Task %s\n", Partitioner::do_parallel(*this) ? "Par" : "Seq");
	}

	tbb::task* execute() {
               const Info& _info = *do_it_serial_struct<void, T, Info, Body, Info::NumChildren>::info;
		_body.pre(_data);

		if (_info.is_base(_data)) {
			_body.base(_data);
			return nullptr;
		}
          
                _body.pre_rec(_data);

                const int c = _info.num_children(_data);

		if (!Partitioner::do_parallel(*this)) {

			//PROFILEDEFINITION( const auto t0 = profile_clock_t::now() );

			for (int i = 0; i < c; ++i)
				do_it_serial_struct<void, T, Info, Body, Info::NumChildren>::do_it_serial(_info.child(i, _data), _info, _body);

                        //PROFILEACTION(const auto t1 = profile_clock_t::now();
                        //              std::cerr << "Seq. Body run in " << profile_duration_t(t1 - t0).count() << " seconds\n";);

		} else {
			tbb::task_list list;

			for (int i = 0; i < c; ++i)
				list.push_back(
					*new (tbb::task::allocate_child()) RecursionTask(general_reference_wrapper<T>(_info.child(i, _data)),
							_body, _level + 1));

			set_ref_count(c + 1);
			spawn_and_wait_for_all(list);

		}

		//_body.post(_data, results);
		return nullptr;
	}

	friend class simple_partitioner<void, T, Info, Body>;

	friend class auto_partitioner<void, T, Info, Body>;

	friend class custom_partitioner<void, T, Info, Body>;
};

template<typename Return, typename T, typename Info, typename Body, typename Partitioner>
class start_recursion {
	typedef RecursionTask<Return, T, Info, Body, Partitioner> task_t;

public:
	static Return run(general_reference_wrapper<T> root, const Info& info, Body body) {
		Return r;
                do_it_serial_struct<Return, T, Info, Body, Info::NumChildren>::info = &info;
                task_t& t = *new (tbb::task::allocate_root()) task_t(r, std::move(root), body, 0);
		tbb::task::spawn_root_and_wait(t);
		return r;
	}
};

/// Specialization for Return==void
template<typename T, typename Info, typename Body, typename Partitioner>
class start_recursion<void, T, Info, Body, Partitioner> {
	typedef RecursionTask<void, T, Info, Body, Partitioner> task_t;

public:
	static void run(general_reference_wrapper<T> root, const Info& info, Body body) {
                do_it_serial_struct<void, T, Info, Body, Info::NumChildren>::info = &info;
		task_t& t = *new (tbb::task::allocate_root()) task_t(std::move(root), body, 0);
		tbb::task::spawn_root_and_wait(t);
	}
};

/// Implementation of the simple parallelization strategy
template<typename Return, typename T, typename Info, typename Body>
struct simple_partitioner {
	typedef simple_partitioner<Return, T, Info, Body> partitioner;
	typedef internal::RecursionTask<Return, T, Info, Body, partitioner> task_t;

	/** Since in the current implementation Partitioner::do_parallel in only called
	  * after r._info.is_base(_data) has returned false, there is no need to reevaluate it
	  */
	static bool do_parallel(task_t& r) noexcept {
		return true; //!r._info.is_base(r._data);
	}
};

/// Implementation of the automatic parallelization strategy
template<typename Return, typename T, typename Info, typename Body>
struct auto_partitioner {
	typedef auto_partitioner<Return, T, Info, Body> partitioner;
	typedef internal::RecursionTask<Return, T, Info, Body, partitioner> task_t;

        /* BBF
	static const int LevelLimits[];
	static const int LevelLimit;
        */
  
	static bool do_parallel(task_t& r) noexcept {
                return r._level < do_it_serial_struct<Return, T, Info, Body, Info::NumChildren>::info->parLevel_; //BBF: LevelLimit;
	}
};

/* BBF
template<typename Return, typename T, typename Info, typename Body>
//const int auto_partitioner<Return, T, Info, Body>::LevelLimits[] =  {10, 0, 15, 10, 8, 7, 6, 6, 5};
const int auto_partitioner<Return, T, Info, Body>::LevelLimits[] =  {8, 0, 15, 8, 7, 6, 5, 5, 4};

template<typename Return, typename T, typename Info, typename Body>
const int auto_partitioner<Return, T, Info, Body>::LevelLimit =
	(Info::NumChildren < (sizeof(LevelLimits) / sizeof(int))) ? LevelLimits[Info::NumChildren] : 4;
*/

/// Implementation of the customized parallelization strategy
template<typename Return, typename T, typename Info, typename Body>
struct custom_partitioner {
	typedef custom_partitioner<Return, T, Info, Body> partitioner;
	typedef internal::RecursionTask<Return, T, Info, Body, partitioner> task_t;

	static bool do_parallel(task_t& r) noexcept(noexcept(do_it_serial_struct<Return, T, Info, Body, Info::NumChildren>::info->do_parallel(r._data))) {
		return do_it_serial_struct<Return, T, Info, Body, Info::NumChildren>::info->do_parallel(r._data);
	}
};
  
} // namespace internal

/// Provides the public interface to the partitioner implementations
namespace partitioner {

/// Request parallelization with granularity automatically controlled by the library
struct automatic {  };

/// Request simple parallelization, that is, each base case is a parallel task
struct simple {  };

/// Request custom parallelization, controlled by the Info::do_parallel method
struct custom {  };
}

/** When no partitioner is specified, the internal::simple_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return parallel_recursion(T&& root, const Info& info, Body body) {
	return internal::start_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_recursion(T&& root, const Info& info, Body body, partitioner::simple) {
	return internal::start_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(
			   std::forward<T>(root), info, body);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_recursion(T&& root, const Info& info, Body body, partitioner::automatic) {
	return internal::start_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::auto_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(
			   std::forward<T>(root), info, body);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_recursion(T&& root, const Info& info, Body body, partitioner::custom) {
	return internal::start_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::custom_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(
			   std::forward<T>(root), info, body);
}

/// Facilitates the initialization of the parallel_recursion TBB environment
template<typename T>
inline void pr_init(T nthreads) {
  static_assert(std::is_integral<T>::value, "Integer required.");
  new tbb::task_scheduler_init(nthreads);
}

} // namespace dpr


#endif /* DPR_PARALLEL_RECURSION_H_ */

