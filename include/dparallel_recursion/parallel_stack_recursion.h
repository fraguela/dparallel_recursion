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
/// \file     parallel_stack_recursion.h
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#ifndef DPR_PARALLEL_STACK_RECURSION_H_
#define DPR_PARALLEL_STACK_RECURSION_H_

#include <iostream>
#include <thread>
#include <mutex>
#ifdef DPR_DONT_USE_SPINLOCK
#include <condition_variable>
#endif
#include <atomic>
#include <vector>
#include <utility>
#include <algorithm>
#include <iterator>
#include <chrono>
#include <map>
#include <limits>
#include "dparallel_recursion/dpr_utils.h"
#include "dparallel_recursion/general_reference_wrapper.h"
#include "dparallel_recursion/EmptyBody.h"
#include "dparallel_recursion/Arity.h"
#include "dparallel_recursion/seq_parallel_stack_recursion.h"
#include "dparallel_recursion/ChunkSelector.h"

namespace dpr {

/*! \namespace internal
*
* \brief Contains the non-public implementation of the library
*
*/
namespace internal {

#ifndef DPR_SS_WORK
#define DPR_SS_WORK    0
#endif
#ifndef DPR_SS_SEARCH
#define DPR_SS_SEARCH  1
#endif
#ifndef DPR_SS_IDLE
#define DPR_SS_IDLE    2
#endif
#ifndef DPR_SS_OVH
#define DPR_SS_OVH     3
#endif
#ifndef DPR_SS_CBOVH
#define DPR_SS_CBOVH   4
#endif
#ifndef DPR_SS_NSTATES
#define DPR_SS_NSTATES 5
#endif

inline std::exception_ptr& globalExceptionPtr() {
	static std::exception_ptr globalExceptionPtr = nullptr;
	return globalExceptionPtr;
}

template<typename T>
struct StealStack {
	int workAvail;                                      /* elements available for stealing, 4 bytes, shared variable */
	int sharedStart;                                    /* index of start of shared portion of stack, 4 bytes, shared variable */
	std::mutex stackLock;                               /* lock for manipulation of shared portion, 40 bytes, shared mutex */
	char falseSharingPadding[64*((sizeof(int)*2+sizeof(std::mutex))/64+1)-sizeof(int)*2-sizeof(std::mutex)];                         /* 4*4=16 bytes padding */
	//////////// 64 bytes ////////////
	std::vector<T> stack;                               /* addr of actual stack of nodes in local addr space, 24 bytes */
	unsigned long long int nNodes;                      /* number of nodes processed, 8 bytes */
	int stackSize;                                      /* total space avail (in number of elements), 4 bytes */
	int local;                                          /* index of start of local portion, 4 bytes */
	int top;                                            /* index of stack top, 4 bytes */
	char falseSharingPadding2[64*((sizeof(std::vector<T>)+sizeof(unsigned long long int)+sizeof(int)*3)/64+1)-sizeof(std::vector<T>)-sizeof(unsigned long long int)-sizeof(int)*3];                        /* 4*5=20 bytes padding */
	//////////// 64 bytes ////////////
#ifdef PROFILE
	int nLeaves;                                        /* tree stats, 4 bytes */
	int maxStackDepth;                                  /* stack stats, 4 bytes */
	int nAcquire, nRelease, nSteal, nFail;              /* steal stats, 4*4=16 bytes */
	int wakeups, falseWakeups;                          /* 4*2=8 bytes */
	unsigned long long int nNodes_last;                 /* 8 bytes */
	std::chrono::steady_clock::time_point timeLast;     /* perf measurements, 8 bytes */
	char falseSharingPadding3[64*((sizeof(int)*8+sizeof(unsigned long long int)+sizeof(std::chrono::steady_clock::time_point))/64+1)-sizeof(int)*8-sizeof(unsigned long long int)-sizeof(std::chrono::steady_clock::time_point)];                        /* 4*4=16 bytes padding */
	//////////// 64 bytes ////////////
	double time[DPR_SS_NSTATES];                        /* 8*5=40 bytes */
	int entries[DPR_SS_NSTATES], curState;              /* 4*5+4=24 bytes */
	//////////// 64 bytes ////////////
#endif
};

template<typename T, typename Info>
class parallel_stack_recursion_internal {

public:
	const int threadId;
	const int num_total_threads;
	const int chunkSize;
	const int doubleChunkSize;

	struct cb_t {
#ifdef DPR_DONT_USE_SPINLOCK
		std::mutex cb_cv_mtx;
		std::condition_variable cb_cv;
#endif
		std::atomic<int> cb_count;
		volatile bool cb_cancel;
		volatile bool cb_done;
#ifdef DPR_DONT_USE_SPINLOCK
		char cb_padding[64*((sizeof(std::mutex)+sizeof(std::condition_variable)+sizeof(bool)*2+sizeof(std::atomic<int>))/64+1)-sizeof(std::mutex)-sizeof(std::condition_variable)-sizeof(bool)*2-sizeof(std::atomic<int>)];
#else
		char cb_padding[64*((sizeof(bool)*2+sizeof(std::atomic<int>))/64+1)-sizeof(bool)*2-sizeof(std::atomic<int>)];
#endif
	};

	alignas(64) static cb_t cb;

	static internal::StealStack<T> **stealStack;

	parallel_stack_recursion_internal(const int threadId_, const int num_total_threads_, const int chunkSize_) : threadId(threadId_), num_total_threads(num_total_threads_), chunkSize(chunkSize_), doubleChunkSize(chunkSize_*2)
	{ }

#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS) || (defined(DPR_DENY_STACK_RESIZE) && !defined(DPR_SKIP_CHECK_STACKOVERFLOW))
	void ss_error(const std::string str) {
		std::string err_str = "*** [Thread " + std::to_string(threadId) + "] " + str;
		throw std::runtime_error(err_str);
	}
#endif

	/* restore stack to empty state */
	static void ss_mkEmpty(internal::StealStack<T> *s) {
		std::lock_guard<std::mutex> lck(s->stackLock);
		s->sharedStart = 0;
		s->local  = 0;
		s->top    = 0;
		s->workAvail = 0;
	}

	static void ss_init(internal::StealStack<T> *s, const int nelts) {
		// allocate stack in shared addr space with affinity to calling thread
		// and record local addr for efficient access in sequel
		s->stack.resize(nelts);
		s->stackSize = nelts;
		s->nNodes = 0;
#ifdef PROFILE
		s->nLeaves = 0;
		s->maxStackDepth = 0;
		//s->maxTreeDepth = 0;
		s->nAcquire = 0;
		s->nRelease = 0;
		s->nSteal = 0;
		s->nFail = 0;
		s->wakeups = 0;
		s->falseWakeups = 0;
		s->nNodes_last = 0;
#endif //PROFILE
		ss_mkEmpty(s);
	}

#ifdef PROFILE
	static void ss_initState(internal::StealStack<T> *s) {
		s->timeLast = getTimepoint();
		for (int i = 0; i < DPR_SS_NSTATES; i++) {
			s->time[i] = 0.0;
			s->entries[i] = 0;
		}
		s->curState = DPR_SS_IDLE;
	}

	void ss_setState(internal::StealStack<T> *s, const int state){
		if (state < 0 || state >= DPR_SS_NSTATES) {
			ss_error("ss_setState: thread state out of range");
		}
		if (state == s->curState) {
			return;
		}
		const std::chrono::steady_clock::time_point time = getTimepoint();
		s->time[s->curState] += getTimediff(s->timeLast, time);
		s->entries[state]++;
		s->timeLast = time;
		s->curState = state;
	}

#endif //PROFILE

#ifndef DPR_DENY_STACK_RESIZE
	/* local grow the size of the stack */
	void ss_growStackSize(internal::StealStack<T> *s) {
		std::lock_guard<std::mutex> lck(s->stackLock);
		s->stackSize += ((s->stackSize >> 1) + 2);
		if ((s->sharedStart > 0) && (s->top > s->sharedStart)) {
			std::vector<T> newStack;
			newStack.reserve(s->stackSize);
			newStack.insert(newStack.begin(), std::make_move_iterator(s->stack.begin()+s->sharedStart), std::make_move_iterator(s->stack.begin()+s->top));
			newStack.resize(s->stackSize);
			s->stack = std::move(newStack);
			s->top -= s->sharedStart;
			s->local -= s->sharedStart;
			s->sharedStart = 0;
		} else {
			s->stack.resize(s->stackSize);
		}
	}

	/* local grow the size of the stack and modify index */
	void ss_growStackSize(internal::StealStack<T> *s, int& index) {
		std::lock_guard<std::mutex> lck(s->stackLock);
		s->stackSize += ((s->stackSize >> 1) + 2);
		if ((s->sharedStart > 0) && (s->top > s->sharedStart)) {
			std::vector<T> newStack;
			newStack.reserve(s->stackSize);
			newStack.insert(newStack.begin(), std::make_move_iterator(s->stack.begin()+s->sharedStart), std::make_move_iterator(s->stack.begin()+s->top));
			newStack.resize(s->stackSize);
			s->stack = std::move(newStack);
			s->top -= s->sharedStart;
			s->local -= s->sharedStart;
			index -= s->sharedStart;
			s->sharedStart = 0;
		} else {
			s->stack.resize(s->stackSize);
		}
	}
#endif

	/* check the stack size of the stack */
	void ss_checkStackSize(internal::StealStack<T> *s, const int numToAdd, T *&data) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top+numToAdd > s->stackSize) {
			ss_error("ss_checkStackSize: stack overflow");
		}
#endif
#else
		while(s->top+numToAdd > s->stackSize) {
			ss_growStackSize(s);
			data = ss_top(s);
		}
#endif
	}

	/* local push */
	void ss_push(internal::StealStack<T> *s, const T& c) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s);
		}
#endif
		s->stack[s->top] = c;
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c.height));
	}

	/* local push (with inner call to child generator instead of receive child directly) */
	void ss_push(internal::StealStack<T> *s, const Info& info, const int i, T& data) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s);
		}
#endif
		s->stack[s->top] = info.child(i, data);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local push and keep a valid reference to data */
	void ss_push(internal::StealStack<T> *s, const T &c, T *&data, int data_index) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s, data_index);
			data = &(s->stack[data_index]);
		}
#endif
		s->stack[s->top] = c;
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local push (with inner call to child generator instead of receive child directly) and keep a valid reference to data */
	void ss_push(internal::StealStack<T> *s, const Info& info, const int i, T *&data, int data_index) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s, data_index);
			data = &(s->stack[data_index]);
		}
#endif
		s->stack[s->top] = info.child(i, *data);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local simple push - only push, don't check the stack size */
	void ss_push_simple(internal::StealStack<T> *s, const T& c) {
		s->stack[s->top] = c;
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c.height));
	}

	/* local simple push (with inner call to child generator instead of receive child directly) - only push, don't check the stack size */
	void ss_push_simple(internal::StealStack<T> *s, const Info& info, const int i, T& data) {
		s->stack[s->top] = info.child(i, data);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local simple push (with inner call to child generator instead of receive child directly) and keep a valid reference to data - only push, don't check the stack size */
	void ss_push_simple(internal::StealStack<T> *s, const Info& info, const int i, T *&data) {
		s->stack[s->top] = info.child(i, *data);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local push delayed (get memory address for future copy instead copy directly) */
	void ss_push_delayed(internal::StealStack<T> *s, T *&c) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push_delayed: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s);
		}
#endif
		c = &(s->stack[s->top]);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local push delayed (get memory address for future copy instead copy directly) and keep a valid reference to data */
	void ss_push_delayed(internal::StealStack<T> *s, T *&c, T *&data, const int data_index) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push_delayed: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s, data_index);
			data = &(s->stack[data_index]);
		}
#endif
		c = &(s->stack[s->top]);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local top:  get local addr of node at top */
	inline T* ss_top(internal::StealStack<T> *s) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->top <= s->local) {
			ss_error("ss_top: empty local stack");
		}
#endif
		return &(s->stack[(s->top) - 1]);
	}

	/* local set value at position: change single element of the stack by other */
	inline void ss_setvalue_pos(internal::StealStack<T> *s, const T& c, const int position) {
		s->stack[position] = c;
	}

	/* local set value at position delayed: change single element of the stack by other (get memory address for future copy instead copy directly) */
	inline void ss_setvalue_pos_delayed(internal::StealStack<T> *s, T *&c, const int position) {
		c = &(s->stack[position]);
	}

	/* local pop */
	inline void ss_pop(internal::StealStack<T> *s) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->top <= s->local) {
			ss_error("ss_pop: empty local stack");
		}
#endif
		s->top--;
	}

	/* local top position:  stack index of top element */
	inline int ss_topPosn(const internal::StealStack<T> *s) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->top <= s->local) {
			ss_error("ss_topPosn: empty local stack");
		}
#endif
		return s->top - 1;
	}

	/* local depth */
	static constexpr int ss_localDepth(const internal::StealStack<T> *s) {
		return (s->top - s->local);
	}

	/* release k values from bottom of local stack */
	void ss_release(internal::StealStack<T> *s, const int chunkSize) {
		std::lock_guard<std::mutex> lck(s->stackLock);
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->top - s->local >= chunkSize) {
#endif
			s->local += chunkSize;
			s->workAvail += chunkSize;
			PROFILEACTION(s->nRelease++);
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		} else {
			ss_error("ss_release: do not have chunks vals to release");
		}
#endif
	}

	/* move chunkSize values from top of shared stack into local stack
	 * return false if chunkSize vals are not avail on shared stack
	 */
	bool ss_acquire(internal::StealStack<T> *s, const int chunkSize) {
		std::lock_guard<std::mutex> lck(s->stackLock);
		if ((s->local - s->sharedStart) >= chunkSize) {
			s->local -= chunkSize;
			s->workAvail -= chunkSize;
			PROFILEACTION(s->nAcquire++);
			return true;
		} else {
			return false;
		}
	}

	bool ss_steal(internal::StealStack<T> *s, const int victim, const int chunkSize) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->sharedStart != s->top) {
			ss_error("ss_steal: thief attempts to steal onto non-empty stack");
		}
#endif

#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top + chunkSize >= s->stackSize) {
			ss_error("ss_steal: steal will overflow thief's stack");
		}
#endif
#else
		while(s->top + chunkSize >= s->stackSize) {
			ss_growStackSize(s);
		}
#endif

		/* lock victim stack and try to reserve chunkSize elts */

		std::lock_guard<std::mutex> lck(stealStack[victim]->stackLock);

		const int victimWorkAvail = stealStack[victim]->workAvail;

		if (victimWorkAvail >= chunkSize) {
			/* reserve a chunk */
			const int victimShared = stealStack[victim]->sharedStart;
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
			const int victimLocal = stealStack[victim]->local;
			if (victimLocal - victimShared != victimWorkAvail) {
				ss_error("ss_steal: stealStack invariant violated");
			}
#endif
			stealStack[victim]->sharedStart = victimShared + chunkSize;
			stealStack[victim]->workAvail -= chunkSize;

			/* if k elts reserved, move them to local portion of our stack */
			std::copy_n(stealStack[victim]->stack.begin() + victimShared, chunkSize, s->stack.begin()+ s->top);

			PROFILEACTION(s->nSteal++);
			s->top += chunkSize;
			return true;
		} else {
			PROFILEACTION(s->nFail++);
			return false;
		}

	}

	inline int findwork(const int chunkSize) {
		for (int i = 1; i < num_total_threads; i++) {
			const int v = (threadId + i) % num_total_threads;
			if (stealStack[v]->workAvail >= chunkSize) {
				return v;
			}
		}
		return -1;
	}

	void releaseNodes(internal::StealStack<T> *ss){
		if (ss_localDepth(ss) > doubleChunkSize) {
			// Attribute this time to runtime overhead
			PROFILEACTION(ss_setState(ss, DPR_SS_OVH));
			ss_release(ss, chunkSize);
			PROFILEACTION(ss_setState(ss, DPR_SS_CBOVH));
			cbarrier_cancel();
			PROFILEACTION(ss_setState(ss, DPR_SS_WORK));
		}
	}

	void releaseMultipleNodes(internal::StealStack<T> *ss){
		if (ss_localDepth(ss) > doubleChunkSize) {
			// Attribute this time to runtime overhead
			PROFILEACTION(ss_setState(ss, DPR_SS_OVH));
			ss_release(ss, chunkSize*(ss_localDepth(ss) / chunkSize - 1));
			//// This has significant overhead on clusters!
			PROFILEACTION(ss_setState(ss, DPR_SS_CBOVH));
			cbarrier_cancel();
			PROFILEACTION(ss_setState(ss, DPR_SS_WORK));
		}
	}

	static void cb_init(){
		cb.cb_count.store(0, std::memory_order_relaxed);
		cb.cb_cancel = false;
		cb.cb_done = false;
		globalExceptionPtr() = nullptr;
	}

	//  delay this thread until all threads arrive at barrier or until barrier is cancelled
	bool cbarrier_wait() {
		cb.cb_count.fetch_add(1, std::memory_order_relaxed);
		if (cb.cb_count.load(std::memory_order_relaxed) == num_total_threads) {
#ifdef DPR_DONT_USE_SPINLOCK
			std::lock_guard<std::mutex> lck(cb.cb_cv_mtx);
			cb.cb_done = true;
			cb.cb_cv.notify_all();
#else
			cb.cb_done = true;
#endif
		}

#ifdef PROFILE
		if (stealStack[threadId]->nNodes_last == stealStack[threadId]->nNodes) {
			++stealStack[threadId]->falseWakeups;
		}
		stealStack[threadId]->nNodes_last = stealStack[threadId]->nNodes;
#endif //PROFILE

#ifdef DPR_DONT_USE_SPINLOCK
		{
			std::unique_lock<std::mutex> lck(cb.cb_cv_mtx);
			cb.cb_cv.wait(lck, [this] { return (cb.cb_cancel || cb.cb_done); } );
		}
#else
		// busy waiting: spinlock
		while (!cb.cb_cancel && !cb.cb_done);
#endif

		cb.cb_count.fetch_sub(1, std::memory_order_relaxed);
		cb.cb_cancel = false;
		PROFILEACTION(++stealStack[threadId]->wakeups);

		return cb.cb_done;
	}

	// causes one or more threads waiting at barrier, if any, to be released
	inline static void cbarrier_cancel() {
#ifdef DPR_DONT_USE_SPINLOCK
		std::lock_guard<std::mutex> lck(cb.cb_cv_mtx);
		cb.cb_cancel = true;
		cb.cb_cv.notify_all();
#else
		cb.cb_cancel = true;
#endif
	}

};

template<typename T, typename Info>
alignas(64) typename parallel_stack_recursion_internal<T, Info>::cb_t parallel_stack_recursion_internal<T, Info>::cb;

template<typename T, typename Info>
internal::StealStack<T>** parallel_stack_recursion_internal<T, Info>::stealStack;


template<typename Return, typename T, typename Info, typename Body, typename Partitioner>
class start_stack_recursion {
	typedef dpr::internal::parallel_stack_recursion_internal<typename std::remove_reference<T>::type, Info> internal_psr;

public:

	static Return run(general_reference_wrapper<T> root, const Info& info, Body body, const int num_total_threads = std::thread::hardware_concurrency(), const int chunkSizeI = dpr::internal::defaultChunkSize, const AutomaticChunkOptions opt = dpr::aco_default, typename std::decay<T>::type * const rootTestV = nullptr) {
		assert(chunkSizeI >= 0);
		int chunkSize = chunkSizeI;
		dpr::internal::lastPsrRunExtraInfo().chunkSize = chunkSize;
		dpr::internal::lastPsrRunExtraInfo().nthreads = num_total_threads;
		std::chrono::steady_clock::time_point init_run_time;
		if (chunkSize <= 0) {
			T rootTest((rootTestV == nullptr) ? root.get() : *rootTestV);
			const std::chrono::steady_clock::time_point init_test_time = getTimepoint();
			const std::vector<dpr::ResultChunkTest> testResults = run_test(std::forward<T>(rootTest), info, body, num_total_threads, chunkSize, opt);
			if (testResults.size() > 0) {
				chunkSize = testResults[0].chunkId;
			} else {
				chunkSize = defaultChunkSize;
			}
			init_run_time = getTimepoint();
			dpr::internal::lastPsrRunExtraInfo().testChunkTime = getTimediff(init_test_time, init_run_time);
		} else {
			init_run_time = getTimepoint();
			dpr::internal::lastPsrRunExtraInfo().testChunkTime = 0.0;
		}
		dpr::internal::lastPsrRunExtraInfo().chunkSizeUsed = chunkSize;

		std::vector<Return> r(num_total_threads);

		internal_psr::cb_init();
		internal_psr::stealStack = new internal::StealStack<typename std::remove_reference<T>::type>*[num_total_threads];

		internal::AlignedBufferedAllocator stealStackAllocator((sizeof(internal::StealStack<typename std::remove_reference<T>::type>)+64)*num_total_threads);

		for (int threadId = 0; threadId < num_total_threads; threadId++) {
			internal_psr::stealStack[threadId] = stealStackAllocator.aligned_alloc<internal::StealStack<typename std::remove_reference<T>::type>>(64);
			internal_psr::ss_init(internal_psr::stealStack[threadId], internal::initialStackSize());
		}

		do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info = &info;
		std::vector<std::thread> threads;
		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			PROFILEACTION(internal_psr::ss_initState(internal_psr::stealStack[threadId]));
			threads.push_back(std::thread(run_thread, threadId, num_total_threads, chunkSize, internal_psr::stealStack[threadId], std::ref(root), info, body, std::ref(r[threadId])));
		}

		Return rr{};
		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			threads[threadId].join();
			if (globalExceptionPtr() == nullptr) {
				body.post(r[threadId], rr);
			}
		}
		if (globalExceptionPtr() != nullptr) {
			try {
				std::rethrow_exception(globalExceptionPtr());
			}catch (const std::exception &ex) {
				std::cerr << ex.what() << std::endl;
				std::rethrow_exception(globalExceptionPtr());
			}
		}
		threads.clear();

		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			internal_psr::stealStack[threadId]->stack.~vector();
		}
		delete[] internal_psr::stealStack;

		dpr::internal::lastPsrRunExtraInfo().runTime = getTimediff(init_run_time, getTimepoint());
		return rr;
	}

	static std::vector<dpr::ResultChunkTest> run_test(general_reference_wrapper<T> root, const Info& info, Body body, const int num_total_threads = std::thread::hardware_concurrency(), const int chunkSizeI = 0, const AutomaticChunkOptions opt = dpr::aco_test_default) {
		assert(chunkSizeI >= 0);
		std::vector<dpr::ResultChunkTest> res;
		std::vector<Return> r(num_total_threads);
		std::vector<unsigned long long int> nNodesArr(num_total_threads);
		std::vector<char> exitNormallyArr(num_total_threads);
		const size_t softLimitTestsSizeTest = 8;
		const size_t hardLimitTestsSizeTest = 14;
		const double softLimitMargin = 0.3;
		const double errorMargingCalculateSizeTest = 0.15;
		const int minorSizeTest = 250;

		dpr::internal::ChunkSelector chunkSel(opt, chunkSizeI);

		std::map<int,unsigned long long int> mapSizeTestChunks;
		for (const std::pair<int, dpr::internal::chunk_attr> &iChunk : chunkSel.selectedChunks) {
			mapSizeTestChunks[iChunk.first] = 0;
		}

		double estimatedTimePerTest = opt.targetTimePerTest;
		if ((opt.limitTimeOfEachTest) && (opt.testSize == 0) && (estimatedTimePerTest <= 0.0000001)) {
			estimatedTimePerTest = chunkSel.estimateNeededTimePerTest();
			if (opt.verbose > 3) {
				std::cout << " [?] It has been estimated a necessary time per test of " << estimatedTimePerTest << " sec [±" << static_cast<int>(errorMargingCalculateSizeTest*100) << "%]" << std::endl;
			}
		} else {
			if (opt.verbose > 3) {
				if (opt.limitTimeOfEachTest) {
					if (opt.testSize == 0) {
						std::cout << " [?] A necessary time per test " << estimatedTimePerTest << " sec [±" << static_cast<int>(errorMargingCalculateSizeTest*100) << "%]" << " has been established" << std::endl;
					} else {
						std::cout << " [?] A fixed test size of " << opt.testSize << " nodes will be used" << std::endl;
					}
				} else {
					std::cout << " [?] No test size or test time limit has been established" << std::endl;
				}
			}
		}

		int checkNo = chunkSel.getNextChunk();
		unsigned long long int iSizeTest;
		while(checkNo > 0) {
			if (opt.verbose > 2) {
				std::cout << "  - Testing Chunk " << checkNo << "..." << std::flush;
			}
			if (opt.limitTimeOfEachTest) {
				iSizeTest = opt.testSize;
			} else {
				iSizeTest = std::numeric_limits<unsigned long long int>::max();
			}
			if (iSizeTest == 0) {
				iSizeTest = mapSizeTestChunks[checkNo];
			}
			if (iSizeTest == 0) {
				iSizeTest = minorSizeTest;
				int minorDistance = std::numeric_limits<int>::max();
				int minorDistChunk = -1;
				for (const std::pair<int, unsigned long long int> &iMap : mapSizeTestChunks) {
					if (iMap.second != 0) {
						const int distanceChunk = std::abs(checkNo - iMap.first);
						if (distanceChunk < minorDistance) {
							minorDistChunk = iMap.first;
							minorDistance = distanceChunk;
						}
					}
				}
				if (minorDistChunk > 0) {
					iSizeTest = mapSizeTestChunks[minorDistChunk];
				}
			}
			std::map<unsigned long long int, std::pair<double, double>> mapHistorySizeTests;
			char exitNormally = 0;
			bool firstMandatoryExecution = true;
			bool needMoreNodes = true;
			double mostAccurate = std::numeric_limits<double>::max();
			unsigned long long int bestSizeTest = 0;
			bool underLimit = false;

			while (firstMandatoryExecution || (
					(mapSizeTestChunks[checkNo] == 0) &&
					(mapHistorySizeTests.find(iSizeTest) == mapHistorySizeTests.end()) &&
					(mapHistorySizeTests.size() < hardLimitTestsSizeTest) &&
					(!underLimit) &&
					((mapHistorySizeTests.size() < softLimitTestsSizeTest) || (std::abs(mostAccurate) > softLimitMargin)) &&
					(!exitNormally || !needMoreNodes)
			)) {
				if ((opt.verbose > 3) && (opt.testSize == 0) && (mapSizeTestChunks[checkNo] == 0)) {
					if (firstMandatoryExecution) {
						std::cout << std::endl;
					}
					if (opt.limitTimeOfEachTest) {
						std::cout << "    · Testing with " << iSizeTest << " nodes..." << std::flush;
					}
				}
				firstMandatoryExecution = false;

				general_reference_wrapper<T> localRoot = call_clone<T>(root.get());

				unsigned long long int nNodes = 0;
				exitNormally = 1;

				const std::chrono::steady_clock::time_point init_time = getTimepoint();

				internal_psr::cb_init();
				internal_psr::stealStack = new internal::StealStack<typename std::remove_reference<T>::type>*[num_total_threads];

				internal::AlignedBufferedAllocator stealStackAllocator((sizeof(internal::StealStack<typename std::remove_reference<T>::type>)+64)*num_total_threads);

				for (int threadId = 0; threadId < num_total_threads; threadId++) {
					internal_psr::stealStack[threadId] = stealStackAllocator.aligned_alloc<internal::StealStack<typename std::remove_reference<T>::type>>(64);
					internal_psr::ss_init(internal_psr::stealStack[threadId], internal::initialStackSize());
				}

				do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info = &info;
				std::vector<std::thread> threads;
				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					PROFILEACTION(internal_psr::ss_initState(internal_psr::stealStack[threadId]));
					threads.push_back(std::thread(run_thread_test, threadId, num_total_threads, checkNo, internal_psr::stealStack[threadId], std::ref(localRoot), info, body, std::ref(r[threadId]), std::ref(nNodesArr[threadId]), iSizeTest, std::ref(exitNormallyArr[threadId])));
				}

				Return rr{};
				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					threads[threadId].join();
					if (globalExceptionPtr() == nullptr) {
						body.post(r[threadId], rr);
					}
				}
				if (globalExceptionPtr() != nullptr) {
					try {
						std::rethrow_exception(globalExceptionPtr());
					}catch (const std::exception &ex) {
						std::cerr << ex.what() << std::endl;
						std::rethrow_exception(globalExceptionPtr());
					}
				}
				threads.clear();

				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					internal_psr::stealStack[threadId]->stack.~vector();
				}
				delete[] internal_psr::stealStack;

				const std::chrono::steady_clock::time_point finish_time = getTimepoint();
				const double total_time = getTimediff(init_time, finish_time);

				call_deallocate<T>(localRoot.get());

				for (int i = 0; i < num_total_threads; i++) {
					nNodes += nNodesArr[i];
				}
				if (opt.limitTimeOfEachTest) {
					for (int i = 0; i < num_total_threads; i++) {
						exitNormally &= exitNormallyArr[i];
					}
				}
				const double nSpeed = nNodes / total_time;
				if ((opt.testSize == 0) && (opt.limitTimeOfEachTest)) {
					if (mapSizeTestChunks[checkNo] == 0) {
						const double diff = (estimatedTimePerTest-total_time)/estimatedTimePerTest;
						mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
						if (std::abs(diff) < std::abs(mostAccurate)) {
							bestSizeTest = iSizeTest;
							mostAccurate = diff;
						}
						if (std::abs(diff) < errorMargingCalculateSizeTest) {
							mapSizeTestChunks[checkNo] = iSizeTest;
						} else {
							if (diff > 0.96) {
								needMoreNodes = true;
								iSizeTest *= 25;
							} else if (diff > 0.0) {
								needMoreNodes = true;
								iSizeTest *= estimatedTimePerTest/total_time;
							} else if (diff < 0.0) {
								needMoreNodes = false;
								if (iSizeTest > minorSizeTest) {
									iSizeTest *= estimatedTimePerTest/total_time;
								} else {
									underLimit = true;
								}
							} else if (diff < -3.0) {
								needMoreNodes = false;
								iSizeTest *= 0.25;
							}
							if (iSizeTest < minorSizeTest) {
								iSizeTest = minorSizeTest;
							}
							while (mapHistorySizeTests.find(iSizeTest) != mapHistorySizeTests.end()) {
								iSizeTest += minorSizeTest;
							}
						}

						if (opt.verbose > 3) {
							if (mapSizeTestChunks[checkNo] == 0) {
								std::cout << "\tFailure! Time: " << total_time << " sec. Error: " << ((diff <= 0.0) ? "+" : "-") << std::abs(diff*100) << "%" << std::endl;
							} else {
								std::cout << "\tDone! Time: " << total_time << " sec. Error: " << ((diff <= 0.0) ? "+" : "-") << std::abs(diff*100) << "%" << std::endl << "  ";
							}
						}
					} else {
						mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
					}
				} else {
					mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
					mapSizeTestChunks[checkNo] = iSizeTest;
				}
			}
			if (mapSizeTestChunks[checkNo] == 0) {
				if (bestSizeTest > 0) {
					mapSizeTestChunks[checkNo] = bestSizeTest;
					if ((opt.verbose > 3) && (opt.testSize == 0)) {
						if ((mapHistorySizeTests.size() >= hardLimitTestsSizeTest)) {
							std::cout << "    · Hard limit of " << hardLimitTestsSizeTest << " test reached.";
						} else if ((mapHistorySizeTests.size() >= softLimitTestsSizeTest) && (std::abs(mostAccurate) <= softLimitMargin)) {
							std::cout << "    · Soft limit of " << softLimitTestsSizeTest << " test reached.";
						} else if (underLimit) {
							std::cout << "    · The execution time is too high for the minimum number of nodes (" << minorSizeTest << ").";
						} else if (exitNormally) {
							std::cout << "    · The number of nodes exceeds the number of nodes in the normal execution of the program.";
						} else {
							std::cout << "    ·";
						}
						std::cout << " Selecting the most accurate test: " << bestSizeTest << " nodes. Error: " << ((mostAccurate <= 0.0) ? "+" : "-") << std::abs(mostAccurate*100) << "%" << std::endl << "  ";
					}
				}
			}
			if (mapSizeTestChunks[checkNo] != 0) {
				const double nSpeed = mapHistorySizeTests[mapSizeTestChunks[checkNo]].second;
				chunkSel.setChunkSpeed(checkNo, nSpeed);
				if (opt.verbose > 2) {
					const double total_time = mapHistorySizeTests[mapSizeTestChunks[checkNo]].first;
					std::cout << "\tFinalized! Test Time: " << total_time << " sec. Test Speed Result: " << nSpeed << " nodes/sec" << std::endl;
				}
				checkNo = chunkSel.getNextChunk();
			} else {
				if (opt.verbose > 2) {
					std::cout << "\tFailed!" << std::endl;
				}
				checkNo = 0;
			}
		}
		res = chunkSel.getChunkScores();

		return res;
	}

	static void run_thread(const int thrId, const int numTotalThreads, const int chunkSize, internal::StealStack<T>* ss, general_reference_wrapper<T>& root, const Info& info, Body body, Return& rr) {
		try {
			const int threadId = thrId;

			Return r = std::move(rr);
			internal_psr threadParStackClass(threadId, numTotalThreads, chunkSize);

			/* put root node on thread 0 stack */
			if (threadId == 0) {
				threadParStackClass.ss_push(internal_psr::stealStack[0], root.get());
			}

			/* tree search */
			bool done = false;
			while(!done) {

				/* local work */
				while (threadParStackClass.ss_localDepth(ss) > 0) {
					PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));

					/* examine node at stack top */
					T *data = threadParStackClass.ss_top(ss);
					body.pre(*data);

					PROFILEACTION(ss->nNodes++);
					if (info.is_base(*data)) {
						PROFILEACTION(ss->nLeaves++);
						body.post(body.base(*data), r);
						threadParStackClass.ss_pop(ss);
					} else {
						body.pre_rec(*data);
						if (body.processNonBase) {
							body.post(body.non_base(*data), r);
						}
						if (!Partitioner::do_parallel(*data))  {
							const int numChildren = info.num_children(*data);
							for (int i = 0; i < numChildren; ++i) {
								do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::do_it_serial(r, info.child(i, *data), info, body);
							}
							threadParStackClass.ss_pop(ss);
						} else {
							const int numChildren = info.num_children(*data);
							if (numChildren > 0) {
								threadParStackClass.ss_checkStackSize(ss, numChildren, data);
								for (int i = 1; i < numChildren; ++i) {
									threadParStackClass.ss_push_simple(ss, info.child(i, *data));
								}
								threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
								threadParStackClass.releaseMultipleNodes(ss);
							} else {
								threadParStackClass.ss_pop(ss);
							}
						}
					}
				}

				/* local work exhausted on this stack - resume tree search if able
				* to re-acquire work from shared portion of this thread's stack
				*/
				if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
					/* no work left in this thread's stack           */
					/* try to steal work from another thread's stack */
					bool goodSteal = false;

					PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
					int victimId = threadParStackClass.findwork(chunkSize);
					while (victimId != -1 && !goodSteal) {
						// some work detected, try to steal it
						goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
						if (!goodSteal) {
							victimId = threadParStackClass.findwork(chunkSize);
						}
					}
					if (!goodSteal) {
						/* unable to steal work from shared portion of other stacks -
						* enter quiescent state waiting for termination (done != false)
						* or cancellation because some thread has made work available
						* (done == false).
						*/
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
						done = threadParStackClass.cbarrier_wait();
					}
				}
			}
			rr = std::move(r);
			/* tree search complete ! */
		}catch (...) {
			//Set the global exception pointer in case of an exception
			globalExceptionPtr() = std::current_exception();
#ifdef DPR_DONT_USE_SPINLOCK
			std::lock_guard<std::mutex> lck(internal_psr::cb.cb_cv_mtx);
			internal_psr::cb.cb_done = true;
			internal_psr::cb.cb_cancel = true;
			internal_psr::cb.cb_cv.notify_all();
#else
			internal_psr::cb.cb_done = true;
			internal_psr::cb.cb_cancel = true;
#endif
		}
	}

	static void run_thread_test(const int thrId, const int numTotalThreads, const int chunkSize, internal::StealStack<T>* ss, general_reference_wrapper<T>& root, const Info& info, Body body, Return& rr, unsigned long long int& nNodes, const unsigned long long int testSize, char& exitNormally) {
		try {
			const int threadId = thrId;
			exitNormally = 1;

			Return r = std::move(rr);
			internal_psr threadParStackClass(threadId, numTotalThreads, chunkSize);

			/* put root node on thread 0 stack */
			if (threadId == 0) {
				//threadParStackClass.ss_push_test(internal_psr::stealStack[0], root.get());
				threadParStackClass.ss_push(internal_psr::stealStack[0], root.get());
			}

			/* tree search */
			bool done = false;
			while(!done) {

				/* local work */
				while (threadParStackClass.ss_localDepth(ss) > 0) {
					PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));

					/* examine node at stack top */
					T *data = threadParStackClass.ss_top(ss);
					body.pre(*data);

					if ((ss->nNodes > testSize) || internal_psr::cb.cb_done) {
						ss->nNodes++;
						exitNormally = 0;
						PROFILEACTION(ss->nLeaves++);
#ifdef DPR_DONT_USE_SPINLOCK
						std::lock_guard<std::mutex> lck(internal_psr::cb.cb_cv_mtx);
						internal_psr::cb.cb_done = true;
						internal_psr::cb.cb_cancel = true;
						internal_psr::cb.cb_cv.notify_all();
#else
						internal_psr::cb.cb_done = true;
						internal_psr::cb.cb_cancel = true;
#endif
						threadParStackClass.ss_mkEmpty(ss);
					} else if (info.is_base(*data)) {
						ss->nNodes++;
						PROFILEACTION(ss->nLeaves++);
						body.post(body.base(*data), r);
						threadParStackClass.ss_pop(ss);
					} else {
						ss->nNodes++;
						body.pre_rec(*data);
						if (body.processNonBase) {
							body.post(body.non_base(*data), r);
						}
						if (!Partitioner::do_parallel(*data))  {
							const int numChildren = info.num_children(*data);
							for (int i = 0; i < numChildren; ++i) {
								do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::do_it_serial(r, info.child(i, *data), info, body);
							}
							threadParStackClass.ss_pop(ss);
						} else {
							const int numChildren = info.num_children(*data);
							if (numChildren > 0) {
								threadParStackClass.ss_checkStackSize(ss, numChildren, data);
								for (int i = 1; i < numChildren; ++i) {
									threadParStackClass.ss_push_simple(ss, info.child(i, *data));
								}
								threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
								threadParStackClass.releaseMultipleNodes(ss);
							} else {
								threadParStackClass.ss_pop(ss);
							}
						}
					}
				}

				/* local work exhausted on this stack - resume tree search if able
				* to re-acquire work from shared portion of this thread's stack
				*/
				if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
					/* no work left in this thread's stack           */
					/* try to steal work from another thread's stack */
					bool goodSteal = false;

					PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
					int victimId = threadParStackClass.findwork(chunkSize);
					while (victimId != -1 && !goodSteal) {
						// some work detected, try to steal it
						goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
						if (!goodSteal) {
							victimId = threadParStackClass.findwork(chunkSize);
						}
					}
					if (!goodSteal) {
						/* unable to steal work from shared portion of other stacks -
						* enter quiescent state waiting for termination (done != false)
						* or cancellation because some thread has made work available
						* (done == false).
						*/
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
						done = threadParStackClass.cbarrier_wait();
					}
				}
			}
			nNodes = ss->nNodes;
			rr = std::move(r);
			/* tree search complete ! */
		}catch (...) {
			//Set the global exception pointer in case of an exception
			globalExceptionPtr() = std::current_exception();
#ifdef DPR_DONT_USE_SPINLOCK
			std::lock_guard<std::mutex> lck(internal_psr::cb.cb_cv_mtx);
			internal_psr::cb.cb_done = true;
			internal_psr::cb.cb_cancel = true;
			internal_psr::cb.cb_cv.notify_all();
#else
			internal_psr::cb.cb_done = true;
			internal_psr::cb.cb_cancel = true;
#endif
		}
	}

	friend class simple_stack_partitioner<Return, T, Info, Body>;

	friend class auto_stack_partitioner<Return, T, Info, Body>;

	friend class custom_stack_partitioner<Return, T, Info, Body>;

};

/// Specialization for Return==void
template<typename T, typename Info, typename Body, typename Partitioner>
class start_stack_recursion<void, T, Info, Body, Partitioner> {
	typedef dpr::internal::parallel_stack_recursion_internal<typename std::remove_reference<T>::type, Info> internal_psr;

public:

	static void run(general_reference_wrapper<T> root, const Info& info, Body body, const int num_total_threads = std::thread::hardware_concurrency(), const int chunkSizeI = dpr::internal::defaultChunkSize, const AutomaticChunkOptions opt = dpr::aco_default, typename std::decay<T>::type * const rootTestV = nullptr) {
		assert(chunkSizeI >= 0);
		int chunkSize = chunkSizeI;
		dpr::internal::lastPsrRunExtraInfo().chunkSize = chunkSize;
		dpr::internal::lastPsrRunExtraInfo().nthreads = num_total_threads;
		std::chrono::steady_clock::time_point init_run_time;
		if (chunkSize <= 0) {
			general_reference_wrapper<T> rootTest((rootTestV == nullptr) ? root.get() : *rootTestV);
			const std::chrono::steady_clock::time_point init_test_time = getTimepoint();
			const std::vector<dpr::ResultChunkTest> testResults = run_test(std::forward<T>(rootTest), info, body, num_total_threads, chunkSize, opt);
			if (testResults.size() > 0) {
				chunkSize = testResults[0].chunkId;
			} else {
				chunkSize = defaultChunkSize;
			}
			init_run_time = getTimepoint();
			dpr::internal::lastPsrRunExtraInfo().testChunkTime = getTimediff(init_test_time, init_run_time);
		} else {
			init_run_time = getTimepoint();
			dpr::internal::lastPsrRunExtraInfo().testChunkTime = 0.0;
		}
		dpr::internal::lastPsrRunExtraInfo().chunkSizeUsed = chunkSize;

		internal_psr::cb_init();
		internal_psr::stealStack = new internal::StealStack<typename std::remove_reference<T>::type>*[num_total_threads];

		internal::AlignedBufferedAllocator stealStackAllocator((sizeof(internal::StealStack<typename std::remove_reference<T>::type>)+64)*num_total_threads);

		for (int threadId = 0; threadId < num_total_threads; threadId++) {
			internal_psr::stealStack[threadId] = stealStackAllocator.aligned_alloc<internal::StealStack<typename std::remove_reference<T>::type>>(64);
			internal_psr::ss_init(internal_psr::stealStack[threadId], internal::initialStackSize());
		}

		do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::info = &info;
		std::vector<std::thread> threads;
		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			PROFILEACTION(internal_psr::ss_initState(internal_psr::stealStack[threadId]));
			threads.push_back(std::thread(run_thread, threadId, num_total_threads, chunkSize, internal_psr::stealStack[threadId], std::ref(root), info, body));
		}

		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			threads[threadId].join();
		}
		if (globalExceptionPtr() != nullptr) {
			try {
				std::rethrow_exception(globalExceptionPtr());
			}catch (const std::exception &ex) {
				std::cerr << ex.what() << std::endl;
				std::rethrow_exception(globalExceptionPtr());
			}
		}

		threads.clear();

		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			internal_psr::stealStack[threadId]->stack.~vector();
		}
		delete[] internal_psr::stealStack;

		dpr::internal::lastPsrRunExtraInfo().runTime = getTimediff(init_run_time, getTimepoint());
	}

	static std::vector<dpr::ResultChunkTest> run_test(general_reference_wrapper<T> root, const Info& info, Body body, const int num_total_threads = std::thread::hardware_concurrency(), const int chunkSizeI = 0, const AutomaticChunkOptions opt = dpr::aco_test_default) {
		assert(chunkSizeI >= 0);
		std::vector<dpr::ResultChunkTest> res;

		std::vector<unsigned long long int> nNodesArr(num_total_threads);
		std::vector<char> exitNormallyArr(num_total_threads);
		const size_t softLimitTestsSizeTest = 8;
		const size_t hardLimitTestsSizeTest = 14;
		const double softLimitMargin = 0.3;
		const double errorMargingCalculateSizeTest = 0.15;
		const int minorSizeTest = 250;

		dpr::internal::ChunkSelector chunkSel(opt, chunkSizeI);

		std::map<int,unsigned long long int> mapSizeTestChunks;
		for (const std::pair<int, dpr::internal::chunk_attr> &iChunk : chunkSel.selectedChunks) {
			mapSizeTestChunks[iChunk.first] = 0;
		}

		double estimatedTimePerTest = opt.targetTimePerTest;
		if ((opt.limitTimeOfEachTest) && (opt.testSize == 0) && (estimatedTimePerTest <= 0.0000001)) {
			estimatedTimePerTest = chunkSel.estimateNeededTimePerTest();
			if (opt.verbose > 3) {
				std::cout << " [?] It has been estimated a necessary time per test of " << estimatedTimePerTest << " sec [±" << static_cast<int>(errorMargingCalculateSizeTest*100) << "%]" << std::endl;
			}
		} else {
			if (opt.verbose > 3) {
				if (opt.limitTimeOfEachTest) {
					if (opt.testSize == 0) {
						std::cout << " [?] A necessary time per test " << estimatedTimePerTest << " sec [±" << static_cast<int>(errorMargingCalculateSizeTest*100) << "%]" << " has been established" << std::endl;
					} else {
						std::cout << " [?] A fixed test size of " << opt.testSize << " nodes will be used" << std::endl;
					}
				} else {
					std::cout << " [?] No test size or test time limit has been established" << std::endl;
				}
			}
		}

		int checkNo = chunkSel.getNextChunk();
		unsigned long long int iSizeTest;
		while(checkNo > 0) {
			if (opt.verbose > 2) {
				std::cout << "  - Testing Chunk " << checkNo << "..." << std::flush;
			}
			if (opt.limitTimeOfEachTest) {
				iSizeTest = opt.testSize;
			} else {
				iSizeTest = std::numeric_limits<unsigned long long int>::max();
			}
			if (iSizeTest == 0) {
				iSizeTest = mapSizeTestChunks[checkNo];
			}
			if (iSizeTest == 0) {
				iSizeTest = minorSizeTest;
				int minorDistance = std::numeric_limits<int>::max();
				int minorDistChunk = -1;
				for (const std::pair<int, unsigned long long int> &iMap : mapSizeTestChunks) {
					if (iMap.second != 0) {
						const int distanceChunk = std::abs(checkNo - iMap.first);
						if (distanceChunk < minorDistance) {
							minorDistChunk = iMap.first;
							minorDistance = distanceChunk;
						}
					}
				}
				if (minorDistChunk > 0) {
					iSizeTest = mapSizeTestChunks[minorDistChunk];
				}
			}
			std::map<unsigned long long int, std::pair<double, double>> mapHistorySizeTests;
			char exitNormally = 0;
			bool firstMandatoryExecution = true;
			bool needMoreNodes = true;
			double mostAccurate = std::numeric_limits<double>::max();
			unsigned long long int bestSizeTest = 0;
			bool underLimit = false;

			while (firstMandatoryExecution || (
					(mapSizeTestChunks[checkNo] == 0) &&
					(mapHistorySizeTests.find(iSizeTest) == mapHistorySizeTests.end()) &&
					(mapHistorySizeTests.size() < hardLimitTestsSizeTest) &&
					(!underLimit) &&
					((mapHistorySizeTests.size() < softLimitTestsSizeTest) || (std::abs(mostAccurate) > softLimitMargin)) &&
					(!exitNormally || !needMoreNodes)
			)) {
				if ((opt.verbose > 3) && (opt.testSize == 0) && (mapSizeTestChunks[checkNo] == 0)) {
					if (firstMandatoryExecution) {
						std::cout << std::endl;
					}
					if (opt.limitTimeOfEachTest) {
						std::cout << "    · Testing with " << iSizeTest << " nodes..." << std::flush;
					}
				}
				firstMandatoryExecution = false;

				general_reference_wrapper<T> localRoot = call_clone<T>(root.get());

				unsigned long long int nNodes = 0;
				exitNormally = 1;

				const std::chrono::steady_clock::time_point init_time = getTimepoint();

				internal_psr::cb_init();
				internal_psr::stealStack = new internal::StealStack<typename std::remove_reference<T>::type>*[num_total_threads];

				internal::AlignedBufferedAllocator stealStackAllocator((sizeof(internal::StealStack<typename std::remove_reference<T>::type>)+64)*num_total_threads);

				for (int threadId = 0; threadId < num_total_threads; threadId++) {
					internal_psr::stealStack[threadId] = stealStackAllocator.aligned_alloc<internal::StealStack<typename std::remove_reference<T>::type>>(64);
					internal_psr::ss_init(internal_psr::stealStack[threadId], internal::initialStackSize());
				}

				do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::info = &info;
				std::vector<std::thread> threads;
				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					PROFILEACTION(internal_psr::ss_initState(internal_psr::stealStack[threadId]));
					threads.push_back(std::thread(run_thread_test, threadId, num_total_threads, checkNo, internal_psr::stealStack[threadId], std::ref(localRoot), info, body, std::ref(nNodesArr[threadId]), iSizeTest, std::ref(exitNormallyArr[threadId])));
				}

				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					threads[threadId].join();
				}
				if (globalExceptionPtr() != nullptr) {
					try {
						std::rethrow_exception(globalExceptionPtr());
					}catch (const std::exception &ex) {
						std::cerr << ex.what() << std::endl;
						std::rethrow_exception(globalExceptionPtr());
					}
				}
				threads.clear();

				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					internal_psr::stealStack[threadId]->stack.~vector();
				}
				delete[] internal_psr::stealStack;

				const std::chrono::steady_clock::time_point finish_time = getTimepoint();
				const double total_time = getTimediff(init_time, finish_time);

				call_deallocate<T>(localRoot.get());

				for (int i = 0; i < num_total_threads; i++) {
					nNodes += nNodesArr[i];
				}
				if (opt.limitTimeOfEachTest) {
					for (int i = 0; i < num_total_threads; i++) {
						exitNormally &= exitNormallyArr[i];
					}
				}
				const double nSpeed = nNodes / total_time;
				if ((opt.testSize == 0) && (opt.limitTimeOfEachTest)) {
					if (mapSizeTestChunks[checkNo] == 0) {
						const double diff = (estimatedTimePerTest-total_time)/estimatedTimePerTest;
						mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
						if (std::abs(diff) < std::abs(mostAccurate)) {
							bestSizeTest = iSizeTest;
							mostAccurate = diff;
						}
						if (std::abs(diff) < errorMargingCalculateSizeTest) {
							mapSizeTestChunks[checkNo] = iSizeTest;
						} else {
							if (diff > 0.96) {
								needMoreNodes = true;
								iSizeTest *= 25;
							} else if (diff > 0.0) {
								needMoreNodes = true;
								iSizeTest *= estimatedTimePerTest/total_time;
							} else if (diff < 0.0) {
								needMoreNodes = false;
								if (iSizeTest > minorSizeTest) {
									iSizeTest *= estimatedTimePerTest/total_time;
								} else {
									underLimit = true;
								}
							} else if (diff < -3.0) {
								needMoreNodes = false;
								iSizeTest *= 0.25;
							}
							if (iSizeTest < minorSizeTest) {
								iSizeTest = minorSizeTest;
							}
							while (mapHistorySizeTests.find(iSizeTest) != mapHistorySizeTests.end()) {
								iSizeTest += minorSizeTest;
							}
						}

						if (opt.verbose > 3) {
							if (mapSizeTestChunks[checkNo] == 0) {
								std::cout << "\tFailure! Time: " << total_time << " sec. Error: " << ((diff <= 0.0) ? "+" : "-") << std::abs(diff*100) << "%" << std::endl;
							} else {
								std::cout << "\tDone! Time: " << total_time << " sec. Error: " << ((diff <= 0.0) ? "+" : "-") << std::abs(diff*100) << "%" << std::endl << "  ";
							}
						}
					} else {
						mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
					}
				} else {
					mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
					mapSizeTestChunks[checkNo] = iSizeTest;
				}
			}
			if (mapSizeTestChunks[checkNo] == 0) {
				if (bestSizeTest > 0) {
					mapSizeTestChunks[checkNo] = bestSizeTest;
					if ((opt.verbose > 3) && (opt.testSize == 0)) {
						if ((mapHistorySizeTests.size() >= hardLimitTestsSizeTest)) {
							std::cout << "    · Hard limit of " << hardLimitTestsSizeTest << " test reached.";
						} else if ((mapHistorySizeTests.size() >= softLimitTestsSizeTest) && (std::abs(mostAccurate) <= softLimitMargin)) {
							std::cout << "    · Soft limit of " << softLimitTestsSizeTest << " test reached.";
						} else if (underLimit) {
							std::cout << "    · The execution time is too high for the minimum number of nodes (" << minorSizeTest << ").";
						} else if (exitNormally) {
							std::cout << "    · The number of nodes exceeds the number of nodes in the normal execution of the program.";
						} else {
							std::cout << "    ·";
						}
						std::cout << " Selecting the most accurate test: " << bestSizeTest << " nodes. Error: " << ((mostAccurate <= 0.0) ? "+" : "-") << std::abs(mostAccurate*100) << "%" << std::endl << "  ";
					}
				}
			}
			if (mapSizeTestChunks[checkNo] != 0) {
				const double nSpeed = mapHistorySizeTests[mapSizeTestChunks[checkNo]].second;
				chunkSel.setChunkSpeed(checkNo, nSpeed);
				if (opt.verbose > 2) {
					const double total_time = mapHistorySizeTests[mapSizeTestChunks[checkNo]].first;
					std::cout << "\tFinalized! Test Time: " << total_time << " sec. Test Speed Result: " << nSpeed << " nodes/sec" << std::endl;
				}
				checkNo = chunkSel.getNextChunk();
			} else {
				if (opt.verbose > 2) {
					std::cout << "\tFailed!" << std::endl;
				}
				checkNo = 0;
			}
		}
		res = chunkSel.getChunkScores();

		return res;
	}

	static void run_thread(const int thrId, const int numTotalThreads, const int chunkSize, internal::StealStack<T>* ss, general_reference_wrapper<T>& root, const Info& info, Body body) {
		try {
			const int threadId = thrId;

			internal_psr threadParStackClass(threadId, numTotalThreads, chunkSize);

			/* put root node on thread 0 stack */
			if (threadId == 0) {
				threadParStackClass.ss_push(internal_psr::stealStack[0], root.get());
			}

			/* tree search */
			bool done = false;
			while(!done) {

				/* local work */
				while (threadParStackClass.ss_localDepth(ss) > 0) {
					PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));

					/* examine node at stack top */
					T *data = threadParStackClass.ss_top(ss);
					body.pre(*data);

					if (info.is_base(*data)) {
						PROFILEACTION(ss->nLeaves++);
						body.base(*data);
						threadParStackClass.ss_pop(ss);
					} else {
						body.pre_rec(*data);
						if (body.processNonBase) {
							body.non_base(*data);
						}
						if (!Partitioner::do_parallel(*data))  {
							const int numChildren = info.num_children(*data);
							for (int i = 0; i < numChildren; ++i) {
								do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::do_it_serial(info.child(i, *data), info, body);
							}
							threadParStackClass.ss_pop(ss);
						} else {
							const int numChildren = info.num_children(*data);
							threadParStackClass.ss_checkStackSize(ss, numChildren, data);
							if (numChildren > 0) {
								for (int i = 1; i < numChildren; ++i) {
									threadParStackClass.ss_push_simple(ss, info.child(i, *data));
								}
								threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
								threadParStackClass.releaseMultipleNodes(ss);
							} else {
								threadParStackClass.ss_pop(ss);
							}
						}
					}
				}

				/* local work exhausted on this stack - resume tree search if able
				* to re-acquire work from shared portion of this thread's stack
				*/
				if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
					/* no work left in this thread's stack           */
					/* try to steal work from another thread's stack */
					bool goodSteal = false;

					PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
					int victimId = threadParStackClass.findwork(chunkSize);
					while (victimId != -1 && !goodSteal) {
						// some work detected, try to steal it
						goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
						if (!goodSteal) {
							victimId = threadParStackClass.findwork(chunkSize);
						}
					}
					if (!goodSteal) {
						/* unable to steal work from shared portion of other stacks -
						* enter quiescent state waiting for termination (done != false)
						* or cancellation because some thread has made work available
						* (done == false).
						*/
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
						done = threadParStackClass.cbarrier_wait();
					}
				}
			}
			/* tree search complete ! */
		}catch (...) {
			//Set the global exception pointer in case of an exception
			globalExceptionPtr() = std::current_exception();
#ifdef DPR_DONT_USE_SPINLOCK
			std::lock_guard<std::mutex> lck(internal_psr::cb.cb_cv_mtx);
			internal_psr::cb.cb_done = true;
			internal_psr::cb.cb_cancel = true;
			internal_psr::cb.cb_cv.notify_all();
#else
			internal_psr::cb.cb_done = true;
			internal_psr::cb.cb_cancel = true;
#endif
		}
	}

	static void run_thread_test(const int thrId, const int numTotalThreads, const int chunkSize, internal::StealStack<T>* ss, general_reference_wrapper<T>& root, const Info& info, Body body, unsigned long long int& nNodes, const unsigned long long int testSize, char& exitNormally) {
		try {
			const int threadId = thrId;
			exitNormally = 1;

			internal_psr threadParStackClass(threadId, numTotalThreads, chunkSize);

			/* put root node on thread 0 stack */
			if (threadId == 0) {
				//threadParStackClass.ss_push_test(internal_psr::stealStack[0], root.get());
				threadParStackClass.ss_push(internal_psr::stealStack[0], root.get());
			}

			/* tree search */
			bool done = false;
			while(!done) {

				/* local work */
				while (threadParStackClass.ss_localDepth(ss) > 0) {
					PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));

					/* examine node at stack top */
					T *data = threadParStackClass.ss_top(ss);
					body.pre(*data);

					if ((ss->nNodes > testSize) || internal_psr::cb.cb_done) {
						ss->nNodes++;
						exitNormally = 0;
						PROFILEACTION(ss->nLeaves++);
#ifdef DPR_DONT_USE_SPINLOCK
						std::lock_guard<std::mutex> lck(internal_psr::cb.cb_cv_mtx);
						internal_psr::cb.cb_done = true;
						internal_psr::cb.cb_cancel = true;
						internal_psr::cb.cb_cv.notify_all();
#else
						internal_psr::cb.cb_done = true;
						internal_psr::cb.cb_cancel = true;
#endif
						threadParStackClass.ss_mkEmpty(ss);
					} else if (info.is_base(*data)) {
						ss->nNodes++;
						PROFILEACTION(ss->nLeaves++);
						body.base(*data);
						threadParStackClass.ss_pop(ss);
					} else {
						ss->nNodes++;
						body.pre_rec(*data);
						if (body.processNonBase) {
							body.non_base(*data);
						}
						if (!Partitioner::do_parallel(*data))  {
							const int numChildren = info.num_children(*data);
							for (int i = 0; i < numChildren; ++i) {
								do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::do_it_serial(info.child(i, *data), info, body);
							}
							threadParStackClass.ss_pop(ss);
						} else {
							const int numChildren = info.num_children(*data);
							threadParStackClass.ss_checkStackSize(ss, numChildren, data);
							if (numChildren > 0) {
								for (int i = 1; i < numChildren; ++i) {
									threadParStackClass.ss_push_simple(ss, info.child(i, *data));
								}
								threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
								threadParStackClass.releaseMultipleNodes(ss);
							} else {
								threadParStackClass.ss_pop(ss);
							}
						}
					}
				}

				/* local work exhausted on this stack - resume tree search if able
				* to re-acquire work from shared portion of this thread's stack
				*/
				if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
					/* no work left in this thread's stack           */
					/* try to steal work from another thread's stack */
					bool goodSteal = false;

					PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
					int victimId = threadParStackClass.findwork(chunkSize);
					while (victimId != -1 && !goodSteal) {
						// some work detected, try to steal it
						goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
						if (!goodSteal) {
							victimId = threadParStackClass.findwork(chunkSize);
						}
					}
					if (!goodSteal) {
						/* unable to steal work from shared portion of other stacks -
						* enter quiescent state waiting for termination (done != false)
						* or cancellation because some thread has made work available
						* (done == false).
						*/
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
						done = threadParStackClass.cbarrier_wait();
					}
				}
			}
			nNodes = ss->nNodes;
			/* tree search complete ! */
		}catch (...) {
			//Set the global exception pointer in case of an exception
			globalExceptionPtr() = std::current_exception();
#ifdef DPR_DONT_USE_SPINLOCK
			std::lock_guard<std::mutex> lck(internal_psr::cb.cb_cv_mtx);
			internal_psr::cb.cb_done = true;
			internal_psr::cb.cb_cancel = true;
			internal_psr::cb.cb_cv.notify_all();
#else
			internal_psr::cb.cb_done = true;
			internal_psr::cb.cb_cancel = true;
#endif
		}
	}

	friend class simple_stack_partitioner<void, T, Info, Body>;

	friend class auto_stack_partitioner<void, T, Info, Body>;

	friend class custom_stack_partitioner<void, T, Info, Body>;

};

/// Implementation of the simple parallelization strategy
template<typename Return, typename T, typename Info, typename Body>
struct simple_stack_partitioner {
	typedef simple_stack_partitioner<Return, T, Info, Body> partitioner;

	/** Since in the current implementation Partitioner::do_parallel in only called
	  * after r._info.is_base(_data) has returned false, there is no need to reevaluate it
	  */
	static bool do_parallel(T& data) noexcept {
		return true;
	}
};

/// Implementation of the automatic parallelization strategy
template<typename Return, typename T, typename Info, typename Body>
struct auto_stack_partitioner {
	typedef auto_stack_partitioner<Return, T, Info, Body> partitioner;

	static bool do_parallel(T& data) noexcept {
		//return r._level < (do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info->parLevel_); //BBF: LevelLimit;
		return false;	//TODO: falta implementar esto
	}
};

/// Implementation of the customized parallelization strategy
template<typename Return, typename T, typename Info, typename Body>
struct custom_stack_partitioner {
	typedef custom_stack_partitioner<Return, T, Info, Body> partitioner;

	static bool do_parallel(T& data) noexcept(noexcept(do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info->do_parallel(data))) {
		return do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info->do_parallel(data);
	}
};

} // namespace internal

/** When no partitioner is specified, the internal::simple_stack_partitioner is used. */
template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize = dpr::internal::defaultChunkSize, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, partitioner::simple, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, partitioner::automatic, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, partitioner::custom, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, const AutomaticChunkOptions opt, partitioner::simple) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, const AutomaticChunkOptions opt, partitioner::automatic) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, const AutomaticChunkOptions opt, partitioner::custom) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

/** When no partitioner is specified, the internal::simple_stack_partitioner is used. */
template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, partitioner::simple, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, partitioner::automatic, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, partitioner::custom, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, const AutomaticChunkOptions opt, partitioner::simple, T&& rootTest) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, const AutomaticChunkOptions opt, partitioner::automatic, T&& rootTest) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return parallel_stack_recursion(T&& root, const Info& info, Body body, const int chunkSize, const AutomaticChunkOptions opt, partitioner::custom, T&& rootTest) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> parallel_stack_recursion_test(T&& root, const Info& info, Body body, const int chunkSize = 0, const AutomaticChunkOptions opt = dpr::aco_test_default) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run_test(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> parallel_stack_recursion_test(T&& root, const Info& info, Body body, const int chunkSize, partitioner::simple, const AutomaticChunkOptions opt = dpr::aco_test_default) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run_test(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> parallel_stack_recursion_test(T&& root, const Info& info, Body body, const int chunkSize, partitioner::automatic, const AutomaticChunkOptions opt = dpr::aco_test_default) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run_test(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> parallel_stack_recursion_test(T&& root, const Info& info, Body body, const int chunkSize, partitioner::custom, const AutomaticChunkOptions opt = dpr::aco_test_default) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run_test(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> parallel_stack_recursion_test(T&& root, const Info& info, Body body, const int chunkSize, const AutomaticChunkOptions opt, partitioner::simple) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run_test(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> parallel_stack_recursion_test(T&& root, const Info& info, Body body, const int chunkSize, const AutomaticChunkOptions opt, partitioner::automatic) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run_test(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> parallel_stack_recursion_test(T&& root, const Info& info, Body body, const int chunkSize, const AutomaticChunkOptions opt, partitioner::custom) {
	return internal::start_stack_recursion<Return, typename std::remove_reference<T>::type, Info, Body, internal::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run_test(std::forward<T>(root), info, body, internal::num_total_threads(), chunkSize, opt);
}

// Public function that allow users get extra info about the last parallel_stack_recursion run (only for normal runs, not test runs)
inline dpr::RunExtraInfo getPsrLastRunExtraInfo() {
	return dpr::internal::lastPsrRunExtraInfo();
}

/// Facilitates the initialization of the parallel_stack_recursion environment
template<typename T>
inline void prs_init(const T nthreads) {
  static_assert(std::is_integral<T>::value, "Integer required.");
  dpr::internal::num_total_threads() = nthreads;
}

/// Facilitates the initialization of the parallel_stack_recursion environment
template<typename T>
inline void prs_init(const T nthreads, const int initial_stack_size) {
  static_assert(std::is_integral<T>::value, "Integer required.");
  assert(initial_stack_size > 0);
  dpr::internal::num_total_threads() = nthreads;
  dpr::internal::initialStackSize() = initial_stack_size;
}

} // namespace dpr


#endif /* DPR_PARALLEL_STACK_RECURSION_H_ */

