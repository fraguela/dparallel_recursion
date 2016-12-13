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
/// \file     AliasVector.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_ALIASVECTOR_H_
#define DPR_ALIASVECTOR_H_

#include "dparallel_recursion/Range.h"

namespace dpr {

/// Vector that never owns data, just points to other container's data
template<typename T>
class AliasVector {

protected:

  T * data_;              ///< Shallow vector starting point
  std::size_t sz_;        ///< Shallow vector size
  T * sourceData_;        ///< Source vector starting point
  //std::size_t sourceSz_;  ///< Source vector size
  
public:
  
  typedef T                  value_type;
  typedef std::size_t        size_type;
  typedef std::ptrdiff_t     difference_type;
  typedef value_type&        reference;
  typedef const value_type&  const_reference;
  typedef value_type*        pointer;
  typedef const value_type*  const_pointer;
  typedef value_type*        iterator;
  typedef const value_type*  const_iterator;
  
  /// Builds an AliasVector from a pointer to data, a size, and a pointer to the beginning of the underlying data
  constexpr AliasVector(T* data = nullptr, std::size_t sz = 0, T* sourcedata = nullptr) noexcept :
  data_(data), sz_(sz), sourceData_(sourcedata ? sourcedata : data)//, sourceSz_(sz)
  { }
  
  /// Builds an AliasVector for an existing container that provides the API data() and size()
  template<template<typename, typename...> class VT, typename... Rest>
  constexpr AliasVector(VT<T, Rest...>& v) noexcept :
  data_(v.data()), sz_(v.size()), sourceData_(v.data())//, sourceSz_(v.size())
  {}
  
  /// Copy constructor
  constexpr AliasVector(const AliasVector& other) noexcept :
  data_(other.data_), sz_(other.sz_), sourceData_(other.sourceData_)//, sourceSz_(other.sourceSz_)
  { }
  
  /// Builds an AliasVector for an exclusive range within this vector, since it is the stardard in C++
  AliasVector range(int start, int end) const
  {
    assert(start <= end);
    assert(end <= sz_);
    return AliasVector(data_ + start, end - start, sourceData_);
  }
  
  /// Builds an AliasVector for the Range \c irange, interpreted as a exclusive range
  AliasVector range(const Range& irange) const
  {
    return range(irange.start, irange.end);
  }
  
  reference operator[] (size_type i )
  { assert(i < sz_); return data_[i]; }
  
  const_reference  operator[] (size_type i ) const
  { assert(i < sz_); return data_[i]; }
  
  iterator begin() noexcept { return data_; }
  
  const_iterator begin() const noexcept { return data_; }
  
  iterator end() noexcept { return data_ + sz_; }
  
  const_iterator end() const noexcept { return data_ + sz_; }

  void resize(std::size_t sz) noexcept
  { sz_ = sz; }

  size_type size() const noexcept
  { return sz_; }

  T *data() const noexcept
  { return data_; }

  size_type offset() const noexcept
  { return data_ - sourceData_; }
  
  template<class Archive>
  void serialize(Archive& ar,  unsigned int file_version)
  {
    
    if (Archive::is_saving::value) {
      ar & sz_;
    } else {
      size_type tmp;
      ar & tmp;
      assert(tmp == sz_);
    }
    
    ar & boost::serialization::make_array(data_, sz_);
  }
  
  /*
  template<class Archive>
  void save(Archive& ar, const unsigned int file_version) const
  {
    ar & sz_ & boost::serialization::make_array(data_, sz_);
  }
  
  template<class Archive>
  void load(Archive& ar, const unsigned int file_version)
  { size_type tmp;
    
    ar & tmp;
    assert(tmp == sz_);
    ar & boost::serialization::make_array(data_, sz_);
  }
   
   BOOST_SERIALIZATION_SPLIT_MEMBER()
  */
  
  ~AliasVector()
  {}
  
  friend void swap(AliasVector& a, AliasVector& b) {
    using std::swap; // bring in swap for built-in types
    
    swap(a.data_, b.data_);
    swap(a.sz_, b.sz_);
    swap(a.sourceData_, b.sourceData_);
  }

};

}

#endif /* DPR_ALIASVECTOR_H_ */
