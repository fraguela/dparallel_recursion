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

#ifndef MATRIX_H
#define MATRIX_H

#include <cstring>
#include <cassert>
#include <iostream>
#include <cmath>

/// A 2D Matrix
template<typename T>
class Matrix {
  
  static const T EPSILON;;

  T *data_;
  int m_, n_;
  int rowStride_;
  bool owned_;
  
public:
  
  /// Square matrix with owned data
  Matrix(int sz) :
  data_(new T [sz * sz]),
  m_(sz),
  n_(sz),
  rowStride_(sz),
  owned_(true)
  { }
  
  /// General matrix with owned data
  Matrix(int m, int n) :
  data_(new T [m * n]),
  m_(m),
  n_(n),
  rowStride_(n),
  owned_(true)
  { }
  
  /// Copy costructor forbidden
  Matrix(const Matrix& other) = delete;
  
  /// Move constructor
  Matrix(Matrix&& other) :
  data_(other.data_),
  m_(other.m_),
  n_(other.n_),
  rowStride_(other.rowStride_),
  owned_(other.owned_)
  {
    other.data_ = nullptr;
    other.owned_ = false;
  }
  
  // Subregion matrix. Indices are [, )
  Matrix(const Matrix& other, int r0, int r1, int c0, int c1) :
  data_(other.data_ + r0 * other.rowStride_ + c0),
  m_(r1-r0),
  n_(c1-c0),
  rowStride_(other.rowStride_),
  owned_(false)
  { }
  
  T* raw() const { return data_;}
  int rows() const { return m_; }
  int cols() const { return n_;}
  int row_stride() const { return rowStride_; }

  /// Constant access to element
  const T& operator() (int i, int j) const { return data_[i * rowStride_ + j]; }
  
  /// Non-constant access to element
  T& operator() (int i, int j) { return data_[i * rowStride_ + j]; }

  Matrix operator+(const Matrix& other) const
  {
    assert(m_ == other.m_);
    assert(n_ == other.n_);
    
    Matrix res(m_, n_);
    
    for (int i = 0; i < m_; i++)
      for (int j = 0; j < n_; j++)
        res.data_[i * res.rowStride_ + j] = data_[i * rowStride_ + j] + other.data_[i * other.rowStride_ + j];
    
    return res;
  }
  
  Matrix operator-(const Matrix& other) const
  {
    assert(m_ == other.m_);
    assert(n_ == other.n_);
    
    Matrix res(m_, n_);
    
    for (int i = 0; i < m_; i++)
      for (int j = 0; j < n_; j++)
        res.data_[i * res.rowStride_ + j] = data_[i * rowStride_ + j] - other.data_[i * other.rowStride_ + j];
    
    return res;
  }

#ifndef NO_VALIDATE
  bool operator==(const Matrix& other) const
  {
    if( (m_ == other.m_) && (n_ == other.n_) ) {
      for (int i = 0; i < m_; i++)
        for (int j = 0; j < n_; j++)
          if(fabs(data_[i * rowStride_ + j] - other.data_[i * other.rowStride_ + j]) > EPSILON) {
            std::cout << '(' << i << ", " << j << ") : "
            << data_[i * rowStride_ + j] << " != " << other.data_[i * other.rowStride_ + j]
            << std::endl;
            return false;
          }
      return true;
    }
    return false;
  }
#endif

  ~Matrix()
  {
    if (owned_) {
      delete [] data_;
    }
  }
  
};

template<typename T>
const T Matrix<T>::EPSILON = (T)1e-5;

#endif
