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
#include <mpi.h>

template<typename T>
struct MPI_Datatype_Trait {
  static const MPI_Datatype DataType;
};

template<typename T>
const MPI_Datatype MPI_Datatype_Trait<T>::DataType = MPI_DATATYPE_NULL;

template<>
const MPI_Datatype MPI_Datatype_Trait<double>::DataType = MPI_DOUBLE;

/// A 2D Matrix
template<typename T>
class Matrix {
  
  static const T EPSILON;;

  T *data_;
  int m_, n_;
  int rowStride_;
  bool owned_;
  
public:
  
  /// Empty object
  constexpr Matrix() noexcept :
  data_(nullptr),
  m_(0),
  n_(0),
  rowStride_(0),
  owned_(false)
  { }
  
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
  
  bool empty() const noexcept { return data_ == nullptr; }

  /// Copy constructor
  /// Mimics the input: owned if owned (but always compact); not owned if not owned
  Matrix(const Matrix& other) :
  data_(other.owned_ ? new T [other.m_ * other.n_] : other.data_),
  m_(other.m_),
  n_(other.n_),
  rowStride_(other.owned_ ? other.n_ : other.rowStride_),
  owned_(other.owned_)
  {
    if (owned_) {
      if (other.rowStride_ == other.n_) {
        memcpy(data_, other.data_, m_ * n_ * sizeof(T));
      } else {
        for (int i = 0; i < m_; i++) {
          memcpy(data_ + n_ * i, other.data_ + other.rowStride_ * i, n_ * sizeof(T));
        }
      }
    }
  }
  
  /// Move constructor
  Matrix(Matrix&& other) noexcept :
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
  
  /*
  /// Whether the storage for this matrix is consecutive
  bool consecutive() const { return n_ == rowStride_; }
  */
  
  /// Move-assignment. Mostly useful to fill-in initially empty Matrices
  Matrix& operator=(Matrix&& other)
  {
    if (this != &other) {
      if (owned_) {
        delete [] data_;
      }
      m_ = other.m_;
      n_ = other.n_;
      rowStride_ = other.rowStride_;
      data_ = other.data_;
      owned_ = other.owned_;
      
      other.data_ = nullptr;
      other.owned_ = false;
    }
    
    return *this;
  }
  
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

  // sends and automatically deallocates matrix
  void send(int rank)
  { int v[3];
    
    v[0] = m_;
    v[1] = n_;
    v[2] = rowStride_;
    
    MPI_Send(v, 3, MPI_INT, rank, 0, MPI_COMM_WORLD);
    
    if (n_ == rowStride_) {
      MPI_Send(data_, m_ * n_, MPI_Datatype_Trait<T>::DataType, rank, 0, MPI_COMM_WORLD );
    } else {
      MPI_Datatype mytype;
      
      MPI_Type_vector( m_, n_, rowStride_, MPI_Datatype_Trait<T>::DataType, &mytype );
      MPI_Type_commit( &mytype );
      MPI_Send(data_, 1, mytype, rank, 0, MPI_COMM_WORLD );
      MPI_Type_free( &mytype );
    }
    
    deallocate();
  }
  
  /// Receives in empty or existing Matrix, but if existing, it must match and be owned
  void recv(int rank)
  { MPI_Status s;
    int v[3];

    MPI_Recv(v, 3, MPI_INT, rank, 0, MPI_COMM_WORLD, &s);

    if(data_ == nullptr) {
      m_ = v[0];
      n_ = rowStride_ = v[1];
      data_ = new T [m_ * n_];
      owned_ = true;
    } else {
      assert(owned_);
      assert(m_ == v[0]);
      assert(n_ == rowStride_);
      assert(n_ == v[1]);
    }
    
    if (n_ == v[2]) {
      MPI_Recv(data_, m_ * n_, MPI_Datatype_Trait<T>::DataType, rank, 0, MPI_COMM_WORLD, &s);
    } else {
      MPI_Datatype mytype;
      
      MPI_Type_vector( m_, n_, n_, MPI_Datatype_Trait<T>::DataType, &mytype );
      MPI_Type_commit( &mytype );
      MPI_Recv(data_, 1, mytype, rank, 0, MPI_COMM_WORLD, &s );
      MPI_Type_free( &mytype );
    }
  }
  
  /// Deallocated the array data, only if it is owned
  void deallocate()
  {
    if (owned_) {
      delete [] data_;
      owned_ = false;
      data_ = nullptr;
      //m_ = n_ = rowStride_ = 0;
    }
  }
  
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
