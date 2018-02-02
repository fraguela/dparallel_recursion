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

#ifdef OPENBLAS

#include "cblas.h"

void mxm(const MMatrix& a, const MMatrix& b, MMatrix& c)
{
  openblas_set_num_threads(1);
  
  cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
              a.rows(), c.cols(), a.cols(),
              1.0, a.raw(), a.row_stride(),
              b.raw(), b.row_stride(),
              0.0, c.raw(), c.row_stride());
}

#else

#ifndef NOBOOST

#define BOOST_UBLAS_SHALLOW_ARRAY_ADAPTOR
#define BOOST_UBLAS_NDEBUG

#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/operation.hpp>

void mxm(const MMatrix& a, const MMatrix& b, MMatrix& c)
{
  using namespace boost::numeric::ublas;

  typedef matrix<double, row_major, shallow_array_adaptor<double> > MyMatrixType;
  
  shallow_array_adaptor<double> am(a.rows() * a.row_stride(), a.raw());
  shallow_array_adaptor<double> bm(b.rows() * b.row_stride(), b.raw());
  shallow_array_adaptor<double> cm(c.rows() * c.row_stride(), c.raw());
  
  MyMatrixType ax(a.rows(), a.row_stride(), am);
  MyMatrixType bx(b.rows(), b.row_stride(), bm);
  MyMatrixType cx(c.rows(), c.row_stride(), cm);

  matrix_range<MyMatrixType> mra (ax, range (0, a.rows()), range (0, a.cols()));
  matrix_range<MyMatrixType> mrb (bx, range (0, b.rows()), range (0, b.cols()));
  matrix_range<MyMatrixType> mrc (cx, range (0, c.rows()), range (0, c.cols()));
  
  axpy_prod(mra, mrb, mrc, true);
}

#else

void mxm(const MMatrix& a, const MMatrix& b, MMatrix& c)
{
  const int common_dim = a.cols();
  assert(common_dim == b.rows());
  for (int i = 0; i < c.rows(); i++) {
    for (int j = 0; j < c.cols(); j++) {
      double r = 0.0;
      for (int k = 0; k < common_dim; k++) {
        r += a(i, k) * b(k, j);
      }
      c(i, j) = r;
    }
  }
}

#endif

#endif
