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
/// \file     dparallel_recursion.h
/// \author   Carlos H. Gonzalez  <cgonzalezv@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
///

#ifndef DPR_PARALLEL_RECURSION_MPI_H_
#define DPR_PARALLEL_RECURSION_MPI_H_


#include <cstdlib>
#include "dparallel_recursion/parallel_recursion.h"
#include "dparallel_recursion/BufferedDInfo.h"
#include <boost/function_types/property_tags.hpp>

#ifdef DPR_RANGE_H_
BOOST_IS_BITWISE_SERIALIZABLE(dpr::Range);
#endif

namespace dpr {

  namespace internal_mpi {
    
// GENERAL
template<typename Return, typename T, typename Info, typename Body>
class AbstractRun {
protected:
	T& global_root;
	T global_root_value;
	Info& info;
	Body& body;
	mutex_t send_mutex;
	int rank;
	int nprocs;
	const Behavior& behavior;
        Return * const dest;
        tbb::task_group pending_tasks;
  
	AbstractRun(T& r, Info& i, Body& b, const Behavior& bhv, Return * const dest)
        : global_root(r), info(i), body(b), behavior(bhv), dest(dest)
        {
          MPI_Comm_rank (MPI_COMM_WORLD, &rank);
          MPI_Comm_size (MPI_COMM_WORLD, &nprocs);
	}

	AbstractRun(T&& r, Info& i, Body& b, const Behavior& bhv, Return * const dest)
        : global_root_value(r), global_root(global_root_value),
          info(i), body(b), behavior(bhv), dest(dest)
        {
            MPI_Comm_rank (MPI_COMM_WORLD, &rank);
            MPI_Comm_size (MPI_COMM_WORLD, &nprocs);
	}

        /* Sending items in parallel to a worker can lead to a deadlock because the worker part makes a
          blocking recv once probe returns a given tag
        struct ParallelSendItemHelper {
          AbstractRun& p_;
          const int dest_rank_, base_;
                
          ParallelSendItemHelper(AbstractRun& p, int dest_rank, int base) :
          p_(p), dest_rank_(dest_rank),base_(base)
          { }
                
          void operator() (const int nitem) const {
            send(dest_rank_, nitem, p_.info.lastTreeLevelElement(base_ + nitem), &p_.send_mutex);
          }
        };
        */
  
	struct ParallelSendHostHelper {
          AbstractRun& p_;
          
          ParallelSendHostHelper(AbstractRun& p) :
          p_(p)
          { }

          void operator() (const int dest_rank) const {
            assert(p_.rank != dest_rank);
            
            const int nelems = p_.info.nLocalElems(dest_rank);
            const int base = p_.info.firstGlobalElem(dest_rank);
            
            // See comment on ParallelSendItemHelper
            //if( (nelems == 1) || boost::serialization::is_bitwise_serializable<T>::value ) {
              for (int i = 0; i < nelems; i++) {
                send(dest_rank, i, p_.info.lastTreeLevelElement(base + i), &p_.send_mutex);
              }
            //} else {
            //  tbb::parallel_for(0, nelems, ParallelSendItemHelper(p_, dest_rank, base));
            //}
          }
	};

public:

  void distribute()
  {
    if (behavior & ReplicateInput) {
      assert(behavior & ReplicatedInput);
      
      if (!info.hasRoot() && (nprocs > 1)) {
        bcast(global_root, rank, 0);
      }
    }
    
    //PROFILEACTION(MPI_Barrier(MPI_COMM_WORLD));
    
    if (!info.hasRoot()) {
      if (behavior & DistributedInput) {
        info.distributed_input_create_tree(global_root, nprocs, rank, info, behavior, body);
      } else if (behavior & ReplicatedInput) {
        info.create_tree(global_root, nprocs, rank, info, behavior, body);
      } else if (behavior & PrioritizeDM) {
        assert(!(behavior & Scatter));
        info.distributed_create_tree(global_root, nprocs, rank, info, behavior, body);
      } else {
        if (rank == 0) {
          PROFILELOG("create_tree in 0", info.create_tree(global_root, nprocs, rank, info, behavior, body));
          
          if (behavior & Scatter) {
            info.template scatter<T>(rank);
          } else {
            tbb::parallel_for(1, nprocs, ParallelSendHostHelper(*this));
          }
        } else {
          info.create_tree(global_root, nprocs, rank, behavior);
          
          if (behavior & Scatter) {
            info.template scatter<T>(rank);
          } else {
            const int nelems = info.nLocalElems();
            if ( (nelems == 1) || boost::serialization::is_bitwise_serializable<T>::value ) {
              for (int i = 0; i < nelems; i++) {
                recv(0, i, info.root(i));
              }
            } else {
              int source = 0;
              for (int i = 0; i < nelems; i++) {
                int tag = MPI_ANY_TAG;
                probe(source, tag);
                recv(0, tag, info.root(tag), nullptr, &pending_tasks);
              }
              pending_tasks.wait();
            }
          }
        }
      }
    }
  
    /* For debugging Balance|UseCost
    for (int i = 0; i < nprocs; i++) {
      if (rank == i) {
        float c = 0.f;
        for (int i = 0; i < info.nLocalElems(); i++) {
          //if(!i || i == (info.nLocalElems() - 1)) std::cerr << "   " << i << " : " << (unsigned long long)info.cost(info.rootRef(i).get()) << std::endl;
         c += info.cost(info.rootRef(i).get());
        }
        std::cerr << "Rank " << rank << " has " << info.nLocalElems() << " items";
        std::cerr << " COST=" << (unsigned long long)c << std::endl;
      }
      MPI_Barrier(MPI_COMM_WORLD);
    }
   

    int mytmp = info.nGlobalElems();
    MPI_Bcast((void*)&mytmp, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if(mytmp != info.nGlobalElems()) {
      fprintf(stderr, "R%3d: lblElems %d != %d\n", rank, mytmp, info.nGlobalElems());
    }
    if(!rank) {
      fprintf(stderr, "Distributed\n");
    }
    */
    
  }
  
  template<typename DummyType>
  Return complete() {
    if (!(behavior & (DistributedOutput|PrioritizeDM))) {
      this->send_results();
    }
    return this->reduce();
  }
  
  virtual void send_results() = 0;
  virtual Return reduce() = 0;

  virtual ~AbstractRun()
  { }

};

/// WITH RETURN VALUE (function)
template<typename Return, typename T, typename Info, typename Body, typename Partitioner>
class ImplRun : public AbstractRun<Return, T, Info, Body> {

        BOOST_TTI_HAS_MEMBER_FUNCTION(distributed_post);

protected:
	typedef AbstractRun<Return, T, Info, Body> super;

	std::vector<Return> local_results;

        struct ParallelComputeHelper {
          ImplRun& p_;
          const int step_;
          
          ParallelComputeHelper(ImplRun& p, const int step) : p_(p), step_(step) { }

          void operator() (int i) const {
            const int limit = std::min(i + step_, p_.info.nLocalElems());
            for(int j = i; j < limit; j++) {
              p_.local_results[j] = dpr::internal::start_recursion<Return, T, Info, Body, Partitioner>::run(p_.info.rootRef(j), p_.info, p_.body);
            }
          }
       };

       struct ParallelReduceHelper {
         std::vector<Return> * const orig;
         std::vector<Return> * const dest;
         Body &super_body;
         typename Info::Datatree_level_t& level_data;
         int * const p_local_result_positions;
        
         ParallelReduceHelper(std::vector<Return> * iorig, std::vector<Return> *idest, Body &isuper_body, typename Info::Datatree_level_t& ilevel_data, int * ip_local_result_positions)
         : orig(iorig), dest(idest), super_body(isuper_body), level_data(ilevel_data), p_local_result_positions(ip_local_result_positions)
         {}
        
         void operator() (int i) const {
           general_reference_wrapper<T>& tmp = level_data[i];
           if (tmp.empty()) {
             //This happens when T was a base case at this level, leaving a hole in the tree and moving one level down.
             // (*orig)[p_local_result_positions[i]] already contains the result for this item
             // We leave T at the bottom of the tree because there are no reductions at this level
             (*dest)[i] = std::move((*orig)[p_local_result_positions[i]]);
           } else {
             (*dest)[i] = super_body.post(tmp.get(), &((*orig)[p_local_result_positions[i]]));
           }
         }

      };

      template<typename T2>
      std::enable_if_t<boost::serialization::is_bitwise_serializable<T2>::value && !TransmitByChunks<T2>::value>
      optimized_send(const int tag, bool& collective_gather_output)
      {
        if(!collective_gather_output) {
          //fprintf(stderr, "R%3d sends %4d ints", super::rank, super::info.nLocalElems()); std::cerr << std::endl;
          send(0, tag, &local_results[0], super::info.nLocalElems());
          collective_gather_output = true; //to disable further sends of results
        }
      }

      template<typename T2>
      std::enable_if_t<!boost::serialization::is_bitwise_serializable<T2>::value || TransmitByChunks<T2>::value>
      optimized_send(const int tag, bool& collective_gather_output)
      { }
  
      template<typename T2>
      std::enable_if_t<boost::serialization::is_bitwise_serializable<T2>::value && !TransmitByChunks<T2>::value, int>
      optimized_recv(const int source, const int tag, const int index)
      {
        const int nelems = super::info.nLocalElems(source);
        //fprintf(stderr, "Recv %d ints from %d", nelems, source); std::cerr << std::endl;
        recv(source, tag, &local_results[index], nelems, nullptr, &(super::pending_tasks));
        return nelems - 1;
      }
  
      template<typename T2>
      std::enable_if_t<!boost::serialization::is_bitwise_serializable<T2>::value || TransmitByChunks<T2>::value, int>
      optimized_recv(const int source, const int tag, const int index)
      {
        recv(source, tag, local_results[index], nullptr, &(super::pending_tasks));
        return 0;
      }
  
public:
	ImplRun(T&& r, Info& i, Body& b, const Behavior& behavior, Return * const dest) : super(std::forward<T>(r), i, b, behavior, dest) {}

	ImplRun(T& r, Info& i, Body& b, const Behavior& behavior, Return * const dest) : super(r, i, b, behavior, dest) {}

        void run() {

          PROFILEDEFINITION( const auto t0 = profile_clock_t::now() );

          const int nlocal_elems = super::info.nLocalElems();
          local_results.resize(nlocal_elems);
          
          if ( (nlocal_elems < 2) ) { //|| !(super::behavior & GreedyParallel)) {
            for (int i = 0; i < nlocal_elems; i++) {
              local_results[i] = dpr::internal::start_recursion<Return, T, Info, Body, Partitioner>::run(super::info.rootRef(i), super::info, super::body);
            }
          }
          else {
            const int pre_level = super::info.parLevel_;
            int step;
            if (super::info.ntasks_ > nlocal_elems) {
              step = 1;
              super::info.parLevel_ = super::info.LevelsForNTasks((super::info.ntasks_  + (nlocal_elems - 1))/ nlocal_elems);
            } else {
              step = nlocal_elems / super::info.ntasks_;
              super::info.parLevel_ = 0;
            }
            tbb::parallel_for(0, nlocal_elems, step, ParallelComputeHelper(*this, step));
            super::info.parLevel_ = pre_level;
          }

          PROFILEDEFINITION( auto t1 = profile_clock_t::now() );
          PROFILEACTION(std::cerr << '[' << super::rank << "] lclCompute: " << profile_duration_t(t1 - t0).count() << std::endl;
                        //MPI_Barrier(MPI_COMM_WORLD);
                        //t1 = profile_clock_t::now();
                        //if (super::rank == 0) std::cerr << " global Compute " << profile_duration_t(t1 - t0).count() << std::endl;
                        );
	}

        void reduction() {
          
                // It is declared here so that it is not destroyed before the Level 0 reduction if it is needed there
                std::vector<Return> local_results_alternate;
                std::vector<Return> *orig = &local_results;
                
                if(super::info.nLevels() > 2) {
                  
                        local_results_alternate.resize( super::info.treeLevel(super::info.nLevels() - 2).size() );
                        std::vector<Return> *dest = &local_results_alternate;
                        
                        for (int j = super::info.nLevels() - 2; j >= 1; j--) {
                                typename Info::Datatree_level_t& level_data = super::info.treeLevel(j);
                                int local_result_position = 0;
                                int local_result_positions[(int)level_data.size()];
                                for(int i = 0; i < (int)level_data.size(); i++) {
                                        local_result_positions[i] = local_result_position;
                                        general_reference_wrapper<T>& tmp = level_data[i];
                                        const int nchildren = (tmp.empty() ||
                                                               (  super::info.is_base(tmp) &&
                                                                !(super::behavior & DistributedInput))) ? 1 : super::info.num_children(tmp);
                                        local_result_position += nchildren;
                                }
                                auto& super_body = super::body;
                                int * p_local_result_positions = local_result_positions;
                                tbb::parallel_for(0, (int)level_data.size(), ParallelReduceHelper(orig, dest, super_body, level_data,p_local_result_positions)
                                /*[orig,dest,&super_body,&level_data,p_local_result_positions](int i) {
                                    (*dest)[i] = super_body.post(level_data[i].get(), &((*orig)[p_local_result_positions[i]]));
                                   }*/);
                                std::swap(orig, dest);
                        }
                }
                
                //Level 0 reduction
                if (super::info.nLevels() > 1) {
                        auto& reduction_root = (super::behavior & DistributedInput) ? super::info.treeLevel(0)[0].get() : super::global_root;
                        local_results[0] = super::body.post(reduction_root, &((*orig)[0]));
                }

        }
  
	Return reduce() {
                Return ret;
          
                if (super::behavior & DistributedOutput) {
                        assert(!(super::behavior & GatherInput));
                        const unsigned int n = super::info.nLocalElems();
                        assert( n <= 1 );
                        if (n == 1) {
                                ret = std::move(local_results[0]);
                        }
                        return ret;
                }

                //Only case where everyone has what it needs to make the reduction
                const bool resultallgatherv = super::behavior.enableResultAllgatherv();

                if (super::behavior & PrioritizeDM) {
                        PROFILELOG(" DistrReduce", super::info.template distributed_reduce_tree<true>(super::rank, super::behavior, super::body, &local_results));
                } else {
                        if (super::rank == 0) {
                                PROFILELOG(" Gather", recv_results());
                        }
                        if ((super::rank == 0) || resultallgatherv) {
                                PROFILELOG(" Reduce", reduction());
                        }

                }

          
                Return& ret_work = (super::dest == nullptr) ? ret : *super::dest;
          
                if ((super::rank == 0) || resultallgatherv) {
                        ret_work = std::move(local_results[0]);
                }

                bool had_global_sync = false;
		if(super::behavior & ReplicateOutput) {
                        if (!resultallgatherv) {
                                bcast(ret_work, super::rank, 0);
                                had_global_sync = true;
                        }
                  
			if (super::behavior & GatherInput) {
				bcast(super::global_root, super::rank, 0);
                                had_global_sync = true;
                        }
                }
          
                // This is so that messages from different skeleton invocations that may not need distribution
                //do not mix
                if (!had_global_sync){
                        MPI_Barrier(MPI_COMM_WORLD);
                }
          
                // Notice that if ret_ptr!=nullptr we are returning an empty Result ret.
                // This is done to avoid copies, losing the NRVO if *ret_ptr is returned.
                // The user should know anyway that the actual result is in *ret_ptr
          
		return ret; // Should be ret_work?
	}

	void send_results() {

                bool gather_input = super::behavior & GatherInput;
                if (gather_input && (super::behavior & Scatter)) {
                        super::info.gather_input();
                        gather_input = false;
                }
          
                if (super::rank != 0) {
                  
                        bool collective_gather_output = super::behavior & Gather;
                        if (collective_gather_output) {
                                super::info.template gather_output<Return>(super::rank, local_results, super::behavior, super::dest);
                        }
                  
                        const int base = super::info.firstGlobalElem(super::rank);
                  
                        // Serializable results are sent in a single chunks
                        // The optimization is not universally applicable to inputs because they are not consecutively
                        //stored in the info data tree, which consists of general_reference_wrapper elements.
                        optimized_send<Return>(base, collective_gather_output);

//                                if ( (!gather_input || boost::serialization::is_bitwise_serializable<T>::value) &&
//                                     (collective_gather_output || boost::serialization::is_bitwise_serializable<Return>::value) ) {
                                for (int i = 0; i < super::info.nLocalElems(); i++) {
                                        if (gather_input) {
                                                send(0, Info::MaxGlobalElems|(base + i), super::info.root(i));
                                        }
                                        if (!collective_gather_output) {
                                                send(0, base + i, local_results[i]);
                                        }
                                }
//                                } else {
//                                        for (int i = 0; i < super::info.nLocalElems(); i++) {
//                                                if (gather_input) {
//                                                  super::pending_tasks.run(SendTask<T>(0, Info::MaxGlobalElems|(base + i), super::info.root(i), &(super::send_mutex)));
//                                                }
//                                                if (!collective_gather_output) {
//                                                  super::pending_tasks.run(SendTask<Return>(0, base + i, local_results[i], &(super::send_mutex)));
//                                                }
//                                        }
//                                        super::pending_tasks.wait();
//                                }
                }

	}

	void recv_results() {
                int cur_pos = local_results.size();
          
                const bool needs_saving_buffer = ! super::info.prepare_reception(local_results, super::info.nGlobalElems());
                const bool gather_input = (super::behavior & GatherInput) && !(super::behavior & Scatter);
                const bool collective_gather_output = super::behavior & Gather;

                if (collective_gather_output) {
                        super::info.template gather_output<Return>(super::rank, local_results, super::behavior, super::dest);
                }
          
                // Must not be parallelized because receives block the thread
                //and we could have more nprocs that available threads
                //tbb::parallel_for(1, super::nprocs, ParallelReceiveHelper(*this, cur_pos, gather_input, collective_gather_output));

                if(gather_input || !collective_gather_output) {
                        int external_results = super::info.nGlobalElems() - super::info.nLocalElems(0);
                        if(gather_input && !collective_gather_output) {
                                external_results *= 2;
                        }
                        for (int i = 0; i < external_results; i++) {
                                int source = MPI_ANY_SOURCE;
                                int tag = MPI_ANY_TAG;
                                probe(source, tag);

                                //openmpi/1.10.2 breaks with "openmpi too many retries sending message; giving up"
                                // with >1 process per node with infiniband. See https://www.open-mpi.org/community/lists/users/2016/06/29444.php
                                //while (!iprobe(source, tag)) {
                                //  std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                //}

                                const bool is_input = tag & Info::MaxGlobalElems;
                                const int index = tag & ~Info::MaxGlobalElems;
                          
                                if (is_input) {
                                    recv(source, tag, super::info.lastTreeLevelElement(index) , nullptr, &(super::pending_tasks));
                                } else {
                                    i += optimized_recv<Return>(source, tag, index);
                                  //fprintf(stderr, "%d out of %d", i + 1, external_results); std::cerr << std::endl;
                                }
                        }
                        super::pending_tasks.wait();
                }
          
                if (needs_saving_buffer) {
                        super::info.save_reception_buffer(local_results);
                }
	}

        template<class ReBody>
        std::enable_if_t<has_member_function_distributed_post<ReBody, void, boost::mpl::vector<std::vector<T*>&,  std::vector<Return>&, int, int, T&, Return&>, boost::function_types::non_const>::value || has_member_function_distributed_post<ReBody, void, boost::mpl::vector<std::vector<T*>&,  std::vector<Return>&, int, int, T&, Return&>, boost::function_types::const_qualified>::value, Return> complete()
        { Return ret;

          const int nlocal_elems = super::info.nLocalElems();
          std::vector<T*> inputs(nlocal_elems);
          for (int i = 0; i < nlocal_elems; i++) {
            const auto& tmp = super::info.rootRef(i);
            inputs[i] = tmp.empty() ? nullptr : &tmp.get();
          }
          
          Return& ret_work = (super::dest == nullptr) ? ret : *super::dest;
          
          super::body.distributed_post(inputs, local_results, super::rank, super::nprocs, super::global_root, ret_work);
          
          return ret;
        }

        template<class ReBody>
        std::enable_if_t<!(has_member_function_distributed_post<ReBody, void, boost::mpl::vector<std::vector<T*>&,  std::vector<Return>&, int, int, T&, Return&>, boost::function_types::non_const>::value || has_member_function_distributed_post<ReBody, void, boost::mpl::vector<std::vector<T*>&,  std::vector<Return>&, int, int, T&, Return&>, boost::function_types::const_qualified>::value), Return> complete()
        {
          return super::template complete<ReBody>();
        }
};

/// WITHOUT RETURN VALUE (procedure)
template<typename T, typename Info, typename Body, typename Partitioner>
class ImplRun<void, T, Info, Body, Partitioner> : public AbstractRun<void, T, Info, Body> {
protected:
	typedef AbstractRun<void, T, Info, Body> super;

        struct ParallelComputeHelper {
          ImplRun& p_;
          const int step_;
          
          ParallelComputeHelper(ImplRun& p, const int step) : p_(p), step_(step) { }
    
          void operator() (int i) const {
            const int limit = std::min(i + step_, p_.info.nLocalElems());
            for(int j = i; j < limit; j++) {
              dpr::internal::start_recursion<void, T, Info, Body, Partitioner>::run(p_.info.rootRef(j), p_.info, p_.body);
            }
          }
       };
  
public:
  
	ImplRun(T&& r, Info& i, Body& b, const Behavior& behavior, void *) : super(std::forward<T>(r), i, b, behavior, nullptr)
        { }
  
	ImplRun(T& r, Info& i, Body& b, const Behavior& behavior, void *) : super(r, i, b, behavior, nullptr)
        { }

	void run() {
          
          PROFILEDEFINITION( const auto t0 = profile_clock_t::now() );
          
          const int nlocal_elems = super::info.nLocalElems();
          
          if ( (nlocal_elems < 2) ) { //|| !(super::behavior & GreedyParallel)) {
            for (int i = 0; i < nlocal_elems; i++) {
              dpr::internal::start_recursion<void, T, Info, Body, Partitioner>::run(super::info.rootRef(i), super::info, super::body);
            }
          }
          else {
            const int pre_level = super::info.parLevel_;
            int step;
            if (super::info.ntasks_ > nlocal_elems) {
              step = 1;
              super::info.parLevel_ = super::info.LevelsForNTasks((super::info.ntasks_  + (nlocal_elems - 1)) / nlocal_elems);
            } else {
              step = nlocal_elems / super::info.ntasks_;
              super::info.parLevel_ = 0;
            }
            tbb::parallel_for(0, nlocal_elems, step, ParallelComputeHelper(*this, step));
            super::info.parLevel_ = pre_level;
          }
          
          PROFILEDEFINITION( auto t1 = profile_clock_t::now() );
          PROFILEACTION(std::cerr << '[' << super::rank << "] lclCompute: " << profile_duration_t(t1 - t0).count() << std::endl;
                        //MPI_Barrier(MPI_COMM_WORLD);
                        //t1 = profile_clock_t::now();
                        //if (super::rank == 0) std::cerr << " global Compute " << profile_duration_t(t1 - t0).count() << std::endl;
                        );
	}

	void reduce() {
          if (!(super::behavior & DistributedOutput)) {
                if (super::behavior & PrioritizeDM) {
                       super::info.template distributed_reduce_tree<false>(super::rank, super::behavior, super::body, (std::vector<char> *) nullptr);
                } else {
		        if (super::rank == 0) {
			        PROFILELOG(" Gather", recv_results());
		        }
                }
            
		if(super::behavior & ReplicateOutput) {
			bcast(super::global_root, super::rank, 0);
                } else {
                  // This is so that messages from different skeleton invocations that may not need distribution
                  //do not mix
                  MPI_Barrier(MPI_COMM_WORLD);
                }
          }
	}

	void send_results() {
                if (super::behavior & Scatter) {
                         super::info.gather_input();
                } else {
                        if (super::rank != 0) {
                                const int base = super::info.firstGlobalElem(super::rank);
//                                        if (boost::serialization::is_bitwise_serializable<T>::value) {
                                      for (int i = 0; i < super::info.nLocalElems(); i++) {
                                              send(0, base + i, super::info.root(i));
                                      }
//                                        } else {
//                                              for (int i = 0; i < super::info.nLocalElems(); i++) {
//                                                      super::pending_tasks.run(SendTask<T>(0, base + i, super::info.root(i), &(super::send_mutex)));
//                                              }
//                                              super::pending_tasks.wait();
//                                        }
                        }
                }
	}

	void recv_results() {
                if (!(super::behavior & Scatter)) {
                        const int external_results = super::info.nGlobalElems() - super::info.nLocalElems(0);
                        for (int i = 0; i < external_results; i++) {
                                int source = MPI_ANY_SOURCE;
                                int tag = MPI_ANY_TAG;
                                probe(source, tag);
                                recv(source, tag, super::info.lastTreeLevelElement(tag) , nullptr, &(super::pending_tasks));
                        }
                        super::pending_tasks.wait();
                }
	}
};

template<typename Return, typename T, typename Info, typename Body, typename Partitioner>
Return start_recursion(T&& root, Info& info, Body body, Behavior behavior, Partitioner, Return * const dest)
{

  if (behavior & ReplicateInput) {
    //if we want to replicate the input, it does not make sense it is distributed
    assert(!(behavior & DistributedInput));
    //if we replicate the input in each node, we can make a ReplicatedInput processing
    behavior |= ReplicatedInput;
  }

  if (behavior & UseCost) {
    //UseCost implies Balance
    behavior |= Balance;
  }

  if (behavior & ReusableGather) {
    //ReusableGather implies Gather
    behavior |= Gather;
  }
  
  if (std::is_void<Return>::value) {
    if (behavior & DistributedOutput) {
      assert(!(behavior & GatherInput)); // contradiction
    } else {
      behavior |= GatherInput;
    }
  }

  // Scatter and PrioritizeDM are incompatible
  assert(!(behavior & Scatter) || !(behavior & PrioritizeDM));

  ImplRun<Return, typename std::remove_reference<T>::type, Info, Body, Partitioner> impl(root, info, body, behavior, dest);

  PROFILELOG(" Distrib", impl.distribute());
  
  impl.run();
  
  return impl.template complete<Body>();
  
}

} //namespace internal_mpi

/** When no partitioner is specified, the internal::simple_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_recursion(T&& root, Info&& info, Body body, const Behavior& behavior = Behavior(DefaultBehavior)) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, behavior, internal::simple_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_recursion(T&& root, Info&& info, Body body, partitioner::simple, const Behavior& behavior = Behavior(DefaultBehavior)) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, behavior, internal::simple_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_recursion(T&& root, Info&& info, Body body, partitioner::automatic, const Behavior& behavior = Behavior(DefaultBehavior)) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, behavior, internal::auto_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_recursion(T&& root, Info&& info, Body body, partitioner::custom, const Behavior& behavior = Behavior(DefaultBehavior)) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, behavior, internal::custom_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr);
}

  template<typename Return, typename T, typename Info, typename Body>
  Return dparallel_recursion(T&& root, Info&& info, Body body, const Behavior& behavior, Return& res) {
    return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, behavior, internal::simple_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res);
  }
  
  template<typename Return, typename T, typename Info, typename Body>
  Return dparallel_recursion(T&& root, Info&& info, Body body, partitioner::simple, const Behavior& behavior, Return& res) {
    return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, behavior, internal::simple_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res);
  }
  
  template<typename Return, typename T, typename Info, typename Body>
  Return dparallel_recursion(T&& root, Info&& info, Body body, partitioner::automatic, const Behavior& behavior, Return& res) {
    return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, behavior, internal::auto_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res);
  }
  
  template<typename Return, typename T, typename Info, typename Body>
  Return dparallel_recursion(T&& root, Info&& info, Body body, partitioner::custom, const Behavior& behavior, Return& res) {
    return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, behavior, internal::custom_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res);
  }
  
} // namespace dpr

/// Facilitates the initialization of the dparallel_recursion MPI+TBB environment
#define dpr_init(argc, argv, nprocs, rank, nthreads) {                             \
          int dpr_provided;                                                        \
          MPI_Init_thread(&(argc), &(argv), MPI_THREAD_SERIALIZED, &dpr_provided); \
          assert(dpr_provided >= MPI_THREAD_SERIALIZED);                           \
          MPI_Comm_rank(MPI_COMM_WORLD, &(rank));                                  \
          MPI_Comm_size(MPI_COMM_WORLD, &(nprocs));                                \
          pr_init(nthreads);                                                       \
       }

#endif /* DPR_PARALLEL_RECURSION_MPI_H_ */
