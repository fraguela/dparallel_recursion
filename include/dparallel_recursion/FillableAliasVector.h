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
/// \file     FillableAliasVector.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_FILLABLEALIASVECTOR_H_
#define DPR_FILLABLEALIASVECTOR_H_

#include <cstring>
#include "dparallel_recursion/AliasVector.h"

namespace dpr {

/// Vector that never owns data, just points to other container's data
template<typename T>
class FillableAliasVector : public AliasVector<T> {

protected:
    
  bool owned_;
    
public:
  
  FillableAliasVector(T* data = nullptr, std::size_t sz = 0, T* sourcedata = nullptr, bool owned = false) noexcept :
  AliasVector<T>(data, sz, sourcedata),
  owned_(owned)
  { }
  
  template<template<typename, typename...> class VT, typename... Rest>
  FillableAliasVector(VT<T, Rest...>& v) noexcept :
  AliasVector<T>(v),
  owned_(false)
  {}
  
  /// Copy constructor
  /// Notice that copies never own data, just point to the original
  FillableAliasVector(const AliasVector<T>& other) noexcept :
  AliasVector<T>(other),
  owned_(false)
  { }
  
  FillableAliasVector(FillableAliasVector&& other) noexcept :
  AliasVector<T>(other),
  owned_(other.owned_)
  {
    other.data_ = other.sourceData_ = nullptr;
    other.sz_ = 0;
    other.owned_ = false;
  }
  
  FillableAliasVector(typename AliasVector<T>::size_type sz) :
  AliasVector<T>((sz ? new T [sz] : nullptr), sz),
  owned_(true)
  {
    this->sourceData_ = this->data_;
  }
  
  FillableAliasVector range(int start, int end) const
  {
    assert(start <= end);
    assert(end <= this->sz_);
    return FillableAliasVector(this->data_ + start, end - start, this->sourceData_);
  }
  
  FillableAliasVector range(const Range& irange) const
  {
    return range(irange.start, irange.end);
  }
  
  FillableAliasVector& operator=(const FillableAliasVector& other) = delete;
  
  FillableAliasVector& operator=(FillableAliasVector&& other) noexcept
  {
    if(this != &other) {
    
      if (owned_) {
        if(this->sourceData_ == other.sourceData_) {
          assert(!other.owned_);
          if((this->sz_ == other.sz_) && (this->data_ == other.data_)) {
            return *this;
          }
          // This is a logical error. Should we leave the memory as a memory-leak?
          assert(false);
        }
        delete [] this->sourceData_;
      }
      
      this->data_ = other.data_;
      this->sz_ = other.sz_;
      this->sourceData_ = other.sourceData_;
      this->owned_ = other.owned_;
      
      other.data_ = other.sourceData_ = nullptr;
      other.sz_ = 0;
      other.owned_ = false;
    }
    return *this;
  }
  
  bool owned() const noexcept { return owned_; }

  void shallowResize(std::size_t sz) noexcept
  {
    this->AliasVector<T>::resize(sz);
  }
  
  void resize(std::size_t sz)
  {
    if (owned_ && (sz > this->sz_)) {
      T * const p = new T [sz];
      //TODO : Make individual copies for objects
      memcpy(p, this->data_, this->sz_ *  sizeof(T));
      delete [] this->sourceData_;
      this->sourceData_ = this->data_ = p;
      
    }
    shallowResize(sz);
  }
  
  FillableAliasVector deepCopy() const
  {
    if (this->sz_) {
      assert(this->data_ != nullptr);
      T * const p = new T [this->sz_];
      //TODO : Make individual copies for objects
      memcpy(p, this->data_, this->sz_ *  sizeof(T));
      return FillableAliasVector(p, this->sz_, p, true);
    } else {
      return FillableAliasVector(nullptr, 0, nullptr, false);
    }
  }
  
  template<class Archive>
  void serialize(Archive& ar,  unsigned int file_version)
  {
    
    if (Archive::is_saving::value) {
      ar & this->sz_;
    } else {
      typename AliasVector<T>::size_type tmp;
      ar & tmp;
      
      if (this->data_ == nullptr) {
        this->sz_ = tmp;
        if (tmp) {
          this->sourceData_ = this->data_ = new T [this->sz_];
          owned_ = true;
        }
      } else {
        assert(tmp == this->sz_);
      }
    }
    
    if (this->sz_ != 0) {
      ar & boost::serialization::make_array(this->data_, this->sz_);
    }
    
  }
  
  template<class Archive>
  void gather_scatter(Archive &ar) {
    const bool was_empty_and_first = (this->data_ == nullptr) && !ar.current_item();

    ar & this->data_ & this->sz_ & this->owned_;

    if(was_empty_and_first && (this->data_ != nullptr)) {
      this->owned_ = true;
      this->sourceData_ = this->data_;
    }
  }
  
  ~FillableAliasVector()
  {
    if(owned_) {
      delete [] this->sourceData_;
    }
  }
  
  friend void swap(FillableAliasVector& a, FillableAliasVector& b) {
    using std::swap; // bring in swap for built-in types
    
    swap(static_cast<AliasVector<T>&>(a), static_cast<AliasVector<T>&>(b));
    swap(a.owned_, b.owned_);
  }

};

}

#endif /* DPR_FILLABLEALIASVECTOR_H_ */
