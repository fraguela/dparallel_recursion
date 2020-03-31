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

#include "dparallel_recursion/seq_parallel_recursion.h"

namespace dpr {

  /*! \namespace internal
   *
   * \brief Contains the non-public implementation of the library
   *
   */
namespace internal {

/// TBB parallel tasks that implements a ::parallel_recursion parallel task
template<typename Return, typename T, typename Info, typename Body, typename Partitioner>
class RecursionTask: public tbb::task {
  
protected:

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
  
protected:

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

} // namespace dpr


#endif /* DPR_PARALLEL_RECURSION_H_ */

