/*
 *         ---- The Unbalanced Tree Search (UTS) Benchmark ----
 *  
 *  Copyright (c) 2010 See AUTHORS file for copyright holders
 *
 *  This file is part of the unbalanced tree search benchmark.  This
 *  project is licensed under the MIT Open Source license.  See the LICENSE
 *  file for copyright and licensing information.
 *
 *  UTS is a collaborative project between researchers at the University of
 *  Maryland, the University of North Carolina at Chapel Hill, and the Ohio
 *  State University.  See AUTHORS file for more information.
 *
 */

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <math.h>

#include "uts.h"
#undef min
#undef max
#include <algorithm>

#ifndef PROFILE
#define PROFILEDEFINITION(...)  /* no profiling */
#define PROFILEACTION(...)      /* no profiling */
#else
#define PROFILEDEFINITION(...) __VA_ARGS__ ;
#define PROFILEACTION(...) do{ __VA_ARGS__ ; } while(0)
#endif

/***********************************************************
 *                                                         *
 *  Compiler Type (these flags are set by at compile time) *
 *     (default) ANSI C compiler - sequential execution    *
 *     (_OPENMP) OpenMP enabled C compiler                 *
 *     (__UPC__) UPC compiler                              *
 *     (_SHMEM)  Cray Shmem                                *
 *     (__PTHREADS__) Pthreads multithreaded execution     *
 *                                                         *
 ***********************************************************/

/**** OpenMP Definitions ****/
#ifdef _OPENMP
#include <omp.h>
#define PARALLEL         1
#define COMPILER_TYPE    1
#define SHARED 
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAX_THREADS       32
#define LOCK_T           omp_lock_t
#define GET_NUM_THREADS  omp_get_num_threads()
#define GET_THREAD_NUM   omp_get_thread_num()
#define SET_LOCK(zlk)    omp_set_lock(zlk)
#define UNSET_LOCK(zlk)  omp_unset_lock(zlk)
#define INIT_LOCK(zlk)   zlk=omp_global_lock_alloc()
#define INIT_SINGLE_LOCK(zlk) zlk=omp_global_lock_alloc()
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define REALLOC          realloc
#define BARRIER          
// OpenMP helper function to match UPC lock allocation semantics
omp_lock_t * omp_global_lock_alloc() {
  omp_lock_t *lock = (omp_lock_t *) malloc(sizeof(omp_lock_t) + 128);
  omp_init_lock(lock);
  return lock;
}

/**** UPC Definitions ****/
#elif defined(__UPC__)
#include <upc.h>
#define PARALLEL         1
#define COMPILER_TYPE    2
#define SHARED           shared
#define SHARED_INDEF     shared [0]
#define VOLATILE         strict
#define MAX_THREADS       (THREADS)
#define LOCK_T           upc_lock_t
#define GET_NUM_THREADS  (THREADS)
#define GET_THREAD_NUM   (MYTHREAD)
#define SET_LOCK(zlk)    upc_lock(zlk)
#define UNSET_LOCK(zlk)  upc_unlock(zlk)
#define INIT_LOCK(zlk)   zlk=upc_global_lock_alloc()
#define INIT_SINGLE_LOCK(zlk) zlk=upc_all_lock_alloc()
#define SMEMCPY          upc_memget
#define ALLOC            upc_alloc
#define REALLOC
#define BARRIER          upc_barrier;
#define UTS_DENY_STACK_RESIZE 1


/**** Shmem Definitions ****/
#elif defined(_SHMEM)
#include <mpp/shmem.h>
#define PARALLEL         1
#define COMPILER_TYPE    3
#define SHARED           
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAX_THREADS       64
#define LOCK_T           long
#define GET_NUM_THREADS  shmem_n_pes()
#define GET_THREAD_NUM   shmem_my_pe()
#define SET_LOCK(zlk)    shmem_set_lock(zlk)
#define UNSET_LOCK(zlk)  shmem_clear_lock(zlk)
#define INIT_LOCK(zlk)   zlk = shmem_global_lock_alloc()
#define INIT_SINGLE_LOCK(zlk)	zlk = shmem_global_lock_alloc()
#define SMEMCPY          shmem_getmem
  // Shmem's get has different semantics from memcpy():
  //   void shmem_getmem(void *target, const void *source, size_t len, int pe)
#define ALLOC            shmalloc
#define REALLOC          shrealloc
#define BARRIER          shmem_barrier_all();

// Shmem helper function to match UPC lock allocation semantics
LOCK_T * shmem_global_lock_alloc() {    
    LOCK_T *lock = (LOCK_T *) shmalloc(sizeof(LOCK_T));
    *lock = 0;
    return lock;
}

#define GET(target,source,from_id) shmem_int_get(&(target),&(source),1,from_id)        
#define PUT(target,source,to_id)   shmem_int_put(&(target),&(source),1,to_id)

#define PUT_ALL(a,b)								\
	do {						 			\
		int _iter, _node; 						\
		for (_iter = 1; _iter < GET_NUM_THREADS; _iter++) {		\
			_node = (GET_THREAD_NUM + _iter) % GET_NUM_THREADS;	\
			shmem_int_put((int *)&a,(int *)&b,1,_node); \
		}								\
	} while(0)


/**** Pthreads Definitions ****/
#elif defined(__PTHREADS__)
#include <pthread.h>
#define PARALLEL         1
#define COMPILER_TYPE    4
#define SHARED
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAX_THREADS       128
#define LOCK_T           pthread_mutex_t
#define GET_NUM_THREADS  pthread_num_threads
#define GET_THREAD_NUM   *(int*)pthread_getspecific(pthread_thread_num)
#define SET_LOCK(zlk)    pthread_mutex_lock(zlk)
#define UNSET_LOCK(zlk)  pthread_mutex_unlock(zlk)
#define INIT_LOCK(zlk)   zlk = pthread_global_lock_alloc()
#define INIT_SINGLE_LOCK(zlk)  zlk = pthread_global_lock_alloc()
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define REALLOC          realloc
#define BARRIER           

int pthread_num_threads = 1;              // Command line parameter - default to 1
pthread_key_t pthread_thread_num;         // Key to store each thread's ID

/* helper function to match UPC lock allocation semantics */
LOCK_T * pthread_global_lock_alloc() {    
    LOCK_T *lock = (LOCK_T *) malloc(sizeof(LOCK_T));
    pthread_mutex_init(lock, NULL);
    return lock;
}

/**** parallel_recursion Definitions ****/
#elif defined(PARALLEL_RECURSION)
#include <dparallel_recursion/parallel_recursion.h>

int tasks_per_thread = 6;
int ntasks;
int height_to_serial = 5;
int nthreads = 8;
int partitioner = 0;	//0 = custom, 1 = simple, 2 = automatic. Default: custom
#define PARALLEL         1
#define COMPILER_TYPE    5
#define SHARED
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAX_THREADS      256
#define LOCK_T           void
#define GET_NUM_THREADS  nthreads
#define GET_THREAD_NUM   0
#define SET_LOCK(zlk)
#define UNSET_LOCK(zlk)
#define INIT_LOCK(zlk)
#define INIT_SINGLE_LOCK(zlk)
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define REALLOC          realloc
#define BARRIER

struct Result_t {
  int tnodes, tleaves, mheight;
};

struct MyInfo : public dpr::Arity<dpr::UNKNOWN> {

  MyInfo() : dpr::Arity<dpr::UNKNOWN> (ntasks, nthreads)
  {}

  static bool is_base(const Node& n) noexcept {
    return !n.numChildren;
  }

  static int num_children(const Node& n) noexcept {
    return n.numChildren;
  }

  static bool do_parallel(const Node& n) noexcept {
    const bool ret = n.height < height_to_serial;
    return ret;
  }

  static Node child(int i, const Node& n) noexcept {
    Node ret;
    ret.type   = uts_childType(&n); // n.type; //OPT1: Instead of uts_childType(&n);
    ret.height = n.height + 1;
    ret.numChildren = -1;
    for (int j = 0; j < computeGranularity; j++) {
      rng_spawn(n.state.state, ret.state.state, i);
    }
    return ret;
  }
};

struct MyBody : public dpr::EmptyBody<Node, Result_t> {

  static void pre(Node &n) noexcept {
    n.numChildren = uts_numChildren(&n);
  }

  /* //OPT1: Store type for children in parent
  void pre_rec(Node &n) const {
    n.type = uts_childType(&n);
  }
  */

  static Result_t base(const Node& n) noexcept {
    return {1, 1, n.height};
  }

  static Result_t post(const Node& n, const Result_t* r) noexcept {
    Result_t ret {1, 0, 0};
    for (int i = 0; i < n.numChildren; i++) {
      ret.tnodes += r[i].tnodes;
      ret.tleaves += r[i].tleaves;
      ret.mheight = std::max(ret.mheight, r[i].mheight);
    }
    /*
    Result_t ret {r->tnodes + 1, r->tleaves, r->mheight};
    for (int i = 1; i < n.numChildren; i++) {
      ret.tnodes += r[i].tnodes;
      ret.tleaves += r[i].tleaves;
      ret.mheight = std::max(ret.mheight, r[i].mheight);
    }
    */
    return ret;
  }
};

/**** dparallel_recursion Definitions ****/
#elif defined(DPARALLEL_RECURSION)
#include <dparallel_recursion/dparallel_recursion.h>

int tasks_per_thread = 6;
int ntasks;
int height_to_serial = 5;
int additional_flags = 0;
int nthreads = 8;
int partitioner = 0;	//0 = custom, 1 = simple, 2 = automatic. Default: custom
#define PARALLEL         1
#define COMPILER_TYPE    6
#define SHARED
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAX_THREADS      256
#define LOCK_T           void
#define GET_NUM_THREADS  nthreads
#define GET_THREAD_NUM   0
#define SET_LOCK(zlk)
#define UNSET_LOCK(zlk)
#define INIT_LOCK(zlk)
#define INIT_SINGLE_LOCK(zlk)
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define REALLOC          realloc
#define BARRIER

struct Result_t {
  int tnodes, tleaves, mheight;
};

BOOST_IS_BITWISE_SERIALIZABLE(Node);
BOOST_IS_BITWISE_SERIALIZABLE(Result_t);

struct MyInfo : public dpr::DInfo<Node, dpr::UNKNOWN> {

  MyInfo() : dpr::DInfo<Node, dpr::UNKNOWN> (ntasks, nthreads)
  {}

  static bool is_base(const Node& n) noexcept {
    return !n.numChildren;
  }

  static int num_children(const Node& n) noexcept {
    return n.numChildren;
  }
  
  static bool do_parallel(const Node& n) noexcept {
    const bool ret = n.height < height_to_serial;
    return ret;
  }
  
  static Node child(int i, const Node& n) noexcept {
    Node ret;
    ret.type   = uts_childType(&n); // n.type; //OPT1: Instead of uts_childType(&n);
    ret.height = n.height + 1;
    ret.numChildren = -1;
    for (int j = 0; j < computeGranularity; j++) {
      rng_spawn(n.state.state, ret.state.state, i);
    }
    return ret;
  }
};

struct MyBody : public dpr::EmptyBody<Node, Result_t> {
  
  static void pre(Node &n) noexcept {
    n.numChildren = uts_numChildren(&n);
  }
  
  /* //OPT1: Store type for children in parent
  void pre_rec(Node &n) const {
    n.type = uts_childType(&n);
  }
  */

  static Result_t base(const Node& n) noexcept {
    return {1, 1, n.height};
  }
  
  static Result_t post(const Node& n, const Result_t* r) noexcept {
    Result_t ret {1, 0, 0};
    for (int i = 0; i < n.numChildren; i++) {
      ret.tnodes += r[i].tnodes;
      ret.tleaves += r[i].tleaves;
      ret.mheight = std::max(ret.mheight, r[i].mheight);
    }
    /*
    Result_t ret {r->tnodes + 1, r->tleaves, r->mheight};
    for (int i = 1; i < n.numChildren; i++) {
      ret.tnodes += r[i].tnodes;
      ret.tleaves += r[i].tleaves;
      ret.mheight = std::max(ret.mheight, r[i].mheight);
    }
    */
    return ret;
  }

};

/**** parallel_stack_recursion Definitions ****/
#elif defined(PARALLEL_STACK_RECURSION)
#ifdef UTS_DENY_STACK_RESIZE
#define DPR_DENY_STACK_RESIZE 1
#endif
#include <dparallel_recursion/parallel_stack_recursion.h>

int nthreads = 8;
int height_to_serial = 5;
int partitioner = 1;		//0 = custom, 1 = simple, 2 = automatic. Default: simple
dpr::AutomaticChunkOptions opt = dpr::aco_default;
#define PARALLEL         1
#define COMPILER_TYPE    7
#define SHARED
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAX_THREADS      256
#define LOCK_T           void
#define GET_NUM_THREADS  nthreads
#define GET_THREAD_NUM   0
#define SET_LOCK(zlk)
#define UNSET_LOCK(zlk)
#define INIT_LOCK(zlk)
#define INIT_SINGLE_LOCK(zlk)
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define REALLOC          realloc
#define BARRIER

struct Result_t {
  int tnodes, tleaves, mheight;

  Result_t() : tnodes(0), tleaves(0), mheight(0)
  {}

  Result_t(int tnodes_, int tleaves_, int mheight_) : tnodes(tnodes_), tleaves(tleaves_), mheight(mheight_)
  {}
};

struct MyInfo : public dpr::Arity<dpr::UNKNOWN> {

  MyInfo() : dpr::Arity<dpr::UNKNOWN> (nthreads, nthreads)
  {}

  static bool is_base(const Node& n) noexcept {
    return !n.numChildren;
  }

  static int num_children(const Node& n) noexcept {
    return n.numChildren;
  }

  static bool do_parallel(const Node& n) noexcept {
    const bool ret = n.height < height_to_serial;
	return ret;
  }

  static Node child(int i, const Node& n) noexcept {
    Node ret;
    ret.type = uts_childType(&n); // n.type; //OPT1: Instead of uts_childType(&n);
    ret.height = n.height + 1;
    ret.numChildren = -1;
    /*if (i == 0) {
      ret.auxNodeCounter = n.auxNodeCounter;
    } else {
      ret.auxNodeCounter = 0;
    }*/
    for (int j = 0; j < computeGranularity; j++) {
      rng_spawn(n.state.state, ret.state.state, i);
    }
    return ret;
  }

//  static void child(int i, const Node& n, Node& ret) noexcept {
//    ret.type = uts_childType(&n); // n.type; //OPT1: Instead of uts_childType(&n);
//    ret.height = n.height + 1;
//    ret.numChildren = -1;
//    if (i == 0) {
//      ret.auxNodeCounter = n.auxNodeCounter;
//    } else {
//      ret.auxNodeCounter = 0;
//    }
//    for (int j = 0; j < computeGranularity; j++) {
//      rng_spawn(n.state.state, ret.state.state, i);
//    }
//  }
};

struct MyBody : public dpr::EmptyBody<Node, Result_t, true> {

  static void pre(Node &n) noexcept {
    n.numChildren = uts_numChildren(&n);
	//n.auxNodeCounter += n.numChildren;
  }

  /* //OPT1: Store type for children in parent
  void pre_rec(Node &n) const {
    n.type = uts_childType(&n);
  }
  */

  static Result_t base(const Node& n) noexcept {
    //return {n.auxNodeCounter, 1, n.height};
	return {1, 1, n.height};
  }

  static Result_t non_base(const Node& n) noexcept {
    //return {n.auxNodeCounter, 1, n.height};
	return {1, 0, 0};
  }

  static void post(const Result_t& r, Result_t& rr) noexcept {
      rr.tnodes += r.tnodes;
      rr.tleaves += r.tleaves;
      rr.mheight = std::max(rr.mheight, r.mheight);
  }

};

/**** dparallel_stack_recursion Definitions ****/
#elif defined(DPARALLEL_STACK_RECURSION)
#ifdef UTS_DENY_STACK_RESIZE
#define DPR_DENY_STACK_RESIZE 1
#endif
#include <dparallel_recursion/dparallel_stack_recursion.h>

int nthreads = 8;
int height_to_serial = 5;
int additional_flags = 0;
int partitioner = 1;													// 0 = custom, 1 = simple, 2 = automatic. Default: simple
int chunksToSteal = dpr::CHUNKS_TO_STEAL_DEFAULT;
int threads_request_policy = dpr::THREAD_MPI_STEAL_POLICY_DEFAULT;
int mpi_workrequest_limits = dpr::MPI_WORKREQUEST_LIMITS_DEFAULT;
int trp_predict_work_count = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;
dpr::AutomaticChunkOptions opt = dpr::aco_default;
#define PARALLEL         1
#define COMPILER_TYPE    8
#define SHARED
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAX_THREADS      256
#define LOCK_T           void
#define GET_NUM_THREADS  nthreads
#define GET_THREAD_NUM   0
#define SET_LOCK(zlk)
#define UNSET_LOCK(zlk)
#define INIT_LOCK(zlk)
#define INIT_SINGLE_LOCK(zlk)
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define REALLOC          realloc
#define BARRIER

struct Result_t {
  int tnodes, tleaves, mheight;

  Result_t() : tnodes(0), tleaves(0), mheight(0)
  {}

  Result_t(int tnodes_, int tleaves_, int mheight_) : tnodes(tnodes_), tleaves(tleaves_), mheight(mheight_)
  {}
};

BOOST_IS_BITWISE_SERIALIZABLE(Node);
BOOST_IS_BITWISE_SERIALIZABLE(Result_t);

struct MyInfo : public dpr::Arity<dpr::UNKNOWN> {

  MyInfo() : dpr::Arity<dpr::UNKNOWN> (nthreads, nthreads)
  {}

  static bool is_base(const Node& n) noexcept {
    return !n.numChildren;
  }

  static int num_children(const Node& n) noexcept {
    return n.numChildren;
  }

  static bool do_parallel(const Node& n) noexcept {
    const bool ret = n.height < height_to_serial;
	return ret;
  }

  static Node child(int i, const Node& n) noexcept {
    Node ret;
    ret.type = uts_childType(&n); // n.type; //OPT1: Instead of uts_childType(&n);
    ret.height = n.height + 1;
    ret.numChildren = -1;
    /*if (i == 0) {
      ret.auxNodeCounter = n.auxNodeCounter;
    } else {
      ret.auxNodeCounter = 0;
    }*/
    for (int j = 0; j < computeGranularity; j++) {
      rng_spawn(n.state.state, ret.state.state, i);
    }
    return ret;
  }

};

struct MyBody : public dpr::EmptyBody<Node, Result_t, true> {

  static void pre(Node &n) noexcept {
    n.numChildren = uts_numChildren(&n);
	//n.auxNodeCounter += n.numChildren;
  }

  /* //OPT1: Store type for children in parent
  void pre_rec(Node &n) const {
    n.type = uts_childType(&n);
  }
  */

  static Result_t base(const Node& n) noexcept {
    //return {n.auxNodeCounter, 1, n.height};
    return {1, 1, n.height};
  }

  static Result_t non_base(const Node& n) noexcept {
    return {1, 0, 0};
  }

  static void post(const Result_t& r, Result_t& rr) noexcept {
      rr.tnodes += r.tnodes;
      rr.tleaves += r.tleaves;
      rr.mheight = std::max(rr.mheight, r.mheight);
  }

};

/**** Cilk Definitions ****/
#elif defined(_CILK) || defined(_CILK_OPT)
#include <cilk/cilk_api.h>

struct Result_t {
  int tnodes, tleaves, mheight;
};

int nthreads = __cilkrts_get_nworkers();
int height_to_serial = 5;
#define PARALLEL         1
#define COMPILER_TYPE    9
#define SHARED
#define SHARED_INDEF
#define VOLATILE         volatile
#define MAX_THREADS      256
#define LOCK_T           void
#define GET_NUM_THREADS  nthreads
#define GET_THREAD_NUM   0
#define SET_LOCK(zlk)
#define UNSET_LOCK(zlk)
#define INIT_LOCK(zlk)
#define INIT_SINGLE_LOCK(zlk)
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define REALLOC          realloc
#define BARRIER
#define PARALLEL         1
#define COMPILER_TYPE    9
#define SHARED
#define SHARED_INDEF


/**** Default Sequential Definitions ****/
#else
#define PARALLEL         0
#define COMPILER_TYPE    0
#define SHARED
#define SHARED_INDEF
#define VOLATILE
#define MAX_THREADS 1
#define LOCK_T           void
#define GET_NUM_THREADS  1
#define GET_THREAD_NUM   0
#define SET_LOCK(zlk)    
#define UNSET_LOCK(zlk)  
#define INIT_LOCK(zlk) 
#define INIT_SINGLE_LOCK(zlk) 
#define SMEMCPY          memcpy
#define ALLOC            malloc
#define REALLOC          realloc
#define BARRIER           

#endif /* END Par. Model Definitions */


/***********************************************************
 *  Parallel execution parameters                          *
 ***********************************************************/

#define DEFAULT_MAXSTACKDEPTH 500000

int doSteal   = PARALLEL; // 1 => use work stealing
int chunkSize = 20;       // number of nodes to move to/from shared area
#if defined(_CILK) || defined(_CILK_OPT)
int initialStackSize = 2000000;	// stack size
#elif defined(PARALLEL_STACK_RECURSION) || defined(DPARALLEL_STACK_RECURSION)
int initialStackSize = 100;	// stack size
#else
int initialStackSize = DEFAULT_MAXSTACKDEPTH;	// stack size
#endif
int cbint     = 1;        // Cancellable barrier polling interval
#ifdef DPARALLEL_STACK_RECURSION
int pollint   = dpr::POLLING_INTERVAL_DEFAULT;	// Default: 0
#else
int pollint   = 1;        // BUPC Polling interval
#endif

#ifdef __BERKELEY_UPC__
/* BUPC nonblocking I/O Handles */
bupc_handle_t cb_handle       = BUPC_COMPLETE_HANDLE;
const int     local_cb_cancel = 1;
#endif


/***********************************************************
 * Tree statistics (if selected via UTS_STAT)              *
 *   compute overall size and imbalance metrics            *
 *   and histogram size and imbalance per level            *
 ***********************************************************/
#ifdef UTS_STAT

/* Check that we are not being asked to compile parallel with stats.
 * Parallel stats collection is presently not supported.  */
#if PARALLEL
#error "ERROR: Parallel stats collection is not supported!"
#endif

#define MAXHISTSIZE      2000  // max tree depth in histogram
int    stats     = 1;
int    unbType   = 1;
int    maxHeight = 0;         // maximum depth of tree
double maxImb    = 0;         // maximum imbalance
double minImb    = 1;
double treeImb   =-1;         // Overall imbalance, undefined

int    hist[MAXHISTSIZE+1][2];      // average # nodes per level
double unbhist[MAXHISTSIZE+1][3];   // average imbalance per level

int    *rootSize;             // size of the root's children 
double *rootUnb;              // imbalance of root's children

/* Tseng statistics */
int    totalNodes = 0;
double imb_max    = 0;         // % of work in largest child (ranges from 100/n to 100%)
double imb_avg    = 0;
double imb_devmaxavg     = 0;  // ( % of work in largest child ) - ( avg work )
double imb_normdevmaxavg = 0;  // ( % of work in largest child ) - ( avg work ) / ( 100% - avg work )
#else
int stats   = 0;
int unbType = -1;
#endif


/***********************************************************
 *  Execution Tracing                                      *
 ***********************************************************/

#define SS_WORK    0
#define SS_SEARCH  1
#define SS_IDLE    2
#define SS_OVH     3
#define SS_CBOVH   4
#define SS_NSTATES 5

/* session record for session visualization */
struct sessionRecord_t {
  double startTime, endTime;
};
typedef struct sessionRecord_t SessionRecord;

/* steal record for steal visualization */
struct stealRecord_t {
  long int nodeCount;           /* count nodes generated during the session */
  int victimThread;             /* thread from which we stole the work  */
};
typedef struct stealRecord_t StealRecord;

/* Store debugging and trace data */
struct metaData_t {
  SessionRecord sessionRecords[SS_NSTATES][20000];   /* session time records */
  StealRecord stealRecords[20000]; /* steal records */
};
typedef struct metaData_t MetaData;

/* holds text string for debugging info */
char debug_str[1000];


/***********************************************************
 * StealStack types                                        *
 ***********************************************************/

/* stack of nodes */
struct stealStack_t
{
  int workAvail;     /* elements available for stealing */
  int sharedStart;   /* index of start of shared portion of stack */
  int padding[14];   /* 4*14=56 bytes padding */
  int local;         /* index of start of local portion */
  int top;           /* index of stack top */
  int stackSize;     /* total space avail (in number of elements) */
  int maxStackDepth;                      /* stack stats */
  int nNodes, maxTreeDepth;               /* tree stats  */
  int nLeaves;
  int nAcquire, nRelease, nSteal, nFail;  /* steal stats */
  int wakeups, falseWakeups, nNodes_last;
  double time[SS_NSTATES], timeLast;         /* perf measurements */
  int entries[SS_NSTATES], curState;
  LOCK_T * stackLock; /* lock for manipulation of shared portion */
  Node * stack;       /* addr of actual stack of nodes in local addr space */
  SHARED_INDEF Node * stack_g; /* addr of same stack in global addr space */
#ifdef TRACE
  MetaData * md;        /* meta data used for debugging and tracing */
#endif
};
typedef struct stealStack_t StealStack;

typedef SHARED StealStack * SharedStealStackPtr;


/***********************************************************
 *  Global shared state                                    *
 ***********************************************************/

// shared access to each thread's stealStack
SHARED SharedStealStackPtr stealStack[MAX_THREADS];

// termination detection 
VOLATILE SHARED int cb_cancel;
VOLATILE SHARED int cb_count;
VOLATILE SHARED int cb_done;
LOCK_T * cb_lock;

SHARED double startTime[MAX_THREADS];


/***********************************************************
 *  UTS Implementation Hooks                               *
 ***********************************************************/

// Return a string describing this implementation
char * impl_getName() {
  const char * name[] = {"Sequential C", "C/OpenMP", "UPC", "SHMEM", "PThreads", "parallel_recursion", "dparallel_recursion", "parallel_stack_recursion", "dparallel_stack_recursion", "Cilk"};
  return (char *)name[COMPILER_TYPE];
}


// construct string with all parameter settings 
int impl_paramsToStr(char *strBuf, int ind) {
  ind += sprintf(strBuf+ind, "Execution strategy:  ");
  if (PARALLEL) {
    ind += sprintf(strBuf+ind, "Parallel search using %d threads\n", GET_NUM_THREADS);
    if (doSteal) {
      ind += sprintf(strBuf+ind, "   Load balance by work stealing, chunk size = %d nodes\n",chunkSize);
      ind += sprintf(strBuf+ind, "  CBarrier Interval: %d\n", cbint);
      ind += sprintf(strBuf+ind, "   Polling Interval: %d\n", pollint);
      ind += sprintf(strBuf+ind, "   Initial Stack Size: %d\n", initialStackSize);
    }
    else
      ind += sprintf(strBuf+ind, "   No load balancing.\n");
  }
  else
    ind += sprintf(strBuf+ind, "Iterative sequential search\n");
      
  return ind;
}


int impl_parseParam(char *param, char *value) {
  int err = 0;  // Return 0 on a match, nonzero on an error

  switch (param[1]) {
#if !defined(PARALLEL_RECURSION) && !defined(DPARALLEL_RECURSION)
    case 'S':
      initialStackSize = atoi(value); break;
#endif
#if (PARALLEL == 1)
    case 'c':
      chunkSize = atoi(value); break;
    case 's':
      doSteal = atoi(value); 
      if (doSteal != 1 && doSteal != 0) {
        err = 1;
      }
      break;
    case 'i':
#ifdef DPARALLEL_STACK_RECURSION
      pollint = atoi(value); break;
#else
      cbint = atoi(value); break;
#ifdef __BERKELEY_UPC__
    case 'I':
      pollint = atoi(value); break;
#endif
#endif
#ifdef __PTHREADS__
    case 'T':
      pthread_num_threads = atoi(value);
      if (pthread_num_threads > MAX_THREADS) {
        printf("Warning: Requested threads > MAX_THREADS.  Truncated to %d threads\n", MAX_THREADS);
        pthread_num_threads = MAX_THREADS;
      }
      break;
#endif
#ifdef PARALLEL_RECURSION
    case 'T':
      tasks_per_thread = atoi(value);
      break;
    case 'U':
      height_to_serial = atoi(value);
      break;
    case 'P':
      partitioner = atoi(value);
      if ((partitioner > 3) || (partitioner < 0)) {
        printf("Warning: Invalid 'partitioner' value. Use default: 'custom'\n");
        partitioner = 0;
      }
      break;
#endif
#ifdef DPARALLEL_RECURSION
    case 'T':
      tasks_per_thread = atoi(value);
      break;
    case 'U':
      height_to_serial = atoi(value);
      break;
    case 'P':
      partitioner = atoi(value);
      if ((partitioner > 3) || (partitioner < 0)) {
        printf("Warning: Invalid 'partitioner' value. Use default: 'custom'\n");
        partitioner = 0;
      }
      break;
    case 'F':
      additional_flags = atoi(value);
      break;
#endif
#ifdef _CILK_OPT
    case 'U':
      height_to_serial = atoi(value);
      break;
#endif
#ifdef PARALLEL_STACK_RECURSION
    case 'U':
      height_to_serial = atoi(value);
      break;
    case 'P':
      partitioner = atoi(value);
      if ((partitioner > 2) || (partitioner < 0)) {
        printf("Warning: Invalid 'partitioner' value. Use default: 'custom'\n");
        partitioner = 0;
      }
      break;
    case 'Z':
      opt.maxTime = static_cast<double>(atoi(value)) / 1000.0;
      break;
#endif
#ifdef DPARALLEL_STACK_RECURSION
    case 'U':
      height_to_serial = atoi(value);
      break;
    case 'P':
      partitioner = atoi(value);
      if ((partitioner > 2) || (partitioner < 0)) {
        printf("Warning: Invalid 'partitioner' value. Use default: 'custom'\n");
        partitioner = 0;
      }
      break;
    case 'C':
      chunksToSteal = atoi(value);
      if (chunksToSteal <= 0) {
        printf("Warning: Invalid 'chunksToSteal' value. Use default: '1'\n");
        chunksToSteal = 1;
      }
      break;
    case 'M':
      threads_request_policy = atoi(value);
	  if ((threads_request_policy > 1) || (threads_request_policy < -1)) {
	    printf("Warning: Invalid 'threads_request_policy' value. Use default: '1: predictive'\n");
	    threads_request_policy = 1;
	  }
	  break;
    case 'L':
    	mpi_workrequest_limits = atoi(value);
  	  break;
    case 'W':
    	trp_predict_work_count = atoi(value);
  	  break;
    case 'F':
      additional_flags = atoi(value);
      break;
    case 'Z':
      opt.maxTime = static_cast<double>(atoi(value)) / 1000.0;
      break;
#endif
#else /* !PARALLEL */
#ifdef UTS_STAT
    case 'u':
      unbType = atoi(value); 
      if (unbType > 2) {
        err = 1;
        break;
      }
      if (unbType < 0)
        stats = 0;
      else
        stats = 1;
      break;
#endif
#endif /* PARALLEL */
    default:
      err = 1;
      break;
  }
  return err;
}

void impl_helpMessage() {
  if (PARALLEL) {
    printf("   -s  int   zero/nonzero to disable/enable work stealing\n");
    printf("   -c  int   chunksize for work stealing\n");
#ifdef DPARALLEL_STACK_RECURSION
    printf("   -i  int   work stealing polling interval\n");
#else
    printf("   -i  int   set cancellable barrier polling interval\n");
#endif
#if !defined(PARALLEL_RECURSION) && !defined(DPARALLEL_RECURSION)
    printf("   -S  int   set initial stack size\n");
#endif
#ifdef __BERKELEY_UPC__
    printf("   -I  int   set working bupc_poll() interval\n");
#endif
#ifdef __PTHREADS__
    printf("   -T  int   set number of threads\n");
#endif
#ifdef PARALLEL_RECURSION
    printf("   -T  int   set number of tasks per thread\n");
    printf("   -P  int   partitioner type (0: custom, 1: simple, 2: automatic)\n");
    printf("   -U  int   set height to process serial (only for custom partitioner)\n");
#endif
#ifdef _CILK_OPT
    printf("   -U  int   set height to process serial\n");
#endif
#ifdef DPARALLEL_RECURSION
    printf("   -T  int   set number of tasks per thread\n");
    printf("   -F  int   set dparallel_recursion additional flags\n");
    printf("   -P  int   partitioner type (0: custom, 1: simple, 2: automatic)\n");
    printf("   -U  int   set height to process serial (only for custom partitioner)\n");
#endif
#ifdef PARALLEL_STACK_RECURSION
    printf("   -P  int   partitioner type (0: custom, 1: simple, 2: automatic)\n");
    printf("   -U  int   set height to process serial (only for custom partitioner)\n");
    printf("   -Z  int   set max time for tests in miliseconds (only for chunkSize=0)\n");
#endif
#ifdef DPARALLEL_STACK_RECURSION
    printf("   -F  int   set dparallel_stack_recursion additional flags\n");
    printf("   -C  int   set dparallel_stack_recursion minimum number of chunks per MPI steal\n");
    printf("   -M  int   set dparallel_stack_recursion thread policy mode (-1: aggressive; 0: multiple; 1: predictive)\n");
    printf("   -L  int   set dparallel_stack_recursion limit of MPI work requests allowed simultaneously for a process\n");
    printf("   -W  int   set dparallel_stack_recursion number of work requested per thread predictively (only used with -M predictive)\n");
    printf("   -P  int   partitioner type (0: custom, 1: simple, 2: automatic)\n");
    printf("   -U  int   set height to process serial (only for custom partitioner)\n");
    printf("   -Z  int   set max time for tests in miliseconds (only for chunkSize=0)\n");
#endif
  } else {
    printf("   -S  int   set initial stack size\n");
#ifdef UTS_STAT
    printf("   -u  int   unbalance measure (-1: none; 0: min/size; 1: min/n; 2: max/n)\n");
#else
    printf("   none.\n");
#endif
  }
}

void impl_abort(int err) {
#if defined(__UPC__)
  upc_global_exit(err);
#elif defined(_OPENMP)
  exit(err);
#elif defined(_SHMEM)
  exit(err);
#else
  exit(err);
#endif
}


/***********************************************************
 *                                                         *
 *  FUNCTIONS                                              *
 *                                                         *
 ***********************************************************/

/* 
 * StealStack
 *    Stack of nodes with sharing at the bottom of the stack
 *    and exclusive access at the top for the "owning" thread 
 *    which has affinity to the stack's address space.
 *
 *    * All operations on the shared portion of the stack
 *      must be guarded using the stack-specific lock.
 *    * Elements move between the shared and exclusive
 *      portion of the stack solely under control of the 
 *      owning thread. (ss_release and ss_acquire)
 *    * workAvail is the count of elements in the shared
 *      portion of the stack.  It may be read without 
 *      acquiring the stack lock, but of course its value
 *      may not be acurate.  Idle threads read workAvail in
 *      this speculative fashion to minimize overhead to 
 *      working threads.
 *    * Elements can be stolen from the bottom of the shared
 *      portion by non-owning threads.  The values are 
 *      reserved under lock by the stealing thread, and then 
 *      copied without use of the lock (currently space for
 *      reserved values is never reclaimed).
 *
 */

/* restore stack to empty state */
void ss_mkEmpty(StealStack *s) {
  SET_LOCK(s->stackLock);
  s->sharedStart = 0;
  s->local  = 0;
  s->top    = 0;
  s->workAvail = 0;
  UNSET_LOCK(s->stackLock);
}

/* fatal error */
void ss_error(const char *str) {
  printf("*** [Thread %i] %s\n",GET_THREAD_NUM, str);
#if defined(DPARALLEL_RECURSION) || defined(DPARALLEL_STACK_RECURSION)
  MPI_Abort(MPI_COMM_WORLD, 4);
#endif
  exit(4);
}

/* initialize the stack */
void ss_init(StealStack *s, const int nelts) {
  int nbytes = nelts * sizeof(Node);

  if (debug & 1)
    printf("Thread %d intializing stealStack %p, sizeof(Node) = %X\n", 
           GET_THREAD_NUM, s, (int)(sizeof(Node)));

  // allocate stack in shared addr space with affinity to calling thread
  // and record local addr for efficient access in sequel
  s->stack_g = (SHARED_INDEF Node *) ALLOC (nbytes);
  s->stack = (Node *) s->stack_g;
#ifdef TRACE
  s->md = (MetaData *) ALLOC (sizeof(MetaData));
  if (s->md == NULL)
    ss_error("ss_init: out of memory");
#endif
  if (s->stack == NULL) {
    printf("Request for %d bytes for stealStack on thread %d failed\n",
           nbytes, GET_THREAD_NUM);
    ss_error("ss_init: unable to allocate space for stealstack");
  }
  INIT_LOCK(s->stackLock);
  if (debug & 1)
    printf("Thread %d init stackLock %p\n", GET_THREAD_NUM, (void *) s->stackLock);
  s->stackSize = nelts;
  s->nNodes = 0;
  s->maxTreeDepth = 0;
  s->nLeaves = 0;
#ifdef PROFILE
  s->maxStackDepth = 0;
  s->nAcquire = 0;
  s->nRelease = 0;
  s->nSteal = 0;
  s->nFail = 0;
  s->wakeups = 0;
  s->falseWakeups = 0;
  s->nNodes_last = 0;
#endif //PROFILE
  ss_mkEmpty(s);
}

#ifndef UTS_DENY_STACK_RESIZE
/* local double the size of the stack */
void ss_doubleStackSize(StealStack *s) {
	SET_LOCK(s->stackLock);
	s->stackSize *= 2;
	s->stack_g = (SHARED_INDEF Node *) REALLOC (s->stack_g, s->stackSize*sizeof(Node));
	if (s->stack_g == NULL) {
		printf("Request for %d elements for stealStack failed\n", s->stackSize);
		printf("ss_doubleStackSize: unable to allocate space for stealstack\n");
		exit(4);
	};
	s->stack = (Node *) s->stack_g;
	UNSET_LOCK(s->stackLock);
}
#endif

/* local push */
void ss_push(StealStack *s, Node *c) {
#ifdef UTS_DENY_STACK_RESIZE
  if (s->top >= s->stackSize) {
    ss_error("ss_push: overflow");
  }
#else
  while(s->top >= s->stackSize) {
    ss_doubleStackSize(s);
  }
#endif

  if (debug & 1)
    printf("ss_push: Thread %d, posn %d: node %s [%d]\n",
           GET_THREAD_NUM, s->top, rng_showstate(c->state.state, debug_str), c->height);
  memcpy(&(s->stack[s->top]), c, sizeof(Node));
  s->top++;
  s->nNodes++;
  PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
  s->maxTreeDepth = std::max(s->maxTreeDepth, c->height);
}

/* local push */
void ss_push_p(StealStack *s, Node *c, Node **parent, const int parent_index) {
#ifdef UTS_DENY_STACK_RESIZE
  if (s->top >= s->stackSize) {
    ss_error("ss_push: overflow");
  }
#else
  while(s->top >= s->stackSize) {
    ss_doubleStackSize(s);
    *parent = &(s->stack[parent_index]);
  }
#endif

  if (debug & 1)
    printf("ss_push: Thread %d, posn %d: node %s [%d]\n",
           GET_THREAD_NUM, s->top, rng_showstate(c->state.state, debug_str), c->height);
  memcpy(&(s->stack[s->top]), c, sizeof(Node));
  s->top++;
  s->nNodes++;
  s->maxStackDepth = std::max(s->top, s->maxStackDepth);
  s->maxTreeDepth = std::max(s->maxTreeDepth, c->height);
}

/* local top:  get local addr of node at top */ 
Node * ss_top(StealStack *s) {
  Node *r;
  if (s->top <= s->local)
    ss_error("ss_top: empty local stack");
  r = &(s->stack[(s->top) - 1]);
  if (debug & 1) 
    printf("ss_top: Thread %d, posn %d: node %s [%d] nchild = %d\n",
           GET_THREAD_NUM, s->top - 1, rng_showstate(r->state.state, debug_str),
           r->height, r->numChildren);
  return r;
}

/* local pop */
void ss_pop(StealStack *s) {
  Node *r;
  if (s->top <= s->local)
    ss_error("ss_pop: empty local stack");
  s->top--;
  r = &(s->stack[s->top]);
  if (debug & 1)
    printf("ss_pop: Thread %d, posn %d: node %s [%d] nchild = %d\n",
           GET_THREAD_NUM, s->top, rng_showstate(r->state.state, debug_str), 
           r->height, r->numChildren);
}
  
/* local top position:  stack index of top element */
int ss_topPosn(StealStack *s)
{
  if (s->top <= s->local)
    ss_error("ss_topPosn: empty local stack");
  return s->top - 1;
}

/* local depth */
int ss_localDepth(StealStack *s) {
  return (s->top - s->local);
}

/* release k values from bottom of local stack */
void ss_release(StealStack *s, const int k) {
  SET_LOCK(s->stackLock);
  if (s->top - s->local >= k) {
    s->local += k;
    s->workAvail += k;
    PROFILEACTION(s->nRelease++);
  }
  else
    ss_error("ss_release:  do not have k vals to release");
  UNSET_LOCK(s->stackLock);
}

/* move k values from top of shared stack into local stack
 * return false if k vals are not avail on shared stack
 */
int ss_acquire(StealStack *s, const int k) {
  int avail;
  SET_LOCK(s->stackLock);
  avail = s->local - s->sharedStart;
  if (avail >= k) {
    s->local -= k;
    s->workAvail -= k;
    PROFILEACTION(s->nAcquire++);
  }
  UNSET_LOCK(s->stackLock);
  return (avail >= k);
}

/* steal k values from shared portion of victim thread's stealStack
 * onto local portion of current thread's stealStack.
 * return false if k vals are not avail in victim thread
 */
int ss_steal(StealStack *s, const int victim, const int k) {
  int victimLocal, victimShared, victimWorkAvail;
  int ok;
  
  if (s->sharedStart != s->top)
    ss_error("ss_steal: thief attempts to steal onto non-empty stack");

#ifdef UTS_DENY_STACK_RESIZE
  if (s->top + k >= s->stackSize) {
    ss_error("ss_steal: steal will overflow thief's stack");
  }
#else
  while(s->top + k >= s->stackSize) {
    ss_doubleStackSize(s);
  }
#endif
  
  /* lock victim stack and try to reserve k elts */
  if (debug & 32)
    printf("Thread %d wants    SS %d\n", GET_THREAD_NUM, victim);
  
  SET_LOCK(stealStack[victim]->stackLock);
  
#ifdef _SHMEM
  /* Get remote steal stack */
  SMEMCPY(stealStack[victim], stealStack[victim], sizeof(StealStack), victim);
#endif

  if (debug & 32)
    printf("Thread %d acquires SS %d\n", GET_THREAD_NUM, victim);
  
  victimLocal = stealStack[victim]->local;
  victimShared = stealStack[victim]->sharedStart;
  victimWorkAvail = stealStack[victim]->workAvail;
  
  if (victimLocal - victimShared != victimWorkAvail)
    ss_error("ss_steal: stealStack invariant violated");
  
  ok = victimWorkAvail >= k;
  if (ok) {
    /* reserve a chunk */
    stealStack[victim]->sharedStart =  victimShared + k;
    stealStack[victim]->workAvail = victimWorkAvail - k;

#ifdef _SHMEM
    // FIXME: These transfers ought to be combined.  They can't be
    // though because the data protected by the stacklock is not
    // the only data in the StealStack structure.
    PUT(stealStack[victim]->sharedStart, stealStack[victim]->sharedStart, victim);
    PUT(stealStack[victim]->workAvail, stealStack[victim]->workAvail, victim);
#endif
  }
  
  if (debug & 32)
    printf("Thread %d releases SS %d\n", GET_THREAD_NUM, victim);
	
  /* if k elts reserved, move them to local portion of our stack */
  if (ok) {
    SHARED_INDEF Node * victimStackBase = stealStack[victim]->stack_g;
    SHARED_INDEF Node * victimSharedStart = victimStackBase + victimShared;

#ifdef _SHMEM
    SMEMCPY(&(s->stack[s->top]), victimSharedStart, k * sizeof(Node), victim);
#else
    SMEMCPY(&(s->stack[s->top]), victimSharedStart, k * sizeof(Node));
#endif
    UNSET_LOCK(stealStack[victim]->stackLock);

    PROFILEACTION(s->nSteal++);
    if (debug & 4) {
      int i;
      for (i = 0; i < k; i ++) {
        PROFILEDEFINITION(Node * r = &(s->stack[s->top + i]));
        PROFILEACTION(printf("ss_steal:  Thread %2d posn %d (steal #%d) receives %s [%d] from thread %d posn %d (%p)\n",
               GET_THREAD_NUM, s->top + i, s->nSteal,
               rng_showstate(r->state.state, debug_str),
               r->height, victim, victimShared + i, 
               (void *) victimSharedStart));
      }
    }
    s->top += k;

   #ifdef TRACE
      /* update session record of theif */
      s->md->stealRecords[s->entries[SS_WORK]].victimThread = victim;
   #endif
  } else {
    UNSET_LOCK(stealStack[victim]->stackLock);
    PROFILEACTION(s->nFail++);
    if (debug & 4) {
      printf("Thread %d failed to steal %d nodes from thread %d, ActAv = %d, sh = %d, loc =%d\n",
	     GET_THREAD_NUM, k, victim, victimWorkAvail, victimShared, victimLocal);
    }
  }
  return (ok);
} 

/* search other threads for work to steal */
int findwork(int k) {
  int i,v;
  for (i = 1; i < GET_NUM_THREADS; i++) {
    v = (GET_THREAD_NUM + i) % GET_NUM_THREADS;
#ifdef _SHMEM
    GET(stealStack[v]->workAvail, stealStack[v]->workAvail, v);
#endif
    if (stealStack[v]->workAvail >= k)
      return v;
  }
  return -1;
}

#ifdef PROFILE
/**
 *  Tracing functions
 *   Track changes in the search state for offline analysis.
**/
void ss_initState(StealStack *s) {
  int i;
  s->timeLast = uts_wctime();
  for (i = 0; i < SS_NSTATES; i++) {
    s->time[i] = 0.0;
    s->entries[i] = 0;
  }
  s->curState = SS_IDLE;
  if (debug & 8)
    printf("Thread %d start state %d (t = %f)\n", 
           GET_THREAD_NUM, s->curState, s->timeLast);
}

void ss_setState(StealStack *s, const int state){
  double time;
  if (state < 0 || state >= SS_NSTATES)
    ss_error("ss_setState: thread state out of range");
  if (state == s->curState)
    return;
  time = uts_wctime();
  s->time[s->curState] +=  time - s->timeLast;

  #ifdef TRACE  
    /* close out last session record */
    s->md->sessionRecords[s->curState][s->entries[s->curState] - 1].endTime = time;
    if (s->curState == SS_WORK)
    {
       s->md->stealRecords[s->entries[SS_WORK] - 1].nodeCount = s->nNodes
           - s->md->stealRecords[s->entries[SS_WORK] - 1].nodeCount;
    }

    /* initialize new session record */
    s->md->sessionRecords[state][s->entries[state]].startTime = time;
    if (state == SS_WORK)
    {
       s->md->stealRecords[s->entries[SS_WORK]].nodeCount = s->nNodes;
    }
  #endif

  s->entries[state]++;
  s->timeLast = time;
  s->curState = state;

  if(debug & 8)
    printf("Thread %d enter state %d [#%d] (t = %f)\n",
           GET_THREAD_NUM, state, s->entries[state], time);
}
#endif //PROFILE

#ifdef UTS_STAT
/*
 * Statistics, 
 * : number of nodes per level
 * : imbalanceness of nodes per level
 *
 */
void initHist()
{
  int i;
  for (i=0; i<MAXHISTSIZE; i++){
    hist[i][0]=0;
    hist[i][1]=0;
    unbhist[i][1]=1;
    unbhist[i][2]=0;
  }
}

void updateHist(Node* c, double unb)
{
  if (c->height<MAXHISTSIZE){
    hist[c->height][1]++;
    hist[c->height][0]+=c->numChildren;

    unbhist[c->height][0]+=unb;
    if (unbhist[c->height][1]>unb)
      unbhist[c->height][1]=unb;
    if (unbhist[c->height][2]<unb)
      unbhist[c->height][2]=unb;
		
  }
  else {
    hist[MAXHISTSIZE][1]++;
    hist[MAXHISTSIZE][0]+=c->numChildren;
  }
}

void showHist(FILE *fp)
{
  int i;	
  fprintf(fp, "depth\tavgNumChildren\t\tnumChildren\t imb\t maxImb\t minImb\t\n");
  for (i=0; i<MAXHISTSIZE; i++){
    if ((hist[i][0]!=0)&&(hist[i][1]!=0))
      fprintf(fp, "%d\t%f\t%d\t %lf\t%lf\t%lf\n", i, (double)hist[i][0]/hist[i][1], 
              hist[i][0], unbhist[i][0]/hist[i][1], unbhist[i][1], unbhist[i][2]);	
  }
}

double getImb(Node *c)
{
  int i=0;
  double avg=.0, tmp=.0;
  double unb=0.0;
  
  avg=(double)c->sizeChildren/c->numChildren;

  for (i=0; i<c->numChildren; i++){		
    if ((type==BIN)&&(c->pp==NULL))
      {
        if (unbType<2)
          tmp=std::min((double)rootSize[i]/avg, avg/(double)rootSize[i]);
        else 
          tmp=std::max((double)rootSize[i]/avg, avg/(double)rootSize[i]);
        
        if (unbType>0)
          unb+=tmp*rootUnb[i];
        else 
          unb+=tmp*rootUnb[i]*rootSize[i];
      }	
    else{
      if (unbType<2)
        tmp=std::min((double)c->size[i]/avg, avg/(double)c->size[i]);
      else 
        tmp=std::max((double)c->size[i]/avg, avg/(double)c->size[i]);
      
      if (unbType>0)
        unb+=tmp*c->unb[i];
      else 
        unb+=tmp*c->unb[i]*c->size[i];
    }
  }
	
  if (unbType>0){
    if (c->numChildren>0) 
      unb=unb/c->numChildren;
    else unb=1.0;
  }
  else {
    if (c->sizeChildren>1) 
      unb=unb/c->sizeChildren;
    else unb=1.0;
  }
  if ((debug & 1) && unb>1) printf("unb>1%lf\t%d\n", unb, c->numChildren);
	
  return unb;
}

void getImb_Tseng(Node *c)
{
  double t_max, t_avg, t_devmaxavg, t_normdevmaxavg;

  if (c->numChildren==0)
    {
      t_avg =0;
      t_max =0;
    }
  else 
    {
      t_max = (double)c->maxSizeChildren/(c->sizeChildren-1);
      t_avg = (double)1/c->numChildren;
    }

  t_devmaxavg = t_max-t_avg;
	
  if (debug & 1)
    printf("max\t%lf, %lf, %d, %d, %d\n", t_max, t_avg, 
           c->maxSizeChildren, c->sizeChildren, c->numChildren);
	
  if (1-t_avg==0)
    t_normdevmaxavg = 1;
  else
    t_normdevmaxavg = (t_max-t_avg)/(1-t_avg);

  imb_max += t_max;
  imb_avg += t_avg;
  imb_devmaxavg += t_devmaxavg;
  imb_normdevmaxavg +=t_normdevmaxavg;
}

void updateParStat(Node *c)
{
  double unb;

  totalNodes++;
  if (maxHeight<c->height) 
    maxHeight=c->height;
	
  unb=getImb(c);
  maxImb=std::max(unb, maxImb);
  minImb=std::min(unb, minImb);
  updateHist(c, unb);
  
  getImb_Tseng(c);
	
  if (c->pp!=NULL){
    if ((c->type==BIN)&&(c->pp->pp==NULL)){
      rootSize[c->pp->ind]=c->sizeChildren;
      rootUnb[c->pp->ind]=unb;
    }
    else{
      c->pp->size[c->pp->ind]=c->sizeChildren;
      c->pp->unb[c->pp->ind]=unb;
    }
    /* update statistics per node*/
    c->pp->ind++;
    c->pp->sizeChildren+=c->sizeChildren;
    if (c->pp->maxSizeChildren<c->sizeChildren)
      c->pp->maxSizeChildren=c->sizeChildren;		
  }
  else 
    treeImb = unb;
}
#endif

/*
 *	Tree Implementation      
 *
 */
void initNode(Node * child)
{
  child->type = -1;
  child->height = -1;
  child->numChildren = -1;    // not yet determined

#ifdef UTS_STAT
  if (stats){	
    int i;
    child->ind = 0;
    child->sizeChildren = 1;
    child->maxSizeChildren = 0;
    child->pp = NULL;
    for (i = 0; i < MAXNUMCHILDREN; i++){
      child->size[i] = 0;
      child->unb[i]  = 0.0;
    }
  }
#endif
}


void initRootNode(Node * root, int type)
{
  uts_initRoot(root, type);

  #ifdef TRACE
    stealStack[0]->md->stealRecords[0].victimThread = 0;  // first session is own "parent session"
  #endif

#ifdef UTS_STAT
  if (stats){
    int i;
    root->ind = 0;
    root->sizeChildren = 1;
    root->maxSizeChildren = 1;
    root->pp = NULL;
    
    if (type != BIN){
      for (i=0; i<MAXNUMCHILDREN; i++){
        root->size[i] = 0;
        root->unb[i]  =.0; 
      }
    }
    else {
      int rbf = (int) ceil(b_0);
      rootSize = malloc(rbf*sizeof(int));
      rootUnb = malloc(rbf*sizeof(double));
      for (i = 0; i < rbf; i++) {
        rootSize[i] = 0;
        rootUnb[i] = 0.0;
      }
    }
  }
#endif
}


// forward decl
void releaseNodes(StealStack *ss);

/* 
 * Generate all children of the parent
 *
 * details depend on tree type, node type and shape function
 *
 */
void genChildren(Node * parent, Node * child, StealStack * ss) {
  int parentHeight = parent->height;
  int numChildren, childType;

  numChildren = uts_numChildren(parent);
  childType   = uts_childType(parent);

  // record number of children in parent
  parent->numChildren = numChildren;
  if (debug & 2) {
    printf("Gen:  Thread %d, posn %2d: node %s [%d] has %2d children\n",
           GET_THREAD_NUM, ss_topPosn(ss),
           rng_showstate(parent->state.state, debug_str), 
           parentHeight, numChildren);
  }
  
  // construct children and push onto stack
  if (numChildren > 0) {
    int i, j;
    child->type = childType;
    child->height = parentHeight + 1;

#ifdef UTS_STAT
    if (stats)
      child->pp = parent;  // pointer to parent
#endif

    for (i = 0; i < numChildren; i++) {
      for (j = 0; j < computeGranularity; j++) {
        // TBD:  add parent height to spawn
        // computeGranularity controls number of rng_spawn calls per node
        rng_spawn(parent->state.state, child->state.state, i);
      }
#ifdef UTS_DENY_STACK_RESIZE
      ss_push(ss, child);
#else
      ss_push_p(ss, child, &parent, ss->top-i-1);
#endif
      releaseNodes(ss);
    }
  } else {
    ss->nLeaves++;
  }
}


#ifdef _CILK
Result_t genChildrenCilk(Node * parent) {
	int nNodes = 1;
	int nLeaves = 0;
	int mheight = 0;
	if (parent->numChildren > 0) {
		_Cilk_for (int i = 0; i < parent->numChildren; ++i) {
			Node child;
			child.type = uts_childType(parent); // n.type; //OPT1: Instead of uts_childType(&n);
			child.height = parent->height + 1;

			for (int j = 0; j < computeGranularity; j++) {
				rng_spawn(parent->state.state, child.state.state, i);
			}
			child.numChildren = uts_numChildren(&child);

			mheight = std::max(mheight, child.height);

			Result_t ret = genChildrenCilk(&child);

			nNodes += ret.tnodes;
			nLeaves += ret.tleaves;
			mheight = std::max(mheight, ret.mheight);
		}
	} else {
		nLeaves++;
	}
	return {nNodes, nLeaves, mheight};
}
#elif _CILK_OPT
void genChildrenCilk_seq(Node * parent, int& nNodes, int& nLeaves, int& mheight) {
	if (parent->numChildren > 0) {
		for (int i = 0; i < parent->numChildren; ++i) {
			Node child;
			child.type = uts_childType(parent);
			child.height = parent->height + 1;

			for (int j = 0; j < computeGranularity; j++) {
				rng_spawn(parent->state.state, child.state.state, i);
			}
			child.numChildren = uts_numChildren(&child);

			mheight = std::max(mheight, child.height);
			genChildrenCilk_seq(&child, nNodes, nLeaves, mheight);
		}
		nNodes += parent->numChildren;
	} else {
		nLeaves++;
	}
}

Result_t genChildrenCilk(Node * parent) {
	int nNodes = 1;
	int nLeaves = 0;
	int mheight = 0;
	if (parent->numChildren > 0) {
		_Cilk_for (int i = 0; i < parent->numChildren; ++i) {
			Node child;
			child.type = uts_childType(parent);
			child.height = parent->height + 1;

			for (int j = 0; j < computeGranularity; j++) {
				rng_spawn(parent->state.state, child.state.state, i);
			}
			child.numChildren = uts_numChildren(&child);

			mheight = std::max(mheight, child.height);
			if (child.height < height_to_serial) {
				Result_t ret = genChildrenCilk(&child);
				nNodes += ret.tnodes;
				nLeaves += ret.tleaves;
				mheight = std::max(mheight, ret.mheight);
			} else {
				genChildrenCilk_seq(&child, nNodes, nLeaves, mheight);
			}
		}
	} else {
		nLeaves++;
	}
	return {nNodes, nLeaves, mheight};
}
#endif


/*
 *  Parallel tree traversal
 *
 */

// cancellable barrier

// initialize lock:  single thread under omp, all threads under upc
void cb_init(){
  INIT_SINGLE_LOCK(cb_lock);
  if (debug & 4)
    printf("Thread %d, cb lock at %p\n", GET_THREAD_NUM, (void *) cb_lock);

  // fixme: no need for all upc threads to repeat this
  SET_LOCK(cb_lock);
  cb_count = 0;
  cb_cancel = 0;
  cb_done = 0;
  UNSET_LOCK(cb_lock);
}

//  delay this thread until all threads arrive at barrier
//     or until barrier is cancelled
int cbarrier_wait() {
  int l_count, l_done, l_cancel;

  SET_LOCK(cb_lock);
  cb_count++;
#ifdef _SHMEM
  PUT_ALL(cb_count, cb_count);
#endif
  if (cb_count == GET_NUM_THREADS) {
    cb_done = 1;
#ifdef _SHMEM
    PUT_ALL(cb_done, cb_done);
#endif
  }
  l_count = cb_count;
  l_done = cb_done;

#ifdef PROFILE
  int pe = GET_THREAD_NUM;
  if (stealStack[pe]->nNodes_last == stealStack[pe]->nNodes) {
    ++stealStack[pe]->falseWakeups;
  }
  stealStack[GET_THREAD_NUM]->nNodes_last = stealStack[pe]->nNodes;
#endif //PROFILE

  UNSET_LOCK(cb_lock);

  if (debug & 16)
    printf("Thread %d enter spin-wait, count = %d, done = %d\n",
           GET_THREAD_NUM, l_count, l_done);

  // spin
  do {
#ifdef __BERKELEY_UPC__
    bupc_poll();
#endif
    l_count = cb_count;
    l_cancel = cb_cancel;
    l_done = cb_done;
  }
  while (!l_cancel && !l_done);

  if (debug & 16)
    printf("Thread %d exit  spin-wait, count = %d, done = %d, cancel = %d\n",
           GET_THREAD_NUM, l_count, l_done, l_cancel);


  SET_LOCK(cb_lock);
  cb_count--;
  l_count = cb_count;
#ifdef _SHMEM
  PUT_ALL(cb_count, cb_count);
#endif
  cb_cancel = 0;
  l_done = cb_done;
  PROFILEACTION(++stealStack[GET_THREAD_NUM]->wakeups);
  UNSET_LOCK(cb_lock);

  if (debug & 16)
    printf("Thread %d exit idle state, count = %d, done = %d\n",
           GET_THREAD_NUM, l_count, cb_done);

  return cb_done;
}

// causes one or more threads waiting at barrier, if any,
//  to be released
void cbarrier_cancel() {
#ifdef _SHMEM
  cb_cancel = 1;
  PUT_ALL(cb_cancel, cb_cancel);
#elif defined (__BERKELEY_UPC__)
  bupc_waitsync(cb_handle);
  cb_handle = bupc_memput_async((shared void*)&cb_cancel, (const void*)&local_cb_cancel, sizeof(int));
#else
  cb_cancel = 1;
#endif /* _SHMEM */
}

void releaseNodes(StealStack *ss){
  if (doSteal) {
    if (ss_localDepth(ss) > 2 * chunkSize) {
      // Attribute this time to runtime overhead
      PROFILEACTION(ss_setState(ss, SS_OVH));
      ss_release(ss, chunkSize);
      // This has significant overhead on clusters!
      if (ss->nNodes % cbint == 0) {
        PROFILEACTION(ss_setState(ss, SS_CBOVH));
        cbarrier_cancel();
      }

#ifdef __BERKELEY_UPC__
      if (ss->nNodes % pollint == 0) {
        PROFILEACTION(ss_setState(ss, SS_OVH));
        bupc_poll();
      }
#endif
      PROFILEACTION(ss_setState(ss, SS_WORK));
    }
  }
}

/* 
 * parallel search of UTS trees using work stealing 
 * 
 *   Note: tree size is measured by the number of
 *         push operations
 */
void parTreeSearch(StealStack *ss) {
  int done = 0;
  Node * parent;
  Node child;

  /* template for children */
  initNode(&child);

  /* tree search */
  while (done == 0) {
    
    /* local work */
    while (ss_localDepth(ss) > 0) {		

      PROFILEACTION(ss_setState(ss, SS_WORK));

      /* examine node at stack top */
      parent = ss_top(ss);
      if (parent->numChildren < 0){
        // first time visited, construct children and place on stack
        genChildren(parent,&child,ss);
      } else {
	    // second time visit, process accumulated statistics and pop
#ifdef UTS_STAT
        if (stats)
          updateParStat(parent);
#endif
        ss_pop(ss);
      }
      
      // release some nodes for stealing, if enough are available
      // and wake up quiescent threads
      releaseNodes(ss);
    }
		
    /* local work exhausted on this stack - resume tree search if able
     * to re-acquire work from shared portion of this thread's stack
     */
    if (ss_acquire(ss, chunkSize))
      continue;

    /* no work left in this thread's stack           */
    /* try to steal work from another thread's stack */
    if (doSteal) {
      int goodSteal = 0;
      int victimId;
      
      PROFILEACTION(ss_setState(ss, SS_SEARCH));
      victimId = findwork(chunkSize);
      while (victimId != -1 && !goodSteal) {
	// some work detected, try to steal it
	goodSteal = ss_steal(ss, victimId, chunkSize);
	if (!goodSteal)
	  victimId = findwork(chunkSize);
      }
      if (goodSteal)
	  continue;
    }
	
    /* unable to steal work from shared portion of other stacks -
     * enter quiescent state waiting for termination (done != 0)
     * or cancellation because some thread has made work available
     * (done == 0).
     */
    PROFILEACTION(ss_setState(ss, SS_IDLE));
    done = cbarrier_wait();
  }
  
  /* tree search complete ! */
}

#ifdef _CILK
Result_t parTreeSearchCilk(Node * parent) {
	parent->numChildren = uts_numChildren(parent);
	Result_t ret;
	ret = genChildrenCilk(parent);
	return ret;
}
#elif _CILK_OPT
Result_t parTreeSearchCilk(Node * parent) {
	parent->numChildren = uts_numChildren(parent);
	Result_t ret;
	if (parent->height < height_to_serial) {
		ret = genChildrenCilk(parent);
	} else {
		genChildrenCilk_seq(parent, ret.tnodes, ret.tleaves, ret.mheight);
	}
	return ret;
}
#endif

#ifdef __PTHREADS__
/* Pthreads ParTreeSearch Arguments */
struct pthread_args {
	StealStack *ss;
	int         id;
};

/* Pthreads ParTreeSearch Wrapper */
void * pthread_spawn_search(void *arg)
{
	pthread_setspecific(pthread_thread_num, &((struct pthread_args*)arg)->id);
	parTreeSearch(((struct pthread_args*)arg)->ss);
	return NULL;
}
#endif /* __PTHREADS__ */


#ifdef TRACE
// print session records for each thread (used when trace is enabled)
void printSessionRecords()
{
  int i, j, k;
  double offset;

  for (i = 0; i < GET_NUM_THREADS; i++) {
    offset = startTime[i] - startTime[0];

    for (j = 0; j < SS_NSTATES; j++)
       for (k = 0; k < stealStack[i]->entries[j]; k++) {
          printf ("%d %d %f %f", i, j,
            stealStack[i]->md->sessionRecords[j][k].startTime - offset,
            stealStack[i]->md->sessionRecords[j][k].endTime - offset);
          if (j == SS_WORK)
            printf (" %d %ld",
              stealStack[i]->md->stealRecords[k].victimThread,
              stealStack[i]->md->stealRecords[k].nodeCount);
            printf ("\n");
     }
  }
}
#endif

// display search statistics
void showStats(double elapsedSecs) {
  int i;
  int tnodes = 0, tleaves = 0, trel = 0, tacq = 0, tsteal = 0, tfail= 0;
  int mdepth = 0, mheight = 0;
  double twork = 0.0, tsearch = 0.0, tidle = 0.0, tovh = 0.0, tcbovh = 0.0;

#ifdef _SHMEM
  {
    int pe;
    /* Assemble all of the stealstacks so we can gather some stats. */
    for (i = 1; i < GET_NUM_THREADS; i++) {
      pe = (GET_THREAD_NUM + i) % GET_NUM_THREADS;
      /* Collect up all of the StealStacks */
      SMEMCPY(stealStack[pe], stealStack[pe], sizeof(StealStack), pe);
#ifdef TRACE
      /* Get the MetaData as well */
      SMEMCPY(stealStack[pe]->md, stealStack[pe]->md, sizeof(StealStack), pe);
#endif
    }
  }
#endif

  const int num_steal_stacks =
#ifdef PARALLEL_RECURSION
  1;
#elif DPARALLEL_RECURSION
  1;
#elif PARALLEL_STACK_RECURSION
  1;
#elif DPARALLEL_STACK_RECURSION
  1;
#elif _CILK
  1;
#elif _CILK_OPT
  1;
#else
  GET_NUM_THREADS;
#endif
  
  // combine measurements from all threads
  for (i = 0; i < num_steal_stacks; i++) {
    tnodes  += stealStack[i]->nNodes;
    tleaves += stealStack[i]->nLeaves;
#ifdef PROFILE
    trel    += stealStack[i]->nRelease;
    tacq    += stealStack[i]->nAcquire;
    tsteal  += stealStack[i]->nSteal;
    tfail   += stealStack[i]->nFail;
    twork   += stealStack[i]->time[SS_WORK];
    tsearch += stealStack[i]->time[SS_SEARCH];
    tidle   += stealStack[i]->time[SS_IDLE];
    tovh    += stealStack[i]->time[SS_OVH];
    tcbovh  += stealStack[i]->time[SS_CBOVH];
    mdepth   = std::max(mdepth, stealStack[i]->maxStackDepth);
#endif //PROFILE
    mheight  = std::max(mheight, stealStack[i]->maxTreeDepth);
  }
  if (trel != tacq + tsteal) {
    printf("*** error! total released != total acquired + total stolen\n");
  }
    
  uts_showStats(GET_NUM_THREADS, chunkSize, elapsedSecs, tnodes, tleaves, mheight);

  if (verbose > 1) {
    if (doSteal) {
      printf("Total chunks released = %d, of which %d reacquired and %d stolen\n",
          trel, tacq, tsteal);
      printf("Failed steal operations = %d, ", tfail);
    }

    printf("Max stealStack size = %d\n", mdepth);
    printf("Avg time per thread: Work = %.6f, Search = %.6f, Idle = %.6f\n", (twork / GET_NUM_THREADS),
        (tsearch / GET_NUM_THREADS), (tidle / GET_NUM_THREADS));
    printf("                     Overhead = %6f, CB_Overhead = %6f\n\n", (tovh / GET_NUM_THREADS),
        (tcbovh/GET_NUM_THREADS));
  }

  // per thread execution info
  if (verbose > 2) {
    for (i = 0; i < GET_NUM_THREADS; i++) {
      printf("** Thread %d\n", i);
      printf("  # nodes explored    = %d\n", stealStack[i]->nNodes);
#ifdef PROFILE
      printf("  # chunks released   = %d\n", stealStack[i]->nRelease);
      printf("  # chunks reacquired = %d\n", stealStack[i]->nAcquire);
      printf("  # chunks stolen     = %d\n", stealStack[i]->nSteal);
      printf("  # failed steals     = %d\n", stealStack[i]->nFail);
      printf("  maximum stack depth = %d\n", stealStack[i]->maxStackDepth);
      printf("  work time           = %.6f secs (%d sessions)\n",
             stealStack[i]->time[SS_WORK], stealStack[i]->entries[SS_WORK]);
      printf("  overhead time       = %.6f secs (%d sessions)\n",
             stealStack[i]->time[SS_OVH], stealStack[i]->entries[SS_OVH]);
      printf("  search time         = %.6f secs (%d sessions)\n",
             stealStack[i]->time[SS_SEARCH], stealStack[i]->entries[SS_SEARCH]);
      printf("  idle time           = %.6f secs (%d sessions)\n",
             stealStack[i]->time[SS_IDLE], stealStack[i]->entries[SS_IDLE]);
      printf("  wakeups             = %d, false wakeups = %d (%.2f%%)",
             stealStack[i]->wakeups, stealStack[i]->falseWakeups,
             (stealStack[i]->wakeups == 0) ? 0.00 : ((((double)stealStack[i]->falseWakeups)/stealStack[i]->wakeups)*100.0));
#endif //PROFILE
      printf("\n");
    }
  }

  #ifdef TRACE
    printSessionRecords();
  #endif

  // tree statistics output to stat.txt, if requested
#ifdef UTS_STAT
  if (stats) {
    FILE *fp;
    char * tmpstr;
    char strBuf[5000];
    int  ind = 0;
    
    fp = fopen("stat.txt", "a+w");
    fprintf(fp, "\n------------------------------------------------------------------------------------------------------\n");
    ind = uts_paramsToStr(strBuf, ind);
    ind = impl_paramsToStr(strBuf, ind);
    //showParametersStr(strBuf);
    fprintf(fp, "%s\n", strBuf);
    
    fprintf(fp, "\nTotal nodes = %d\n", totalNodes); 
    fprintf(fp, "Max depth   = %d\n\n", maxHeight); 
    fprintf(fp, "Tseng ImbMeasure(overall)\n max:\t\t%lf \n avg:\t\t%lf \n devMaxAvg:\t %lf\n normDevMaxAvg: %lf\t\t\n\n", 
            imb_max/totalNodes, imb_avg/totalNodes, imb_devmaxavg/totalNodes, 
            imb_normdevmaxavg/totalNodes);
    
    switch (unbType){
    case 0: tmpstr = "(min imb weighted by size)"; break;
    case 1: tmpstr = "(min imb not weighted by size)"; break;
    case 2: tmpstr = "(max imb not weighted by size)"; break;
    default: tmpstr = "(?unknown measure)"; break;
    }
    fprintf(fp, "ImbMeasure:\t%s\n Overall:\t %lf\n Max:\t\t%lf\n Min:\t\t%lf\n\n", 
            tmpstr, treeImb, minImb, maxImb);
    showHist(fp);
    fprintf(fp, "\n------------------------------------------------------------------------------------------------------\n\n\n");
    fclose(fp);
  }
#endif
}


/* PThreads main() function:
 *   Pthreads is quite a bit different because of how global data has to be stored
 *   using setspecific() and getspecific().  So, many functions are not safe to call
 *   in the single-threaded context.
 */
#ifdef __PTHREADS__
int pthread_main(int argc, char *argv[]) {
  Node   root;
  double t1, t2;
  int    i, err;
  void  *rval;
  struct pthread_args *args;
  pthread_t *thread_ids;

  uts_parseParams(argc, argv);
  uts_printParams();
  cb_init();

  /* allocate stealstacks */
  for (i = 0; i < GET_NUM_THREADS; i++) {
    stealStack[i] = (SharedStealStackPtr)ALLOC (sizeof(StealStack));
    ss_init(stealStack[i], initialStackSize);
  }

  /* initialize root node and push on thread 0 stack */
  uts_initRoot(&root, type);
  ss_push(stealStack[0], &root);

  thread_ids = (pthread_t*)malloc(sizeof(pthread_t)*GET_NUM_THREADS);
  args = (struct pthread_args *)malloc(sizeof(struct pthread_args)*GET_NUM_THREADS);
  pthread_key_create(&pthread_thread_num, NULL);

  /* start timing */
  t1 = uts_wctime();

  for (i = 0; i < GET_NUM_THREADS; i++) {
    PROFILEACTION(ss_initState(stealStack[i]));
    args[i].ss = stealStack[i];
    args[i].id = i;

    err = pthread_create(&thread_ids[i], NULL, pthread_spawn_search, (void*)&args[i]);
    if (err != 0) {
      printf("FATAL: Error spawning thread %d\n", err);
      impl_abort(1);
    }
  }
  for (i = 0; i < GET_NUM_THREADS; i++) {
    pthread_join(thread_ids[i], &rval);
  }

  /* stop timing */
  t2 = uts_wctime();

  showStats(t2-t1);

  return 0;
}
#endif /* __PTHREADS__ */


/*  Main() function for: Sequential, OpenMP, UPC, and Shmem
 *
 *  Notes on execution model:
 *     - under openMP, global vars are all shared
 *     - under UPC, global vars are private unless explicitly shared
 *     - UPC is SPMD starting with main, OpenMP goes SPMD after
 *       parsing parameters
 */
int main(int argc, char *argv[]) {
  Node root;

#ifdef __PTHREADS__
  return pthread_main(argc, argv);
#endif

#ifdef _SHMEM 
  start_pes(0);
#endif

  /* determine benchmark parameters (all PEs) */
  uts_parseParams(argc, argv);

#if defined(PARALLEL_RECURSION)
  if (getenv("OMP_NUM_THREADS")) {
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  }
  ntasks = nthreads * tasks_per_thread;
  dpr::pr_init(nthreads);
#elif defined(PARALLEL_STACK_RECURSION)
  if (getenv("OMP_NUM_THREADS")) {
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  }
  dpr::prs_init(nthreads, initialStackSize);
  opt.verbose = 5;
#elif defined(DPARALLEL_RECURSION)
  int rank, nprocs;
  int provided;
  if (getenv("OMP_NUM_THREADS")) {
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  }
  ntasks = nthreads * tasks_per_thread;
  dpr::pr_init(nthreads);
#elif defined(DPARALLEL_STACK_RECURSION)
  int rank, nprocs;
  int provided;
  if (getenv("OMP_NUM_THREADS")) {
    nthreads = atoi(getenv("OMP_NUM_THREADS"));
  }
  dpr::dprs_init(nthreads, chunksToSteal, (unsigned int)pollint, initialStackSize, threads_request_policy, mpi_workrequest_limits, trp_predict_work_count);
  opt.verbose = 5;
#endif
  
#ifdef UTS_STAT
  if (stats)
    initHist();
#endif  

  /* cancellable barrier initialization (single threaded under OMP) */
  cb_init();

/********** SPMD Parallel Region **********/
#pragma omp parallel
  {
    double t1, t2, et;
    StealStack * ss;    

#ifdef DPARALLEL_RECURSION
	MPI_Init_thread(&argc, &argv, MPI_THREAD_SERIALIZED, &provided);
	assert(provided >= MPI_THREAD_SERIALIZED);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
	/* show parameter settings */
	if ((rank == 0) && (GET_THREAD_NUM == 0)) {
		uts_printParams();
	}
#elif DPARALLEL_STACK_RECURSION
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
    MPI_Init_thread(&(argc), &(argv), MPI_THREAD_SERIALIZED, &provided);
    //assert(provided >= MPI_THREAD_SERIALIZED);
    if (provided < MPI_THREAD_SERIALIZED) {
        printf("*** MPI_THREAD_SERIALIZED not supported, requested '%d' and provided is '%d'\n", MPI_THREAD_SERIALIZED, provided);
        MPI_Abort(MPI_COMM_WORLD, 5);
        exit(5);
    }
#else
    MPI_Init_thread(&(argc), &(argv), MPI_THREAD_MULTIPLE, &provided);
    //assert(provided >= MPI_THREAD_MULTIPLE);
    if (provided < MPI_THREAD_MULTIPLE) {
        printf("*** MPI_THREAD_MULTIPLE not supported, requested '%d' and provided is '%d'\n", MPI_THREAD_MULTIPLE, provided);
        MPI_Abort(MPI_COMM_WORLD, 5);
        exit(5);
    }
#endif
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    dpr::internal_mpi::dpr_rank() = rank;
    dpr::internal_mpi::dpr_nprocs() = nprocs;
	/* show parameter settings */
	if ((rank == 0) && (GET_THREAD_NUM == 0)) {
		uts_printParams();
	}
#else
    /* show parameter settings */
    if (GET_THREAD_NUM == 0) {
      uts_printParams();
	}
#endif

    /* initialize stealstacks */
#ifdef _SHMEM
    {
      /* Shared allocation is a collective operation in Shmem.  These
       * need to be done all at once and in the same order on each PE.
       *
       * Note: Only our own stealstack will contain valid data as UTS runs.
       * For stats, we'll need to gather everyone else's stealstacks
       */
      int i;
      for (i = 0; i < GET_NUM_THREADS; i++) {
        stealStack[i] = (SHARED StealStack *) ALLOC (sizeof(StealStack));
        ss = (StealStack *) stealStack[i];	
        ss_init(ss, initialStackSize);
      }

      ss = stealStack[GET_THREAD_NUM];
    }

#elif !defined(PARALLEL_RECURSION) && !defined(DPARALLEL_RECURSION) && !defined(PARALLEL_STACK_RECURSION) && !defined(DPARALLEL_STACK_RECURSION)
    stealStack[GET_THREAD_NUM] = (SHARED StealStack *) ALLOC (sizeof(StealStack));
    ss = (StealStack *) stealStack[GET_THREAD_NUM];	
    ss_init(ss, initialStackSize);
#else
    stealStack[GET_THREAD_NUM] = (SHARED StealStack *) ALLOC (sizeof(StealStack));
    ss = (StealStack *) stealStack[GET_THREAD_NUM];
    ss_init(ss, 1);
#endif /* _SHMEM */
    
    /* initialize root node and push on thread 0 stack */
    if (GET_THREAD_NUM == 0) {
      initRootNode(&root, type);
      ss_push(ss, &root);
    }

    // line up for the start
#pragma omp barrier    
    BARRIER
    
    /* time parallel search */
    PROFILEACTION(ss_initState(ss));
    t1 = uts_wctime();
#ifdef PARALLEL_RECURSION
    Result_t result;
    if (partitioner == 2) {
        result = dpr::parallel_recursion<Result_t>(root, MyInfo(), MyBody(), dpr::partitioner::automatic());
    } else if (partitioner == 1) {
        result = dpr::parallel_recursion<Result_t>(root, MyInfo(), MyBody(), dpr::partitioner::simple());
    } else {
        result = dpr::parallel_recursion<Result_t>(root, MyInfo(), MyBody(), dpr::partitioner::custom());
    }
    stealStack[0]->nNodes = result.tnodes;
    stealStack[0]->nLeaves = result.tleaves;
    stealStack[0]->maxTreeDepth = result.mheight;
    printf("%d threads %d tasks requested\n", nthreads, ntasks);
#elif DPARALLEL_RECURSION
    Result_t result;
    if (partitioner == 2) {
    	result = dpr::dparallel_recursion<Result_t>(root, MyInfo(), MyBody(), dpr::partitioner::automatic(), dpr::ReplicatedInput | additional_flags);
    } else if (partitioner == 1) {
    	result = dpr::dparallel_recursion<Result_t>(root, MyInfo(), MyBody(), dpr::partitioner::simple(), dpr::ReplicatedInput | additional_flags);
    } else {
    	result = dpr::dparallel_recursion<Result_t>(root, MyInfo(), MyBody(), dpr::partitioner::custom(), dpr::ReplicatedInput | additional_flags);
    }
    stealStack[0]->nNodes = result.tnodes;
    stealStack[0]->nLeaves = result.tleaves;
    stealStack[0]->maxTreeDepth = result.mheight;
    if (rank == 0) {
    	printf("%d processes %d threads %d tasks requested\n", nprocs, nthreads*nprocs, ntasks*nprocs);
    }
#elif PARALLEL_STACK_RECURSION
    //root.auxNodeCounter = 1;
    Result_t result;
	if (chunkSize > 0) {
		if (partitioner == 2) {
			result = dpr::parallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::automatic());
		} else if (partitioner == 1) {
			result = dpr::parallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::simple());
		} else {
			result = dpr::parallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::custom());
		}
	} else {
		if (partitioner == 2) {
			result = dpr::parallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::automatic(), opt);
		} else if (partitioner == 1) {
			result = dpr::parallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::simple(), opt);
		} else {
			result = dpr::parallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::custom(), opt);
		}
	}
	stealStack[0]->nNodes = result.tnodes;
	stealStack[0]->nLeaves = result.tleaves;
	stealStack[0]->maxTreeDepth = result.mheight;
	printf("%d threads\n", nthreads);
#elif DPARALLEL_STACK_RECURSION
	//root.auxNodeCounter = 1;
    Result_t result;
    if (chunkSize > 0) {
		if (partitioner == 2) {
			result = dpr::dparallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::automatic(), dpr::ReplicatedInput | additional_flags);
		} else if (partitioner == 1) {
			result = dpr::dparallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::simple(), dpr::ReplicatedInput | additional_flags);
		} else {
			result = dpr::dparallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::custom(), dpr::ReplicatedInput | additional_flags);
		}
    } else {
		if (partitioner == 2) {
			result = dpr::dparallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::automatic(), dpr::ReplicatedInput | additional_flags, dpr::dspar_config_info_default, opt);
		} else if (partitioner == 1) {
			result = dpr::dparallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::simple(), dpr::ReplicatedInput | additional_flags, dpr::dspar_config_info_default, opt);
		} else {
			result = dpr::dparallel_stack_recursion<Result_t>(root, MyInfo(), MyBody(), chunkSize, dpr::partitioner::custom(), dpr::ReplicatedInput | additional_flags, dpr::dspar_config_info_default, opt);
		}
    }
    stealStack[0]->nNodes = result.tnodes;
    stealStack[0]->nLeaves = result.tleaves;
    stealStack[0]->maxTreeDepth = result.mheight;
    if (rank == 0) {
    	printf("%d processes %d threads\n", nprocs, nthreads*nprocs);
    }
#elif _CILK
    char initialStackSizeStr[255];
    sprintf(initialStackSizeStr, "%d", initialStackSize);
    __cilkrts_set_param("stack size", initialStackSizeStr);
    Result_t result = parTreeSearchCilk(&root);
    stealStack[0]->nNodes = result.tnodes;
	stealStack[0]->nLeaves = result.tleaves;
	stealStack[0]->maxTreeDepth = result.mheight;
	printf("%d threads\n", nthreads);
#elif _CILK_OPT
	char initialStackSizeStr[255];
	sprintf(initialStackSizeStr, "%d", initialStackSize);
	__cilkrts_set_param("stack size", initialStackSizeStr);
	Result_t result = parTreeSearchCilk(&root);
	stealStack[0]->nNodes = result.tnodes;
	stealStack[0]->nLeaves = result.tleaves;
	stealStack[0]->maxTreeDepth = result.mheight;
	printf("%d threads\n", nthreads);
#else
    parTreeSearch(ss);
#endif
    t2 = uts_wctime();
    et = t2 - t1;

#ifdef TRACE
    startTime[GET_THREAD_NUM] = t1;
    ss->md->sessionRecords[SS_IDLE][ss->entries[SS_IDLE] - 1].endTime = t2;
#endif

#pragma omp barrier
    BARRIER

#if defined(DPARALLEL_RECURSION) || defined(DPARALLEL_STACK_RECURSION)
    /* display results */
    if ((rank == 0) && (GET_THREAD_NUM == 0)) {
      showStats(et);
    }
  }
  MPI_Finalize();
#else
    /* display results */
    if (GET_THREAD_NUM == 0) {
      showStats(et);
    } 
  }
#endif
#if defined(PARALLEL_STACK_RECURSION) || defined(DPARALLEL_STACK_RECURSION)
	if (chunkSize == 0) {
#ifdef DPARALLEL_STACK_RECURSION
		dpr::RunExtraInfo extraInfo = dpr::getDpsrLastRunExtraInfo();
#else
		dpr::RunExtraInfo extraInfo = dpr::getPsrLastRunExtraInfo();
#endif
		std::cout << "EXECUTION SUMMARY:" << std::endl;
		std::cout << "  - ChunkSize Used: " << extraInfo.chunkSizeUsed << std::endl;
		std::cout << "  - Test Time: " << extraInfo.testChunkTime << " (s)" << std::endl;
		std::cout << "  - Run Time: " << extraInfo.runTime << " (s)" << std::endl;
		std::cout << std::endl;
	}
#endif
/********** End Parallel Region **********/

  return 0;
}
