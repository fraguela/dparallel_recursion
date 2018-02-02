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
/// \file     Arity.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_ARITY_H_
#define DPR_ARITY_H_

#include <thread>

/*! \namespace dpr
 *
 * \brief Contains the ::dparallel_recursion and ::parallel_recursion skeletons and associated items
 *
 */
namespace dpr {

/// Convenience constant to indicate that the arity of the problem is unknown or variable
const int UNKNOWN = 0;

/// Helper info base class suitable for ::parallel_recursion
template<int NCHILDREN>
struct Arity {

        /// Number of children of the problem
	static const int NumChildren = NCHILDREN;
  
        /// Original number of tasks requested by the user
        const int ntasks_;
  
        /// Level at which the tasks should become sequential
        mutable int parLevel_;
  
        static constexpr int Nlevels(unsigned int t, int i, unsigned int ntasks) noexcept
        {
           return (t < ntasks) ? Nlevels(t * NCHILDREN, i+1, ntasks) : i;
        }
  
        static constexpr int LevelsForNTasks(unsigned int ntasks) noexcept
        {
          return (!NCHILDREN) ? 3 /*clueless*/
                              : (NCHILDREN == 1) ? 0 /* subdivision makes no sense */
                                                 : Nlevels(1, 0, ntasks);
        }

        constexpr Arity(int ntasks = std::thread::hardware_concurrency() * 2) noexcept :
        ntasks_(ntasks), parLevel_(LevelsForNTasks(ntasks))
        { /* printf("%d -> ParLevel=%d\n", ntasks, parLevel_);*/ }

        constexpr Arity(const Arity<NCHILDREN>& other) noexcept :
        ntasks_(other.ntasks_), parLevel_(other.parLevel_)
        { }
  
        /// Provide the default number of children of the problem
	template<typename T>
	static int num_children(const T& t) noexcept {
          return NCHILDREN;
	}
  
        /// Provide the default cost for a problem
        template<typename T>
        static float cost(const T& t) noexcept {
          return 1.0f;
        }
};


} // namespace dpr

#endif /* DPR_ARITY_H_ */

