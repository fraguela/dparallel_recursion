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
/// \file     SRange_comm.h
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro <jc.cabaleiro@usc.es>
///

#ifndef DPR_SRANGE_COMM_H_
#define DPR_SRANGE_COMM_H_

#include <algorithm>
#include "dparallel_recursion/EmptyBody.h"
#include "dparallel_recursion/Arity.h"
#include "dparallel_recursion/seq_parallel_stack_recursion.h"

namespace dpr {

/// Represents a range of integers
struct SRange {
	int start;     ///< First element of the Range
	int end;       ///< Last plus one element of the Range
	int rangeLen;  ///< Size of the range (end-start)

	SRange()
	{ }

	SRange(int start_, int end_) : start(start_), end(end_), rangeLen(end_-start_)
	{ }

	SRange(int start_, int end_, int rangeLen_) : start(start_), end(end_), rangeLen(rangeLen_)
	{ }

	//constexpr int size() const { return rangeLen; }

	/// Returns a Range shifted \c offset positions in the positive direction
	SRange operator>> (int offset) const {
		return SRange { start + offset, end + offset };
	}

	/// Returns a Range shifted \c offset positions in the negative direction
	SRange operator<< (int offset) const {
		return SRange { start - offset, end - offset };
	}

	friend void swap(SRange& a, SRange& b) {
		using std::swap; // bring in swap for built-in types
		swap(a.start, b.start);
		swap(a.end, b.end);
		swap(a.rangeLen, b.rangeLen);
	}

};

//SRange Info Object that allows define a limitParallel limit in a do_parallel function for a custom partitioner
template<int DCHUNK = 0, int LIMITPARALLEL = -1>
struct SRangeInfo : public Arity<DCHUNK> {

	SRangeInfo(const int nthreads = dpr::internal::num_total_threads()) : Arity<DCHUNK> (nthreads, nthreads)
	{}

	static bool is_base(const SRange& sr) noexcept {
		return sr.rangeLen < DCHUNK;
	}

	static constexpr bool do_parallel(const SRange& sr) noexcept {
		return sr.rangeLen > LIMITPARALLEL;
	}

	static SRange child(int i, const SRange& sr) noexcept {
		int start, end;
		int nmi, nmiR;
		nmi = sr.rangeLen / DCHUNK;
		nmiR = sr.rangeLen % DCHUNK;
		if (i < nmiR) {
			start = sr.start + i * nmi + i;
			end = start + nmi + 1;
		} else {
			start = sr.start + i * nmi + nmiR;
			end = start + nmi;
		}
		SRange sr_new(start, end);
		return sr_new;
	}

//	static void child(int i, const SRange& sr, SRange& sr_new) noexcept {
//		int nmi, nmiR;
//		nmi = sr.rangeLen / DCHUNK;
//		nmiR = sr.rangeLen % DCHUNK;
//		if (i < nmiR) {
//			sr_new.start = sr.start + i * nmi + i;
//			sr_new.end = sr_new.start + nmi + 1;
//		} else {
//			sr_new.start = sr.start + i * nmi + nmiR;
//			sr_new.end = sr_new.start + nmi;
//		}
//		sr_new.rangeLen = sr_new.end - sr_new.start;
//	}

};

//SRange Info Object that allows define a limitParallel limit in a do_parallel function for a custom partitioner
///Specialization for DCHUNK==0
template<int LIMITPARALLEL>
struct SRangeInfo<0, LIMITPARALLEL> : public Arity<0> {

	const int DCHUNK;

	SRangeInfo(const int _DCHUNK = 2, const int nthreads = dpr::internal::num_total_threads()) : Arity<0> (nthreads, nthreads), DCHUNK(_DCHUNK)
	{}

	bool is_base(const SRange& sr) const noexcept {
		return sr.rangeLen < DCHUNK;
	}

	static constexpr bool do_parallel(const SRange& sr) noexcept {
		return sr.rangeLen > LIMITPARALLEL;
	}

	SRange child(int i, const SRange& sr) const noexcept {
		int start, end;
		int nmi, nmiR;
		nmi = sr.rangeLen / DCHUNK;
		nmiR = sr.rangeLen % DCHUNK;
		if (i < nmiR) {
			start = sr.start + i * nmi + i;
			end = start + nmi + 1;
		} else {
			start = sr.start + i * nmi + nmiR;
			end = start + nmi;
		}
		SRange sr_new(start, end);
		return sr_new;
	}

//	void child(int i, const SRange& sr, SRange& sr_new) const noexcept {
//		int nmi, nmiR;
//		nmi = sr.rangeLen / DCHUNK;
//		nmiR = sr.rangeLen % DCHUNK;
//		if (i < nmiR) {
//			sr_new.start = sr.start + i * nmi + i;
//			sr_new.end = sr_new.start + nmi + 1;
//		} else {
//			sr_new.start = sr.start + i * nmi + nmiR;
//			sr_new.end = sr_new.start + nmi;
//		}
//		sr_new.rangeLen = sr_new.end - sr_new.start;
//	}

	int num_children(const SRange& sr) const noexcept {
		return DCHUNK;
	}

};

///SRange Info Object that create a recursive iteration tree for a linear range
///Specialization for DCHUNK==0, LIMITPARALLEL==-1
template<int DCHUNK>
struct SRangeInfo<DCHUNK, -1> : public Arity<DCHUNK> {

	SRangeInfo(const int nthreads = dpr::internal::num_total_threads()) : Arity<DCHUNK> (nthreads, nthreads)
	{}

	static bool is_base(const SRange& sr) noexcept {
		return sr.rangeLen < DCHUNK;
	}

	static constexpr bool do_parallel(const SRange& sr) noexcept {
		return true;
	}

	static SRange child(int i, const SRange& sr) noexcept {
		int start, end;
		int nmi, nmiR;
		nmi = sr.rangeLen / DCHUNK;
		nmiR = sr.rangeLen % DCHUNK;
		if (i < nmiR) {
			start = sr.start + i * nmi + i;
			end = start + nmi + 1;
		} else {
			start = sr.start + i * nmi + nmiR;
			end = start + nmi;
		}
		SRange sr_new(start, end);
		return sr_new;
	}

//	static void child(int i, const SRange& sr, SRange& sr_new) noexcept {
//		int nmi, nmiR;
//		nmi = sr.rangeLen / DCHUNK;
//		nmiR = sr.rangeLen % DCHUNK;
//		if (i < nmiR) {
//			sr_new.start = sr.start + i * nmi + i;
//			sr_new.end = sr_new.start + nmi + 1;
//		} else {
//			sr_new.start = sr.start + i * nmi + nmiR;
//			sr_new.end = sr_new.start + nmi;
//		}
//		sr_new.rangeLen = sr_new.end - sr_new.start;
//	}

};

///SRange Info Object that create a recursive iteration tree for a linear range
///Specialization for DCHUNK==0, LIMITPARALLEL==-1
template<>
struct SRangeInfo<0, -1> : public Arity<0> {

	const int DCHUNK;

	SRangeInfo(const int _DCHUNK = 2, const int nthreads = dpr::internal::num_total_threads()) : Arity<0> (nthreads, nthreads), DCHUNK(_DCHUNK)
	{}

	bool is_base(const SRange& sr) const noexcept {
		return sr.rangeLen < DCHUNK;
	}

	static constexpr bool do_parallel(const SRange& sr) noexcept {
		return true;
	}

	SRange child(int i, const SRange& sr) const noexcept {
		int start, end;
		int nmi, nmiR;
		nmi = sr.rangeLen / DCHUNK;
		nmiR = sr.rangeLen % DCHUNK;
		if (i < nmiR) {
			start = sr.start + i * nmi + i;
			end = start + nmi + 1;
		} else {
			start = sr.start + i * nmi + nmiR;
			end = start + nmi;
		}
		SRange sr_new(start, end);
		return sr_new;
	}

//	void child(int i, const SRange& sr, SRange& sr_new) const noexcept {
//		int nmi, nmiR;
//		nmi = sr.rangeLen / DCHUNK;
//		nmiR = sr.rangeLen % DCHUNK;
//		if (i < nmiR) {
//			sr_new.start = sr.start + i * nmi + i;
//			sr_new.end = sr_new.start + nmi + 1;
//		} else {
//			sr_new.start = sr.start + i * nmi + nmiR;
//			sr_new.end = sr_new.start + nmi;
//		}
//		sr_new.rangeLen = sr_new.end - sr_new.start;
//	}

	int num_children(const SRange& sr) const noexcept {
		return DCHUNK;
	}

};

//definition and function set of global limitParallel to implement a static do_parallel for a unknow compile-time variable 'limitParallel'
int globalLimitParallel = 4;
static void setSRangeGlobalLimitParallel(int limitParallel) {
	globalLimitParallel = limitParallel;
}

//SRange Info Object that allows define a limitParallel limit in a do_parallel function for a custom partitioner (uses globalLimitParallel)
///Specialization for LIMITPARALLEL==0
template<int DCHUNK>
struct SRangeInfo<DCHUNK, 0> : public Arity<DCHUNK> {

	SRangeInfo(const int nthreads = dpr::internal::num_total_threads()) : Arity<DCHUNK> (nthreads, nthreads)
	{}

	static bool is_base(const SRange& sr) noexcept {
		return sr.rangeLen < DCHUNK;
	}

	static bool do_parallel(const SRange& sr) noexcept {
		return sr.rangeLen > globalLimitParallel;
	}

	static SRange child(int i, const SRange& sr) noexcept {
		int start, end;
		int nmi, nmiR;
		nmi = sr.rangeLen / DCHUNK;
		nmiR = sr.rangeLen % DCHUNK;
		if (i < nmiR) {
			start = sr.start + i * nmi + i;
			end = start + nmi + 1;
		} else {
			start = sr.start + i * nmi + nmiR;
			end = start + nmi;
		}
		SRange sr_new(start, end);
		return sr_new;
	}

//	static void child(int i, const SRange& sr, SRange& sr_new) noexcept {
//		int nmi, nmiR;
//		nmi = sr.rangeLen / DCHUNK;
//		nmiR = sr.rangeLen % DCHUNK;
//		if (i < nmiR) {
//			sr_new.start = sr.start + i * nmi + i;
//			sr_new.end = sr_new.start + nmi + 1;
//		} else {
//			sr_new.start = sr.start + i * nmi + nmiR;
//			sr_new.end = sr_new.start + nmi;
//		}
//		sr_new.rangeLen = sr_new.end - sr_new.start;
//	}

};

//SRange Info Object that allows define a limitParallel limit in a do_parallel function for a custom partitioner (uses globalLimitParallel)
///Specialization for DCHUNK==0, LIMITPARALLEL==0
template<>
struct SRangeInfo<0, 0> : public Arity<0> {

	const int DCHUNK;

	SRangeInfo(const int _DCHUNK = 2, const int nthreads = dpr::internal::num_total_threads()) : Arity<0> (nthreads, nthreads), DCHUNK(_DCHUNK)
	{}

	bool is_base(const SRange& sr) const noexcept {
		return sr.rangeLen < DCHUNK;
	}

	static bool do_parallel(const SRange& sr) noexcept {
		return sr.rangeLen > globalLimitParallel;
	}

	SRange child(int i, const SRange& sr) const noexcept {
		int start, end;
		int nmi, nmiR;
		nmi = sr.rangeLen / DCHUNK;
		nmiR = sr.rangeLen % DCHUNK;
		if (i < nmiR) {
			start = sr.start + i * nmi + i;
			end = start + nmi + 1;
		} else {
			start = sr.start + i * nmi + nmiR;
			end = start + nmi;
		}
		SRange sr_new(start, end);
		return sr_new;
	}

//	void child(int i, const SRange& sr, SRange& sr_new) const noexcept {
//		int nmi, nmiR;
//		nmi = sr.rangeLen / DCHUNK;
//		nmiR = sr.rangeLen % DCHUNK;
//		if (i < nmiR) {
//			sr_new.start = sr.start + i * nmi + i;
//			sr_new.end = sr_new.start + nmi + 1;
//		} else {
//			sr_new.start = sr.start + i * nmi + nmiR;
//			sr_new.end = sr_new.start + nmi;
//		}
//		sr_new.rangeLen = sr_new.end - sr_new.start;
//	}

	int num_children(const SRange& sr) const noexcept {
		return DCHUNK;
	}

};

namespace internal {

//allow creation of a generic EmptyBody for a parallel for
template<typename F>
struct generic_psr_for_body : EmptyBody<SRange, void> {
	const F& f_;

	generic_psr_for_body(const F& f) : f_(f) {}

	generic_psr_for_body(const generic_psr_for_body& other) : f_(other.f_) {}

	generic_psr_for_body(generic_psr_for_body&& other) : f_(other.f_) {}

	void base(const SRange& range) {
		f_(range.start, range.end);
	}
};

template<typename F>
inline const generic_psr_for_body<F> make_generic_psr_for_body(F&& f) {
	return generic_psr_for_body<F>(f);
}

//allow creation of a generic EmptyBody for a parallel reduce
template<typename Return, typename F, typename ReduceF>
struct generic_psr_for_reduce_body : EmptyBody<SRange, Return> {
	const F& f_;
	const ReduceF& rf_;

	generic_psr_for_reduce_body(const F& f, const ReduceF& rf) :
	f_(f), rf_(rf)
	{}

	generic_psr_for_reduce_body(const generic_psr_for_reduce_body& other) :
	f_(other.f_), rf_(other.rf_)
	{}

	generic_psr_for_reduce_body(generic_psr_for_reduce_body&& other) :
	f_(other.f_), rf_(other.rf_)
	{}

	Return base(const SRange& range) {
		return f_(range.start, range.end);
	}

	void post(const Return& r, Return& rr) {
		rr = rf_(rr, r);
	}

};

template<typename Return, typename F, typename ReduceF>
inline const generic_psr_for_reduce_body<Return, F, ReduceF> make_generic_psr_for_reduce_body(const F& f, const ReduceF& rf) {
	return generic_psr_for_reduce_body<Return, F, ReduceF>(f, rf);
}

} // namespace internal

} // namespace dpr

#endif /* DPR_SRANGE_COMM_H_ */
