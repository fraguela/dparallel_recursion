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
/// \file     general_reference_wrapper.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef __GENERAL_REFERENCE_WRAPPER_H__
#define __GENERAL_REFERENCE_WRAPPER_H__

#include <cassert>
#include <type_traits>

/// \brief Internal class that allows to store general references to lvalues and rvalues
template<class T>
class general_reference_wrapper {
  
        T* ptr;
        T rvalue;

public:
	// types
	typedef T type;

	// construct/copy/destroy
	general_reference_wrapper() noexcept :
        ptr( std::addressof(rvalue) /* nullptr */)
        {} //new

	general_reference_wrapper(T& ref) noexcept :
        ptr(std::addressof(ref))
        {}
  
        general_reference_wrapper(T&& t) noexcept(std::is_nothrow_move_constructible<T>::value) :
        ptr(std::addressof(rvalue)), rvalue(std::move(t))
        { }

        /// Copy constructor
        general_reference_wrapper(const general_reference_wrapper& x) noexcept(std::is_nothrow_copy_assignable<T>::value)
        {
		if(x.ptr == std::addressof(x.rvalue)) {
			rvalue = x.rvalue;
			ptr = std::addressof(rvalue);
		}
		else
			ptr = x.ptr;
	}
  
        /// Move constructor
        general_reference_wrapper(general_reference_wrapper&& x) noexcept(noexcept(T(std::declval<T>()))) :
        ptr(x.ptr == std::addressof(x.rvalue) ? std::addressof(rvalue) : x.ptr),
        rvalue(std::move(x.rvalue))
        {
                x.ptr = nullptr; //new
        }
  
	/// Copy assignment
 	general_reference_wrapper& operator=(const general_reference_wrapper& x) noexcept(std::is_nothrow_copy_assignable<T>::value)
        {
		if(x.ptr == std::addressof(x.rvalue)) {
			rvalue = x.rvalue;
			ptr = std::addressof(rvalue);
		}
		else
			ptr = x.ptr;

		return *this;
	};
  
        /// Move assignment
        general_reference_wrapper& operator=(general_reference_wrapper&& x) noexcept(std::is_nothrow_move_assignable<T>::value)
        {
                if(x.ptr == std::addressof(x.rvalue)) {
                         rvalue = std::move(x.rvalue);
                         ptr = std::addressof(rvalue);
                }
                else
                         ptr = x.ptr;
          
                x.ptr = nullptr; //new
          
                return *this;
        }

        bool empty() const noexcept { return ptr == nullptr; }
  
	/// access
	operator T& () const noexcept {
		assert(ptr != nullptr);
		return *ptr;
	}

	T& get() const noexcept {
		assert(ptr != nullptr);
		return *ptr;
	}

};

#endif
