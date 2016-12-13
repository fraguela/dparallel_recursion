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
/// \file     dpr_utils.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_UTILS_H_
#define DPR_UTILS_H_

// We include it always because it is also used by the balancing distribution heuristics
#include <chrono>

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


#endif /* DPR_UTILS_H_ */
