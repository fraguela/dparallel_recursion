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
/// \file     dpr_utils.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_UTILS_H_
#define DPR_UTILS_H_

// We include it always because it is also used by the balancing distribution heuristics
#include <chrono>
#include <algorithm>
#include <array>
#include <cmath>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace dpr {

  /// clock used for profiling
  using profile_clock_t    = std::chrono::high_resolution_clock;

  /// standard duration used to express profiling periods
  using profile_duration_t = std::chrono::duration<double>;

} //namespace dpr


///Remove to avoid profiling messages
//#define PROFILE 1

#ifdef PROFILE

#include <iostream>

#define PROFILELOG(TEXT, ...)                        \
do { const auto _t0_ = dpr::profile_clock_t::now();  \
  __VA_ARGS__ ;                                      \
  const auto _t1_ = dpr::profile_clock_t::now();     \
  std::cerr << TEXT << ' ' << dpr::profile_duration_t(_t1_ - _t0_).count() << std::endl; } while(0)

#ifdef MPI_PROFILE
#define MPI_PROFILELOG(TEXT, ...) PROFILELOG(TEXT, __VA_ARGS__)
#else
#define MPI_PROFILELOG(TEXT, ...)  do{ __VA_ARGS__ ; } while(0)
#endif

#define PROFILEACTION(...) do{ __VA_ARGS__ ; } while(0)

#define PROFILEDEFINITION(...) __VA_ARGS__ ;

#else

#define PROFILELOG(TEXT, ...)  do{ __VA_ARGS__ ; } while(0)
#define MPI_PROFILELOG(TEXT, ...)  do{ __VA_ARGS__ ; } while(0)

#define PROFILEACTION(...)      /* no profiling */

#define PROFILEDEFINITION(...)  /* no profiling */

#endif //PROFILE

#ifndef _unused
#define _unused(x) ((void)(x))
#endif

namespace dpr {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////// Start of time measurement functions /////////////////////////////////////////////////////////
// Get a Timepoint (timestamp) of now with steady_clock (monotonic clock that is guaranteed to never be adjusted)
static std::chrono::steady_clock::time_point getTimepoint() {
	return std::chrono::steady_clock::now();
}

// Get the difference in seconds (double) between two Timepoints
static double getTimediff(const std::chrono::steady_clock::time_point& init_time, const std::chrono::steady_clock::time_point& finish_time) {
	return std::chrono::duration_cast<std::chrono::duration<double>>(finish_time - init_time).count();
}
/////////////////////////////////////////////////////////// End of time measurement functions //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

namespace internal {

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////// Start of auxiliary synchronization functions ////////////////////////////////////////////////////

class thread_barrier {
public:
	explicit thread_barrier(std::size_t iCount) :
	mThreshold(iCount),
	mCount(iCount),
	mGeneration(0) { }

	void wait() {
		std::unique_lock<std::mutex> lLock{mMutex};
		auto lGen = mGeneration;
		if (!--mCount) {
			mGeneration++;
			mCount = mThreshold;
			mCond.notify_all();
		} else {
			mCond.wait(lLock, [this, lGen] { return lGen != mGeneration; });
		}
	}

private:
	std::mutex mMutex;
	std::condition_variable mCond;
	std::size_t mThreshold;
	std::size_t mCount;
	std::size_t mGeneration;
};

/////////////////////////////////////////////////////// End of auxiliary synchronization functions /////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////// Start of SFINAE Helper functions ///////////////////////////////////////////////////////////
//// The "call_clone" function allow us call a .clone() function of a class if this function exists, otherwise this call our clone() implementation
// Our implementation of clone() function
template <typename T>
T clone(T &t) {
	return T(t);
}

// Function called with "call_clone" if .clone() function of a class exists
template <typename T>
auto call_clone_h (T t, int) -> decltype( t.clone() ) {
	return t.clone();
}

// Function called with "call_clone" if .clone() function of a class don't exists
template <typename T>
auto call_clone_h (T t, long) -> decltype( clone(t) ) {
	return T(t);
}

// Main Function: to call a .clone() function of a class if this function exists, otherwise this call our clone() implementation
template <typename T>
T call_clone (T const & t) {
	return call_clone_h(t, 0);
}

//// The "call_deallocate" function allow us call a .deallocate() function of a class if this function exists, otherwise this call our deallocate() implementation
// Our implementation of deallocate() function
template <typename T>
void deallocate(T &t) {
}

// Function called with "call_deallocate" if .deallocate() function of a class exists
template <typename T>
auto call_deallocate_h (T t, int) -> decltype( t.deallocate() ) {
	t.deallocate();
}

// Function called with "call_deallocate" if .deallocate() function of a class don't exists
template <typename T>
auto call_deallocate_h (T t, long) -> decltype( deallocate(t) ) {
	deallocate(t);
}

// Main Function: to call a .deallocate() function of a class if this function exists, otherwise this call our deallocate() implementation
template <typename T>
void call_deallocate (T const & t) {
	call_deallocate_h(t, 0);
}
//////////////////////////////////////////////////////////// End of SFINAE Helper functions ////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////// Start of auxiliary memory functions /////////////////////////////////////////////////////////
// Class that allows allocate dynamic memory aligned (warning: the memory is deallocated when the class destructor is called)
struct AlignedBufferedAllocator {
	std::vector<char> vdata;
	void* p;
	std::size_t sz;
	AlignedBufferedAllocator(std::size_t _buffsize) : sz(_buffsize)
	{
		vdata.resize(_buffsize);
		p = vdata.data();
	}

	template <typename T>
	T* aligned_alloc(std::size_t alignment = alignof(T)) {
		if (std::align(alignment, sizeof(T), p, sz)) {
			T* result = reinterpret_cast<T*>(p);
                        new (result) T();
			p = (char*)p + sizeof(T);
			sz -= sizeof(T);
			return result;
		}
		return nullptr;
	}
};
/////////////////////////////////////////////////////////// End of auxiliary memory functions //////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////// Start of auxiliary math functions //////////////////////////////////////////////////////////
// Function that calculates the arithmetic mean of the values contained in an STL container
template<typename Container, typename T = typename Container::value_type>
static T calculateMean(const Container &cont_data) {
	T mean{};
	if (cont_data.size() > 0) {
		for (const T &data : cont_data) {
			mean += data;
		}
		if (std::is_integral<T>::value) {
			mean = std::round(static_cast<double>(mean) / static_cast<double>(cont_data.size()));
		} else {
			mean = mean / static_cast<double>(cont_data.size());
		}
	}
	return mean;
}

// Function that calculates the harmonic mean of the values contained in an STL container
template<typename Container, typename T = typename Container::value_type>
static T calculateHarmonicMean(const Container &cont_data) {
	double hmean = 0.0;
	if (cont_data.size() > 0) {
		for (const T &data : cont_data) {
			hmean += 1.0 / static_cast<double>(data);
		}
		hmean = static_cast<double>(cont_data.size()) / hmean;
	}
	if (std::is_integral<T>::value) {
		return static_cast<T>(std::round(hmean));
	} else {
		return static_cast<T>(hmean);
	}
}

// Function that calculates the geometric mean of the values contained in an STL container
template<typename Container, typename T = typename Container::value_type>
static T calculateGeometricMean(const Container &cont_data) {
	double gmean = 0.0;
	if (cont_data.size() > 0) {
		for (const T &data : cont_data) {
			gmean += std::log(data);
		}
		gmean = std::exp(gmean / static_cast<double>(cont_data.size()));
	}
	if (std::is_integral<T>::value) {
		return static_cast<T>(std::round(gmean));
	} else {
		return static_cast<T>(gmean);
	}
	return gmean;
}

// Function that calculates the median of the values contained in an STL container
template<typename Container, typename T = typename Container::value_type>
static T calculateMedian(Container cont_data) {
	const size_t size = cont_data.size();
	if (size == 0) {
		return 0;
	}
	std::sort(cont_data.begin(), cont_data.end());
	if (size % 2) {
		return cont_data[size/2];
	} else {
		return calculateMean(std::array<T,2>{cont_data[size/2-1],cont_data[size/2]});
	}
}
//////////////////////////////////////////////////////////// End of auxiliary math functions ///////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

} //namespace dpr

#endif /* DPR_UTILS_H_ */
