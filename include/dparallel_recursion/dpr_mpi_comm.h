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
/// \file     dpr_mpi_comm.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_MPI_COMM_H_
#define DPR_MPI_COMM_H_

#include <tbb/spin_mutex.h>
#include <tbb/task_group.h>

#include "dparallel_recursion/dpr_mpi_comm_comm.h"

namespace dpr {

  /*! \namespace internal_mpi
   *
   * \brief Contains all the non-public implementation of dpr::dparallel_recursion,
   *        particularly the high level interface to MPI internally used.
   *
   */
  namespace internal_mpi {
    
    typedef tbb::spin_mutex mutex_t;
    
    /// Generic send that serializes the data to transfer
    template<typename T>
    std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value && !TransmitByChunks<T>::value>
    send(int dest_rank, int tag, const T& data, mutex_t* const mutex = nullptr)
    { buffer_type buffer;
      
      pack(data, buffer);
      
      MPI_PROFILELOG(" send ",
                     
                 const int len = static_cast<int> (buffer.size());
                 assert(len >= 0);
                     
                 mutex_t::scoped_lock l;
                 if (mutex != nullptr) {
                   l.acquire(*mutex);
                 }
                     
                 MPI_Send(buffer.data(), len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
                 );
    }
    

    /// Optimized send for bitwise serializable data (no serialization required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value && !TransmitByChunks<T>::value>
    send(int dest_rank, int tag, const T& data, mutex_t* const mutex = nullptr)
    {
      MPI_PROFILELOG(" send ",
                 
                 const int len = static_cast<int> (sizeof(T));
                 assert(len >= 0);
                
                 mutex_t::scoped_lock l;
                 if (mutex != nullptr) {
                   l.acquire(*mutex);
                 }
                 MPI_Send((void*)&data, len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
                 );
    }

    /// Optimized multiple send for bitwise serializable data (no serialization required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value && !TransmitByChunks<T>::value>
    send(int dest_rank, int tag, const T* data, const int nitems, mutex_t* const mutex = nullptr)
    {
      MPI_PROFILELOG(" send ",
                     
                     const int len = static_cast<int> (sizeof(T) * nitems);
                     assert(len >= 0);
                     
                     mutex_t::scoped_lock l;
                     if (mutex != nullptr) {
                       l.acquire(*mutex);
                     }
                     MPI_Send((void *)data, len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
                     );
    }
    
    // Predefiniton of send by chunks
    template<typename T>
    std::enable_if_t<TransmitByChunks<T>::value>
    send(int dest_rank, int tag, const T& data, mutex_t* const mutex = nullptr);
    
    /// Generic recv that gets variable-length messages and deserializes the data received
    template<typename T>
    std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value && !TransmitByChunks<T>::value>
    recv(int from_rank, int tag, T& data, mutex_t* const mutex = nullptr, tbb::task_group* const pending_tasks = nullptr)
    { MPI_Status st;
      int len;
      buffer_type buffer;
      
      MPI_PROFILELOG(" recv ",
                 
                 mutex_t::scoped_lock l;
                 if (mutex != nullptr) {
                   l.acquire(*mutex);
                 }
                 
                 MPI_Probe(from_rank, tag, MPI_COMM_WORLD, &st);
                 MPI_Get_count(&st, MPI_BYTE, &len);
                 buffer.reserve(len);
                 MPI_Recv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
                 );
      
      if (pending_tasks == nullptr) {
        unpack(data, buffer, len);
      } else {
        pending_tasks->run(UnpackTask<T>(data, std::move(buffer), len));
      }
      
    }
    
    /// Optimized recv for bitwise serializable data (no serialization or variable-length messages required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value && !TransmitByChunks<T>::value>
    recv(int from_rank, int tag, T& data, mutex_t* const mutex = nullptr, tbb::task_group* const pending_tasks = nullptr)
    { MPI_Status st;
      
      MPI_PROFILELOG(" recv ",
                 
                 mutex_t::scoped_lock l;
                 if (mutex != nullptr) {
                   l.acquire(*mutex);
                 }
                 MPI_Recv((void*)&data, sizeof(T), MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
                 );
    }


    /// Optimized multiple recv for bitwise serializable data (no serialization or variable-length messages required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value && !TransmitByChunks<T>::value>
    recv(int from_rank, int tag, T* data, const int nitems, mutex_t* const mutex = nullptr, tbb::task_group* const pending_tasks = nullptr)
    { MPI_Status st;
      
      MPI_PROFILELOG(" recv ",
                     
                     mutex_t::scoped_lock l;
                     if (mutex != nullptr) {
                       l.acquire(*mutex);
                     }
                     MPI_Recv((void*)data, sizeof(T) * nitems, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
                     );
    }

    
    // Predefiniton of receive by chunks
    template<typename T>
    std::enable_if_t<TransmitByChunks<T>::value>
    recv(int from_rank, int tag, T& data, mutex_t* const mutex = nullptr, tbb::task_group* const pending_tasks = nullptr);

    /// Generic bcast that serializes/deserializes the data to transfer
    template<typename T>
    std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value && !TransmitByChunks<T>::value>
    bcast(T& data, int myrank, int root = 0)
    { buffer_type buffer;
      int count;

      if (myrank == root) {
        pack(data, buffer);

        count = static_cast<int> (buffer.size());

        MPI_PROFILELOG(" bcast ",
                   MPI_Bcast((void*)&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
                   MPI_Bcast(buffer.data(), count, MPI_BYTE, root, MPI_COMM_WORLD);
                   );
      } else {
        MPI_PROFILELOG(" bcast ",
                   MPI_Bcast((void*)&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
                   buffer.reserve(count);
                   MPI_Bcast(buffer.data(), count, MPI_BYTE, root, MPI_COMM_WORLD);
                   );
        unpack(data, buffer, count);
      }
    }

    /// Optimized bcast for bitwise serializable data
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value && !TransmitByChunks<T>::value>
    bcast(T& data, int myrank, int root = 0)
    {
      MPI_PROFILELOG(" bcast ",
                 MPI_Bcast((void*)&data, sizeof(T), MPI_BYTE, root, MPI_COMM_WORLD));
    }
    
    // Predefinition of broadcast by chunks
    template<typename T>
    std::enable_if_t<TransmitByChunks<T>::value>
    bcast(T& data, int myrank, int root = 0);
    
    /// Saving (output) archive used for types to be sent by chunks
    template<bool isBcast>
    class SenderArchive {
      
      const int rank_, tag_, myrank_;
      mutex_t* const mutex_;
      
    public:
      
      SenderArchive(int rank, int tag, mutex_t* mutex = nullptr, int myrank = 0)
      : rank_(rank), tag_(tag), myrank_(myrank), mutex_(mutex) {}
      
      typedef boost::mpl::bool_<true> is_saving;
      typedef boost::mpl::bool_<false> is_loading;
      
      template<class T> void register_type(){}
      
      template<class T> SenderArchive & operator&(const T & t){
        return *this << t;
      }
      
      void save_binary(void *address, std::size_t count){ assert(false); };
      
      template<typename T>
      SenderArchive& operator<<(const T& data)
      {
        //printf("%s %lu bytes bws=%d\n", isBcast ? "BCAST" : "SEND", sizeof(T), boost::serialization::is_bitwise_serializable<T>::value);
        
        if (isBcast) {
          bcast(const_cast<T&>(data), myrank_, rank_);
        } else {
          send(rank_, tag_, data, mutex_);
        }
        
        return *this;
      }
      
      template<typename T>
      std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value, SenderArchive&> operator<<(boost::serialization::array_wrapper<T> data)
      {
        void * const p = (void *)data.address();
        const int sz = sizeof(T) * data.count();
        assert(sz >= 0);
        
        //printf("%s %lu*%lu bytes bws=%d\n", isBcast ? "BCAST" : "SEND", data.count(), sizeof(T), boost::serialization::is_bitwise_serializable<T>::value);
        
        if (isBcast) {
          MPI_PROFILELOG(" bcast ", MPI_Bcast(p, sz, MPI_BYTE, rank_, MPI_COMM_WORLD));
        } else {
          mutex_t::scoped_lock l;
          if (mutex_ != nullptr) {
            l.acquire(*mutex_);
          }
          MPI_PROFILELOG(" send ", MPI_Send(p, sz, MPI_BYTE, rank_, tag_, MPI_COMM_WORLD));
        }
        
        return *this;
      }
      
      template<typename T>
      std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value, SenderArchive&> operator<<(matrix_by_chunks<T> data)
      { MPI_Datatype mytype;
        
        mutex_t::scoped_lock l;
        if (mutex_ != nullptr) {
          l.acquire(*mutex_);
        }
        
        const int len = data.n_ * sizeof(T);
        const int stride = data.rowStride_ * sizeof(T);
        assert(len >= 0);
        assert(stride >= 0);
        
        MPI_Type_vector( data.m_, len, stride, MPI_BYTE, &mytype );
        MPI_Type_commit( &mytype );
        
        if (isBcast) {
          MPI_PROFILELOG(" bcast ", MPI_Bcast(data.data_, 1, mytype, rank_, MPI_COMM_WORLD));
        } else {
          MPI_PROFILELOG(" send ", MPI_Send(data.data_, 1, mytype, rank_, tag_, MPI_COMM_WORLD));
        }
        
        MPI_Type_free( &mytype );
        
        return *this;
      }
      
      template<typename T>
      std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value, SenderArchive&> operator<<(matrix_by_chunks<T> data)
      {
        for (int i  = 0; i < data.m_; i++) {
          (*this) << boost::serialization::array_wrapper<T>(data.data_ + i * data.rowStride_, data.n_);
        }
        return *this;
      }
      
    };
    
    /// Loading (input) archive used for types to be received by chunks
    template<bool isBcast>
    class ReceiverArchive {
      
      const int rank_, tag_, myrank_;
      mutex_t* const mutex_;
      
    public:
      
      ReceiverArchive(int rank, int tag, mutex_t* mutex = nullptr, int myrank = 0)
      : rank_(rank), tag_(tag), myrank_(myrank), mutex_(mutex) {}
      
      typedef boost::mpl::bool_<true> is_loading;
      typedef boost::mpl::bool_<false> is_saving;
      
      template<class T> void register_type(){}
      
      template<class T> ReceiverArchive & operator&(T & t){
        return *this >> t;
      }
      
      void save_binary(void *address, std::size_t count){ assert(false); };
      
      template<typename T>
      ReceiverArchive& operator>>(T& data)
      {
        //printf("%s %lu bytes bws=%d\n", isBcast ? "BCAST" : "RECV", sizeof(T), boost::serialization::is_bitwise_serializable<T>::value);
        if (isBcast) {
          bcast(data, myrank_, rank_);
        } else {
          recv(rank_, tag_, data, mutex_);
        }
        
        return *this;
      }
      
      template<typename T>
      std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value, ReceiverArchive&> operator>>(boost::serialization::array_wrapper<T> data)
      {
        void * p = (void *)data.address();
        int sz = sizeof(T) * data.count();
        
        //printf("%s %lu*%lu bytes bws=%d\n", isBcast ? "BCAST" : "RECV", data.count(), sizeof(T), boost::serialization::is_bitwise_serializable<T>::value);
        
        if (isBcast) {
          MPI_PROFILELOG(" bcast ", MPI_Bcast(p, sz, MPI_BYTE, rank_, MPI_COMM_WORLD));
        } else {
          MPI_Status st;
          mutex_t::scoped_lock l;
          if (mutex_ != nullptr) {
            l.acquire(*mutex_);
          }
          MPI_PROFILELOG(" recv ", MPI_Recv(p, sz, MPI_BYTE, rank_, tag_, MPI_COMM_WORLD, &st));
        }
        
        return *this;
      }
      
      template<typename T>
      std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value, ReceiverArchive&> operator>>(matrix_by_chunks<T> data)
      { MPI_Datatype mytype;
        
        mutex_t::scoped_lock l;
        if (mutex_ != nullptr) {
          l.acquire(*mutex_);
        }
        
        MPI_Type_vector( data.m_, data.n_ * sizeof(T), data.rowStride_ * sizeof(T), MPI_BYTE, &mytype );
        MPI_Type_commit( &mytype );
        if (isBcast) {
          MPI_PROFILELOG(" bcast ", MPI_Bcast(data.data_, 1, mytype, rank_, MPI_COMM_WORLD));
        } else {
          MPI_Status st;
          MPI_PROFILELOG(" recv ", MPI_Recv(data.data_, 1, mytype, rank_, tag_, MPI_COMM_WORLD, &st));
        }
        
        MPI_Type_free( &mytype );
        
        return *this;
      }
      
      template<typename T>
      std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value, ReceiverArchive&> operator>>(matrix_by_chunks<T> data)
      {
        for (int i  = 0; i < data.m_; i++) {
          (*this) >> boost::serialization::array_wrapper<T>(data.data_ + i * data.rowStride_, data.n_);
        }
        return *this;
      }
      
    };
    
    /// Send by chunks
    template<typename T>
    std::enable_if_t<TransmitByChunks<T>::value>
    send(int dest_rank, int tag, const T& data, mutex_t* const mutex)
    {
      SenderArchive<false> send_srb(dest_rank, tag, mutex);
      ((T&)data).serialize(send_srb, 0);
    }
    
    template<typename T>
    std::enable_if_t<TransmitByChunks<T>::value>
    send(int dest_rank, int tag, const T*& data, mutex_t* const mutex = nullptr)
    {
      SenderArchive<false> send_srb(dest_rank, tag, mutex);
      //printf("send ptr to %d\n", dest_rank);
      ((T&)(*data)).serialize(send_srb, 0);
    }
    
    template<typename T>
    std::enable_if_t<TransmitByChunks<T>::value>
    send(int dest_rank, int tag, T*& data, mutex_t* const mutex = nullptr)
    {
      send(dest_rank, tag, (const T *&)data, mutex);
    }
    
    /// Receive by chunks
    template<typename T>
    std::enable_if_t<TransmitByChunks<T>::value>
    recv(int from_rank, int tag, T& data, mutex_t* const mutex, tbb::task_group* const pending_tasks)
    {
      ReceiverArchive<false> recv_srb(from_rank, tag, mutex);
      data.serialize(recv_srb, 0);
    }
    
    template<typename T>
    std::enable_if_t<TransmitByChunks<T>::value>
    recv(int from_rank, int tag, T*& data, mutex_t* const mutex = nullptr, tbb::task_group* const pending_tasks = nullptr)
    {
      ReceiverArchive<false> recv_srb(from_rank, tag, mutex);
      //printf("recv ptr from %d\n", from_rank);
      data = new T();
      data->serialize(recv_srb, 0);
    }

    template<typename Container, typename T = typename Container::value_type>
	std::enable_if_t<TransmitByChunks<T>::value>
	recv(int from_rank, int tag, Container& cont_data, const int nitems, mutex_t* const mutex, tbb::task_group* const pending_tasks)
	{
        ReceiverArchive<false> recv_srb(from_rank, tag, mutex);
        for (const T &data : cont_data) {
			data.serialize(recv_srb, 0);
		}
	}

    template<typename T>
	std::enable_if_t<TransmitByChunks<T>::value>
	recv(int from_rank, int tag, T* data, const int nitems, mutex_t* const mutex = nullptr, tbb::task_group* const pending_tasks = nullptr)
	{
        ReceiverArchive<false> recv_srb(from_rank, tag, mutex);
        //printf("recv ptr from %d\n", from_rank);
        for (int i = 0; i < nitems; ++i) {
        	data[i].serialize(recv_srb, 0);
		}
	}

    /// Broadcast by chunks
    template<typename T>
    std::enable_if_t<TransmitByChunks<T>::value>
    bcast(T& data, int myrank, int root)
    { //printf("Bcast by chunks in rank %d with root=%d\n", myrank, root);
      if (myrank == root) {
        SenderArchive<true> send_srb(root, 0, nullptr, myrank);
        data.serialize(send_srb, 0);
      } else {
        ReceiverArchive<true> recv_srb(root, 0, nullptr, myrank);
        data.serialize(recv_srb, 0);
      }
    }
    
    template<typename T>
    std::enable_if_t<TransmitByChunks<T>::value>
    bcast(T*& data, int myrank, int root = 0)
    {
      if (myrank == root) {
        SenderArchive<true> send_srb(root, 0, nullptr, myrank);
        data->serialize(send_srb, 0);
      } else {
        ReceiverArchive<true> recv_srb(root, 0, nullptr, myrank);
        data = new T();
        data->serialize(recv_srb, 0);
      }
    }

    template<typename T>
    struct SendTask {
      const int dest_rank_;
      const int tag_;
      T& data_;
      mutex_t* const mutex_;
      
      SendTask(int dest_rank, int tag, T& data, mutex_t* const mutex = nullptr) :
      dest_rank_(dest_rank), tag_(tag), data_(data), mutex_(mutex)
      { }
      
      SendTask(const SendTask& other) :
      dest_rank_(other.dest_rank_), tag_(other.tag_), data_(other.data_), mutex_(other.mutex_)
      { }
      
      void operator() () const {
        send(dest_rank_, tag_, data_, mutex_);
      }
    };
    
  } //namespace internal_mpi
  
} //namespace dpr

#endif

