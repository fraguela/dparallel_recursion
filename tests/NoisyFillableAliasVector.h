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
/// \file     NoisyFillableAliasVector.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <dparallel_recursion/FillableAliasVector.h>
#include <tbb/atomic.h>

using namespace dpr;

template<typename T>
class NoisyFillableAliasVector : public FillableAliasVector<T> {
public:
  
  static int rank;
  static tbb::atomic<int> allocations;
  
  NoisyFillableAliasVector(T* data = nullptr, std::size_t sz = 0, T* sourcedata = nullptr, bool owned = false) :
  FillableAliasVector<T>(data, sz, sourcedata, owned)
  {
    /* There is no allocation
     if (owned) {
     printf("[%d] Teor.AllocSz=%d\n", rank, static_cast<int>(this->sz_));
     }
     */
  }
  
  template<typename VT>
  NoisyFillableAliasVector(VT& v) :
  FillableAliasVector<T>(v)
  {}
  
  /// Copy constructor
  /// Notice that copies never own data, just point to the original
  NoisyFillableAliasVector(const AliasVector<T>& other) :
  FillableAliasVector<T>(other)
  { }
  
  NoisyFillableAliasVector(NoisyFillableAliasVector&& other) :
  FillableAliasVector<T>(std::move(other))
  { }
  
  NoisyFillableAliasVector(typename AliasVector<T>::size_type sz) :
  FillableAliasVector<T>(sz)
  {
    if(this->sz_) {
      printf("[%d] AllocSz=%d\n", rank, static_cast<int>(this->sz_));
      allocations.fetch_and_increment();
    }
    
  }
  
  NoisyFillableAliasVector range(int start, int end) const
  {
    assert(start <= end);
    assert(end <= this->sz_);
    return NoisyFillableAliasVector(this->data_ + start, end - start, this->sourceData_);
  }
  
  NoisyFillableAliasVector range(const Range& irange) const
  {
    return range(irange.start, irange.end);
  }
  
  NoisyFillableAliasVector& operator=(const NoisyFillableAliasVector& other) = delete;
  
  NoisyFillableAliasVector& operator=(NoisyFillableAliasVector&& other)
  {
    FillableAliasVector<T>::operator=(std::move(other));
    return *this;
  }
  
  NoisyFillableAliasVector deepCopy() const
  {
    if (this->sz_) {
      assert(this->data_ != nullptr);
      T * const p = new T [this->sz_];
      //TODO : Make individual copies for objects
      memcpy(p, this->data_, this->sz_ *  sizeof(T));
      printf("[%d] DeepCopySz=%d\n", rank, static_cast<int>(this->sz_));
      allocations.fetch_and_increment();
      return NoisyFillableAliasVector(p, this->sz_, p, true);
    } else {
      return NoisyFillableAliasVector(nullptr, 0, nullptr, false);
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
          printf("[%d] DeepLoadSz=%d\n", rank, static_cast<int>(this->sz_));
          allocations.fetch_and_increment();
          this->owned_ = true;
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
    const int nitem = ar.current_item();
    const bool was_first = !nitem;
    const bool was_empty = (this->data_ == nullptr);
    T* const prev_data = this->data_;
    
    ar & this->data_ & this->sz_ & this->owned_;
    
    if(was_first && was_empty && (this->data_ != nullptr)) {
      this->owned_ = true;
      this->sourceData_ = this->data_;
      printf("[%d] %d gather_scatter built=%d\n", rank, nitem, static_cast<int>(this->sz_));
    } else {
      if (this->data_ != prev_data) {
        printf("[%d] %d gather_scatter reloc=%d (own=%c) (wasnull=%c) %p -> %p\n",
               rank, nitem, static_cast<int>(this->sz_),
               this->owned_ ? 'Y' : 'N', was_empty ? 'Y' : 'N', prev_data, this->data_);
        if (this->sourceData_ == prev_data) {
          this->sourceData_ = this->data_;
        }
      } else {
        printf("[%d] %d gather_scatter decl=%d\n", rank, nitem, static_cast<int>(this->sz_));
      }
    }
  }
  
  ~NoisyFillableAliasVector()
  {
    if(this->owned_) {
      //printf("[%d] FreeSz=%d\n", rank, static_cast<int>(this->sz_));
    }
  }
  
};

template<typename T>
int NoisyFillableAliasVector<T>::rank;

template<typename T>
tbb::atomic<int> NoisyFillableAliasVector<T>::allocations;
