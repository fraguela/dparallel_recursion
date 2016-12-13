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
/// \file     EmptyBody.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_EMPTYBODY_H_
#define DPR_EMPTYBODY_H_

namespace dpr {

/// Helper body base class suitable for both ::parallel_recursion and ::dparallel_recursion
template<typename T, typename Ret>
class EmptyBody {
public:
        EmptyBody() { }
  
	~EmptyBody() { }

	Ret base(const T& t) { }
	void pre(const T& t) { }
	void pre_rec(const T& t) { }
	Ret post(const T& t, Ret* r) { }
};

}

#endif /* DPR_EMPTYBODY_H_ */
