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
/// \file     dpr_mpi_gather_scatter.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#include <algorithm>
#include <numeric>

namespace dpr {

  namespace internal_mpi {

    class CommonGatherScatterHelper {
      
    protected:
      
      const int nprocs_;
      const int rank_;
      const int nglobal_elems_;
      int ctr_;
      unsigned long elemSize_;
      char * orixPtr_;
      int * const itemsPerProc_;
      int * const elemsPerItem_;
      int * const elemsPerProc_;
      int * const displsPerProc_;
      MPI_Datatype mytype_;

    public:
      
      CommonGatherScatterHelper(int nprocs, int rank, int nglobal_elems) :
      nprocs_(nprocs), rank_(rank), nglobal_elems_(nglobal_elems), ctr_(0),
      elemSize_(0), orixPtr_(nullptr),
      itemsPerProc_(new int [nprocs]),
      elemsPerItem_(new int [nglobal_elems]),
      elemsPerProc_(new int [nprocs]),
      displsPerProc_(new int [nprocs])
      { }

      ~CommonGatherScatterHelper()
      { int mpi_is_finalized;
        
        delete [] itemsPerProc_;
        delete [] elemsPerItem_;
        delete [] elemsPerProc_;
        delete [] displsPerProc_;
        
        MPI_Finalized( &mpi_is_finalized );
        if (!mpi_is_finalized) {
          MPI_Type_free( &mytype_ );
        }

        /* No, because it becomes owned by the destination object
         if (rank_ && (orixPtr_ != nullptr)) {
         delete [] orixPtr_;
         } */
      }
      
      void set_items_proc(int pr, int nitems) const noexcept
      { itemsPerProc_[pr] = nitems; }
      
      void advance_item() noexcept { ctr_++; }

      int current_item() const noexcept { return ctr_; }

      bool uniform_items_per_proc() const noexcept {
        return std::all_of(itemsPerProc_ + 1,
                           itemsPerProc_ + nprocs_,
                           [&](int i)
                           { return i == itemsPerProc_[0]; });

      }
      
      void build_type() {
        MPI_Type_contiguous(elemSize_, MPI_BYTE, &mytype_);
        MPI_Type_commit( &mytype_ );
      }
      
    };
    
    class ScatterHelper : public CommonGatherScatterHelper {
      
    public:
      
      ScatterHelper(int nprocs, int rank, int nglobal_elems) :
      CommonGatherScatterHelper(nprocs, rank, nglobal_elems)
      { }

      /// Always provide first ptr, then size
      template<typename T2>
      ScatterHelper& operator& (T2 *& ptr) {
        
        // elemSize_ is broadcasted in scatter_info(). The root learns it here
        if (elemSize_ == 0) {
          elemSize_ = sizeof(T2);
        } else {
          assert(elemSize_ == sizeof(T2));
        }
        
        if(!rank_) { // reading
          assert(ctr_ < nglobal_elems_);
          if (!ctr_) {
            orixPtr_ = reinterpret_cast<char *>(ptr);
          } else {
            assert( (reinterpret_cast<char *>(ptr) - orixPtr_) == std::accumulate(elemsPerItem_, elemsPerItem_ + ctr_, (size_t)0) * elemSize_ );
          }
        } else { // writing
          assert(ctr_ < itemsPerProc_[rank_]);
          ptr = reinterpret_cast<T2 *>(orixPtr_ + std::accumulate(elemsPerItem_, elemsPerItem_ + ctr_, (size_t)0) * elemSize_);
        }

        return *this;
      }
      
      template<typename T>
      ScatterHelper& operator& (T& sz) {
        
        if(!rank_) { // reading
          assert(ctr_ < nglobal_elems_);
          elemsPerItem_[ctr_] = sz; //in low level elements
          assert(elemsPerItem_[ctr_] >= 0);
        } else { // writing
          assert(ctr_ < itemsPerProc_[rank_]);
          sz = elemsPerItem_[ctr_];
        }
        
        return *this;
      }
      
      /// Set ownership when writing in rank != 0 : The first one owns, the others do not
      ScatterHelper& operator& (bool& owned) {
        if (rank_) { // writing
          owned = !ctr_;
        }
        
        return *this;
      }

      void scatter_info() {
        
        PROFILEDEFINITION( const auto t0 = profile_clock_t::now() );
        
        if (!rank_) {
          assert(ctr_ == nglobal_elems_);
        }
        
        MPI_Bcast(&elemSize_, 1, MPI_UNSIGNED_LONG, 0, MPI_COMM_WORLD);
        build_type();
        
        ctr_ = 0;
        
        const bool uniform_items = uniform_items_per_proc();

        //MPI_Bcast((void *)&elemSize_, 1, MPI_INT, 0, MPI_COMM_WORLD);
        
        if (uniform_items) {
          MPI_Scatter(elemsPerItem_, itemsPerProc_[0], MPI_INT,
                      rank_ ? elemsPerItem_ : MPI_IN_PLACE, itemsPerProc_[0], MPI_INT,
                      0, MPI_COMM_WORLD);
        } else {
          displsPerProc_[0] = 0;
          std::partial_sum(itemsPerProc_, itemsPerProc_ + (nprocs_ - 1), displsPerProc_ + 1);
          MPI_Scatterv(elemsPerItem_, itemsPerProc_, displsPerProc_, MPI_INT,
                       rank_ ? elemsPerItem_ : MPI_IN_PLACE, itemsPerProc_[rank_], MPI_INT,
                       0, MPI_COMM_WORLD);
        }
        
        if (!rank_) {
          int first_item_proc = 0;
          for (int i = 0; i < nprocs_; i++) {
            elemsPerProc_[i] = 0;
            for (int j = 0; j < itemsPerProc_[i]; j++) {
              elemsPerProc_[i] += elemsPerItem_[first_item_proc + j];
              //printf("Pr %d + %d bytes\n", i, elemsPerItem_[first_elem_proc + j]);
            }
            displsPerProc_[i] = (i == 0) ? 0 : (displsPerProc_[i - 1] + elemsPerProc_[i - 1]);
            first_item_proc += itemsPerProc_[i];
          }
        } else {
          elemsPerProc_[0] = std::accumulate(elemsPerItem_, elemsPerItem_ + itemsPerProc_[rank_], 0);
	  //printf("Pr %d Alloc %d bytes\n", rank_, elemsPerProc_[0]);
          orixPtr_ = new char [(size_t)elemsPerProc_[0] * (size_t)elemSize_]; // owned?
        }
        
        PROFILEACTION(std::cerr << "info " << rank_ << ' ' << profile_duration_t(profile_clock_t::now() - t0).count() << std::endl);
        
      }
      
      void scatter_data() const {
        PROFILEDEFINITION( const auto t0 = profile_clock_t::now() );
        
        MPI_Scatterv(orixPtr_, elemsPerProc_, displsPerProc_, mytype_,
                     rank_ ? orixPtr_ : MPI_IN_PLACE, elemsPerProc_[0], mytype_,
                     0, MPI_COMM_WORLD);
        
        PROFILEACTION(std::cerr << "scatter " << rank_ << ' ' << profile_duration_t(profile_clock_t::now() - t0).count() << std::endl);
      }
      
      void gather_data() const {
	PROFILEDEFINITION( const auto t0 = profile_clock_t::now() );
        
        MPI_Gatherv(rank_ ? orixPtr_ : MPI_IN_PLACE, elemsPerProc_[0], mytype_,
                    orixPtr_, elemsPerProc_, displsPerProc_, mytype_,
                    0, MPI_COMM_WORLD);
        
        PROFILEACTION(std::cerr << "gather " << rank_ << ' ' << profile_duration_t(profile_clock_t::now() - t0).count() << std::endl);
      }
      
    };
    
    
    class GatherHelper : public CommonGatherScatterHelper {
      
      bool reading_;
      bool ownedOrixPtr_;
      char * newOrixPtr_;
      int resultBufferSize_;
      
    public:
      
      GatherHelper(int nprocs, int rank, int nglobal_elems)
      : CommonGatherScatterHelper(nprocs, rank, nglobal_elems),
      reading_(true), ownedOrixPtr_(false), newOrixPtr_(nullptr), resultBufferSize_(0)
      { }
      
      /// Always provide first ptr, then size
      template<typename T2>
      GatherHelper& operator& (T2 *& ptr)
      {
        if (reading_) {

          // elemSize_ is never broadcasted, but locally learnt from first (de)serialized pointer
          if (elemSize_ == 0) {
            elemSize_ = sizeof(T2);
          } else {
            assert(elemSize_ == sizeof(T2));
          }
          
          assert(ctr_ < itemsPerProc_[rank_]); //except may be when used from get_resultBuffer?
          
          if (!ctr_) {
            orixPtr_ = reinterpret_cast<char *>(ptr);
          } else {
            assert( (reinterpret_cast<char *>(ptr) - orixPtr_) == std::accumulate(elemsPerItem_, elemsPerItem_ + ctr_, (size_t)0) * elemSize_);
          }
        } else { // writing
          //assert(!rank_); Could be replicating
          assert(ctr_ < nglobal_elems_);
          if (!ctr_ && ownedOrixPtr_) {
            delete [] ptr;
          }
          ptr = reinterpret_cast<T2 *>(newOrixPtr_ + std::accumulate(elemsPerItem_, elemsPerItem_ + ctr_, (size_t)0) * elemSize_);
        }

        return *this;
      }
      
      template<typename T>
      GatherHelper& operator& (T& sz)
      {
        if (reading_) {
          assert(ctr_ < itemsPerProc_[rank_]); //except may be when used from get_resultBuffer?
          elemsPerItem_[ctr_] = sz; //in low level elements
          assert(elemsPerItem_[ctr_] >= 0);
        } else { // writing
          // assert(!rank_); Could be replicating
          assert(ctr_ < nglobal_elems_);
          sz = elemsPerItem_[ctr_];
        }
        
        return *this;
      }
      
      GatherHelper& operator& (bool& owned) {
        if (reading_) {
          // The first pointer may be owned or not, but the other ones should be consecutive
          assert(!ctr_ || !owned);
          ownedOrixPtr_ = !ctr_ && owned;
        } else { // writing
          // assert(!rank_); Could be replicating
          // ctr_  > 0 => never owner
          // ctr_ == 0 => owner if not buffer (!resultBufferSize_)
          //              ownedOrixPtr_ plays no role, because if it was not owner,
          //              it became so after the allocation of ownedOrixPtr_ in gather_output()
          owned = !ctr_ && !resultBufferSize_;
        }
        
        return *this;
      }
      
      /// Gathers in a buffer provided by a BufferedDInfo
      template<typename T>
      void get_resultBuffer(T& resultBuffer)
      {
        assert(reading_);
        assert(!ctr_);
        
        resultBuffer.gather_scatter(*this);
        newOrixPtr_ = orixPtr_;
        resultBufferSize_ = elemsPerItem_[0];
        orixPtr_ = nullptr;
        elemSize_ = 0;
        ownedOrixPtr_ = false;
      }
      
      /// Gathers the structure information from all the processes, and then the data
      void gather_output(bool replicate)
      {
        assert(ctr_ == itemsPerProc_[rank_]);
        
        const bool uniform_items = uniform_items_per_proc();
        
        assert(elemSize_ > 0);
        
        build_type();

        if (uniform_items) {
          if (replicate) {
            MPI_Allgather(elemsPerItem_, itemsPerProc_[0], MPI_INT,
                          elemsPerItem_, itemsPerProc_[0], MPI_INT,
                          MPI_COMM_WORLD);
          } else {
            MPI_Gather(elemsPerItem_, itemsPerProc_[0], MPI_INT,
                       elemsPerItem_, itemsPerProc_[0], MPI_INT,
                       0, MPI_COMM_WORLD);
          }
        } else {
          displsPerProc_[0] = 0;
          std::partial_sum(itemsPerProc_, itemsPerProc_ + (nprocs_ - 1), displsPerProc_ + 1);
          if (replicate) {
            MPI_Allgatherv(elemsPerItem_, itemsPerProc_[rank_], MPI_INT,
                           elemsPerItem_, itemsPerProc_, displsPerProc_, MPI_INT,
                           MPI_COMM_WORLD);
          } else {
            MPI_Gatherv(elemsPerItem_, itemsPerProc_[rank_], MPI_INT,
                        elemsPerItem_, itemsPerProc_, displsPerProc_, MPI_INT,
                        0, MPI_COMM_WORLD);
          }
        }
        
        if (!rank_ || replicate) {
          int first_item_proc = 0;
          for (int i = 0; i < nprocs_; i++) {
            elemsPerProc_[i] = 0;
            for (int j = 0; j < itemsPerProc_[i]; j++) {
              elemsPerProc_[i] += elemsPerItem_[first_item_proc + j];
              //printf("Pr %d + %d bytes\n", i, elemsPerItem_[first_elem_proc + j]);
            }
            displsPerProc_[i] = (i == 0) ? 0 : (displsPerProc_[i - 1] + elemsPerProc_[i - 1]);
            first_item_proc += itemsPerProc_[i];
          }
          
          const int required_size = displsPerProc_[nprocs_-1] + elemsPerProc_[nprocs_-1];
          if (newOrixPtr_ == nullptr) {
            //printf("Pr %d Alloc %d bytes\n", rank_, elemsPerProc_[0]);
            newOrixPtr_ = new char [(size_t)required_size * (size_t)elemSize_]; // owned?
          } else {
            assert(resultBufferSize_ >= required_size);
            /* If this does not hold, we do not try to fix the pointer because even if
             it were owned by the object that provided it, and could keep a pointer to it,
             we do not know whether there are other pointers pointing to that location
            if(resultBufferSize_ < required_size) {
              // We try to deallocate newOrixPtr_ and reallocate with proper size
              // muy be bug: since we lost the data type, the delete on char * might break
              delete [] newOrixPtr_;
              //printf("Pr %d Alloc %d bytes\n", rank_, elemsPerProc_[0]);
              newOrixPtr_ = new char [required_size]; // owned?
            } */
          }
          
        } else {
          elemsPerProc_[rank_] = std::accumulate(elemsPerItem_, elemsPerItem_ + itemsPerProc_[rank_], 0);
        }
        
        quick_gather_output(replicate);
        
      }
      
      /// Gathers the data using the structure information stored in the object
      void quick_gather_output(bool replicate) noexcept
      {
        if (replicate) {
          MPI_Allgatherv(orixPtr_, elemsPerProc_[rank_], mytype_,
                         newOrixPtr_, elemsPerProc_, displsPerProc_, mytype_,
                         MPI_COMM_WORLD);
        } else {
          MPI_Gatherv(orixPtr_, elemsPerProc_[rank_], mytype_,
                      newOrixPtr_, elemsPerProc_, displsPerProc_, mytype_,
                      0, MPI_COMM_WORLD);
        }
        
        ctr_ = 0;
        if (!rank_ || replicate) {
          reading_ = false;
        }
        
      }
      
    };
    
  } //namespace internal_mpi
  
} //namespace dpr
