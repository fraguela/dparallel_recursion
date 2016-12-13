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
/// \file     MyArray.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef _DPR_MYARRAY_H_
#define _DPR_MYARRAY_H_

/// Behaves similar to a FillableAliasVector
template<typename T>
class MyArray {
  int sz_;
  T* p_;
  bool owner_;
  
  public:
  static bool verbose;
  
  MyArray(int sz = 0) :
		sz_(sz), p_(sz ? new T [sz] : NULL), owner_(sz != 0) {
                  if (verbose && sz_) {
                    std::cout << " Building v of size " << sz_ << std::endl;
                  }
                }
  
  //Non-shallow Arrays are deep copied
  MyArray(const MyArray& other) :
		sz_(other.sz_), p_(other.owner_ ? new T[other.sz_] : other.p_), owner_(other.owner_) {
                  if(owner_) {
                    memcpy((void*)p_, (void*)other.p_, sizeof(T) * sz_);
                    if (verbose && sz_) {
                      std::cout << " Copying v of size " << sz_ << std::endl;
                    }
                  }
                }
  
  MyArray& operator=(const MyArray& other) {
    sz_ = other.sz_;
    p_ = other.p_;
    owner_ = false;
    return *this;
  }
  
  //Shallow reference
  MyArray(T* p, int sz) :
  sz_(sz), p_(p), owner_(false) {
  }
  
  int size() const {
    return sz_;
  }
  
  T& operator[] (int i) {
    return p_[i];
  }
  
  T operator[] (int i) const {
    return p_[i];
  }
  
  T* data() const {
    return p_ ;
  }
  
  ~MyArray() {
    if (owner_ && sz_) {
      delete [] p_;
    } else {
      //std::cout << rank << " VDest Sz=" << sz_ << std::endl;
    }
  }
  
  template<class Archive>
  void save(Archive& ar, const unsigned int file_version) const {
    ar & sz_ & boost::serialization::make_array(p_, sz_);
  }
  
  template<class Archive>
  void load(Archive& ar, const unsigned int file_version) {
    if(!sz_ && !owner_) { //fill in from scratch
      ar & sz_;
      p_ = new T [sz_];
      owner_ = true;
    } else {
      int tmp;
      ar & tmp;
      
      if (tmp != sz_) {
        std::cerr << " Error: load archive mismatch on destination existing MyArray\n";
        std::cerr << "Existing=" << sz_ << " != " << "Received=" << tmp << std::endl;
        std::cerr << "Existing ptr=" << p_ << " owned=" << (owner_ ? "TRUE" : "FALSE") << std::endl;
        exit(-1);
      }
    }
    
    ar& boost::serialization::make_array(p_, sz_);
  }
  
  BOOST_SERIALIZATION_SPLIT_MEMBER()
};

template<typename T>
bool MyArray<T>::verbose = false;

#endif

