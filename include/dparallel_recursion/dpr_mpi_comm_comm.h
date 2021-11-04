/*
 dparallel_recursion: distributed parallel_recursion skeleton
 Copyright (C) 2015-2020 Millan A. Martinez, Basilio B. Fraguela, Jose C. Cabaleiro. Universidade da Coruna

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
/// \file     dpr_mpi_comm_comm.h
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#ifndef DPR_MPI_COMM_COMM_H_
#define DPR_MPI_COMM_COMM_H_

#include <vector>
#include <mpi.h>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

#ifdef ALT_BUFFERING

#define BOOST_MPI_HOMOGENEOUS
#include <boost/mpi/packed_iarchive.hpp>
#include <boost/mpi/packed_oarchive.hpp>

#include <boost/archive/detail/archive_serializer_map.hpp>
#include <boost/archive/impl/archive_serializer_map.ipp>

#ifndef LINK_WITH_BOOST_MPI
//BBF: This should be included only if there is no libboost_mpi to link with
namespace boost {

  namespace mpi {

    exception::exception(const char* routine, int result_code)
    : routine_(routine), result_code_(result_code)
    {
      // Query the MPI implementation for its reason for failure
      char buffer[MPI_MAX_ERROR_STRING];
      int len;
      MPI_Error_string(result_code, buffer, &len);

      // Construct the complete error message
      message.append(routine_);
      message.append(": ");
      message.append(buffer, len);
    }

    exception::~exception() throw() { }

  } // end namespace boost::mpi

  namespace archive {

    // explicitly instantiate all required templates

    template class detail::archive_serializer_map<mpi::packed_iarchive> ;
    template class detail::archive_serializer_map<mpi::packed_oarchive> ;

  } // end namespace boost::archive


} // end namespace boost
#endif

#else

#include <boost/iostreams/stream_buffer.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/iostreams/device/back_inserter.hpp>

#endif


#include <boost/serialization/is_bitwise_serializable.hpp>
#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/list.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/utility.hpp>

#include <boost/mpl/if.hpp>

#include <type_traits>

#include <utility>

#include <boost/version.hpp>

#if BOOST_VERSION < 106100
namespace boost {
  namespace serialization {
    template<typename T>
    using array_wrapper = array<T>;
  };
}
#endif

#if __cplusplus < 201400
namespace std {
  template< bool B, typename T = void >
  using enable_if_t = typename std::enable_if<B, T>::type;
}
#endif

#include "dparallel_recursion/dpr_utils.h"

namespace dpr {

  /// Trait that marks whether a given datatype must be sent by chunks
  template<typename T>
  struct TransmitByChunks {
    static const bool value = false;
  };

  /// Mark a type to be sent by chunks
#define TRANSMIT_BY_CHUNKS(T) namespace dpr { template<> struct TransmitByChunks<T> { static const bool value=true; }; };

  template<typename T>
  struct matrix_by_chunks : public boost::serialization::wrapper_traits<const matrix_by_chunks< T > > {
    T* data_;
    int m_, n_, rowStride_;

    typedef T value_type;

    matrix_by_chunks(T *data, int m, int n, int rowStride) :
    data_(data), m_(m), n_(n), rowStride_(rowStride)
    {}

    matrix_by_chunks(const matrix_by_chunks& other) :
    data_(other.data_), m_(other.m_), n_(other.n_), rowStride_(other.rowStride_)
    {}

    matrix_by_chunks& operator=(const matrix_by_chunks& other)
    {
      data_ = other.data_;
      m_ = other.m_;
      n_ = other.n_;
      rowStride_ = other.rowStride_;
    }

    template<class Archive>
    void serialize(Archive& ar,  unsigned int file_version) {
      if(n_ == rowStride_) {
        ar & boost::serialization::make_array(data_, m_ * n_);
      } else {
        for(int i = 0; i < m_; i++)
          ar & boost::serialization::make_array(data_ + i * rowStride_, n_);
      }
    }

  };

  /// Helper to build matrix_by_chunks temporaries
  /// \internal the constness of the result allows it to successfully match T& arguments
  ///           in the Archive::operator& despite being a temporary
  template<typename T>
  inline const matrix_by_chunks<T> make_matrix_by_chunks(T *data, int m, int n, int rowStride) {
    return matrix_by_chunks<T>(data, m , n, rowStride);
  }

  /*! \namespace internal_mpi
   *
   * \brief Contains all the non-public implementation of dpr::dparallel_recursion,
   *        particularly the high level interface to MPI internally used.
   *
   */
  namespace internal_mpi {

    /// type of the buffer used for serialization
    typedef std::vector<char
#ifdef ALT_BUFFERING
    , boost::mpi::allocator<char>
#endif
    > buffer_type;

    /// Packs an arbitrary data item into a buffer using Boost.archive
    template<typename T>
    void pack(const T& data, buffer_type& buf)
    {
      assert(buf.empty());

#ifdef ALT_BUFFERING
      boost::mpi::packed_oarchive oa(MPI_COMM_WORLD, buf);
#else
      buf.reserve(65536);
      boost::iostreams::stream<boost::iostreams::back_insert_device<buffer_type>> output_stream(buf);
      boost::archive::binary_oarchive oa(output_stream, boost::archive::no_header);
#endif

      MPI_PROFILELOG(" pack ", oa << data);

#ifndef ALT_BUFFERING
      output_stream.flush();
#endif
    }

    /// Packs an arbitrary number of data items into a buffer using Boost.archive
    template<typename T>
    void pack_n(const T* data, buffer_type& buf, const int nitems)
    {
      assert(buf.empty());

#ifdef ALT_BUFFERING
      boost::mpi::packed_oarchive oa(MPI_COMM_WORLD, buf);
#else
      buf.reserve(65536);
      boost::iostreams::stream<boost::iostreams::back_insert_device<buffer_type>> output_stream(buf);
      boost::archive::binary_oarchive oa(output_stream, boost::archive::no_header);
#endif

      for (int i = 0; i < nitems; ++i) {
    	  MPI_PROFILELOG(" pack ", oa << data[i]);
    	  //output_stream.flush();
      }

#ifndef ALT_BUFFERING
      output_stream.flush();
#endif
    }

    /// Packs an arbitrary unknown-number of data items into a buffer using Boost.archive
    template<typename T>
    void pack_nu(const T* data, buffer_type& buf, const int nuitems)
    {
      assert(buf.empty());

#ifdef ALT_BUFFERING
      boost::mpi::packed_oarchive oa(MPI_COMM_WORLD, buf);
#else
      buf.reserve(65536);
      boost::iostreams::stream<boost::iostreams::back_insert_device<buffer_type>> output_stream(buf);
      boost::archive::binary_oarchive oa(output_stream, boost::archive::no_header);
#endif

      MPI_PROFILELOG(" pack ", oa << nuitems);
      for (int i = 0; i < nuitems; ++i) {
    	  MPI_PROFILELOG(" pack ", oa << data[i]);
    	  //output_stream.flush();
      }

#ifndef ALT_BUFFERING
      output_stream.flush();
#endif
    }

    /// Unpacks an arbitrary data item from a buffer using Boost.archive
    template<typename T>
    void unpack(T& data, const buffer_type& buf, const int len)
    {
      assert(buf.capacity() >= static_cast<size_t>(len));

#ifdef ALT_BUFFERING
      boost::mpi::packed_iarchive ia(MPI_COMM_WORLD, buf);
#else
      boost::iostreams::basic_array_source<char> source(buf.data(), static_cast<std::size_t>(len));
      boost::iostreams::stream<boost::iostreams::basic_array_source <char>> s(source);
      boost::archive::binary_iarchive ia(s, boost::archive::no_header);
#endif

      MPI_PROFILELOG(" unpack ", ia >> data);
    }

    /// Unpacks an arbitrary number of data items from a buffer using Boost.archive
	template<typename T>
	void unpack_n(T* data, const buffer_type& buf, const int len, const int nitems)
	{
	  assert(buf.capacity() >= static_cast<size_t>(len));

#ifdef ALT_BUFFERING
	  boost::mpi::packed_iarchive ia(MPI_COMM_WORLD, buf);
#else
	  boost::iostreams::basic_array_source<char> source(buf.data(), static_cast<std::size_t>(len));
	  boost::iostreams::stream<boost::iostreams::basic_array_source <char>> s(source);
	  boost::archive::binary_iarchive ia(s, boost::archive::no_header);
#endif
	  for (int i = 0; i < nitems; ++i) {
	  	  MPI_PROFILELOG(" unpack ", ia >> data[i]);
	  }
	}

    /// Unpacks an arbitrary unknown-number of data items from a buffer using Boost.archive
	template<typename T>
	void unpack_nu(T* data, const buffer_type& buf, const int len, int* nuitems)
	{
	  assert(buf.capacity() >= static_cast<size_t>(len));

#ifdef ALT_BUFFERING
	  boost::mpi::packed_iarchive ia(MPI_COMM_WORLD, buf);
#else
	  boost::iostreams::basic_array_source<char> source(buf.data(), static_cast<std::size_t>(len));
	  boost::iostreams::stream<boost::iostreams::basic_array_source <char>> s(source);
	  boost::archive::binary_iarchive ia(s, boost::archive::no_header);
#endif
	  MPI_PROFILELOG(" unpack ", ia >> *nuitems);
	  for (int i = 0; i < *nuitems; ++i) {
	  	  MPI_PROFILELOG(" unpack ", ia >> data[i]);
	  }
	}

    template<typename T>
    struct UnpackTask {
      T& data_;
      buffer_type buf_;
      const int len_;

      UnpackTask(T& data, buffer_type&& buf, int len) :
      data_(data), buf_(std::move(buf)), len_(len)
      {
        //printf("C %lu >= %d\n", buf_.capacity(), len_ );
      }

      UnpackTask(const UnpackTask& other) :
      data_(other.data_), buf_(std::move(const_cast<buffer_type&>(other.buf_))), len_(other.len_)
      {
        //printf("MV %p %lu >= %d (%p %lu)\n", buf_.data(), buf_.capacity(), len_, other.buf_.data(), other.buf_.capacity());
      }

      void operator() () const {
        unpack(data_, buf_, len_);
      }
    };


    /// Wait for matching messages using MPI_Probe
    /// \internal The only purpose of the template is to avoid redefinitions of the
    ///           function due to being included in several object files.
    template<typename T>
    void probe(T& source, T& tag, T *length = nullptr)
    { MPI_Status st;

      const int err = MPI_Probe(source, tag, MPI_COMM_WORLD, &st);

      assert(err == MPI_SUCCESS);
      _unused(err);
      assert(st.MPI_ERROR == MPI_SUCCESS);

      if (length != nullptr) {
        MPI_Get_count(&st, MPI_BYTE, length);
      }

      source = st.MPI_SOURCE;
      tag = st.MPI_TAG;
    }

  }

  /// Wait for potential matching messages using MPI_Iprobe
  /// \internal The only purpose of the template is to avoid redefinitions of the
  ///           function due to being included in several object files.
  template<typename T>
  int iprobe(T& source, T& tag, T *length = nullptr, MPI_Status *status = nullptr)
  { MPI_Status st;
    int flag;

    const int err = MPI_Iprobe(source, tag, MPI_COMM_WORLD, &flag, &st);

    assert(err == MPI_SUCCESS);
    _unused(err);

    if (flag) {
      assert(st.MPI_ERROR == MPI_SUCCESS);
      if (length != nullptr) {
        MPI_Get_count(&st, MPI_BYTE, length);
      }
      if (status != nullptr) {
        *status = st;
      }
      source = st.MPI_SOURCE;
      tag = st.MPI_TAG;
    }

    return flag;
  }

}

#endif
