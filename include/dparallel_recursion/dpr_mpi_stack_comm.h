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
/// \file     dpr_mpi_stack_comm.h
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#ifndef DPR_MPI_STACK_COMM_H_
#define DPR_MPI_STACK_COMM_H_

#include "dparallel_recursion/dpr_mpi_comm_comm.h"

#include <vector>

namespace dpr {

  /*! \namespace internal_mpi
   *
   * \brief Contains all the non-public implementation of dpr::dparallel_recursion,
   *        particularly the high level interface to MPI internally used.
   *
   */
  namespace internal_mpi {
    
    /// Generic send that serializes the data to transfer
    template<typename T>
    std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value>
    st_send(int dest_rank, int tag, const T& data, std::mutex* const mutex = nullptr) {
		buffer_type buffer;

		pack(data, buffer);

		MPI_PROFILELOG(" send ",

			const int len = static_cast<int> (buffer.size());
			assert(len >= 0);

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Send(buffer.data(), len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
			} else {
				MPI_Send(buffer.data(), len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
			}

		);
    }
    
    /// Generic send that serializes a nitems of data to transfer
    template<typename T>
    std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value>
    st_send(int dest_rank, int tag, const T* data, const int nitems, std::mutex* const mutex = nullptr, const bool send_nitems = true) {
		buffer_type buffer;

		if (send_nitems) {
			pack_nu(data, buffer, nitems);
		} else {
			pack_n(data, buffer, nitems);
		}

		MPI_PROFILELOG(" send ",

			const int len = static_cast<int> (buffer.size());
			assert(len >= 0);

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Send(buffer.data(), len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
			} else {
				MPI_Send(buffer.data(), len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
			}

		);
    }

    /// Optimized send for bitwise serializable data (no serialization required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value>
    st_send(int dest_rank, int tag, const T& data, std::mutex* const mutex = nullptr) {
		MPI_PROFILELOG(" send ",

			const int len = static_cast<int> (sizeof(T));
			assert(len >= 0);

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Send((void*)&data, len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
			} else {
				MPI_Send((void*)&data, len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
			}

		);
    }

    /// Optimized multiple send for bitwise serializable data (no serialization required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value>
    st_send(int dest_rank, int tag, const T* data, const int nitems, std::mutex* const mutex = nullptr) {
		MPI_PROFILELOG(" send ",

			const int len = static_cast<int> (sizeof(T) * nitems);
			assert(len >= 0);

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Send((void *)data, len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
			} else {
				MPI_Send((void *)data, len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD);
			}

		);
    }

    /// Generic send that serializes the data to transfer
    template<typename T>
    std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value>
    st_isend(int dest_rank, int tag, const T& data, buffer_type& buffer, MPI_Request& request, std::mutex* const mutex = nullptr) {

		buffer.clear();
		pack(data, buffer);

		MPI_PROFILELOG(" isend ",

			const int len = static_cast<int> (buffer.size());
			assert(len >= 0);

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Isend(buffer.data(), len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD, &request);
			} else {
				MPI_Isend(buffer.data(), len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD, &request);
			}

		);

    }

    /// Generic send that serializes a nitems of data to transfer
    template<typename T>
    std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value>
    st_isend(int dest_rank, int tag, const T* data, const int nitems, buffer_type& buffer, MPI_Request& request, std::mutex* const mutex = nullptr, const bool send_nitems = true) {

		buffer.clear();
		if (send_nitems) {
			pack_nu(data, buffer, nitems);
		} else {
			pack_n(data, buffer, nitems);
		}

		MPI_PROFILELOG(" isend ",

			const int len = static_cast<int> (buffer.size());
			assert(len >= 0);

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Isend(buffer.data(), len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD, &request);
			} else {
				MPI_Isend(buffer.data(), len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD, &request);
			}

		);

    }

    /// Optimized send for bitwise serializable data (no serialization required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value>
    st_isend(int dest_rank, int tag, const T& data, buffer_type& buffer, MPI_Request& request, std::mutex* const mutex = nullptr) {
		MPI_PROFILELOG(" isend ",

			const int len = static_cast<int> (sizeof(T));
			assert(len >= 0);

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Isend((void*)&data, len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD, &request);
			} else {
				MPI_Isend((void*)&data, len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD, &request);
			}

		);
    }

    /// Optimized multiple send for bitwise serializable data (no serialization required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value>
    st_isend(int dest_rank, int tag, const T* data, const int nitems, buffer_type& buffer, MPI_Request& request, std::mutex* const mutex = nullptr) {
		MPI_PROFILELOG(" isend ",

			const int len = static_cast<int> (sizeof(T) * nitems);
			assert(len >= 0);

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Isend((void *)data, len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD, &request);
			} else {
				MPI_Isend((void *)data, len, MPI_BYTE, dest_rank, tag, MPI_COMM_WORLD, &request);
			}

		);
    }

    /// Generic recv that gets variable-length messages and deserializes the data received
    template<typename T>
    std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value>
    st_recv(int from_rank, int tag, T& data, std::mutex* const mutex = nullptr) {
		MPI_Status st;
		int len;
		buffer_type buffer;

		MPI_PROFILELOG(" recv ",

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Probe(from_rank, tag, MPI_COMM_WORLD, &st);
				MPI_Get_count(&st, MPI_BYTE, &len);
				buffer.reserve(len);
				MPI_Recv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
			} else {
				MPI_Probe(from_rank, tag, MPI_COMM_WORLD, &st);
				MPI_Get_count(&st, MPI_BYTE, &len);
				buffer.reserve(len);
				MPI_Recv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
			}

		);

		unpack(data, buffer, len);

    }
    
    /// Generic recv that gets multiple variable-length messages and deserializes the data received
	template<typename T>
	std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value>
	st_recv(int from_rank, int tag, T* data, const int nitems, std::mutex* const mutex = nullptr) {
		MPI_Status st;
		int len;
		buffer_type buffer;

		MPI_PROFILELOG(" recv ",
			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Probe(from_rank, tag, MPI_COMM_WORLD, &st);
				MPI_Get_count(&st, MPI_BYTE, &len);
				buffer.reserve(len);
				MPI_Recv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
			} else {
				MPI_Probe(from_rank, tag, MPI_COMM_WORLD, &st);
				MPI_Get_count(&st, MPI_BYTE, &len);
				buffer.reserve(len);
				MPI_Recv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
			}
		);

		unpack_n(data, buffer, len, nitems);
	}

	/// Generic recv that gets multiple-unknown variable-length messages and deserializes the data received
	template<typename T>
	std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value>
	st_recv(int from_rank, int tag, T* data, int* nuitems, std::mutex* const mutex = nullptr) {
		MPI_Status st;
		int len;
		buffer_type buffer;

		MPI_PROFILELOG(" recv ",
			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Probe(from_rank, tag, MPI_COMM_WORLD, &st);
				MPI_Get_count(&st, MPI_BYTE, &len);
				buffer.reserve(len);
				MPI_Recv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
			} else {
				MPI_Probe(from_rank, tag, MPI_COMM_WORLD, &st);
				MPI_Get_count(&st, MPI_BYTE, &len);
				buffer.reserve(len);
				MPI_Recv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
			}
		);

		if (len > 0) {
			unpack_nu(data, buffer, len, nuitems);
		} else {
			*nuitems = 0;
		}
	}

    /// Optimized recv for bitwise serializable data (no serialization or variable-length messages required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value>
    st_recv(int from_rank, int tag, T& data, std::mutex* const mutex = nullptr) {

		MPI_PROFILELOG(" recv ",

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
			MPI_Recv((void*)&data, sizeof(T), MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			} else {
				MPI_Recv((void*)&data, sizeof(T), MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			}

		);
    }


    /// Optimized multiple recv for bitwise serializable data (no serialization or variable-length messages required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value>
    st_recv(int from_rank, int tag, T* data, const int nitems, std::mutex* const mutex = nullptr) {

		MPI_PROFILELOG(" recv ",

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Recv((void*)data, sizeof(T) * nitems, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			} else {
				MPI_Recv((void*)data, sizeof(T) * nitems, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			}
		);
    }

    /// Optimized multiple recv for bitwise unknown-multiple serializable data (no serialization or variable-length messages required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value>
    st_recv(int from_rank, int tag, T* data, int* nuitems, std::mutex* const mutex = nullptr) {
		MPI_Status st;
		int len;

		MPI_PROFILELOG(" recv ",
			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_Probe(from_rank, tag, MPI_COMM_WORLD, &st);
				MPI_Get_count(&st, MPI_BYTE, &len);
				MPI_Recv((void*)data, len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
			} else {
				MPI_Probe(from_rank, tag, MPI_COMM_WORLD, &st);
				MPI_Get_count(&st, MPI_BYTE, &len);
				MPI_Recv((void*)data, len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &st);
			}
		);

		*nuitems = len / sizeof(T);
    }

    /// Generic irecv that gets variable-length messages and deserializes the data received
    template<typename T>
    std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value, std::pair<bool, int>>
	st_irecv(int from_rank, int tag, T& data, buffer_type& buffer, MPI_Request& request, std::mutex* const mutex = nullptr) {
		int len;

		if (request == MPI_REQUEST_NULL) {
			MPI_PROFILELOG(" irecv ",
				if (mutex != nullptr) {
					std::lock_guard<std::mutex> lck(*mutex);
					int flag = iprobe(from_rank, tag, &len);
					if (flag) {
						buffer.reserve(len);
						MPI_Irecv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
					}
				} else {
					int flag = iprobe(from_rank, tag, &len);
					if (flag) {
						buffer.reserve(len);
						MPI_Irecv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
					}
				}
			);
			return std::pair<bool, int>(false, -1);
		} else {
			MPI_Status st;
			int flag;
			MPI_Test(&request, &flag, &st);
			if (flag) {
				MPI_Get_count(&st, MPI_BYTE, &len);
				if (len > 0) {
					unpack(data, buffer, len);
				}
				return std::pair<bool, int>(true, len);
			} else {
				return std::pair<bool, int>(false, -1);
			}

		}
	}

    /// Generic irecv that gets multiple variable-length messages and deserializes the data received
	template<typename T>
	std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value, std::pair<bool, int>>
	st_irecv(int from_rank, int tag, T* data, const int nitems, buffer_type& buffer, MPI_Request& request, std::mutex* const mutex = nullptr) {
		int len;

		if (request == MPI_REQUEST_NULL) {
			MPI_PROFILELOG(" irecv ",
				if (mutex != nullptr) {
					std::lock_guard<std::mutex> lck(*mutex);
					int flag = iprobe(from_rank, tag, &len);
					if (flag) {
						buffer.reserve(len);
						MPI_Irecv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
					}
				} else {
					int flag = iprobe(from_rank, tag, &len);
					if (flag) {
						buffer.reserve(len);
						MPI_Irecv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
					}
				}
			);
			return std::pair<bool, int>(false, -1);
		} else {
			MPI_Status st;
			int flag;
			MPI_Test(&request, &flag, &st);
			if (flag) {
				MPI_Get_count(&st, MPI_BYTE, &len);
				if (len > 0) {
					unpack_n(data, buffer, len, nitems);
				}
				return std::pair<bool, int>(true, len);
			} else {
				return std::pair<bool, int>(false, -1);
			}

		}
	}

	/// Generic irecv that gets multiple-unknown variable-length messages and deserializes the data received
	template<typename T>
	std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value, std::pair<bool, int>>
	st_irecv(int from_rank, int tag, T* data, int* nuitems, buffer_type& buffer, MPI_Request& request, std::mutex* const mutex = nullptr) {
		int len;

		if (request == MPI_REQUEST_NULL) {
			MPI_PROFILELOG(" irecv ",
				if (mutex != nullptr) {
					std::lock_guard<std::mutex> lck(*mutex);
					int flag = iprobe(from_rank, tag, &len);
					if (flag) {
						buffer.reserve(len);
						MPI_Irecv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
					}
				} else {
					int flag = iprobe(from_rank, tag, &len);
					if (flag) {
						buffer.reserve(len);
						MPI_Irecv(buffer.data(), len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
					}
				}
			);
			return std::pair<bool, int>(false, -1);
		} else {
			MPI_Status st;
			int flag;
			MPI_Test(&request, &flag, &st);
			if (flag) {
				MPI_Get_count(&st, MPI_BYTE, &len);
				if (len > 1) {
					unpack_nu(data, buffer, len, nuitems);
				} else {
					*nuitems = 0;
				}
				return std::pair<bool, int>(true, len);
			} else {
				return std::pair<bool, int>(false, -1);
			}

		}
	}

    /// Optimized irecv for bitwise serializable data (no serialization or variable-length messages required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value, std::pair<bool, int>>
	st_irecv(int from_rank, int tag, T& data, buffer_type& nullbuffer, MPI_Request& request, std::mutex* const mutex = nullptr) {

		if (request == MPI_REQUEST_NULL) {
			MPI_PROFILELOG(" irecv ",
				if (mutex != nullptr) {
					std::lock_guard<std::mutex> lck(*mutex);
					MPI_Irecv((void*)&data, sizeof(T), MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
				} else {

				}

			);
			return std::pair<bool, int>(false, -1);
		} else {
			MPI_Status st;
			int flag;
			MPI_Test(&request, &flag, &st);
			if (flag) {
				int len;
				MPI_Get_count(&st, MPI_BYTE, &len);
				return std::pair<bool, int>(true, len);
			} else {
				return std::pair<bool, int>(false, -1);
			}
		}
    }


    /// Optimized multiple irecv for multiple bitwise serializable data (no serialization or variable-length messages required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value, std::pair<bool, int>>
	st_irecv(int from_rank, int tag, T* data, const int nitems, buffer_type& nullbuffer, MPI_Request& request, std::mutex* const mutex = nullptr) {

		if (request == MPI_REQUEST_NULL) {
			MPI_PROFILELOG(" irecv ",
				if (mutex != nullptr) {
					std::lock_guard<std::mutex> lck(*mutex);
					MPI_Irecv((void*)data, sizeof(T) * nitems, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
				} else {
					MPI_Irecv((void*)data, sizeof(T) * nitems, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
				}
			);
		} else {
			MPI_Status st;
			int flag;
			MPI_Test(&request, &flag, &st);
			if (flag) {
				int len;
				MPI_Get_count(&st, MPI_BYTE, &len);
				return std::pair<bool, int>(true, len);
			} else {
				return std::pair<bool, int>(false, -1);
			}
		}
    }


    /// Optimized multiple irecv for bitwise unknown-multiple serializable data (no serialization or variable-length messages required)
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value, std::pair<bool, int>>
	st_irecv(int from_rank, int tag, T* data, int* nuitems, buffer_type& nullbuffer, MPI_Request& request, std::mutex* const mutex = nullptr) {

		if (request == MPI_REQUEST_NULL) {
			MPI_PROFILELOG(" irecv ",
				if (mutex != nullptr) {
					std::lock_guard<std::mutex> lck(*mutex);
					int len;
					int flag = iprobe(from_rank, tag, &len);
					if (flag) {
						MPI_Irecv((void*)data, len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
					}
				} else {
					int len;
					int flag = iprobe(from_rank, tag, &len);
					if (flag) {
						MPI_Irecv((void*)data, len, MPI_BYTE, from_rank, tag, MPI_COMM_WORLD, &request);
					}
				}
			);
			return std::pair<bool, int>(false, -1);
		} else {
			MPI_Status st;
			int flag;
			MPI_Test(&request, &flag, &st);
			if (flag) {
				int len;
				MPI_Get_count(&st, MPI_BYTE, &len);
				*nuitems = len / sizeof(T);
				return std::pair<bool, int>(true, len);
			} else {
				return std::pair<bool, int>(false, -1);
			}
		}
    }

    /// Generic bcast that serializes/deserializes the data to transfer
    template<typename T>
    std::enable_if_t<!boost::serialization::is_bitwise_serializable<T>::value>
    st_bcast(T& data, int myrank, int root = 0, std::mutex* const mutex = nullptr) {
		buffer_type buffer;
		int count;

		if (myrank == root) {
			pack(data, buffer);

			count = static_cast<int> (buffer.size());

			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_PROFILELOG(" bcast ",
					MPI_Bcast((void*)&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
					MPI_Bcast(buffer.data(), count, MPI_BYTE, root, MPI_COMM_WORLD);
				);
			} else {
				MPI_PROFILELOG(" bcast ",
					MPI_Bcast((void*)&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
					MPI_Bcast(buffer.data(), count, MPI_BYTE, root, MPI_COMM_WORLD);
				);
			}
		} else {
			if (mutex != nullptr) {
				std::lock_guard<std::mutex> lck(*mutex);
				MPI_PROFILELOG(" bcast ",
					MPI_Bcast((void*)&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
					buffer.reserve(count);
					MPI_Bcast(buffer.data(), count, MPI_BYTE, root, MPI_COMM_WORLD);
				);
			} else {
				MPI_PROFILELOG(" bcast ",
					MPI_Bcast((void*)&count, 1, MPI_INT, 0, MPI_COMM_WORLD);
					buffer.reserve(count);
					MPI_Bcast(buffer.data(), count, MPI_BYTE, root, MPI_COMM_WORLD);
				);
			}
			unpack(data, buffer, count);
		}
    }

    /// Optimized bcast for bitwise serializable data
    template<typename T>
    std::enable_if_t<boost::serialization::is_bitwise_serializable<T>::value>
    st_bcast(T& data, int myrank, int root = 0, std::mutex* const mutex = nullptr) {
		if (mutex != nullptr) {
			std::lock_guard<std::mutex> lck(*mutex);
			MPI_PROFILELOG(" bcast ",
				MPI_Bcast((void*)&data, sizeof(T), MPI_BYTE, root, MPI_COMM_WORLD)
			);
		} else {
			MPI_PROFILELOG(" bcast ",
				MPI_Bcast((void*)&data, sizeof(T), MPI_BYTE, root, MPI_COMM_WORLD)
			);
		}
    }
    
  } //namespace internal_mpi
  
} //namespace dpr

#endif

