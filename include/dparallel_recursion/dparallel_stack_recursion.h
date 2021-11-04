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
/// \file     dparallel_stack_recursion.h
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#ifndef DPR_PARALLEL_STACK_RECURSION_MPI_H_
#define DPR_PARALLEL_STACK_RECURSION_MPI_H_

#include <iostream>
#include <thread>
#include <mutex>
#ifdef DPR_DONT_USE_SPINLOCK
#include <condition_variable>
#endif
#include <atomic>
#include <vector>
#include <utility>
#include <algorithm>
#include <iterator>
#include <chrono>
#include <map>
#include <limits>
#include <cmath>
#include <mpi.h>
#include "dparallel_recursion/dpr_utils.h"
#include "dparallel_recursion/general_reference_wrapper.h"
#include "dparallel_recursion/EmptyBody.h"
#include "dparallel_recursion/Arity.h"
#include "dparallel_recursion/DSParConfigInfo.h"
#include "dparallel_recursion/ChunkSelector.h"
#include "dparallel_recursion/parallel_stack_recursion.h"
#include "dparallel_recursion/seq_parallel_stack_recursion.h"
#include "dparallel_recursion/Behavior.h"
#include "dparallel_recursion/dpr_mpi_stack_comm.h"

namespace dpr {

namespace internal_mpi {

static constexpr int defaultChunkSize = internal::defaultChunkSize;

inline int& num_total_threads() {
   static int& num_total_threads = internal::num_total_threads();
   return num_total_threads;
}

constexpr int num_total_threads(const DSParConfigInfo& dspar_config_info) {
	return (dspar_config_info.nthreads == UNDEFINED_PARAMETER) ? internal::num_total_threads() : dspar_config_info.nthreads;
}

inline int& initialStackSize() {
	static int& initialStackSize = internal::initialStackSize();
	return initialStackSize;
}

constexpr int initialStackSize(const DSParConfigInfo& dspar_config_info) {
	return (dspar_config_info.initial_stack_size == UNDEFINED_PARAMETER) ? initialStackSize() : dspar_config_info.initial_stack_size;
}

inline int& chunksToStealGl() {
	static int chunksToStealGl = CHUNKS_TO_STEAL_DEFAULT;
	return chunksToStealGl;
}

constexpr int chunksToStealGl(const DSParConfigInfo& dspar_config_info) {
	return (dspar_config_info.chunksToSteal == UNDEFINED_PARAMETER) ? chunksToStealGl() : dspar_config_info.chunksToSteal;
}

inline int& pollingIntervalGl() {
	static int pollingIntervalGl = POLLING_INTERVAL_DEFAULT;
	return pollingIntervalGl;
}

constexpr int pollingIntervalGl(const DSParConfigInfo& dspar_config_info) {
	return (dspar_config_info.polling_interval == UNDEFINED_PARAMETER) ? pollingIntervalGl() : dspar_config_info.polling_interval;
}

inline dpr::RunExtraInfo& lastDpsrRunExtraInfo() {
	static dpr::RunExtraInfo lastDpsrRunExtraInfo;
	return lastDpsrRunExtraInfo;
}

inline int& dpr_rank() {
   static int dpr_rank = -1;
   return dpr_rank;
}

inline int& dpr_nprocs() {
   static int dpr_nprocs = -1;
   return dpr_nprocs;
}

template<typename Return, typename T, typename Info, typename Body>
class simple_stack_partitioner;

template<typename Return, typename T, typename Info, typename Body>
class auto_stack_partitioner;

template<typename Return, typename T, typename Info, typename Body>
class custom_stack_partitioner;

#ifndef DPR_STATUS_HAVEWORK
#define DPR_STATUS_HAVEWORK 0
#endif
#ifndef DPR_STATUS_TERM
#define DPR_STATUS_TERM     1
#endif
#ifndef DPR_SS_WORK
#define DPR_SS_WORK    0
#endif
#ifndef DPR_SS_SEARCH
#define DPR_SS_SEARCH  1
#endif
#ifndef DPR_SS_IDLE
#define DPR_SS_IDLE    2
#endif
#ifndef DPR_SS_OVH
#define DPR_SS_OVH     3
#endif
#ifndef DPR_SS_CBOVH
#define DPR_SS_CBOVH   4
#endif
#ifndef DPR_SS_NSTATES
#define DPR_SS_NSTATES 5
#endif

inline std::exception_ptr& globalExceptionPtr() {
	static std::exception_ptr globalExceptionPtr = nullptr;
	return globalExceptionPtr;
}

enum dpsr_mpi_tags { DPSR_MPIWS_WORKREQUEST = 1, DPSR_MPIWS_WORKRESPONSE, DPSR_MPIWS_TDTOKEN, DPSR_MPIWS_REDUCTION, DPSR_MPIWS_REDUCTION_INFO, DPSR_MPIWS_TEST_NEXTPARAMS, DPSR_MPIWS_TEST_EXECRES, DPSR_MPIWS_TEST_FINALDATA };
enum dpsr_td_colors : unsigned long int { DPSR_WHITE = 0, DPSR_PINK, DPSR_RED };

/** Adaptive Polling Interval Parameters **/
static const unsigned int pollint_min = 1u;
static const unsigned int pollint_max = 1024u;
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
#define DPR_POLLINT_GROW polling_interval + 4u
#define DPR_POLLINT_SHRINK polling_interval / 2u
#else
#define DPR_POLLINT_GROW polling_interval + 1u
#define DPR_POLLINT_SHRINK polling_interval / 4u
#endif

inline int& threadsRequestPolicyGl() {
	static int threadsRequestPolicyGl = dpr::THREAD_MPI_STEAL_POLICY_DEFAULT;	// thread policy when making work request
	return threadsRequestPolicyGl;
}

constexpr int threadsRequestPolicyGl(const DSParConfigInfo& dspar_config_info) {
	return (dspar_config_info.threads_request_policy == UNDEFINED_PARAMETER) ? threadsRequestPolicyGl() : dspar_config_info.threads_request_policy;
}

inline int& mpiWorkRequestLimitGl() {
	static int mpiWorkRequestLimitGl = dpr::MPI_WORKREQUEST_LIMITS_DEFAULT;		// limit of MPI work requests allowed simultaneously for a process
	return mpiWorkRequestLimitGl;
}

constexpr int mpiWorkRequestLimitGl(const DSParConfigInfo& dspar_config_info) {
	return (dspar_config_info.mpi_workrequest_limits == UNDEFINED_PARAMETER) ? mpiWorkRequestLimitGl() : dspar_config_info.mpi_workrequest_limits;
}

inline int& trpPredictWorkCountGl() {
	static int trpPredictWorkCountGl = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;	// number of work requested per thread predictively (only used for THREAD_MPI_STEAL_POLICY_PREDICTIVE threadsRequestPolicy)
	return trpPredictWorkCountGl;
}

constexpr int trpPredictWorkCountGl(const DSParConfigInfo& dspar_config_info) {
	return (dspar_config_info.trp_predict_workcount == UNDEFINED_PARAMETER) ? trpPredictWorkCountGl() : dspar_config_info.trp_predict_workcount;
}

typedef struct {
	enum dpsr_td_colors color;
	std::vector<unsigned long int> send_count;
	std::vector<unsigned long int> recv_count;
	std::vector<unsigned long int> status;
} td_token_t;

template<typename T>
struct StealStack {
	int sharedStart;                                 /* index of start of shared portion of stack, 4 bytes, shared variable */
	std::mutex stackLock;                            /* lock for manipulation of shared portion, 40 bytes, shared mutex */
	volatile int statusJobSearch;                    /* stealing status of new jobs, 0 = free, 1 = blocked and waiting jobs, 2 = blocked and jobs ready in stack, 3 = blocked and waiting response ; 4 bytes, shared variable */
	char falseSharingPadding[64*((sizeof(int)*2+sizeof(std::mutex))/64+1)-sizeof(int)*2-sizeof(std::mutex)];                      /* 4*4=16 bytes padding */
	//////////// 64 bytes ////////////
	std::vector<T> stack;                            /* addr of actual stack of nodes in local addr space, 24 bytes */
	unsigned long long int nNodes;                   /* number of nodes processed, 8 bytes */
	int stackSize;                                   /* total space avail (in number of elements), 4 bytes */
	int local;                                       /* index of start of local portion, 4 bytes */
	int top;                                         /* index of stack top, 4 bytes */
	unsigned int polling_count;                      /* polling count, 4 bytes */
	int baseInputTop;                                /* top of stack of copied base-nodes (only used with GatherInput), 4 bytes */
	int baseInputStackSize;                          /* size of stack of copied base-nodes (only used with GatherInput), 4 bytes */
	char falseSharingPadding2[64*((sizeof(std::vector<T>)+sizeof(unsigned long long int)+sizeof(unsigned int)+sizeof(int)*5)/64+1)-sizeof(std::vector<T>)-sizeof(unsigned long long int)-sizeof(unsigned int)-sizeof(int)*5];                     /* 2*4=8 bytes padding */
	//////////// 64 bytes ////////////
	std::vector<T> baseInputStack;                   /* addr of actual stack of copied base-nodes in local addr space  (only used with GatherInput), 24 bytes */
	char falseSharingPadding3[64*(sizeof(std::vector<T>)/64+1)-sizeof(std::vector<T>)];                    /* 10*4=40 bytes padding */
	//////////// 64 bytes ////////////
#ifdef PROFILE
	int nLeaves;                                     /* tree stats, 4 bytes */
	int maxStackDepth;                               /* stack stats, 4 bytes */
	int nAcquire, nRelease, nSteal, nFail;           /* steal stats, 4*4=16 bytes */
	int wakeups, falseWakeups;                       /* 4*2=8 bytes */
	unsigned long long int nNodes_last;              /* 8 bytes */
	std::chrono::steady_clock::time_point timeLast;  /* perf measurements, 8 bytes */
	char falseSharingPadding4[64*((sizeof(int)*8+sizeof(unsigned long long int)+sizeof(std::chrono::steady_clock::time_point))/64+1)-sizeof(int)*8-sizeof(unsigned long long int)-sizeof(std::chrono::steady_clock::time_point)];                        /* 4*4=16 bytes padding */
	//////////// 64 bytes ////////////
	double time[DPR_SS_NSTATES];                     /* 8*5=40 bytes */
	int entries[DPR_SS_NSTATES], curState;           /* 4*5+4=24 bytes */
	//////////// 64 bytes ////////////
#endif
};

template<typename T, typename Info>
class parallel_stack_recursion_internal_mpi {

public:
	const int threadId;
	const int num_total_threads;
	const int rank;
	const int num_total_procs;
	const int chunkSize;
	const int chunksToSteal;
	const int doubleChunkSize;
	const int threadsRequestPolicy;
	const int mpiWorkRequestLimit;
	const int trpPredictWorkCount;
	const bool pollint_isadaptive;
	unsigned int polling_interval;	// Polling interval for check incoming MPI requests

	struct cb_t {
#ifdef DPR_DONT_USE_SPINLOCK
		std::mutex cb_cv_mtx;
		std::condition_variable cb_cv;
#endif
		std::atomic<int> cb_count;
		volatile bool cb_cancel;
		volatile bool cb_done;
#ifdef DPR_DONT_USE_SPINLOCK
		char cb_padding[64*((sizeof(std::mutex)+sizeof(std::condition_variable)+sizeof(bool)*2+sizeof(std::atomic<int>))/64+1)-sizeof(std::mutex)-sizeof(std::condition_variable)-sizeof(bool)*2-sizeof(std::atomic<int>)];
#else
		char cb_padding[64*((sizeof(bool)*2+sizeof(std::atomic<int>))/64+1)-sizeof(bool)*2-sizeof(std::atomic<int>)];
#endif
	};

	alignas(64) static cb_t cb;

	static int mpi_processing;

	static enum dpsr_td_colors my_color;
	static std::atomic<unsigned long int> chunks_recvd;	// Total messages received
	static std::atomic<unsigned long int> ctrl_recvd;	// Total messages received
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
	static unsigned long int chunks_sent;   			// Total messages sent
	static unsigned long int ctrl_sent;     			// Total messages sent
#else
	static std::atomic<unsigned long int> chunks_sent;	// Total messages sent
	static std::atomic<unsigned long int> ctrl_sent;	// Total messages sent
#endif

#ifndef DPR_USE_MPI_THREAD_MULTIPLE
	static std::mutex mpiMutex;					// Mutex used for serialized all threaded MPI calls (needed only for MPI_THREAD_SERIALIZED)
#endif
	static std::mutex requestStatusMutex;		// Mutex used for the read/write access to the requestStatus of the stealStack (only needed for threadsRequestPolicy > 0)

	/** Global Communication handles **/
	MPI_Request wrin_request;		// Incoming steal request
	MPI_Request wrout_request;		// Outbound steal request
	MPI_Request iw_request;			// Incoming work requested
	MPI_Request ow_request;			// Outbound work requested
	MPI_Request td_request;			// Term. Detection listener

	std::vector<int> addRequestThreads;		// Local list of threads from which work has been requested from this thread

	int nextStartFindThread = (threadId + 1) % num_total_threads;

	/** Global Communication Buffers **/
	int wrin_buff[2];					// Buffer for accepting incoming work requests
	int wrout_buff[2];					// Buffer to send outgoing work requests
	std::vector<T> iw_buff;				// Buffer input work
	std::vector<T> ow_buff;				// Buffer output work
	buffer_type ow_raw_buffer;			// Raw buffer output work
	buffer_type iw_raw_buffer;			// Raw buffer input work
	td_token_t td_token;				// Term. Detection token
	unsigned long int *td_token_buff;	// Term. Detection token buffer

	int last_steal;						// Rank of last thread stolen from

	static internal_mpi::StealStack<T> **stealStack;	// Global access to all stealStack from any thread

	parallel_stack_recursion_internal_mpi(const int threadId_, const int num_total_threads_, const int rank_, const int num_total_procs_, const int chunkSize_, const int chunksToSteal_, const unsigned int pollingInterval_, const int threadsRequestPolicy_, const int mpiWorkRequestLimit_, const int trpPredictWorkCount_) :
		threadId(threadId_),
		num_total_threads(num_total_threads_),
		rank(rank_),
		num_total_procs(num_total_procs_),
		chunkSize(chunkSize_),
		chunksToSteal(chunksToSteal_),
		doubleChunkSize(chunkSize_*2),
		threadsRequestPolicy(threadsRequestPolicy_),
		mpiWorkRequestLimit(mpiWorkRequestLimit_),
		trpPredictWorkCount(threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_PREDICTIVE ? trpPredictWorkCount_ : 0),
		pollint_isadaptive(pollingInterval_ == POLLING_RATE_DYNAMIC),
		polling_interval((pollingInterval_ == POLLING_RATE_DYNAMIC) ? 1 : pollingInterval_),
		wrin_request(MPI_REQUEST_NULL),
		wrout_request(MPI_REQUEST_NULL),
		iw_request(MPI_REQUEST_NULL),
		ow_request(MPI_REQUEST_NULL),
		td_request(MPI_REQUEST_NULL),
		td_token_buff((threadId == 0) ? (new unsigned long int[1+num_total_procs*3]) : nullptr),
		last_steal((rank > 0) ? (rank - 1) : num_total_procs-1)
	{
		iw_buff.resize((num_total_procs > 1) ? (chunkSize*chunksToSteal*((threadsRequestPolicy != THREAD_MPI_STEAL_POLICY_AGGRESSIVE) ? num_total_threads : 1)) : 0);
		ow_buff.resize((num_total_procs > 1) ? (chunkSize*chunksToSteal*((threadsRequestPolicy != THREAD_MPI_STEAL_POLICY_AGGRESSIVE) ? num_total_threads : 1)) : 0);
		if (threadsRequestPolicy != THREAD_MPI_STEAL_POLICY_AGGRESSIVE) {
			addRequestThreads.reserve(num_total_threads);
		}
	}

	~parallel_stack_recursion_internal_mpi() {
		if (threadId == 0) {
			delete[] td_token_buff;
		}
	}

#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS) || (defined(DPR_DENY_STACK_RESIZE) && !defined(DPR_SKIP_CHECK_STACKOVERFLOW))
	void ss_error(const std::string str) {
		std::string err_str = "*** [Thread " + std::to_string(threadId) + "] " + str;
		throw std::runtime_error(err_str);
	}
#endif

	/** Make progress on any outstanding WORKREQUESTs or WORKRESPONSEs */
	void ws_make_progress(internal_mpi::StealStack<T> *s) {
		MPI_Status status;
		int flag, index;

		if (my_color != DPSR_RED) {
			// Test for incoming work_requests
			if (ow_request != MPI_REQUEST_NULL) {
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
				std::lock_guard<std::mutex> lck(mpiMutex);
#endif
				MPI_Test(&ow_request, &flag, MPI_STATUS_IGNORE);
				if (ow_request != MPI_REQUEST_NULL) {
					return;
				}
			}

			{
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
				std::lock_guard<std::mutex> lck(mpiMutex);
#endif
				MPI_Test(&wrin_request, &flag, &status);
			}

			if (flag) {
				// Got a work request
				/* Repost that work request listener */
				int nWorkSize = wrin_buff[0];
				int threadDest = wrin_buff[1];

				index = status.MPI_SOURCE;
				{
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
					std::lock_guard<std::mutex> lck(mpiMutex);
#endif
					ctrl_recvd.fetch_add(1, std::memory_order_relaxed);
					MPI_Irecv(wrin_buff, 2, MPI_INT, MPI_ANY_SOURCE, DPSR_MPIWS_WORKREQUEST, MPI_COMM_WORLD, &wrin_request);
				}

				bool goodSteal = false;
				std::vector<int> victimsIds;
				findwork_for_remote(victimsIds, chunkSize, chunksToSteal, nextStartFindThread);
				while ((victimsIds.size() > 0) && !goodSteal) {
					// some work detected, try to steal it
					goodSteal = ss_steal_for_remote(s, victimsIds, chunkSize, chunksToSteal, nWorkSize, index, threadDest);
					if (!goodSteal) {
						findwork_for_remote(victimsIds, chunkSize, chunksToSteal, nextStartFindThread);
					}
				}

				/* Check if we have any surplus work */
				if (goodSteal) {
					nextStartFindThread = ((victimsIds.size() > 0) ? victimsIds.back() : threadId) + 1;
				} else {
					// Send a "no work" response
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
					std::lock_guard<std::mutex> lck(mpiMutex);
					++ctrl_sent;
#else
					ctrl_sent.fetch_add(1, std::memory_order_relaxed);
#endif
					MPI_Isend(NULL, 0, MPI_BYTE, index, (DPSR_MPIWS_WORKRESPONSE | (threadDest<<8)), MPI_COMM_WORLD, &ow_request);
				}

				if (pollint_isadaptive && ss_localDepth(s) != 0) {
					polling_interval = std::max(pollint_min, DPR_POLLINT_SHRINK);
				}
			} else {

				if (pollint_isadaptive && ss_localDepth(s) != 0) {
					polling_interval = std::min(pollint_max, DPR_POLLINT_GROW);
				}
			}
		}
	}

	void tokenbuff_to_token(const unsigned long int *td_token_buff, td_token_t& td_token) {
		td_token.color = static_cast<dpsr_td_colors>(td_token_buff[0]);
		for (int i = 0; i < num_total_procs; i++) {
			td_token.recv_count[i] = td_token_buff[i+1];
			td_token.send_count[i] = td_token_buff[i+1+num_total_procs];
			td_token.status[i] = td_token_buff[i+1+num_total_procs*2];
		}
	}

	void token_to_tokenbuff(const td_token_t& td_token, unsigned long int *td_token_buff) {
		td_token_buff[0] = static_cast<unsigned long int>(td_token.color);
		for (int i = 0; i < num_total_procs; i++) {
			td_token_buff[i+1] = td_token.recv_count[i];
			td_token_buff[i+1+num_total_procs] = td_token.send_count[i];
			td_token_buff[i+1+num_total_procs*2] = td_token.status[i];
		}
	}

	int ss_get_work(internal_mpi::StealStack<T> *s) {
		bool flag_iw = false;
		int work_rcv;
		int nuitems;
		int workStealed = 0;

		/* If no more work */
		if (num_total_procs == 1) {
			PROFILEACTION(ss_setState(s, DPR_SS_IDLE));
			return -2;
		}
		if (my_color == DPSR_RED) {
			clear_mpi_communications();
			PROFILEACTION(ss_setState(s, DPR_SS_IDLE));
			return -1;
		}

		if (ss_localDepth(s) == 0) {

			PROFILEACTION(ss_setState(s, (my_color == DPSR_PINK) ? DPR_SS_IDLE: DPR_SS_SEARCH));

			/* Check if we should post another steal request */
			if (wrout_request == MPI_REQUEST_NULL && my_color < DPSR_PINK) {

				int workRequestNumber;
				if (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_AGGRESSIVE && mpiWorkRequestLimit == num_total_threads) {
					workRequestNumber = 1;
				} else if (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_AGGRESSIVE) {
					workRequestNumber = 0;
					std::lock_guard<std::mutex> slck(requestStatusMutex);
					if (mpi_processing < mpiWorkRequestLimit) {
						workRequestNumber = 1;
						++mpi_processing;
					}
				} else if (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_MULTIPLE && mpiWorkRequestLimit == num_total_threads) {
					workRequestNumber = 0;
					std::lock_guard<std::mutex> slck(requestStatusMutex);
					if (s->statusJobSearch == 0) {
						workRequestNumber = 1;
						for (int thId = 0; thId < num_total_threads; thId++) {
							if (thId != threadId) {
								if ((stealStack[thId]->statusJobSearch == 0) && (ss_localFullDepth(stealStack[thId]) == 0)) {
									stealStack[thId]->statusJobSearch = 1;
									addRequestThreads.push_back(thId);
									workRequestNumber++;
								}
							}
						}
						s->statusJobSearch = 3;
					} else if (s->statusJobSearch == 2) {
						s->statusJobSearch = 0;
						return ss_localDepth(s);
					}
				} else if (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_MULTIPLE) {
					workRequestNumber = 0;
					std::lock_guard<std::mutex> slck(requestStatusMutex);
					if (s->statusJobSearch == 0) {
						if (mpi_processing < mpiWorkRequestLimit) {
							workRequestNumber = 1;
							for (int thId = 0; thId < num_total_threads; thId++) {
								if (thId != threadId) {
									if ((stealStack[thId]->statusJobSearch == 0) && (ss_localFullDepth(stealStack[thId]) == 0)) {
										stealStack[thId]->statusJobSearch = 1;
										addRequestThreads.push_back(thId);
										workRequestNumber++;
									}
								}
							}
							s->statusJobSearch = 3;
							++mpi_processing;
						}
					} else if (s->statusJobSearch == 2) {
						s->statusJobSearch = 0;
						return ss_localDepth(s);
					}
				} else if (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_PREDICTIVE && mpiWorkRequestLimit == num_total_threads) {
					workRequestNumber = 0;
					std::lock_guard<std::mutex> slck(requestStatusMutex);
					if (s->statusJobSearch == 0) {
						workRequestNumber = trpPredictWorkCount;
						s->statusJobSearch = 3;
					} else if (s->statusJobSearch == 2) {
						s->statusJobSearch = 0;
						return ss_localDepth(s);
					}
				} else if (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_PREDICTIVE) {
					workRequestNumber = 0;
					std::lock_guard<std::mutex> slck(requestStatusMutex);
					if (s->statusJobSearch == 0) {
						if (mpi_processing < mpiWorkRequestLimit) {
							workRequestNumber = trpPredictWorkCount;
							s->statusJobSearch = 3;
							++mpi_processing;
						} else {
							s->statusJobSearch = 1;
						}
					} else if (s->statusJobSearch == 2) {
						s->statusJobSearch = 0;
						return ss_localDepth(s);
					}
				} else {
					workRequestNumber = 0;
				}

				if (workRequestNumber > 0) {
					/* Send the request and wait for a work response */
					last_steal = (last_steal + 1) % num_total_procs;
					if (last_steal == rank) last_steal = (last_steal + 1) % num_total_procs;
					wrout_buff[0] = workRequestNumber;
					wrout_buff[1] = threadId;

#ifndef DPR_USE_MPI_THREAD_MULTIPLE
					std::lock_guard<std::mutex> lck(mpiMutex);
					++ctrl_sent;
#else
					ctrl_sent.fetch_add(1, std::memory_order_relaxed);
#endif

					MPI_Isend(wrout_buff, 2, MPI_INT, last_steal, DPSR_MPIWS_WORKREQUEST, MPI_COMM_WORLD, &wrout_request);

					std::pair<bool, int> rcv_return = dpr::internal_mpi::st_irecv(last_steal, (DPSR_MPIWS_WORKRESPONSE | (threadId<<8)), iw_buff.data(), &nuitems, iw_raw_buffer, iw_request, nullptr);
					flag_iw = rcv_return.first;
					work_rcv = rcv_return.second;

				}
			}

			// Call into the stealing progress engine
			ws_make_progress(s);

			// Test for incoming work
			if (!flag_iw && wrout_request != MPI_REQUEST_NULL) {
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
				std::lock_guard<std::mutex> lck(mpiMutex);
#endif
				std::pair<bool, int> rcv_return = dpr::internal_mpi::st_irecv(last_steal, (DPSR_MPIWS_WORKRESPONSE | (threadId<<8)), iw_buff.data(), &nuitems, iw_raw_buffer, iw_request, nullptr);
				flag_iw = rcv_return.first;
				work_rcv = rcv_return.second;
			}

			if (flag_iw && wrout_request != MPI_REQUEST_NULL) {
				if (threadsRequestPolicy != THREAD_MPI_STEAL_POLICY_PREDICTIVE) {
					if (work_rcv > 0) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
						if (nuitems <= 0) {
							ss_error("ss_get_work(): wrong count of number of items received");
						}
#endif
						workStealed = nuitems;
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
						if (s->top + chunkSize*chunksToSteal >= s->stackSize) {
							ss_error("ss_get_work(): steal will overflow thief's stack");
						}
#endif
#else
						while(s->top + chunkSize*chunksToSteal >= s->stackSize) {
							ss_growStackSize(s);
						}
#endif

						std::copy_n(iw_buff.begin(), chunkSize*chunksToSteal, s->stack.begin()+ s->top);

						s->top += chunkSize*chunksToSteal;

						if (threadsRequestPolicy != THREAD_MPI_STEAL_POLICY_AGGRESSIVE) {
							const int threadsWithWork = nuitems / (chunkSize*chunksToSteal);
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
							if ((threadsWithWork <= 0) || (threadsWithWork > static_cast<int>(addRequestThreads.size())+1) || (nuitems % (chunkSize*chunksToSteal) != 0)) {
								ss_error("ss_get_work(): wrong count of number of threads with work");
							}
#endif
							for (int i = 1; i < threadsWithWork; ++i) {
								int desti;
								if ((i-1) < static_cast<int>(addRequestThreads.size())) {
									desti = addRequestThreads[i-1];
								} else {
									// This situation should not occur (more work than the maximum requested for all threads has been received), but manage it anyway (put the leftover work in this thread).
									desti = threadId;
									//std::cout << "\tERROR SNO1 [" << rank << "][" << threadId << "]>>[" << desti << "]. Some var dumps: threadsWithWork=" <<  threadsWithWork << " addRequestThreads.size()=" << addRequestThreads.size() << ", top=" << stealStack[desti]->top << ", local=" << stealStack[desti]->local << ", sharedStart=" << stealStack[desti]->sharedStart << ", statusJobSearch=" << stealStack[desti]->statusJobSearch << std::endl;
								}

								if (stealStack[desti]->top-stealStack[desti]->sharedStart > 0) {
									// This situation should not occur (the stack should be empty at this point), but manage it anyway (put the work safely in the stack local section of this thread).
									//std::cout << "\tERROR SNO2 [" << rank << "][" << threadId << "]>>[" << desti << "]. Some var dumps: threadsWithWork=" <<  threadsWithWork << " addRequestThreads.size()=" << addRequestThreads.size() << ", top=" << stealStack[desti]->top << ", local=" << stealStack[desti]->local << ", sharedStart=" << stealStack[desti]->sharedStart << ", statusJobSearch=" << stealStack[desti]->statusJobSearch << std::endl;
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
									if (s->top + chunkSize*chunksToSteal >= s->stackSize) {
										ss_error("ss_get_work(): steal will overflow thief's stack");
									}
#endif
#else
									while(s->top + chunkSize*chunksToSteal >= s->stackSize) {
										ss_growStackSize(s);
									}
#endif
									std::copy_n(iw_buff.begin() + (chunkSize*chunksToSteal*i), chunkSize*chunksToSteal, s->stack.begin()+ s->top);
									s->top += chunkSize*chunksToSteal;
								} else {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
									if (stealStack[desti]->top + chunkSize*chunksToSteal >= stealStack[desti]->stackSize) {
										ss_error("ss_get_work(): steal will overflow thief's stack");
									}
#endif
#else
									while(stealStack[desti]->top + chunkSize*chunksToSteal >= stealStack[desti]->stackSize) {
										ss_growStackSize(stealStack[desti]);
									}
#endif
									std::copy_n(iw_buff.begin() + (chunkSize*chunksToSteal*i), chunkSize*chunksToSteal, stealStack[desti]->stack.begin()+ stealStack[desti]->top);
									stealStack[desti]->top += chunkSize*chunksToSteal;
								}
							}
						}
						chunks_recvd.fetch_add(1, std::memory_order_relaxed);
					} else {
						// Received "No Work" message
						ctrl_recvd.fetch_add(1, std::memory_order_relaxed);
					}
					if (threadsRequestPolicy != THREAD_MPI_STEAL_POLICY_AGGRESSIVE) {
						std::lock_guard<std::mutex> lck(requestStatusMutex);
						if (mpiWorkRequestLimit != num_total_threads) --mpi_processing;
						s->statusJobSearch = 0;
						for (const int& v : addRequestThreads) {
							stealStack[v]->statusJobSearch = 2;
						}
						addRequestThreads.clear();
					} else if (mpiWorkRequestLimit != num_total_threads) {
						 --mpi_processing;
					}
				} else {
					std::lock_guard<std::mutex> lck(requestStatusMutex);
					if (work_rcv > 0) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
						if (nuitems <= 0) {
							ss_error("ss_get_work(): wrong count of number of items received");
						}
#endif
						workStealed = nuitems;
						for (int j = 0; j < num_total_threads; ++j) {
							if ((j != threadId) && (stealStack[j]->statusJobSearch == 1)) {
								addRequestThreads.push_back(j);
							}
						}
						const int threadsWithWork = nuitems / (chunkSize*chunksToSteal);
						const int workPerThread = threadsWithWork / (static_cast<int>(addRequestThreads.size()) + 1);
						const int workPerThreadM = threadsWithWork % (static_cast<int>(addRequestThreads.size()) + 1);
						const int finalWorkForThread = workPerThread + (workPerThreadM > 0);
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
						if (s->top + chunkSize*chunksToSteal*finalWorkForThread >= s->stackSize) {
							ss_error("ss_get_work(): steal will overflow thief's stack");
						}
#endif
#else
						while(s->top + chunkSize*chunksToSteal*finalWorkForThread >= s->stackSize) {
							ss_growStackSize(s);
						}
#endif
						std::copy_n(iw_buff.begin(), chunkSize*chunksToSteal*finalWorkForThread, s->stack.begin()+ s->top);

						s->top += chunkSize*chunksToSteal*finalWorkForThread;

						int xBase = chunkSize*chunksToSteal*finalWorkForThread;
						int i = 1;

						for (const int& desti : addRequestThreads) {
							const int finalWorkForThread = workPerThread + (i < workPerThreadM);
							if (finalWorkForThread > 0) {
								if (stealStack[desti]->top-stealStack[desti]->sharedStart > 0) {
									// This situation should not occur (the stack should be empty at this point), but manage it anyway (put the work safely in the stack local section of this thread).
									//std::cout << "\tERROR SNO2 [" << rank << "][" << threadId << "]>>[" << desti << "]. Some var dumps: threadsWithWork=" <<  threadsWithWork << " addRequestThreads.size()=" << addRequestThreads.size() << ", top=" << stealStack[desti]->top << ", local=" << stealStack[desti]->local << ", sharedStart=" << stealStack[desti]->sharedStart << ", statusJobSearch=" << stealStack[desti]->statusJobSearch << std::endl;
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
									if (s->top + chunkSize*chunksToSteal*finalWorkForThread >= s->stackSize) {
										ss_error("ss_get_work(): steal will overflow thief's stack");
									}
#endif
#else
									while(s->top + chunkSize*chunksToSteal*finalWorkForThread >= s->stackSize) {
										ss_growStackSize(s);
									}
#endif
									std::copy_n(iw_buff.begin() + xBase, chunkSize*chunksToSteal*finalWorkForThread, s->stack.begin()+ s->top);
									s->top += chunkSize*chunksToSteal*finalWorkForThread;
									xBase += chunkSize*chunksToSteal*finalWorkForThread;
							} else {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
									if (stealStack[desti]->top + chunkSize*chunksToSteal*finalWorkForThread >= stealStack[desti]->stackSize) {
										ss_error("ss_get_work(): steal will overflow thief's stack");
									}
#endif
#else
									while(stealStack[desti]->top + chunkSize*chunksToSteal*finalWorkForThread >= stealStack[desti]->stackSize) {
										ss_growStackSize(stealStack[desti]);
									}
#endif
									std::copy_n(iw_buff.begin() + xBase, chunkSize*chunksToSteal*finalWorkForThread, stealStack[desti]->stack.begin()+ stealStack[desti]->top);
									stealStack[desti]->top += chunkSize*chunksToSteal*finalWorkForThread;
									xBase += chunkSize*chunksToSteal*finalWorkForThread;
								}
							}
							++i;
						}
						chunks_recvd.fetch_add(1, std::memory_order_relaxed);
					} else {
						// Received "No Work" message
						for (int j = 0; j < num_total_threads; ++j) {
							if ((j != threadId) && (stealStack[j]->statusJobSearch == 1)) {
								addRequestThreads.push_back(j);
							}
						}
						ctrl_recvd.fetch_add(1, std::memory_order_relaxed);
					}
					if (mpiWorkRequestLimit != num_total_threads) --mpi_processing;
					s->statusJobSearch = 0;
					for (const int& v : addRequestThreads) {
						stealStack[v]->statusJobSearch = 2;
					}
					addRequestThreads.clear();
				}

				// Clear on the outgoing work_request
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
				std::lock_guard<std::mutex> lck(mpiMutex);
#endif
				MPI_Wait(&wrout_request, MPI_STATUS_IGNORE);
			}

			if (threadId == 0) {	//only the thread0 manages the token
				/* Test if we have the token */
				MPI_Status status;
				int flag_td;
				{
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
					std::lock_guard<std::mutex> lck(mpiMutex);
#endif
					MPI_Test(&td_request, &flag_td, &status);
				}

				if (flag_td) {

					if (!is_mpi_status_empty(status)) {
						tokenbuff_to_token(td_token_buff, td_token);
					}
					const int ret = manage_token(s);
					if (ret != 0) {
						return ret;
					}
				}
			} else {
				if (my_color == DPSR_RED) {
					// This is safe now that the pink token has mopped up all outstanding messages.
					clear_mpi_communications();
					PROFILEACTION(ss_setState(s, DPR_SS_IDLE));
					return -1;
				}
			}


		} else {
			if (threadsRequestPolicy != THREAD_MPI_STEAL_POLICY_AGGRESSIVE) {
				if (s->statusJobSearch == 2) {
					std::lock_guard<std::mutex> slck(requestStatusMutex);
					s->statusJobSearch = 0;
				}
			}
			return ss_localDepth(s);
		}

		return workStealed;  // Local work exists
	}

	void init_mpi_communications() {
		// Setup non-blocking recieve for recieving shared work requests
		if (num_total_procs > 1) {
			{
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
			std::lock_guard<std::mutex> lck(mpiMutex);
#endif
			MPI_Irecv(wrin_buff, 2, MPI_INT, MPI_ANY_SOURCE, DPSR_MPIWS_WORKREQUEST, MPI_COMM_WORLD, &wrin_request);
			}

			if (threadId == 0) {

				td_token.color = DPSR_WHITE;

				td_token.send_count.resize(num_total_procs);
				td_token.recv_count.resize(num_total_procs);
				td_token.status.resize(num_total_procs);

				/* Set up the termination detection receives */
				if (rank == 0) {
					td_request = MPI_REQUEST_NULL;
				} else {
					/* Post termination detection listener */
					int j = (rank == 0) ? num_total_procs - 1 : rank - 1; // Receive the token from the processor to your left
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
					std::lock_guard<std::mutex> lck(mpiMutex);
#endif
					MPI_Irecv(&td_token_buff[0], 1+num_total_procs*3, MPI_UNSIGNED_LONG, j, DPSR_MPIWS_TDTOKEN, MPI_COMM_WORLD, &td_request);
				}
			}
		}
	}

	int manage_token(internal_mpi::StealStack<T> *s) {;
		enum dpsr_td_colors next_token;
		int forward_token = 0;

		switch (td_token.color) {
			case DPSR_WHITE:
				{
					if (ss_allThreadsLocalFullDepth(s) == 0) {		// No work in any local thread
						next_token = DPSR_WHITE;
						if (td_token.status[rank] == 0) {	//Status = UNSETTED
							td_token.recv_count[rank] = chunks_recvd.load(std::memory_order_relaxed);
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
							td_token.send_count[rank] = chunks_sent;
#else
							td_token.send_count[rank] = chunks_sent.load(std::memory_order_relaxed);
#endif
							td_token.status[rank] = 1;
						} else if (td_token.status[rank] == 1) {	//Status = SETTED
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
							if ((chunks_recvd.load(std::memory_order_relaxed) == td_token.recv_count[rank]) && (chunks_sent == td_token.send_count[rank])) {
#else
							if ((chunks_recvd.load(std::memory_order_relaxed) == td_token.recv_count[rank]) && (chunks_sent.load(std::memory_order_relaxed) == td_token.send_count[rank])) {
#endif
								td_token.status[rank] = 2;
							} else {
								for (int i = 0; i < num_total_procs; i++) {
									if (rank != i) {
										td_token.recv_count[i] = 0;
										td_token.send_count[i] = 0;
										td_token.status[i] = 0;
									} else {
										td_token.recv_count[i] = chunks_recvd.load(std::memory_order_relaxed);
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
										td_token.send_count[i] = chunks_sent;
#else
										td_token.send_count[i] = chunks_sent.load(std::memory_order_relaxed);
#endif
										td_token.status[i] = 1;
									}
								}
							}
						} else if (td_token.status[rank] == 2) {	//Status = CONFIRMED
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
							if ((chunks_recvd.load(std::memory_order_relaxed) == td_token.recv_count[rank]) && (chunks_sent == td_token.send_count[rank])) {
#else
							if ((chunks_recvd.load(std::memory_order_relaxed) == td_token.recv_count[rank]) && (chunks_sent.load(std::memory_order_relaxed) == td_token.send_count[rank])) {
#endif
								bool allConfirmed = true;
								for (int i = 0; i < num_total_procs; i++) {
									if (td_token.status[i] != 2) {
										allConfirmed = false;
										break;
									}
								}
								if (allConfirmed) {
									unsigned long int sum_recv = 0;
									unsigned long int sum_send = 0;
									for (int i = 0; i < num_total_procs; i++) {
										sum_recv += td_token.recv_count[i];
										sum_send += td_token.send_count[i];
									}
									if (sum_recv == sum_send) {
										if (rank == 0) {
											my_color = DPSR_PINK;
											next_token = DPSR_PINK;
											for (int i = 0; i < num_total_procs; i++) {
												if (rank != i) {
													td_token.recv_count[i] = 0;
													td_token.send_count[i] = 0;
													td_token.status[i] = 2;
												} else {
													td_token.recv_count[rank] = ctrl_recvd.load(std::memory_order_relaxed);
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
													td_token.send_count[rank] = ctrl_sent;
#else
													td_token.send_count[rank] = ctrl_sent.load(std::memory_order_relaxed);
#endif
													td_token.status[rank] = 3;
												}
											}
										}
									} else {
										for (int i = 0; i < num_total_procs; i++) {
											if (rank != i) {
												td_token.recv_count[i] = 0;
												td_token.send_count[i] = 0;
												td_token.status[i] = 0;
											} else {
												td_token.recv_count[i] = chunks_recvd.load(std::memory_order_relaxed);
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
												td_token.send_count[i] = chunks_sent;
#else
												td_token.send_count[i] = chunks_sent.load(std::memory_order_relaxed);
#endif
												td_token.status[i] = 1;
											}
										}
									}
								}
							} else {
								for (int i = 0; i < num_total_procs; i++) {
									if (rank != i) {
										td_token.recv_count[i] = 0;
										td_token.send_count[i] = 0;
										td_token.status[i] = 0;
									} else {
										td_token.recv_count[i] = chunks_recvd.load(std::memory_order_relaxed);
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
										td_token.send_count[i] = chunks_sent;
#else
										td_token.send_count[i] = chunks_sent.load(std::memory_order_relaxed);
#endif
										td_token.status[i] = 1;
									}
								}
							}
						}
						// forward message
						forward_token = 1;
					} else {
						forward_token = 0;
					}
				}
				break;
			case DPSR_PINK:
				my_color = DPSR_PINK;
				next_token = DPSR_PINK;
				if (td_token.status[rank] == 2) {	// STATUS: FINALIZING (UNSETTED)
					td_token.recv_count[rank] = ctrl_recvd.load(std::memory_order_relaxed);
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
					td_token.send_count[rank] = ctrl_sent;
#else
					td_token.send_count[rank] = ctrl_sent.load(std::memory_order_relaxed);
#endif
					td_token.status[rank] = 3;
				} else if (td_token.status[rank] == 3) {	// STATUS: FINALIZING (SETTED)
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
					if ((ctrl_recvd.load(std::memory_order_relaxed) == td_token.recv_count[rank]) && (ctrl_sent == td_token.send_count[rank])) {
#else
					if ((ctrl_recvd.load(std::memory_order_relaxed) == td_token.recv_count[rank]) && (ctrl_sent.load(std::memory_order_relaxed) == td_token.send_count[rank])) {
#endif
						td_token.status[rank] = 4;
					} else {
						for (int i = 0; i < num_total_procs; i++) {
							if (rank != i) {
								td_token.recv_count[i] = 0;
								td_token.send_count[i] = 0;
								td_token.status[i] = 2;
							} else {
								td_token.recv_count[i] = ctrl_recvd.load(std::memory_order_relaxed);
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
								td_token.send_count[i] = ctrl_sent;
#else
								td_token.send_count[i] = ctrl_sent.load(std::memory_order_relaxed);
#endif
								td_token.status[i] = 3;
							}
						}
					}
				} else if (td_token.status[rank] == 4) {	// STATUS: FINALIZING (CONFIRMED)
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
					if ((ctrl_recvd.load(std::memory_order_relaxed) == td_token.recv_count[rank]) && (ctrl_sent == td_token.send_count[rank])) {
#else
					if ((ctrl_recvd.load(std::memory_order_relaxed) == td_token.recv_count[rank]) && (ctrl_sent.load(std::memory_order_relaxed) == td_token.send_count[rank])) {
#endif
						bool allConfirmed = true;
						for (int i = 0; i < num_total_procs; i++) {
							if (td_token.status[i] != 4) {
								allConfirmed = false;
								break;
							}
						}
						if (allConfirmed) {
							unsigned long int sum_recv = 0;
							unsigned long int sum_send = 0;
							for (int i = 0; i < num_total_procs; i++) {
								sum_recv += td_token.recv_count[i];
								sum_send += td_token.send_count[i];
							}
							if (sum_recv == sum_send) {
								if (rank == 0) {
									// Termination detected, pass DPSR_RED token
									my_color = DPSR_RED;
									next_token = DPSR_RED;
								}
							} else {
								for (int i = 0; i < num_total_procs; i++) {
									if (rank != i) {
										td_token.recv_count[i] = 0;
										td_token.send_count[i] = 0;
										td_token.status[i] = 2;
									} else {
										td_token.recv_count[i] = ctrl_recvd.load(std::memory_order_relaxed);
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
										td_token.send_count[i] = ctrl_sent;
#else
										td_token.send_count[i] = ctrl_sent.load(std::memory_order_relaxed);
#endif
										td_token.status[i] = 3;
									}
								}
							}
						}
					} else {
						for (int i = 0; i < num_total_procs; i++) {
							if (rank != i) {
								td_token.recv_count[i] = 0;
								td_token.send_count[i] = 0;
								td_token.status[i] = 2;
							} else {
								td_token.recv_count[i] = ctrl_recvd.load(std::memory_order_relaxed);
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
								td_token.send_count[i] = ctrl_sent;
#else
								td_token.send_count[i] = ctrl_sent.load(std::memory_order_relaxed);
#endif
								td_token.status[i] = 3;
							}
						}
					}
				}
				forward_token = 1;
				break;
			case DPSR_RED:
				// Termination: Set our state to DPSR_RED and circulate term message
				my_color = DPSR_RED;
				next_token = DPSR_RED;
				if (rank == num_total_procs - 1) {
					forward_token = 0;
				} else {
					forward_token = 1;
				}
		}

		/* Forward the token to the next node in the ring */
		if (forward_token) {
			td_token.color = next_token;
			token_to_tokenbuff(td_token, td_token_buff);
			{
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
				std::lock_guard<std::mutex> lck(mpiMutex);
#endif
				MPI_Send(&td_token_buff[0], 1+num_total_procs*3, MPI_UNSIGNED_LONG, (rank+1)%num_total_procs, DPSR_MPIWS_TDTOKEN, MPI_COMM_WORLD);
			}

			if (my_color != DPSR_RED) {
				/* re-Post termination detection listener */
				int j = (rank == 0) ? num_total_procs - 1 : rank - 1; // Receive the token from the processor to your left
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
				std::lock_guard<std::mutex> lck(mpiMutex);
#endif
				MPI_Irecv(&td_token_buff[0], 1+num_total_procs*3, MPI_UNSIGNED_LONG, j, DPSR_MPIWS_TDTOKEN, MPI_COMM_WORLD, &td_request);
			}
		}

		if (my_color == DPSR_RED) {
			// This is safe now that the pink token has mopped up all outstanding messages.
			clear_mpi_communications();
			PROFILEACTION(ss_setState(s, DPR_SS_IDLE));
			return -1;
		}
		return 0;
	}

	inline void clear_mpi_communications() {
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
		if ((ow_request != MPI_REQUEST_NULL) || (wrin_request != MPI_REQUEST_NULL) || (iw_request != MPI_REQUEST_NULL) || (wrout_request != MPI_REQUEST_NULL) || (td_request != MPI_REQUEST_NULL)) {
			std::lock_guard<std::mutex> lck(mpiMutex);
#endif
			if (ow_request != MPI_REQUEST_NULL) {
				MPI_Wait(&ow_request, MPI_STATUS_IGNORE);
			}
			if (wrin_request != MPI_REQUEST_NULL) {
				MPI_Cancel(&wrin_request);
				MPI_Wait(&wrin_request, MPI_STATUS_IGNORE);
			}
			if (iw_request != MPI_REQUEST_NULL) {
				MPI_Cancel(&iw_request);
				MPI_Wait(&iw_request, MPI_STATUS_IGNORE);
			}
			if (wrout_request != MPI_REQUEST_NULL) {
				MPI_Cancel(&wrout_request);
				MPI_Wait(&wrout_request, MPI_STATUS_IGNORE);
			}
			if (td_request != MPI_REQUEST_NULL) {
				MPI_Cancel(&td_request);
				MPI_Wait(&td_request, MPI_STATUS_IGNORE);
			}

#ifndef DPR_USE_MPI_THREAD_MULTIPLE
		}
#endif
	}

	/* restore stack to empty state */
	static void ss_mkEmpty(internal_mpi::StealStack<T> *s) {
		std::lock_guard<std::mutex> lck(s->stackLock);
		s->sharedStart = 0;
		s->local = 0;
		s->top = 0;
	}

	static void ss_init(internal_mpi::StealStack<T> *s, const int nelts, const Behavior behavior) {
		// allocate stack in shared addr space with affinity to calling thread
		// and record local addr for efficient access in sequel
		s->stack.resize(nelts);
		s->stackSize = nelts;
		s->nNodes = 0;
		s->statusJobSearch = 0;
		if (behavior & GatherInput) {
			s->baseInputStack.resize(nelts);
			s->baseInputStackSize = nelts;
			s->baseInputTop = 0;
		}
#ifdef PROFILE
		s->nLeaves = 0;
		s->maxStackDepth = 0;
		//s->maxTreeDepth = 0;
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

#ifdef PROFILE
	static void ss_initState(internal_mpi::StealStack<T> *s) {
		s->timeLast = getTimepoint();
		for (int i = 0; i < DPR_SS_NSTATES; i++) {
			s->time[i] = 0.0;
			s->entries[i] = 0;
		}
		s->curState = DPR_SS_IDLE;
	}

	void ss_setState(internal_mpi::StealStack<T> *s, const int state){
		if (state < 0 || state >= DPR_SS_NSTATES) {
			ss_error("ss_setState: thread state out of range");
		}
		if (state == s->curState) {
			return;
		}
		const std::chrono::steady_clock::time_point time = getTimepoint();
		s->time[s->curState] += getTimediff(s->timeLast, time);
		s->entries[state]++;
		s->timeLast = time;
		s->curState = state;
	}

#endif //PROFILE

#ifndef DPR_DENY_STACK_RESIZE
	/* local grow the size of the stack */
	void ss_growStackSize(internal_mpi::StealStack<T> *s) {
		std::lock_guard<std::mutex> lck(s->stackLock);
		s->stackSize += ((s->stackSize >> 1) + 2);
		if ((s->sharedStart > 0) && (s->top > s->sharedStart)) {
			std::vector<T> newStack;
			newStack.reserve(s->stackSize);
			newStack.insert(newStack.begin(), std::make_move_iterator(s->stack.begin()+s->sharedStart), std::make_move_iterator(s->stack.begin()+s->top));
			newStack.resize(s->stackSize);
			s->stack = std::move(newStack);
			s->top -= s->sharedStart;
			s->local -= s->sharedStart;
			s->sharedStart = 0;
		} else {
			s->stack.resize(s->stackSize);
		}
	}

	/* local grow the size of the stack and modify index */
	void ss_growStackSize(internal_mpi::StealStack<T> *s, int& index) {
		std::lock_guard<std::mutex> lck(s->stackLock);
		s->stackSize += ((s->stackSize >> 1) + 2);
		if ((s->sharedStart > 0) && (s->top > s->sharedStart)) {
			std::vector<T> newStack;
			newStack.reserve(s->stackSize);
			newStack.insert(newStack.begin(), std::make_move_iterator(s->stack.begin()+s->sharedStart), std::make_move_iterator(s->stack.begin()+s->top));
			newStack.resize(s->stackSize);
			s->stack = std::move(newStack);
			s->top -= s->sharedStart;
			s->local -= s->sharedStart;
			index -= s->sharedStart;
			s->sharedStart = 0;
		} else {
			s->stack.resize(s->stackSize);
		}
	}
#endif

	/* check the stack size of the stack */
	void ss_checkStackSize(internal_mpi::StealStack<T> *s, const int numToAdd, T *&data) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top+numToAdd > s->stackSize) {
			ss_error("ss_checkStackSize: stack overflow");
		}
#endif
#else
		while(s->top+numToAdd > s->stackSize) {
			ss_growStackSize(s);
			data = ss_top(s);
		}
#endif
	}

	void ss_growBaseInputStackSize(internal_mpi::StealStack<T> *s) {
		s->baseInputStackSize += ((s->baseInputStackSize >> 1) + 2);
		s->baseInputStack.resize(s->baseInputStackSize);
	}

	/* local push */
	void ss_push(internal_mpi::StealStack<T> *s, const T& c) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s);
		}
#endif
		s->stack[s->top] = c;
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c.height));
	}

	/* local push (with inner call to child generator instead of receive child directly) */
	void ss_push(internal_mpi::StealStack<T> *s, const Info& info, const int i, T& data) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s);
		}
#endif
		s->stack[s->top] = info.child(i, data);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local push and keep a valid reference to data */
	void ss_push(internal_mpi::StealStack<T> *s, const T &c, T *&data, const int data_index) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s, data_index);
			data = &(s->stack[data_index]);
		}
#endif
		s->stack[s->top] = c;
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local push (with inner call to child generator instead of receive child directly) and keep a valid reference to data */
	void ss_push(internal_mpi::StealStack<T> *s, const Info& info, const int i, T *&data, const int data_index) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s, data_index);
			data = &(s->stack[data_index]);
		}
#endif
		s->stack[s->top] = info.child(i, *data);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local simple push - only push, don't check the stack size */
	void ss_push_simple(internal_mpi::StealStack<T> *s, const T& c) {
		s->stack[s->top] = c;
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c.height));
	}

	/* local simple push (with inner call to child generator instead of receive child directly) - only push, don't check the stack size */
	void ss_push_simple(internal_mpi::StealStack<T> *s, const Info& info, const int i, T& data) {
		s->stack[s->top] = info.child(i, data);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local simple push (with inner call to child generator instead of receive child directly) and keep a valid reference to data - only push, don't check the stack size */
	void ss_push_simple(internal_mpi::StealStack<T> *s, const Info& info, const int i, T *&data) {
		s->stack[s->top] = info.child(i, *data);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local push delayed (get memory address for future copy instead copy directly) */
	void ss_push_delayed(internal_mpi::StealStack<T> *s, T *&c) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push_delayed: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s);
		}
#endif
		c = &(s->stack[s->top]);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local push delayed (get memory address for future copy instead copy directly) and keep a valid reference to data */
	void ss_push_delayed(internal_mpi::StealStack<T> *s, T *&c, T *&data, const int data_index) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->top >= s->stackSize) {
			ss_error("ss_push_delayed: stack overflow");
		}
#endif
#else
		while(s->top >= s->stackSize) {
			ss_growStackSize(s, data_index);
			data = &(s->stack[data_index]);
		}
#endif
		c = &(s->stack[s->top]);
		s->top++;
		PROFILEACTION(s->maxStackDepth = std::max(s->top, s->maxStackDepth));
		//PROFILEACTION(s->maxTreeDepth = std::max(s->maxTreeDepth, c->height));
	}

	/* local push_copy */
	void ss_push_copy(internal_mpi::StealStack<T> *s, const T& c) {
#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
		if (s->baseInputTop >= s->baseInputStackSize) {
			ss_error("ss_push_copy: baseInputStack overflow");
		}
#endif
#else
		while(s->baseInputTop >= s->baseInputStackSize) {
			ss_growBaseInputStackSize(s);
		}
#endif
		s->baseInputStack[s->baseInputTop] = c;
		s->baseInputTop++;
	}

	/* local top:  get local addr of node at top */
	inline T* ss_top(internal_mpi::StealStack<T> *s) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->top <= s->local) {
			ss_error("ss_top: empty local stack");
		}
#endif
		return &(s->stack[(s->top) - 1]);
	}

	/* local top:  get local addr of node at top */
	inline T* ss_top_copy(internal_mpi::StealStack<T> *s) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->baseInputTop <= 0) {
			ss_error("ss_top_copy: empty local baseInputStack");
		}
#endif
		return &(s->baseInputStack[(s->baseInputTop) - 1]);
	}

	/* local set value at position: change single element of the stack by other */
	inline void ss_setvalue_pos(internal_mpi::StealStack<T> *s, const T& c, const int position) {
		s->stack[position] = c;
	}

	/* local set value at position delayed: change single element of the stack by other (get memory address for future copy instead copy directly) */
	inline void ss_setvalue_pos_delayed(internal_mpi::StealStack<T> *s, T *&c, const int position) {
		c = &(s->stack[position]);
	}

	/* local pop */
	inline void ss_pop(internal_mpi::StealStack<T> *s) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->top <= s->local) {
			ss_error("ss_pop: empty local stack");
		}
#endif
		s->top--;
	}

	/* local pop_copy */
	inline void ss_pop_copy(internal_mpi::StealStack<T> *s) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->baseInputTop <= 0) {
			ss_error("ss_pop_copy: empty local baseInputStack");
		}
#endif
		s->baseInputTop--;
	}

	/* local top position:  stack index of top element */
	inline int ss_topPosn(const internal_mpi::StealStack<T> *s) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->top <= s->local) {
			ss_error("ss_topPosn: empty local stack");
		}
#endif
		return s->top - 1;
	}

	/* local depth */
	static constexpr int ss_localDepth(const internal_mpi::StealStack<T> *s) {
		return (s->top - s->local);
	}

	/* local full depth */
	static constexpr int ss_localFullDepth(const internal_mpi::StealStack<T> *s) {
		return (s->top - s->sharedStart);
	}

	/* shared depth */
	static constexpr int ss_sharedDepth(const internal_mpi::StealStack<T> *s) {
		return (s->local - s->sharedStart);
	}

	/* all threads depth */
	int ss_allThreadsLocalFullDepth(const internal_mpi::StealStack<T> *s) {
		int totalThreadsDepth = 0;
		for (int i = 0; i < num_total_threads; ++i) {
			totalThreadsDepth += ss_localFullDepth(stealStack[i]);
		}
		return totalThreadsDepth;
	}

	/* release k values from bottom of local stack */
	void ss_release(internal_mpi::StealStack<T> *s, const int chunkSize) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (s->top - s->local >= chunkSize) {
#endif
			s->local += chunkSize;
			PROFILEACTION(s->nRelease++);
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		} else {
			ss_error("ss_release: do not have chunks vals to release");
		}
#endif
	}

	/* move chunkSize values from top of shared stack into local stack
	 * return false if chunkSize vals are not avail on shared stack
	 */
	bool ss_acquire(internal_mpi::StealStack<T> *s, const int chunkSize) {
		std::lock_guard<std::mutex> lck(s->stackLock);
		if (ss_sharedDepth(s) >= chunkSize) {
			s->local -= chunkSize;
			PROFILEACTION(s->nAcquire++);
			return true;
		} else {
			return false;
		}
	}

	/* lock victim stack and try to reserve chunkSize elts from local process */
	bool ss_steal(internal_mpi::StealStack<T> *s, const int victim, const int chunkSize) {
		if (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_AGGRESSIVE) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
			if (s->sharedStart != s->top) {
				ss_error("ss_steal: thief attempts to steal onto non-empty stack");
			}
#endif

#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
			if (s->top + chunkSize >= s->stackSize) {
				ss_error("ss_steal: steal will overflow thief's stack");
			}
#endif
#else
			while(s->top + chunkSize >= s->stackSize) {
				ss_growStackSize(s);
			}
#endif

			/* lock victim stack and try to reserve chunkSize elts */
			std::lock_guard<std::mutex> lck(stealStack[victim]->stackLock);

			const int victimWorkAvail = stealStack[victim]->local - stealStack[victim]->sharedStart;
			if (victimWorkAvail >= chunkSize) {
				/* reserve a chunk */
				const int victimShared = stealStack[victim]->sharedStart;

#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
				const int victimLocal = stealStack[victim]->local;
				if (victimLocal - victimShared != victimWorkAvail) {
					ss_error("ss_steal: stealStack invariant violated");
				}
#endif

				/* if k elts reserved, move them to local portion of our stack */
				std::copy_n(stealStack[victim]->stack.begin() + victimShared, chunkSize, s->stack.begin()+ s->top);

				PROFILEACTION(s->nSteal++);
				s->top += chunkSize;
				stealStack[victim]->sharedStart = victimShared + chunkSize;
				return true;
			} else {
				PROFILEACTION(s->nFail++);
				return false;
			}
		} else {
			std::lock_guard<std::mutex> lck2(requestStatusMutex);
			if (s->statusJobSearch == 0) {
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
				if (s->sharedStart != s->top) {
					ss_error("ss_steal: thief attempts to steal onto non-empty stack");
				}
#endif

#ifdef DPR_DENY_STACK_RESIZE
#ifndef DPR_SKIP_CHECK_STACKOVERFLOW
				if (s->top + chunkSize >= s->stackSize) {
					ss_error("ss_steal: steal will overflow thief's stack");
				}
#endif
#else
				while(s->top + chunkSize >= s->stackSize) {
					ss_growStackSize(s);
				}
#endif

				/* lock victim stack and try to reserve chunkSize elts */
				std::lock_guard<std::mutex> lck(stealStack[victim]->stackLock);

				const int victimWorkAvail = stealStack[victim]->local - stealStack[victim]->sharedStart;
				if (victimWorkAvail >= chunkSize) {
					/* reserve a chunk */
					const int victimShared = stealStack[victim]->sharedStart;

#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
					const int victimLocal = stealStack[victim]->local;
					if (victimLocal - victimShared != victimWorkAvail) {
						ss_error("ss_steal: stealStack invariant violated");
					}
#endif

					/* if k elts reserved, move them to local portion of our stack */
					std::copy_n(stealStack[victim]->stack.begin() + victimShared, chunkSize, s->stack.begin()+ s->top);

					PROFILEACTION(s->nSteal++);
					s->top += chunkSize;
					stealStack[victim]->sharedStart = victimShared + chunkSize;
					return true;
				} else {
					PROFILEACTION(s->nFail++);
					return false;
				}
			} else {
				return false;
			}
		}
	}

	inline void findwork_for_remote(std::vector<int>& listThreadsWithWork, const int chunkSize, const int chunksToSteal, const int startFindThread) {
		listThreadsWithWork.clear();
		int tempWorkAvail[num_total_threads];
		for (int i = 0; i < num_total_threads; i++) {
			const int v = (startFindThread + i) % num_total_threads;
			tempWorkAvail[v] = stealStack[v]->local - stealStack[v]->sharedStart;
			if (tempWorkAvail[v] >= (chunkSize*chunksToSteal)) {
				int insPos = 0;
				for (const int& iEl: listThreadsWithWork) {
					if (tempWorkAvail[v] > tempWorkAvail[iEl]) {
						break;
					} else {
						++insPos;
					}
				}
				listThreadsWithWork.insert(listThreadsWithWork.begin() + insPos, v);
			}
		}
	}

	bool ss_steal_for_remote(internal_mpi::StealStack<T> *s, std::vector<int>& victimList, const int chunkSize, const int chunksToSteal, const int nWorkSize, const int index, const int threadDest) {
		const int maxTrysToGetAllRequestedWork = 3;
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
		if (nWorkSize <= 0) {
			ss_error("ss_steal_for_remote: wrong number of work to steal");
		}
		if (victimList.size() == 0) {
			ss_error("ss_steal_for_remote: victimList empty");
		}
#endif

		int nWorkRemain = nWorkSize;
		int tryNumber = 0;
		while((nWorkRemain > 0) && (victimList.size() > 0) && (tryNumber < maxTrysToGetAllRequestedWork)) {
			for (const int& victim : victimList) {

				if (nWorkRemain > 0) {
					std::lock_guard<std::mutex> lck(stealStack[victim]->stackLock);

					const int victimWorkAvail = stealStack[victim]->local - stealStack[victim]->sharedStart;
					const int numSteals = std::min(victimWorkAvail / (chunkSize*chunksToSteal), nWorkRemain);

					if (numSteals > 0) {
						/* reserve a chunk */
						const int victimShared = stealStack[victim]->sharedStart;
#if defined(PROFILE) || defined(DPR_ENABLE_CONSISTENCY_CHECKS)
						const int victimLocal = stealStack[victim]->local;
						if (victimLocal - victimShared != victimWorkAvail) {
							ss_error("ss_steal_for_remote: stealStack invariant violated");
						}
#endif

						std::copy_n(stealStack[victim]->stack.begin() + victimShared, chunkSize*chunksToSteal*numSteals, ow_buff.begin() + (chunkSize*chunksToSteal*(nWorkSize-nWorkRemain)));

						stealStack[victim]->sharedStart = victimShared + chunkSize*chunksToSteal*numSteals;
						nWorkRemain -= numSteals;

						PROFILEACTION(s->nSteal++);
					} else {
						PROFILEACTION(s->nFail++);
					}
				}
			}
			if (nWorkRemain == nWorkSize) {
				// It has not been possible to steal any work
				return false;
			} else {
				// Only some work has been stolen
				tryNumber++;
				victimList.clear();
				findwork_for_remote(victimList, chunkSize, chunksToSteal, nextStartFindThread);
			}
		}
		if (nWorkRemain < nWorkSize) {
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
			std::lock_guard<std::mutex> lck(mpiMutex);
			++chunks_sent;
#else
			chunks_sent.fetch_add(1, std::memory_order_relaxed);
#endif
			dpr::internal_mpi::st_isend(index, (DPSR_MPIWS_WORKRESPONSE | (threadDest<<8)), ow_buff.data(), chunkSize*chunksToSteal*(nWorkSize-nWorkRemain), ow_raw_buffer, ow_request, nullptr);
			return true;
		} else {
			return false;
		}
	}

	inline int findwork(const int chunkSize) {
		for (int i = 1; i < num_total_threads; i++) {
			const int v = (threadId + i) % num_total_threads;
			if (stealStack[v]->local - stealStack[v]->sharedStart >= chunkSize) {
				return v;
			}
		}
		return -1;
	}

//	inline int findwork(const int chunkSize) {
//		int maxThreadId = -1;
//		int maxThreadWork = 0;
//		for (int i = 1; i < num_total_threads; i++) {
//			const int v = (threadId + i) % num_total_threads;
//			const int threadWork = stealStack[v]->local - stealStack[v]->sharedStart;
//			if (threadWork >= chunkSize && threadWork > maxThreadWork) {
//				maxThreadId = v;
//				maxThreadWork = threadWork;
//			}
//		}
//		return maxThreadId;
//	}

	void releaseNodes(internal_mpi::StealStack<T> *ss){
		if (ss_localDepth(ss) > doubleChunkSize) {
			// Attribute this time to runtime overhead
			PROFILEACTION(ss_setState(ss, DPR_SS_OVH));
			ss_release(ss, chunkSize);
			PROFILEACTION(ss_setState(ss, DPR_SS_CBOVH));
			if (num_total_procs > 1) {
#ifndef DPR_USE_REGULAR_POLLING_INTERVAL
				if (ss->polling_count % polling_interval == 0) {
					ws_make_progress(ss);
				}
				ss->polling_count++;
#endif
			} else {
				cbarrier_cancel();
			}
			PROFILEACTION(ss_setState(ss, DPR_SS_WORK));
		}
	}

	void releaseMultipleNodes(internal_mpi::StealStack<T> *ss){
		if (ss_localDepth(ss) > doubleChunkSize) {
			// Attribute this time to runtime overhead
			PROFILEACTION(ss_setState(ss, DPR_SS_OVH));
			ss_release(ss, chunkSize*(ss_localDepth(ss) / chunkSize));
			PROFILEACTION(ss_setState(ss, DPR_SS_CBOVH));
			if (num_total_procs > 1) {
#ifndef DPR_USE_REGULAR_POLLING_INTERVAL
				if (ss->polling_count % polling_interval == 0) {
					ws_make_progress(ss);
				}
				ss->polling_count++;
#endif
			} else {
				cbarrier_cancel();
			}
			PROFILEACTION(ss_setState(ss, DPR_SS_WORK));
		}
	}

	static constexpr bool is_mpi_status_empty(MPI_Status& status) {
		return (status.MPI_TAG == MPI_ANY_TAG && status.MPI_SOURCE == MPI_ANY_SOURCE && status.MPI_ERROR == MPI_SUCCESS);
	}

	static void cb_init() {
		cb.cb_count.store(0, std::memory_order_relaxed);
		cb.cb_cancel = false;
		cb.cb_done = false;
		globalExceptionPtr() = nullptr;
		mpi_processing = 0;
		my_color = DPSR_WHITE;
		chunks_recvd.store(0, std::memory_order_relaxed);
		ctrl_recvd.store(0, std::memory_order_relaxed);
#ifndef DPR_USE_MPI_THREAD_MULTIPLE
		chunks_sent = 0;
		ctrl_sent = 0;
#else
		chunks_sent.store(0, std::memory_order_relaxed);
		ctrl_sent.store(0, std::memory_order_relaxed);
#endif
	}

	//  delay this thread until all threads arrive at barrier or until barrier is cancelled
	bool cbarrier_wait() {
		cb.cb_count.fetch_add(1, std::memory_order_relaxed);
		if (cb.cb_count.load(std::memory_order_relaxed) == num_total_threads) {
#ifdef DPR_DONT_USE_SPINLOCK
			std::lock_guard<std::mutex> lck(cb.cb_cv_mtx);
			cb.cb_done = true;
			cb.cb_cv.notify_all();
#else
			cb.cb_done = true;
#endif
		}

#ifdef PROFILE
		if (stealStack[threadId]->nNodes_last == stealStack[threadId]->nNodes) {
			++stealStack[threadId]->falseWakeups;
		}
		stealStack[threadId]->nNodes_last = stealStack[threadId]->nNodes;
#endif //PROFILE

#ifdef DPR_DONT_USE_SPINLOCK
		{
			std::unique_lock<std::mutex> lck(cb.cb_cv_mtx);
			cb.cb_cv.wait(lck, [this] { return (cb.cb_cancel || cb.cb_done); } );
		}
#else
		// busy waiting: spinlock
		while (!cb.cb_cancel && !cb.cb_done);
#endif

		cb.cb_count.fetch_sub(1, std::memory_order_relaxed);
		cb.cb_cancel = false;
		PROFILEACTION(++stealStack[threadId]->wakeups);

		return cb.cb_done;
	}

	// causes one or more threads waiting at barrier, if any, to be released
	inline static void cbarrier_cancel() {
#ifdef DPR_DONT_USE_SPINLOCK
		std::lock_guard<std::mutex> lck(cb.cb_cv_mtx);
		cb.cb_cancel = true;
		cb.cb_cv.notify_all();
#else
		cb.cb_cancel = true;
#endif
	}

};

template<typename T, typename Info>
alignas(64) typename parallel_stack_recursion_internal_mpi<T, Info>::cb_t parallel_stack_recursion_internal_mpi<T, Info>::cb;

template<typename T, typename Info>
int parallel_stack_recursion_internal_mpi<T, Info>::mpi_processing;

template<typename T, typename Info>
enum dpsr_td_colors parallel_stack_recursion_internal_mpi<T, Info>::my_color;

template<typename T, typename Info>
std::atomic<unsigned long int> parallel_stack_recursion_internal_mpi<T, Info>::chunks_recvd;

template<typename T, typename Info>
std::atomic<unsigned long int> parallel_stack_recursion_internal_mpi<T, Info>::ctrl_recvd;

#ifndef DPR_USE_MPI_THREAD_MULTIPLE

template<typename T, typename Info>
unsigned long int parallel_stack_recursion_internal_mpi<T, Info>::chunks_sent;

template<typename T, typename Info>
unsigned long int parallel_stack_recursion_internal_mpi<T, Info>::ctrl_sent;

#else

template<typename T, typename Info>
std::atomic<unsigned long int> parallel_stack_recursion_internal_mpi<T, Info>::chunks_sent;

template<typename T, typename Info>
std::atomic<unsigned long int> parallel_stack_recursion_internal_mpi<T, Info>::ctrl_sent;

#endif

#ifndef DPR_USE_MPI_THREAD_MULTIPLE
template<typename T, typename Info>
std::mutex parallel_stack_recursion_internal_mpi<T, Info>::mpiMutex;
#endif

template<typename T, typename Info>
std::mutex parallel_stack_recursion_internal_mpi<T, Info>::requestStatusMutex;

template<typename T, typename Info>
internal_mpi::StealStack<T>** parallel_stack_recursion_internal_mpi<T, Info>::stealStack;

template<typename Return, typename T, typename Info, typename Body, typename Partitioner>
class start_stack_recursion_mpi {
	typedef dpr::internal_mpi::parallel_stack_recursion_internal_mpi<typename std::remove_reference<T>::type, Info> internal_mpi_psr;

public:

	static Return run(general_reference_wrapper<T> root, const Info& info, Body body, const int rank = internal_mpi::dpr_rank(), const int num_total_procs = internal_mpi::dpr_nprocs(), const int chunkSizeI = dpr::internal_mpi::defaultChunkSize, const Behavior behavior = Behavior(DefaultBehavior), Return * const dest = nullptr, const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default, typename std::decay<T>::type * const rootTestV = nullptr) {
		int chunkSize = chunkSizeI;
		const int num_total_threads = internal_mpi::num_total_threads(dspar_config_info);
		const int chunksToSteal = internal_mpi::chunksToStealGl(dspar_config_info);
		const int threadsRequestPolicy = internal_mpi::threadsRequestPolicyGl(dspar_config_info);
		const int mpiWorkRequestLimit = ((internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) >= num_total_threads) || (internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) == 0)) ? num_total_threads : ((internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) < 0) ? ((std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)) > 0) ? ((std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)) < num_total_threads) ? std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)):num_total_threads):1) : internal_mpi::mpiWorkRequestLimitGl(dspar_config_info));
		const int trpPredictWorkCount = (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_PREDICTIVE) ? (((internal_mpi::trpPredictWorkCountGl(dspar_config_info) >= num_total_threads) || (internal_mpi::trpPredictWorkCountGl(dspar_config_info) == 0) || (internal_mpi::trpPredictWorkCountGl(dspar_config_info) < TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_QUARTERS)) ? num_total_threads : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_QUARTERS) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)-1)/4+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_HALF) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)-1)/2+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_THREE_QUARTERS) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)*3-1)/4+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_ALL) ? (num_total_threads-1)/mpiWorkRequestLimit+1 : internal_mpi::trpPredictWorkCountGl(dspar_config_info)))))) : 0;
		const int pollingInterval = internal_mpi::pollingIntervalGl(dspar_config_info);
		dpr::internal_mpi::lastDpsrRunExtraInfo().chunkSize = chunkSize;
		dpr::internal_mpi::lastDpsrRunExtraInfo().nthreads = num_total_threads;
		std::chrono::steady_clock::time_point init_run_time;
		if (chunkSize <= 0) {
			T rootTest((rootTestV == nullptr) ? root.get() : *rootTestV);
			const std::chrono::steady_clock::time_point init_test_time = getTimepoint();
			if (behavior & ReplicateInput) {
				dpr::internal_mpi::st_bcast(rootTest, rank, 0);
			}
			const std::vector<dpr::ResultChunkTest> testResults = run_test(std::forward<T>(rootTest), info, body, rank, num_total_procs, chunkSize, behavior, dspar_config_info, opt);
			if (testResults.size() > 0) {
				chunkSize = testResults[0].chunkId;
			} else {
				chunkSize = defaultChunkSize;
			}
			init_run_time = getTimepoint();
			dpr::internal_mpi::lastDpsrRunExtraInfo().testChunkTime = getTimediff(init_test_time, init_run_time);
		} else {
			init_run_time = getTimepoint();
			dpr::internal_mpi::lastDpsrRunExtraInfo().testChunkTime = 0.0;
		}
		dpr::internal_mpi::lastDpsrRunExtraInfo().chunkSizeUsed = chunkSize;

		std::vector<Return> r(num_total_threads);

		internal_mpi_psr::cb_init();
		internal_mpi_psr::stealStack = new internal_mpi::StealStack<typename std::remove_reference<T>::type>*[num_total_threads];

		internal::AlignedBufferedAllocator stealStackAllocator((sizeof(internal_mpi::StealStack<typename std::remove_reference<T>::type>)+64)*num_total_threads);

		if (behavior & ReplicateInput) {
			dpr::internal_mpi::st_bcast(root.get(), rank, 0);
		}

		for (int threadId = 0; threadId < num_total_threads; threadId++) {
			internal_mpi_psr::stealStack[threadId] = stealStackAllocator.aligned_alloc<internal_mpi::StealStack<typename std::remove_reference<T>::type>>(64);
			internal_mpi_psr::ss_init(internal_mpi_psr::stealStack[threadId], internal_mpi::initialStackSize(), behavior);
		}

		internal::do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info = &info;
		std::vector<std::thread> threads;
		dpr::internal::thread_barrier threadBarrier(num_total_threads);
		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			PROFILEACTION(internal_mpi_psr::ss_initState(internal_mpi_psr::stealStack[threadId]));
			threads.push_back(std::thread(run_thread, threadId, num_total_threads, rank, num_total_procs, chunkSize, chunksToSteal, static_cast<unsigned int>(pollingInterval), threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount, internal_mpi_psr::stealStack[threadId], std::ref(root), info, body, std::ref(r[threadId]), behavior, std::ref(threadBarrier)));
		}

		Return rrr{};
		Return *rr = ((dest!=nullptr) ? dest : &rrr);

		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			threads[threadId].join();
			if (globalExceptionPtr() == nullptr) {
				body.post(r[threadId], *rr);
			}
		}
		if (globalExceptionPtr() != nullptr) {
			try {
				std::rethrow_exception(globalExceptionPtr());
			}catch (const std::exception &ex) {
				std::cerr << ex.what() << std::endl;
				std::rethrow_exception(globalExceptionPtr());
			}
		}
		threads.clear();

		// Post results of all processes
		if (num_total_procs > 1) {
			if (!(behavior & DistributedOutput)) {
				if (rank == 0) {
					Return rbuf[num_total_procs-1];
					for (int rankId = 1; rankId < num_total_procs; ++rankId) {
						dpr::internal_mpi::st_recv(rankId, DPSR_MPIWS_REDUCTION, rbuf[rankId-1]);
					}
					for (int rankId = 1; rankId < num_total_procs; ++rankId) {
						body.post(rbuf[rankId-1], *rr);
					}
				} else {
					dpr::internal_mpi::st_send(0, DPSR_MPIWS_REDUCTION, *rr);
				}
				if (behavior & ReplicateOutput) {
					dpr::internal_mpi::st_bcast(*rr, rank, 0);
				} else {
					if (rank != 0) {
						*rr = Return{};
					}
				}
			}
			if (behavior & GatherInput) {
				if (rank == 0) {
					int inbuf_size = 0;
					for (int threadId = 0; threadId < num_total_threads; threadId++) {
						inbuf_size += internal_mpi_psr::stealStack[threadId]->baseInputTop;
					}
					for (int rankId = 1; rankId < num_total_procs; ++rankId) {
						int buffSize;
						MPI_Recv(&buffSize, 1, MPI_INT, MPI_ANY_SOURCE, DPSR_MPIWS_REDUCTION_INFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						inbuf_size += buffSize;
					}
					std::vector<T> inbuf(inbuf_size);
					int c = 0;
					for (int threadId = 0; threadId < num_total_threads; threadId++) {
						std::copy_n(internal_mpi_psr::stealStack[threadId]->baseInputStack.begin(), internal_mpi_psr::stealStack[threadId]->baseInputTop, inbuf.begin()+c);
						c += internal_mpi_psr::stealStack[threadId]->baseInputTop;
					}
					for (int rankId = 1; rankId < num_total_procs; ++rankId) {
						for (int threadId = 0; threadId < num_total_threads; threadId++) {
							int nelem;
							dpr::internal_mpi::st_recv(rankId, DPSR_MPIWS_REDUCTION, &(inbuf[c]), &nelem);
							c += nelem;
						}
					}
					assert(inbuf_size == c);
					for (int i = 0; i < c; ++i) {
						body.gather_input_post(inbuf[i], c, root.get());
					}
				} else {
					int procBaseTotalSize = 0;
					for (int threadId = 0; threadId < num_total_threads; threadId++) {
						procBaseTotalSize += internal_mpi_psr::stealStack[threadId]->baseInputTop;
					}
					MPI_Send(&procBaseTotalSize, 1, MPI_INT, 0, DPSR_MPIWS_REDUCTION_INFO, MPI_COMM_WORLD);
					for (int threadId = 0; threadId < num_total_threads; threadId++) {
						dpr::internal_mpi::st_send(0, DPSR_MPIWS_REDUCTION, internal_mpi_psr::stealStack[threadId]->baseInputStack.data(), internal_mpi_psr::stealStack[threadId]->baseInputTop);
					}
				}
				if (behavior & ReplicateOutput) {
					dpr::internal_mpi::st_bcast(root.get(), rank, 0);
				}
			}
			MPI_Barrier(MPI_COMM_WORLD);
		}

		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			internal_mpi_psr::stealStack[threadId]->stack.~vector();
			internal_mpi_psr::stealStack[threadId]->baseInputStack.~vector();
		}
		delete[] internal_mpi_psr::stealStack;

		dpr::internal_mpi::lastDpsrRunExtraInfo().runTime = getTimediff(init_run_time, getTimepoint());
		return *rr;
	}

	static std::vector<dpr::ResultChunkTest> run_test(general_reference_wrapper<T> root, const Info& info, Body body, const int rank = internal_mpi::dpr_rank(), const int num_total_procs = internal_mpi::dpr_nprocs(), const int chunkSizeI = 0, const Behavior behavior = Behavior(DefaultBehavior), const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_test_default) {
		const int num_total_threads = internal_mpi::num_total_threads(dspar_config_info);
		const int chunksToSteal = internal_mpi::chunksToStealGl(dspar_config_info);
		const int threadsRequestPolicy = internal_mpi::threadsRequestPolicyGl(dspar_config_info);
		const int mpiWorkRequestLimit = ((internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) >= num_total_threads) || (internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) == 0)) ? num_total_threads : ((internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) < 0) ? ((std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)) > 0) ? ((std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)) < num_total_threads) ? std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)):num_total_threads):1) : internal_mpi::mpiWorkRequestLimitGl(dspar_config_info));
		const int trpPredictWorkCount = (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_PREDICTIVE) ? (((internal_mpi::trpPredictWorkCountGl(dspar_config_info) >= num_total_threads) || (internal_mpi::trpPredictWorkCountGl(dspar_config_info) == 0) || (internal_mpi::trpPredictWorkCountGl(dspar_config_info) < TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_QUARTERS)) ? num_total_threads : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_QUARTERS) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)-1)/4+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_HALF) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)-1)/2+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_THREE_QUARTERS) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)*3-1)/4+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_ALL) ? (num_total_threads-1)/mpiWorkRequestLimit+1 : internal_mpi::trpPredictWorkCountGl(dspar_config_info)))))) : 0;
		const int pollingInterval = internal_mpi::pollingIntervalGl(dspar_config_info);
		std::vector<dpr::ResultChunkTest> res;
		std::vector<Return> r(num_total_threads);
		std::vector<unsigned long long int> nNodesArr(num_total_threads);
		std::vector<char> exitNormallyArr(num_total_threads);
		const size_t softLimitTestsSizeTest = 8;
		const size_t hardLimitTestsSizeTest = 14;
		const double softLimitMargin = 0.3;
		const double errorMargingCalculateSizeTest = 0.15;
		const int minorSizeTest = 250;

		if (rank == 0) {
			dpr::internal::ChunkSelector chunkSel(opt, chunkSizeI);

			std::map<int,unsigned long long int> mapSizeTestChunks;
			for (const std::pair<int, dpr::internal::chunk_attr> &iChunk : chunkSel.selectedChunks) {
				mapSizeTestChunks[iChunk.first] = 0;
			}

			double estimatedTimePerTest = opt.targetTimePerTest;
			if ((opt.limitTimeOfEachTest) && (opt.testSize == 0) && (estimatedTimePerTest <= 0.0000001)) {
				estimatedTimePerTest = chunkSel.estimateNeededTimePerTest();
				if (opt.verbose > 3) {
					std::cout << " [?] It has been estimated a necessary time per test of " << estimatedTimePerTest << " sec [±" << static_cast<int>(errorMargingCalculateSizeTest*100) << "%]" << std::endl;
				}
			} else {
				if (opt.verbose > 3) {
					if (opt.limitTimeOfEachTest) {
						if (opt.testSize == 0) {
							std::cout << " [?] A necessary time per test " << estimatedTimePerTest << " sec [±" << static_cast<int>(errorMargingCalculateSizeTest*100) << "%]" << " has been established" << std::endl;
						} else {
							std::cout << " [?] A fixed test size of " << opt.testSize << " nodes will be used" << std::endl;
						}
					} else {
						std::cout << " [?] No test size or test time limit has been established" << std::endl;
					}
				}
			}

			int checkNo = chunkSel.getNextChunk();
			unsigned long long int iSizeTest;
			while(checkNo > 0) {
				if (opt.verbose > 2) {
					std::cout << "  - Testing Chunk " << checkNo << "..." << std::flush;
				}
				if (opt.limitTimeOfEachTest) {
					iSizeTest = opt.testSize;
				} else {
					iSizeTest = std::numeric_limits<unsigned long long int>::max();
				}
				if (iSizeTest == 0) {
					iSizeTest = mapSizeTestChunks[checkNo];
				}
				if (iSizeTest == 0) {
					iSizeTest = minorSizeTest;
					int minorDistance = std::numeric_limits<int>::max();
					int minorDistChunk = -1;
					for (const std::pair<int, unsigned long long int> &iMap : mapSizeTestChunks) {
						if (iMap.second != 0) {
							const int distanceChunk = std::abs(checkNo - iMap.first);
							if (distanceChunk < minorDistance) {
								minorDistChunk = iMap.first;
								minorDistance = distanceChunk;
							}
						}
					}
					if (minorDistChunk > 0) {
						iSizeTest = mapSizeTestChunks[minorDistChunk];
					}
				}
				std::map<unsigned long long int, std::pair<double, double>> mapHistorySizeTests;
				char exitNormally = 0;
				bool firstMandatoryExecution = true;
				bool needMoreNodes = true;
				double mostAccurate = std::numeric_limits<double>::max();
				unsigned long long int bestSizeTest = 0;
				bool underLimit = false;

				while (firstMandatoryExecution || (
						(mapSizeTestChunks[checkNo] == 0) &&
						(mapHistorySizeTests.find(iSizeTest) == mapHistorySizeTests.end()) &&
						(mapHistorySizeTests.size() < hardLimitTestsSizeTest) &&
						(!underLimit) &&
						((mapHistorySizeTests.size() < softLimitTestsSizeTest) || (std::abs(mostAccurate) > softLimitMargin)) &&
						(!exitNormally || !needMoreNodes)
				)) {
					if ((opt.verbose > 3) && (opt.testSize == 0) && (mapSizeTestChunks[checkNo] == 0)) {
						if (firstMandatoryExecution) {
							std::cout << std::endl;
						}
						if (opt.limitTimeOfEachTest) {
							std::cout << "    · Testing with " << iSizeTest << " nodes..." << std::flush;
						}
					}
					firstMandatoryExecution = false;

					for (int rankId = 1; rankId < num_total_procs; ++rankId) {
						int resInfo[2];
						resInfo[0] = checkNo;
						resInfo[1] = iSizeTest;
						MPI_Send(resInfo, 2, MPI_INT, rankId, DPSR_MPIWS_TEST_NEXTPARAMS, MPI_COMM_WORLD);
					}

					general_reference_wrapper<T> localRoot = dpr::internal::call_clone<T>(root.get());

					unsigned long long int nNodes = 0;
					exitNormally = 1;

					const std::chrono::steady_clock::time_point init_time = getTimepoint();

					internal_mpi_psr::cb_init();
					internal_mpi_psr::stealStack = new internal_mpi::StealStack<typename std::remove_reference<T>::type>*[num_total_threads];

					internal::AlignedBufferedAllocator stealStackAllocator((sizeof(internal_mpi::StealStack<typename std::remove_reference<T>::type>)+64)*num_total_threads);

					if (behavior & ReplicateInput) {
						dpr::internal_mpi::st_bcast(localRoot.get(), rank, 0);
					}

					for (int threadId = 0; threadId < num_total_threads; threadId++) {
						internal_mpi_psr::stealStack[threadId] = stealStackAllocator.aligned_alloc<internal_mpi::StealStack<typename std::remove_reference<T>::type>>(64);
						internal_mpi_psr::ss_init(internal_mpi_psr::stealStack[threadId], internal_mpi::initialStackSize(), behavior);
					}

					internal::do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info = &info;
					std::vector<std::thread> threads;
					dpr::internal::thread_barrier threadBarrier(num_total_threads);
					for(int threadId = 0; threadId < num_total_threads; threadId++) {
						PROFILEACTION(internal_mpi_psr::ss_initState(internal_mpi_psr::stealStack[threadId]));
						threads.push_back(std::thread(run_thread_test, threadId, num_total_threads, rank, num_total_procs, checkNo, chunksToSteal, static_cast<unsigned int>(pollingInterval), threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount, internal_mpi_psr::stealStack[threadId], std::ref(localRoot), info, body, std::ref(r[threadId]), behavior, std::ref(nNodesArr[threadId]), iSizeTest, std::ref(exitNormallyArr[threadId]), std::ref(threadBarrier)));
					}

					Return rr{};
					for(int threadId = 0; threadId < num_total_threads; threadId++) {
						threads[threadId].join();
						if (globalExceptionPtr() == nullptr) {
							body.post(r[threadId], rr);
						}
					}
					if (globalExceptionPtr() != nullptr) {
						try {
							std::rethrow_exception(globalExceptionPtr());
						}catch (const std::exception &ex) {
							std::cerr << ex.what() << std::endl;
							std::rethrow_exception(globalExceptionPtr());
						}
					}
					threads.clear();

					for(int threadId = 0; threadId < num_total_threads; threadId++) {
						internal_mpi_psr::stealStack[threadId]->stack.~vector();
						internal_mpi_psr::stealStack[threadId]->baseInputStack.~vector();
					}
					delete[] internal_mpi_psr::stealStack;
					if (num_total_procs > 1) {
						MPI_Barrier(MPI_COMM_WORLD);
					}

					const std::chrono::steady_clock::time_point finish_time = getTimepoint();
					const double total_time = getTimediff(init_time, finish_time);

					dpr::internal::call_deallocate<T>(localRoot.get());

					for (int i = 0; i < num_total_threads; i++) {
						nNodes += nNodesArr[i];
					}
					if (opt.limitTimeOfEachTest) {
						for (int i = 0; i < num_total_threads; i++) {
							exitNormally &= exitNormallyArr[i];
						}
					}
					for (int rankId = 1; rankId < num_total_procs; ++rankId) {
						int resInfo[2];
						MPI_Recv(resInfo, 2, MPI_INT, MPI_ANY_SOURCE, DPSR_MPIWS_TEST_EXECRES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						nNodes += resInfo[0];
						exitNormally &= static_cast<char>(resInfo[1]);
					}

					const double nSpeed = nNodes / total_time;
					if ((opt.testSize == 0) && (opt.limitTimeOfEachTest)) {
						if (mapSizeTestChunks[checkNo] == 0) {
							const double diff = (estimatedTimePerTest-total_time)/estimatedTimePerTest;
							mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
							if (std::abs(diff) < std::abs(mostAccurate)) {
								bestSizeTest = iSizeTest;
								mostAccurate = diff;
							}
							if (std::abs(diff) < errorMargingCalculateSizeTest) {
								mapSizeTestChunks[checkNo] = iSizeTest;
							} else {
								if (diff > 0.96) {
									needMoreNodes = true;
									iSizeTest *= 25;
								} else if (diff > 0.0) {
									needMoreNodes = true;
									iSizeTest *= estimatedTimePerTest/total_time;
								} else if (diff < 0.0) {
									needMoreNodes = false;
									if (iSizeTest > minorSizeTest) {
										iSizeTest *= estimatedTimePerTest/total_time;
									} else {
										underLimit = true;
									}
								} else if (diff < -3.0) {
									needMoreNodes = false;
									iSizeTest *= 0.25;
								}
								if (iSizeTest < minorSizeTest) {
									iSizeTest = minorSizeTest;
								}
								while (mapHistorySizeTests.find(iSizeTest) != mapHistorySizeTests.end()) {
									iSizeTest += minorSizeTest;
								}
							}

							if (opt.verbose > 3) {
								if (mapSizeTestChunks[checkNo] == 0) {
									std::cout << "\tFailure! Time: " << total_time << " sec. Error: " << ((diff <= 0.0) ? "+" : "-") << std::abs(diff*100) << "%" << std::endl;
								} else {
									std::cout << "\tDone! Time: " << total_time << " sec. Error: " << ((diff <= 0.0) ? "+" : "-") << std::abs(diff*100) << "%" << std::endl << "  ";
								}
							}
						} else {
							mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
						}
					} else {
						mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
						mapSizeTestChunks[checkNo] = iSizeTest;
					}
				}
				if (mapSizeTestChunks[checkNo] == 0) {
					if (bestSizeTest > 0) {
						mapSizeTestChunks[checkNo] = bestSizeTest;
						if ((opt.verbose > 3) && (opt.testSize == 0)) {
							if ((mapHistorySizeTests.size() >= hardLimitTestsSizeTest)) {
								std::cout << "    · Hard limit of " << hardLimitTestsSizeTest << " test reached.";
							} else if ((mapHistorySizeTests.size() >= softLimitTestsSizeTest) && (std::abs(mostAccurate) <= softLimitMargin)) {
								std::cout << "    · Soft limit of " << softLimitTestsSizeTest << " test reached.";
							} else if (underLimit) {
								std::cout << "    · The execution time is too high for the minimum number of nodes (" << minorSizeTest << ").";
							} else if (exitNormally) {
								std::cout << "    · The number of nodes exceeds the number of nodes in the normal execution of the program.";
							} else {
								std::cout << "    ·";
							}
							std::cout << " Selecting the most accurate test: " << bestSizeTest << " nodes. Error: " << ((mostAccurate <= 0.0) ? "+" : "-") << std::abs(mostAccurate*100) << "%" << std::endl << "  ";
						}
					}
				}
				if (mapSizeTestChunks[checkNo] != 0) {
					const double nSpeed = mapHistorySizeTests[mapSizeTestChunks[checkNo]].second;
					chunkSel.setChunkSpeed(checkNo, nSpeed);
					if (opt.verbose > 2) {
						const double total_time = mapHistorySizeTests[mapSizeTestChunks[checkNo]].first;
						std::cout << "\tFinalized! Test Time: " << total_time << " sec. Test Speed Result: " << nSpeed << " nodes/sec" << std::endl;
					}
					checkNo = chunkSel.getNextChunk();
				} else {
					if (opt.verbose > 2) {
						std::cout << "\tFailed!" << std::endl;
					}
					checkNo = 0;
				}
			}
			for (int rankId = 1; rankId < num_total_procs; ++rankId) {
				int resInfo[2];
				resInfo[0] = -1;
				resInfo[1] = -1;
				MPI_Send(resInfo, 2, MPI_INT, rankId, DPSR_MPIWS_TEST_NEXTPARAMS, MPI_COMM_WORLD);
			}
			res = chunkSel.getChunkScores();
			for (int rankId = 1; rankId < num_total_procs; ++rankId) {
				dpr::internal_mpi::st_send(rankId, DPSR_MPIWS_TEST_FINALDATA, res.data(), res.size());
			}
			return res;
		} else {
			while(true) {
				int resInfo[2];
				MPI_Recv(&resInfo, 2, MPI_INT, 0, DPSR_MPIWS_TEST_NEXTPARAMS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				const int checkNo = resInfo[0];
				const int iSizeTest = resInfo[1];
				if (checkNo <= 0) {
					break;
				}

				general_reference_wrapper<T> localRoot = dpr::internal::call_clone<T>(root.get());

				unsigned long long int nNodes = 0;
				char exitNormally = 1;

				internal_mpi_psr::cb_init();
				internal_mpi_psr::stealStack = new internal_mpi::StealStack<typename std::remove_reference<T>::type>*[num_total_threads];

				internal::AlignedBufferedAllocator stealStackAllocator((sizeof(internal_mpi::StealStack<typename std::remove_reference<T>::type>)+64)*num_total_threads);

				if (behavior & ReplicateInput) {
					dpr::internal_mpi::st_bcast(localRoot.get(), rank, 0);
				}

				for (int threadId = 0; threadId < num_total_threads; threadId++) {
					internal_mpi_psr::stealStack[threadId] = stealStackAllocator.aligned_alloc<internal_mpi::StealStack<typename std::remove_reference<T>::type>>(64);
					internal_mpi_psr::ss_init(internal_mpi_psr::stealStack[threadId], internal_mpi::initialStackSize(), behavior);
				}

				internal::do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info = &info;
				std::vector<std::thread> threads;
				dpr::internal::thread_barrier threadBarrier(num_total_threads);
				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					PROFILEACTION(internal_mpi_psr::ss_initState(internal_mpi_psr::stealStack[threadId]));
					threads.push_back(std::thread(run_thread_test, threadId, num_total_threads, rank, num_total_procs, checkNo, chunksToSteal, static_cast<unsigned int>(pollingInterval), threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount, internal_mpi_psr::stealStack[threadId], std::ref(localRoot), info, body, std::ref(r[threadId]), behavior, std::ref(nNodesArr[threadId]), iSizeTest, std::ref(exitNormallyArr[threadId]), std::ref(threadBarrier)));
				}

				Return rr{};
				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					threads[threadId].join();
					if (globalExceptionPtr() == nullptr) {
						body.post(r[threadId], rr);
					}
				}
				if (globalExceptionPtr() != nullptr) {
					try {
						std::rethrow_exception(globalExceptionPtr());
					}catch (const std::exception &ex) {
						std::cerr << ex.what() << std::endl;
						std::rethrow_exception(globalExceptionPtr());
					}
				}
				threads.clear();

				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					internal_mpi_psr::stealStack[threadId]->stack.~vector();
					internal_mpi_psr::stealStack[threadId]->baseInputStack.~vector();
				}
				delete[] internal_mpi_psr::stealStack;
				if (num_total_procs > 1) {
					MPI_Barrier(MPI_COMM_WORLD);
				}

				dpr::internal::call_deallocate<T>(localRoot.get());

				for (int i = 0; i < num_total_threads; i++) {
					nNodes += nNodesArr[i];
				}
				if (opt.limitTimeOfEachTest) {
					for (int i = 0; i < num_total_threads; i++) {
						exitNormally &= exitNormallyArr[i];
					}
				}
				resInfo[0] = nNodes;
				resInfo[1] = static_cast<int>(exitNormally);
				MPI_Send(resInfo, 2, MPI_INT, 0, DPSR_MPIWS_TEST_EXECRES, MPI_COMM_WORLD);
			}
			int resSize;
			res.resize(opt.maxNumChunksTestAllowed + (opt.maxNumChunksTestAllowed >> 1) + 2);
			dpr::internal_mpi::st_recv(0, DPSR_MPIWS_TEST_FINALDATA, res.data(), &resSize);
			res.resize(resSize);
			return res;
		}
	}

	static void run_thread(const int thrId, const int numTotalThreads, const int rank, const int numTotalProcs, const int chunkSize, const int chunksToSteal, const unsigned int polling_interval, const int threadsRequestPolicy, const int mpiWorkRequestLimit, const int trpPredictWorkCount, internal_mpi::StealStack<T>* ss, general_reference_wrapper<T>& root, const Info& info, Body body, Return& rr, const Behavior behavior, dpr::internal::thread_barrier& threadBarrier) {
		try {
			const int threadId = thrId;

			Return r = std::move(rr);
			internal_mpi_psr threadParStackClass(threadId, numTotalThreads, rank, numTotalProcs, chunkSize, chunksToSteal, polling_interval, threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount);
			threadParStackClass.init_mpi_communications();

			/* put root node on thread 0 stack */
			if (threadId == 0) {
				if ((behavior & DistributedInput) || (rank == 0)) {
					threadParStackClass.ss_push(internal_mpi_psr::stealStack[0], root.get());
				}
			}
			threadBarrier.wait();

			/* tree search */
			bool done = false;
			if (behavior & GatherInput) {
				while(!done) {
					/* local work */
					while (threadParStackClass.ss_localDepth(ss) > 0 && (ss->statusJobSearch == 0)) {
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));
						/* examine node at stack top */
						T *data = threadParStackClass.ss_top(ss);
						body.pre(*data);

						PROFILEACTION(ss->nNodes++);
						if (info.is_base(*data)) {
							PROFILEACTION(ss->nLeaves++);
							body.post(body.base(*data), r);
							threadParStackClass.ss_push_copy(ss, *data);
							threadParStackClass.ss_pop(ss);
						} else {
							body.pre_rec(*data);
							if (body.processNonBase) {
								body.post(body.non_base(*data), r);
								threadParStackClass.ss_push_copy(ss, *data);
							}
							if (!Partitioner::do_parallel(*data))  {
								const int numChildren = info.num_children(*data);
#ifdef DPR_FORCE_ORDER_R_TO_L
								for (int i = numChildren-1; i >= 0; --i) {
#else
								for (int i = 0; i < numChildren; ++i) {
#endif
									internal::do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::do_it_serial(r, info.child(i, *data), info, body);
								}
								threadParStackClass.ss_pop(ss);
							} else {
								const int numChildren = info.num_children(*data);
								if (numChildren > 0) {
									threadParStackClass.ss_checkStackSize(ss, numChildren, data);
#ifdef DPR_FORCE_ORDER_L_TO_R
									for (int i = numChildren-2; i >= 0; --i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#else
									for (int i = 1; i < numChildren; ++i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#endif
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
										if (threadParStackClass.num_total_procs > 1) {
											if (ss->polling_count % threadParStackClass.polling_interval == 0) {
												threadParStackClass.ws_make_progress(ss);
											}
											ss->polling_count++;
										}
#endif
									}
#ifdef DPR_FORCE_ORDER_L_TO_R
									threadParStackClass.ss_setvalue_pos(ss, info.child(numChildren-1, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#else
									threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#endif
									threadParStackClass.releaseMultipleNodes(ss);
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
									if (threadParStackClass.num_total_procs > 1) {
										if (ss->polling_count % threadParStackClass.polling_interval == 0) {
											threadParStackClass.ws_make_progress(ss);
										}
										ss->polling_count++;
									}
#endif
								} else {
									threadParStackClass.ss_pop(ss);
								}
							}
						}
					}

					/* local work exhausted on this stack - resume tree search if able
					* to re-acquire work from shared portion of this thread's stack
					*/
					if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
						/* no work left in this thread's stack           */
						/* try to steal work from another thread's stack */
						bool goodSteal = false;
						int remoteSteal = 0;
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
						while ((!goodSteal) && (remoteSteal == 0)) {
							// First try steal work from the other local threads
							if (!goodSteal && (threadParStackClass.wrout_request == MPI_REQUEST_NULL) && (threadParStackClass.iw_request == MPI_REQUEST_NULL) && (ss->statusJobSearch == 0)) {	// don't steal locally while there a work request to another process is active
								int victimId = threadParStackClass.findwork(chunkSize);
								while ((victimId != -1) && !goodSteal && (ss->statusJobSearch == 0)) {
									// some work detected, try to steal it
									goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
									if (!goodSteal) {
										victimId = threadParStackClass.findwork(chunkSize);
									}
								}
							}
							if (!goodSteal) {
								// Check other processes and try to steal from them
								remoteSteal = threadParStackClass.ss_get_work(ss);
								if (remoteSteal == 0) {
									// No work has been stolen at the moment
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
								} else if (remoteSteal > 0) {
									// Remote work successfully stolen
									goodSteal = true;
									break;
								} else if (remoteSteal == -1) {
									// Termination: DPSR_RED code
									done = threadParStackClass.cbarrier_wait();
									break;
								} else if (remoteSteal == -2) {
									// Only one process active, nothing to steal from other processes
									/* unable to steal work from shared portion of other stacks -
									* enter quiescent state waiting for termination (done != false)
									* or cancellation because some thread has made work available
									* (done == false).
									*/
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
									done = threadParStackClass.cbarrier_wait();
									break;
								}
							}
						}
					}
				}
			} else {
				while(!done) {
					/* local work */
					while (threadParStackClass.ss_localDepth(ss) > 0 && (ss->statusJobSearch == 0)) {
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));
						/* examine node at stack top */
						T *data = threadParStackClass.ss_top(ss);
						body.pre(*data);

						PROFILEACTION(ss->nNodes++);
						if (info.is_base(*data)) {
							PROFILEACTION(ss->nLeaves++);
							body.post(body.base(*data), r);
							threadParStackClass.ss_pop(ss);
						} else {
							body.pre_rec(*data);
							if (body.processNonBase) {
								body.post(body.non_base(*data), r);
							}
							if (!Partitioner::do_parallel(*data))  {
								const int numChildren = info.num_children(*data);
#ifdef DPR_FORCE_ORDER_R_TO_L
								for (int i = numChildren-1; i >= 0; --i) {
#else
								for (int i = 0; i < numChildren; ++i) {
#endif
									internal::do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::do_it_serial(r, info.child(i, *data), info, body);
								}
								threadParStackClass.ss_pop(ss);
							} else {
								const int numChildren = info.num_children(*data);
								if (numChildren > 0) {
									threadParStackClass.ss_checkStackSize(ss, numChildren, data);
#ifdef DPR_FORCE_ORDER_L_TO_R
									for (int i = numChildren-2; i >= 0; --i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#else
									for (int i = 1; i < numChildren; ++i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#endif
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
										if (threadParStackClass.num_total_procs > 1) {
											if (ss->polling_count % threadParStackClass.polling_interval == 0) {
												threadParStackClass.ws_make_progress(ss);
											}
											ss->polling_count++;
										}
#endif
									}
#ifdef DPR_FORCE_ORDER_L_TO_R
									threadParStackClass.ss_setvalue_pos(ss, info.child(numChildren-1, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#else
									threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#endif
									threadParStackClass.releaseMultipleNodes(ss);
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
									if (threadParStackClass.num_total_procs > 1) {
										if (ss->polling_count % threadParStackClass.polling_interval == 0) {
											threadParStackClass.ws_make_progress(ss);
										}
										ss->polling_count++;
									}
#endif
								} else {
									threadParStackClass.ss_pop(ss);
								}
							}
						}
					}

					/* local work exhausted on this stack - resume tree search if able
					* to re-acquire work from shared portion of this thread's stack
					*/
					if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
						/* no work left in this thread's stack           */
						/* try to steal work from another thread's stack */
						bool goodSteal = false;
						int remoteSteal = 0;
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
						while ((!goodSteal) && (remoteSteal == 0)) {
							// First try steal work from the other local threads
							if (!goodSteal && (threadParStackClass.wrout_request == MPI_REQUEST_NULL) && (threadParStackClass.iw_request == MPI_REQUEST_NULL) && (ss->statusJobSearch == 0)) {	// don't steal locally while there a work request to another process is active
								int victimId = threadParStackClass.findwork(chunkSize);
								while ((victimId != -1) && !goodSteal && (ss->statusJobSearch == 0)) {
									// some work detected, try to steal it
									goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
									if (!goodSteal) {
										victimId = threadParStackClass.findwork(chunkSize);
									}
								}
							}
							if (!goodSteal) {
								// Check other processes and try to steal from them
								remoteSteal = threadParStackClass.ss_get_work(ss);
								if (remoteSteal == 0) {
									// No work has been stolen at the moment
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
								} else if (remoteSteal > 0) {
									// Remote work successfully stolen
									goodSteal = true;
									break;
								} else if (remoteSteal == -1) {
									// Termination: DPSR_RED code
									done = threadParStackClass.cbarrier_wait();
									break;
								} else if (remoteSteal == -2) {
									// Only one process active, nothing to steal from other processes
									/* unable to steal work from shared portion of other stacks -
									* enter quiescent state waiting for termination (done != false)
									* or cancellation because some thread has made work available
									* (done == false).
									*/
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
									done = threadParStackClass.cbarrier_wait();
									break;
								}
							}
						}
					}
				}
			}
			rr = std::move(r);
			/* tree search complete ! */
		}catch (...) {
			//Set the global exception pointer in case of an exception
			globalExceptionPtr() = std::current_exception();
			if (numTotalProcs > 1) {
				try {
					std::rethrow_exception(globalExceptionPtr());
				}catch (const std::exception &ex) {
					std::cerr << ex.what() << std::endl;
				}
				MPI_Abort(MPI_COMM_WORLD, -10);
			} else {
#ifdef DPR_DONT_USE_SPINLOCK
				std::lock_guard<std::mutex> lck(internal_mpi_psr::cb.cb_cv_mtx);
				internal_mpi_psr::cb.cb_done = true;
				internal_mpi_psr::cb.cb_cancel = true;
				internal_mpi_psr::cb.cb_cv.notify_all();
#else
				internal_mpi_psr::cb.cb_done = true;
				internal_mpi_psr::cb.cb_cancel = true;
#endif
			}
		}
	}

	static void run_thread_test(const int thrId, const int numTotalThreads, const int rank, const int numTotalProcs, const int chunkSize, const int chunksToSteal, const unsigned int polling_interval, const int threadsRequestPolicy, const int mpiWorkRequestLimit, const int trpPredictWorkCount, internal_mpi::StealStack<T>* ss, general_reference_wrapper<T>& root, const Info& info, Body body, Return& rr, const Behavior behavior, unsigned long long int& nNodes, const unsigned long long int testSize, char& exitNormally, dpr::internal::thread_barrier& threadBarrier) {
		try {
			const int threadId = thrId;
			exitNormally = 1;

			Return r = std::move(rr);
			internal_mpi_psr threadParStackClass(threadId, numTotalThreads, rank, numTotalProcs, chunkSize, chunksToSteal, polling_interval, threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount);
			threadParStackClass.init_mpi_communications();

			/* put root node on thread 0 stack */
			if (threadId == 0) {
				if ((behavior & DistributedInput) || (rank == 0)) {
					threadParStackClass.ss_push(internal_mpi_psr::stealStack[0], root.get());
				}
			}
			threadBarrier.wait();

			/* tree search */
			bool done = false;
			if (behavior & GatherInput) {
				while(!done) {
					/* local work */
					while (threadParStackClass.ss_localDepth(ss) > 0 && (ss->statusJobSearch == 0)) {
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));
						/* examine node at stack top */
						T *data = threadParStackClass.ss_top(ss);
						body.pre(*data);

						if ((ss->nNodes > testSize) || internal_mpi_psr::cb.cb_done) {
							ss->nNodes++;
							exitNormally = 0;
							PROFILEACTION(ss->nLeaves++);
#ifdef DPR_DONT_USE_SPINLOCK
							std::lock_guard<std::mutex> lck(internal_mpi_psr::cb.cb_cv_mtx);
							internal_mpi_psr::cb.cb_done = true;
							internal_mpi_psr::cb.cb_cancel = true;
							internal_mpi_psr::cb.cb_cv.notify_all();
#else
							internal_mpi_psr::cb.cb_done = true;
							internal_mpi_psr::cb.cb_cancel = true;
#endif
							threadParStackClass.ss_mkEmpty(ss);
						} else if (info.is_base(*data)) {
							ss->nNodes++;
							PROFILEACTION(ss->nLeaves++);
							body.post(body.base(*data), r);
							threadParStackClass.ss_push_copy(ss, *data);
							threadParStackClass.ss_pop(ss);
						} else {
							ss->nNodes++;
							body.pre_rec(*data);
							if (body.processNonBase) {
								body.post(body.non_base(*data), r);
								threadParStackClass.ss_push_copy(ss, *data);
							}
							if (!Partitioner::do_parallel(*data))  {
								const int numChildren = info.num_children(*data);
#ifdef DPR_FORCE_ORDER_R_TO_L
								for (int i = numChildren-1; i >= 0; --i) {
#else
								for (int i = 0; i < numChildren; ++i) {
#endif
									internal::do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::do_it_serial(r, info.child(i, *data), info, body);
								}
								threadParStackClass.ss_pop(ss);
							} else {
								const int numChildren = info.num_children(*data);
								if (numChildren > 0) {
									threadParStackClass.ss_checkStackSize(ss, numChildren, data);
#ifdef DPR_FORCE_ORDER_L_TO_R
									for (int i = numChildren-2; i >= 0; --i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#else
									for (int i = 1; i < numChildren; ++i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#endif
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
										if (threadParStackClass.num_total_procs > 1) {
											if (ss->polling_count % threadParStackClass.polling_interval == 0) {
												threadParStackClass.ws_make_progress(ss);
											}
											ss->polling_count++;
										}
#endif
									}
#ifdef DPR_FORCE_ORDER_L_TO_R
									threadParStackClass.ss_setvalue_pos(ss, info.child(numChildren-1, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#else
									threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#endif
									threadParStackClass.releaseMultipleNodes(ss);
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
									if (threadParStackClass.num_total_procs > 1) {
										if (ss->polling_count % threadParStackClass.polling_interval == 0) {
											threadParStackClass.ws_make_progress(ss);
										}
										ss->polling_count++;
									}
#endif
								} else {
									threadParStackClass.ss_pop(ss);
								}
							}
						}
					}

					/* local work exhausted on this stack - resume tree search if able
					* to re-acquire work from shared portion of this thread's stack
					*/
					if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
						/* no work left in this thread's stack           */
						/* try to steal work from another thread's stack */
						bool goodSteal = false;
						int remoteSteal = 0;
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
						while ((!goodSteal) && (remoteSteal == 0)) {
							// First try steal work from the other local threads
							if (!goodSteal && (threadParStackClass.wrout_request == MPI_REQUEST_NULL) && (threadParStackClass.iw_request == MPI_REQUEST_NULL) && (ss->statusJobSearch == 0)) {	// don't steal locally while there a work request to another process is active
								int victimId = threadParStackClass.findwork(chunkSize);
								while ((victimId != -1) && !goodSteal && (ss->statusJobSearch == 0)) {
									// some work detected, try to steal it
									goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
									if (!goodSteal) {
										victimId = threadParStackClass.findwork(chunkSize);
									}
								}
							}
							if (!goodSteal) {
								// Check other processes and try to steal from them
								remoteSteal = threadParStackClass.ss_get_work(ss);
								if (remoteSteal == 0) {
									// No work has been stolen at the moment
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
								} else if (remoteSteal > 0) {
									// Remote work successfully stolen
									goodSteal = true;
									break;
								} else if (remoteSteal == -1) {
									// Termination: DPSR_RED code
									done = threadParStackClass.cbarrier_wait();
									break;
								} else if (remoteSteal == -2) {
									// Only one process active, nothing to steal from other processes
									/* unable to steal work from shared portion of other stacks -
									* enter quiescent state waiting for termination (done != false)
									* or cancellation because some thread has made work available
									* (done == false).
									*/
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
									done = threadParStackClass.cbarrier_wait();
									break;
								}
							}
						}
					}
				}
			} else {
				while(!done) {
					/* local work */
					while (threadParStackClass.ss_localDepth(ss) > 0 && (ss->statusJobSearch == 0)) {
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));
						/* examine node at stack top */
						T *data = threadParStackClass.ss_top(ss);
						body.pre(*data);

						if ((ss->nNodes > testSize) || internal_mpi_psr::cb.cb_done) {
							ss->nNodes++;
							exitNormally = 0;
							PROFILEACTION(ss->nLeaves++);
#ifdef DPR_DONT_USE_SPINLOCK
							std::lock_guard<std::mutex> lck(internal_mpi_psr::cb.cb_cv_mtx);
							internal_mpi_psr::cb.cb_done = true;
							internal_mpi_psr::cb.cb_cancel = true;
							internal_mpi_psr::cb.cb_cv.notify_all();
#else
							internal_mpi_psr::cb.cb_done = true;
							internal_mpi_psr::cb.cb_cancel = true;
#endif
							threadParStackClass.ss_mkEmpty(ss);
						} else if (info.is_base(*data)) {
							ss->nNodes++;
							PROFILEACTION(ss->nLeaves++);
							body.post(body.base(*data), r);
							threadParStackClass.ss_pop(ss);
						} else {
							ss->nNodes++;
							body.pre_rec(*data);
							if (body.processNonBase) {
								body.post(body.non_base(*data), r);
							}
							if (!Partitioner::do_parallel(*data))  {
								const int numChildren = info.num_children(*data);
#ifdef DPR_FORCE_ORDER_R_TO_L
								for (int i = numChildren-1; i >= 0; --i) {
#else
								for (int i = 0; i < numChildren; ++i) {
#endif
									internal::do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::do_it_serial(r, info.child(i, *data), info, body);
								}
								threadParStackClass.ss_pop(ss);
							} else {
								const int numChildren = info.num_children(*data);
								if (numChildren > 0) {
									threadParStackClass.ss_checkStackSize(ss, numChildren, data);
#ifdef DPR_FORCE_ORDER_L_TO_R
									for (int i = numChildren-2; i >= 0; --i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#else
									for (int i = 1; i < numChildren; ++i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#endif
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
										if (threadParStackClass.num_total_procs > 1) {
											if (ss->polling_count % threadParStackClass.polling_interval == 0) {
												threadParStackClass.ws_make_progress(ss);
											}
											ss->polling_count++;
										}
#endif
									}
#ifdef DPR_FORCE_ORDER_L_TO_R
									threadParStackClass.ss_setvalue_pos(ss, info.child(numChildren-1, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#else
									threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#endif
									threadParStackClass.releaseMultipleNodes(ss);
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
									if (threadParStackClass.num_total_procs > 1) {
										if (ss->polling_count % threadParStackClass.polling_interval == 0) {
											threadParStackClass.ws_make_progress(ss);
										}
										ss->polling_count++;
									}
#endif
								} else {
									threadParStackClass.ss_pop(ss);
								}
							}
						}
					}

					/* local work exhausted on this stack - resume tree search if able
					* to re-acquire work from shared portion of this thread's stack
					*/
					if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
						/* no work left in this thread's stack           */
						/* try to steal work from another thread's stack */
						bool goodSteal = false;
						int remoteSteal = 0;
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
						while ((!goodSteal) && (remoteSteal == 0)) {
							// First try steal work from the other local threads
							if (!goodSteal && (threadParStackClass.wrout_request == MPI_REQUEST_NULL) && (threadParStackClass.iw_request == MPI_REQUEST_NULL) && (ss->statusJobSearch == 0)) {	// don't steal locally while there a work request to another process is active
								int victimId = threadParStackClass.findwork(chunkSize);
								while ((victimId != -1) && !goodSteal && (ss->statusJobSearch == 0)) {
									// some work detected, try to steal it
									goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
									if (!goodSteal) {
										victimId = threadParStackClass.findwork(chunkSize);
									}
								}
							}
							if (!goodSteal) {
								// Check other processes and try to steal from them
								remoteSteal = threadParStackClass.ss_get_work(ss);
								if (remoteSteal == 0) {
									// No work has been stolen at the moment
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
								} else if (remoteSteal > 0) {
									// Remote work successfully stolen
									goodSteal = true;
									break;
								} else if (remoteSteal == -1) {
									// Termination: DPSR_RED code
									done = threadParStackClass.cbarrier_wait();
									break;
								} else if (remoteSteal == -2) {
									// Only one process active, nothing to steal from other processes
									/* unable to steal work from shared portion of other stacks -
									* enter quiescent state waiting for termination (done != false)
									* or cancellation because some thread has made work available
									* (done == false).
									*/
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
									done = threadParStackClass.cbarrier_wait();
									break;
								}
							}
						}
					}
				}
			}
			nNodes = ss->nNodes;
			rr = std::move(r);
			/* tree search complete ! */
		}catch (...) {
			//Set the global exception pointer in case of an exception
			globalExceptionPtr() = std::current_exception();
			if (numTotalProcs > 1) {
				try {
					std::rethrow_exception(globalExceptionPtr());
				}catch (const std::exception &ex) {
					std::cerr << ex.what() << std::endl;
				}
				MPI_Abort(MPI_COMM_WORLD, -10);
			} else {
#ifdef DPR_DONT_USE_SPINLOCK
				std::lock_guard<std::mutex> lck(internal_mpi_psr::cb.cb_cv_mtx);
				internal_mpi_psr::cb.cb_done = true;
				internal_mpi_psr::cb.cb_cancel = true;
				internal_mpi_psr::cb.cb_cv.notify_all();
#else
				internal_mpi_psr::cb.cb_done = true;
				internal_mpi_psr::cb.cb_cancel = true;
#endif
			}
		}
	}

	friend class dpr::internal_mpi::simple_stack_partitioner<Return, T, Info, Body>;

	friend class dpr::internal_mpi::auto_stack_partitioner<Return, T, Info, Body>;

	friend class dpr::internal_mpi::custom_stack_partitioner<Return, T, Info, Body>;

};

/// Specialization for Return==void
template<typename T, typename Info, typename Body, typename Partitioner>
class start_stack_recursion_mpi<void, T, Info, Body, Partitioner> {
	typedef dpr::internal_mpi::parallel_stack_recursion_internal_mpi<typename std::remove_reference<T>::type, Info> internal_mpi_psr;

public:

	static void run(general_reference_wrapper<T> root, const Info& info, Body body, const int rank = internal_mpi::dpr_rank(), const int num_total_procs = internal_mpi::dpr_nprocs(), const int chunkSizeI = dpr::internal_mpi::defaultChunkSize, const Behavior behavior = Behavior(DefaultBehavior), void * const dest = nullptr, const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default, typename std::decay<T>::type * const rootTestV = nullptr) {
		int chunkSize = chunkSizeI;
		const int num_total_threads = internal_mpi::num_total_threads(dspar_config_info);
		const int chunksToSteal = internal_mpi::chunksToStealGl(dspar_config_info);
		const int threadsRequestPolicy = internal_mpi::threadsRequestPolicyGl(dspar_config_info);
		const int mpiWorkRequestLimit = ((internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) >= num_total_threads) || (internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) == 0)) ? num_total_threads : ((internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) < 0) ? ((std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)) > 0) ? ((std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)) < num_total_threads) ? std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)):num_total_threads):1) : internal_mpi::mpiWorkRequestLimitGl(dspar_config_info));
		const int trpPredictWorkCount = (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_PREDICTIVE) ? (((internal_mpi::trpPredictWorkCountGl(dspar_config_info) >= num_total_threads) || (internal_mpi::trpPredictWorkCountGl(dspar_config_info) == 0) || (internal_mpi::trpPredictWorkCountGl(dspar_config_info) < TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_QUARTERS)) ? num_total_threads : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_QUARTERS) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)-1)/4+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_HALF) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)-1)/2+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_THREE_QUARTERS) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)*3-1)/4+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_ALL) ? (num_total_threads-1)/mpiWorkRequestLimit+1 : internal_mpi::trpPredictWorkCountGl(dspar_config_info)))))) : 0;
		const int pollingInterval = internal_mpi::pollingIntervalGl(dspar_config_info);
		dpr::internal_mpi::lastDpsrRunExtraInfo().chunkSize = chunkSize;
		dpr::internal_mpi::lastDpsrRunExtraInfo().nthreads = num_total_threads;
		std::chrono::steady_clock::time_point init_run_time;
		if (chunkSize <= 0) {
			T rootTest((rootTestV == nullptr) ? root.get() : *rootTestV);
			const std::chrono::steady_clock::time_point init_test_time = getTimepoint();
			if (behavior & ReplicateInput) {
				dpr::internal_mpi::st_bcast(rootTest, rank, 0);
			}
			const std::vector<dpr::ResultChunkTest> testResults = run_test(std::forward<T>(rootTest), info, body, rank, num_total_procs, chunkSize, behavior, dspar_config_info, opt);
			if (testResults.size() > 0) {
				chunkSize = testResults[0].chunkId;
			} else {
				chunkSize = defaultChunkSize;
			}
			init_run_time = getTimepoint();
			dpr::internal_mpi::lastDpsrRunExtraInfo().testChunkTime = getTimediff(init_test_time, init_run_time);
		} else {
			init_run_time = getTimepoint();
			dpr::internal_mpi::lastDpsrRunExtraInfo().testChunkTime = 0.0;
		}
		dpr::internal_mpi::lastDpsrRunExtraInfo().chunkSizeUsed = chunkSize;

		internal_mpi_psr::cb_init();
		internal_mpi_psr::stealStack = new internal_mpi::StealStack<typename std::remove_reference<T>::type>*[num_total_threads];

		internal::AlignedBufferedAllocator stealStackAllocator((sizeof(internal_mpi::StealStack<typename std::remove_reference<T>::type>)+64)*num_total_threads);

		if (behavior & ReplicateInput) {
			dpr::internal_mpi::st_bcast(root.get(), rank, 0);
		}

		for (int threadId = 0; threadId < num_total_threads; threadId++) {
			internal_mpi_psr::stealStack[threadId] = stealStackAllocator.aligned_alloc<internal_mpi::StealStack<typename std::remove_reference<T>::type>>(64);
			internal_mpi_psr::ss_init(internal_mpi_psr::stealStack[threadId], internal_mpi::initialStackSize(), behavior);
		}

		internal::do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::info = &info;
		std::vector<std::thread> threads;
		dpr::internal::thread_barrier threadBarrier(num_total_threads);
		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			PROFILEACTION(internal_mpi_psr::ss_initState(internal_mpi_psr::stealStack[threadId]));
			threads.push_back(std::thread(run_thread, threadId, num_total_threads, rank, num_total_procs, chunkSize, chunksToSteal, static_cast<unsigned int>(pollingInterval), threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount, internal_mpi_psr::stealStack[threadId], std::ref(root), info, body, behavior, std::ref(threadBarrier)));
		}

		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			threads[threadId].join();
		}
		if (globalExceptionPtr() != nullptr) {
			try {
				std::rethrow_exception(globalExceptionPtr());
			}catch (const std::exception &ex) {
				std::cerr << ex.what() << std::endl;
				std::rethrow_exception(globalExceptionPtr());
			}
		}

		threads.clear();

		// Post results of all processes
		if (num_total_procs > 1) {
			if (behavior & GatherInput) {
				if (rank == 0) {
					long long int inbuf_size = 0;
					for (int threadId = 0; threadId < num_total_threads; threadId++) {
						inbuf_size += internal_mpi_psr::stealStack[threadId]->baseInputTop;
					}
					for (int rankId = 1; rankId < num_total_procs; ++rankId) {
						int buffSize;
						MPI_Recv(&buffSize, 1, MPI_INT, MPI_ANY_SOURCE, DPSR_MPIWS_REDUCTION_INFO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						inbuf_size += static_cast<long long int>(buffSize);
					}

					std::vector<T> inbuf(inbuf_size);
					int c = 0;
					for (int threadId = 0; threadId < num_total_threads; threadId++) {
						std::copy_n(internal_mpi_psr::stealStack[threadId]->baseInputStack.begin(), internal_mpi_psr::stealStack[threadId]->baseInputTop, inbuf.begin()+c);
						c += internal_mpi_psr::stealStack[threadId]->baseInputTop;
					}
					for (int rankId = 1; rankId < num_total_procs; ++rankId) {
						for (int threadId = 0; threadId < num_total_threads; threadId++) {
							int nelem;
							dpr::internal_mpi::st_recv(rankId, DPSR_MPIWS_REDUCTION, &(inbuf[c]), &nelem);
							c += nelem;
						}
					}
					assert(inbuf_size == c);
					for (int i = 0; i < c; ++i) {
						body.gather_input_post(inbuf[i], c, root.get());
					}
				} else {
					int procBaseTotalSize = 0;
					for (int threadId = 0; threadId < num_total_threads; threadId++) {
						procBaseTotalSize += internal_mpi_psr::stealStack[threadId]->baseInputTop;
					}
					MPI_Send(&procBaseTotalSize, 1, MPI_INT, 0, DPSR_MPIWS_REDUCTION_INFO, MPI_COMM_WORLD);
					for (int threadId = 0; threadId < num_total_threads; threadId++) {
						dpr::internal_mpi::st_send(0, DPSR_MPIWS_REDUCTION, internal_mpi_psr::stealStack[threadId]->baseInputStack.data(), internal_mpi_psr::stealStack[threadId]->baseInputTop, nullptr);
					}
				}
				if (behavior & ReplicateOutput) {
					dpr::internal_mpi::st_bcast(root.get(), rank, 0);
				}
			}
			MPI_Barrier(MPI_COMM_WORLD);
		}

		for(int threadId = 0; threadId < num_total_threads; threadId++) {
			internal_mpi_psr::stealStack[threadId]->stack.~vector();
			internal_mpi_psr::stealStack[threadId]->baseInputStack.~vector();
		}
		delete[] internal_mpi_psr::stealStack;

		dpr::internal_mpi::lastDpsrRunExtraInfo().runTime = getTimediff(init_run_time, getTimepoint());
	}

	static std::vector<dpr::ResultChunkTest> run_test(general_reference_wrapper<T> root, const Info& info, Body body, const int rank = internal_mpi::dpr_rank(), const int num_total_procs = internal_mpi::dpr_nprocs(), const int chunkSizeI = 0, const Behavior behavior = Behavior(DefaultBehavior), const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_test_default) {
		const int num_total_threads = internal_mpi::num_total_threads(dspar_config_info);
		const int chunksToSteal = internal_mpi::chunksToStealGl(dspar_config_info);
		const int threadsRequestPolicy = internal_mpi::threadsRequestPolicyGl(dspar_config_info);
		const int mpiWorkRequestLimit = ((internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) >= num_total_threads) || (internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) == 0)) ? num_total_threads : ((internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) < 0) ? ((std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)) > 0) ? ((std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)) < num_total_threads) ? std::round(static_cast<double>(internal_mpi::mpiWorkRequestLimitGl(dspar_config_info) * num_total_threads) / static_cast<double>(internal_mpi::INTERNAL_MPIWS_CONSTADJUST)):num_total_threads):1) : internal_mpi::mpiWorkRequestLimitGl(dspar_config_info));
		const int trpPredictWorkCount = (threadsRequestPolicy == THREAD_MPI_STEAL_POLICY_PREDICTIVE) ? (((internal_mpi::trpPredictWorkCountGl(dspar_config_info) >= num_total_threads) || (internal_mpi::trpPredictWorkCountGl(dspar_config_info) == 0) || (internal_mpi::trpPredictWorkCountGl(dspar_config_info) < TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_QUARTERS)) ? num_total_threads : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_QUARTERS) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)-1)/4+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_HALF) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)-1)/2+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_THREE_QUARTERS) ? (((num_total_threads-1)/mpiWorkRequestLimit+1)*3-1)/4+1 : ((internal_mpi::trpPredictWorkCountGl(dspar_config_info) == TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_ALL) ? (num_total_threads-1)/mpiWorkRequestLimit+1 : internal_mpi::trpPredictWorkCountGl(dspar_config_info)))))) : 0;
		const int pollingInterval = internal_mpi::pollingIntervalGl(dspar_config_info);
		std::vector<dpr::ResultChunkTest> res;

		std::vector<unsigned long long int> nNodesArr(num_total_threads);
		std::vector<char> exitNormallyArr(num_total_threads);
		const size_t softLimitTestsSizeTest = 8;
		const size_t hardLimitTestsSizeTest = 14;
		const double softLimitMargin = 0.3;
		const double errorMargingCalculateSizeTest = 0.15;
		const int minorSizeTest = 250;

		if (rank == 0) {
			dpr::internal::ChunkSelector chunkSel(opt, chunkSizeI);

			std::map<int,unsigned long long int> mapSizeTestChunks;
			for (const std::pair<int, dpr::internal::chunk_attr> &iChunk : chunkSel.selectedChunks) {
				mapSizeTestChunks[iChunk.first] = 0;
			}

			double estimatedTimePerTest = opt.targetTimePerTest;
			if ((opt.limitTimeOfEachTest) && (opt.testSize == 0) && (estimatedTimePerTest <= 0.0000001)) {
				estimatedTimePerTest = chunkSel.estimateNeededTimePerTest();
				if (opt.verbose > 3) {
					std::cout << " [?] It has been estimated a necessary time per test of " << estimatedTimePerTest << " sec [±" << static_cast<int>(errorMargingCalculateSizeTest*100) << "%]" << std::endl;
				}
			} else {
				if (opt.verbose > 3) {
					if (opt.limitTimeOfEachTest) {
						if (opt.testSize == 0) {
							std::cout << " [?] A necessary time per test " << estimatedTimePerTest << " sec [±" << static_cast<int>(errorMargingCalculateSizeTest*100) << "%]" << " has been established" << std::endl;
						} else {
							std::cout << " [?] A fixed test size of " << opt.testSize << " nodes will be used" << std::endl;
						}
					} else {
						std::cout << " [?] No test size or test time limit has been established" << std::endl;
					}
				}
			}

			int checkNo = chunkSel.getNextChunk();
			unsigned long long int iSizeTest;
			while(checkNo > 0) {
				if (opt.verbose > 2) {
					std::cout << "  - Testing Chunk " << checkNo << "..." << std::flush;
				}
				if (opt.limitTimeOfEachTest) {
					iSizeTest = opt.testSize;
				} else {
					iSizeTest = std::numeric_limits<unsigned long long int>::max();
				}
				if (iSizeTest == 0) {
					iSizeTest = mapSizeTestChunks[checkNo];
				}
				if (iSizeTest == 0) {
					iSizeTest = minorSizeTest;
					int minorDistance = std::numeric_limits<int>::max();
					int minorDistChunk = -1;
					for (const std::pair<int, unsigned long long int> &iMap : mapSizeTestChunks) {
						if (iMap.second != 0) {
							const int distanceChunk = std::abs(checkNo - iMap.first);
							if (distanceChunk < minorDistance) {
								minorDistChunk = iMap.first;
								minorDistance = distanceChunk;
							}
						}
					}
					if (minorDistChunk > 0) {
						iSizeTest = mapSizeTestChunks[minorDistChunk];
					}
				}
				std::map<unsigned long long int, std::pair<double, double>> mapHistorySizeTests;
				char exitNormally = 0;
				bool firstMandatoryExecution = true;
				bool needMoreNodes = true;
				double mostAccurate = std::numeric_limits<double>::max();
				unsigned long long int bestSizeTest = 0;
				bool underLimit = false;

				while (firstMandatoryExecution || (
						(mapSizeTestChunks[checkNo] == 0) &&
						(mapHistorySizeTests.find(iSizeTest) == mapHistorySizeTests.end()) &&
						(mapHistorySizeTests.size() < hardLimitTestsSizeTest) &&
						(!underLimit) &&
						((mapHistorySizeTests.size() < softLimitTestsSizeTest) || (std::abs(mostAccurate) > softLimitMargin)) &&
						(!exitNormally || !needMoreNodes)
				)) {
					if ((opt.verbose > 3) && (opt.testSize == 0) && (mapSizeTestChunks[checkNo] == 0)) {
						if (firstMandatoryExecution) {
							std::cout << std::endl;
						}
						if (opt.limitTimeOfEachTest) {
							std::cout << "    · Testing with " << iSizeTest << " nodes..." << std::flush;
						}
					}
					firstMandatoryExecution = false;

					for (int rankId = 1; rankId < num_total_procs; ++rankId) {
						int resInfo[2];
						resInfo[0] = checkNo;
						resInfo[1] = iSizeTest;
						MPI_Send(resInfo, 2, MPI_INT, rankId, DPSR_MPIWS_TEST_NEXTPARAMS, MPI_COMM_WORLD);
					}

					general_reference_wrapper<T> localRoot = dpr::internal::call_clone<T>(root.get());

					unsigned long long int nNodes = 0;
					exitNormally = 1;

					const std::chrono::steady_clock::time_point init_time = getTimepoint();

					internal_mpi_psr::cb_init();
					internal_mpi_psr::stealStack = new internal_mpi::StealStack<typename std::remove_reference<T>::type>*[num_total_threads];

					internal::AlignedBufferedAllocator stealStackAllocator((sizeof(internal_mpi::StealStack<typename std::remove_reference<T>::type>)+64)*num_total_threads);

					if (behavior & ReplicateInput) {
						dpr::internal_mpi::st_bcast(localRoot.get(), rank, 0);
					}

					for (int threadId = 0; threadId < num_total_threads; threadId++) {
						internal_mpi_psr::stealStack[threadId] = stealStackAllocator.aligned_alloc<internal_mpi::StealStack<typename std::remove_reference<T>::type>>(64);
						internal_mpi_psr::ss_init(internal_mpi_psr::stealStack[threadId], internal_mpi::initialStackSize(), behavior);
					}

					internal::do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::info = &info;
					std::vector<std::thread> threads;
					dpr::internal::thread_barrier threadBarrier(num_total_threads);
					for(int threadId = 0; threadId < num_total_threads; threadId++) {
						PROFILEACTION(internal_mpi_psr::ss_initState(internal_mpi_psr::stealStack[threadId]));
						threads.push_back(std::thread(run_thread_test, threadId, num_total_threads, rank, num_total_procs, checkNo, chunksToSteal, static_cast<unsigned int>(pollingInterval), threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount, internal_mpi_psr::stealStack[threadId], std::ref(localRoot), info, body, behavior, std::ref(nNodesArr[threadId]), iSizeTest, std::ref(exitNormallyArr[threadId]), std::ref(threadBarrier)));
					}

					for(int threadId = 0; threadId < num_total_threads; threadId++) {
						threads[threadId].join();
					}
					if (globalExceptionPtr() != nullptr) {
						try {
							std::rethrow_exception(globalExceptionPtr());
						}catch (const std::exception &ex) {
							std::cerr << ex.what() << std::endl;
							std::rethrow_exception(globalExceptionPtr());
						}
					}
					threads.clear();

					for(int threadId = 0; threadId < num_total_threads; threadId++) {
						internal_mpi_psr::stealStack[threadId]->stack.~vector();
						internal_mpi_psr::stealStack[threadId]->baseInputStack.~vector();
					}
					delete[] internal_mpi_psr::stealStack;
					if (num_total_procs > 1) {
						MPI_Barrier(MPI_COMM_WORLD);
					}

					const std::chrono::steady_clock::time_point finish_time = getTimepoint();
					const double total_time = getTimediff(init_time, finish_time);

					dpr::internal::call_deallocate<T>(localRoot.get());

					for (int i = 0; i < num_total_threads; i++) {
						nNodes += nNodesArr[i];
					}
					if (opt.limitTimeOfEachTest) {
						for (int i = 0; i < num_total_threads; i++) {
							exitNormally &= exitNormallyArr[i];
						}
					}
					for (int rankId = 1; rankId < num_total_procs; ++rankId) {
						int resInfo[2];
						MPI_Recv(resInfo, 2, MPI_INT, MPI_ANY_SOURCE, DPSR_MPIWS_TEST_EXECRES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
						nNodes += resInfo[0];
						exitNormally &= static_cast<char>(resInfo[1]);
					}

					const double nSpeed = nNodes / total_time;
					if ((opt.testSize == 0) && (opt.limitTimeOfEachTest)) {
						if (mapSizeTestChunks[checkNo] == 0) {
							const double diff = (estimatedTimePerTest-total_time)/estimatedTimePerTest;
							mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
							if (std::abs(diff) < std::abs(mostAccurate)) {
								bestSizeTest = iSizeTest;
								mostAccurate = diff;
							}
							if (std::abs(diff) < errorMargingCalculateSizeTest) {
								mapSizeTestChunks[checkNo] = iSizeTest;
							} else {
								if (diff > 0.96) {
									needMoreNodes = true;
									iSizeTest *= 25;
								} else if (diff > 0.0) {
									needMoreNodes = true;
									iSizeTest *= estimatedTimePerTest/total_time;
								} else if (diff < 0.0) {
									needMoreNodes = false;
									if (iSizeTest > minorSizeTest) {
										iSizeTest *= estimatedTimePerTest/total_time;
									} else {
										underLimit = true;
									}
								} else if (diff < -3.0) {
									needMoreNodes = false;
									iSizeTest *= 0.25;
								}
								if (iSizeTest < minorSizeTest) {
									iSizeTest = minorSizeTest;
								}
								while (mapHistorySizeTests.find(iSizeTest) != mapHistorySizeTests.end()) {
									iSizeTest += minorSizeTest;
								}
							}

							if (opt.verbose > 3) {
								if (mapSizeTestChunks[checkNo] == 0) {
									std::cout << "\tFailure! Time: " << total_time << " sec. Error: " << ((diff <= 0.0) ? "+" : "-") << std::abs(diff*100) << "%" << std::endl;
								} else {
									std::cout << "\tDone! Time: " << total_time << " sec. Error: " << ((diff <= 0.0) ? "+" : "-") << std::abs(diff*100) << "%" << std::endl << "  ";
								}
							}
						} else {
							mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
						}
					} else {
						mapHistorySizeTests[iSizeTest] = std::pair<double, double>(total_time, nSpeed);
						mapSizeTestChunks[checkNo] = iSizeTest;
					}
				}
				if (mapSizeTestChunks[checkNo] == 0) {
					if (bestSizeTest > 0) {
						mapSizeTestChunks[checkNo] = bestSizeTest;
						if ((opt.verbose > 3) && (opt.testSize == 0)) {
							if ((mapHistorySizeTests.size() >= hardLimitTestsSizeTest)) {
								std::cout << "    · Hard limit of " << hardLimitTestsSizeTest << " test reached.";
							} else if ((mapHistorySizeTests.size() >= softLimitTestsSizeTest) && (std::abs(mostAccurate) <= softLimitMargin)) {
								std::cout << "    · Soft limit of " << softLimitTestsSizeTest << " test reached.";
							} else if (underLimit) {
								std::cout << "    · The execution time is too high for the minimum number of nodes (" << minorSizeTest << ").";
							} else if (exitNormally) {
								std::cout << "    · The number of nodes exceeds the number of nodes in the normal execution of the program.";
							} else {
								std::cout << "    ·";
							}
							std::cout << " Selecting the most accurate test: " << bestSizeTest << " nodes. Error: " << ((mostAccurate <= 0.0) ? "+" : "-") << std::abs(mostAccurate*100) << "%" << std::endl << "  ";
						}
					}
				}
				if (mapSizeTestChunks[checkNo] != 0) {
					const double nSpeed = mapHistorySizeTests[mapSizeTestChunks[checkNo]].second;
					chunkSel.setChunkSpeed(checkNo, nSpeed);
					if (opt.verbose > 2) {
						const double total_time = mapHistorySizeTests[mapSizeTestChunks[checkNo]].first;
						std::cout << "\tFinalized! Test Time: " << total_time << " sec. Test Speed Result: " << nSpeed << " nodes/sec" << std::endl;
					}
					checkNo = chunkSel.getNextChunk();
				} else {
					if (opt.verbose > 2) {
						std::cout << "\tFailed!" << std::endl;
					}
					checkNo = 0;
				}
			}
			for (int rankId = 1; rankId < num_total_procs; ++rankId) {
				int resInfo[2];
				resInfo[0] = -1;
				resInfo[1] = -1;
				MPI_Send(resInfo, 2, MPI_INT, rankId, DPSR_MPIWS_TEST_NEXTPARAMS, MPI_COMM_WORLD);
			}
			res = chunkSel.getChunkScores();
			for (int rankId = 1; rankId < num_total_procs; ++rankId) {
				dpr::internal_mpi::st_send(rankId, DPSR_MPIWS_TEST_FINALDATA, res.data(), res.size());
			}
			return res;
		} else {
			while(true) {
				int resInfo[2];
				MPI_Recv(&resInfo, 2, MPI_INT, 0, DPSR_MPIWS_TEST_NEXTPARAMS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				const int checkNo = resInfo[0];
				const int iSizeTest = resInfo[1];
				if (checkNo <= 0) {
					break;
				}

				general_reference_wrapper<T> localRoot = dpr::internal::call_clone<T>(root.get());

				unsigned long long int nNodes = 0;
				char exitNormally = 1;

				internal_mpi_psr::cb_init();
				internal_mpi_psr::stealStack = new internal_mpi::StealStack<typename std::remove_reference<T>::type>*[num_total_threads];

				internal::AlignedBufferedAllocator stealStackAllocator((sizeof(internal_mpi::StealStack<typename std::remove_reference<T>::type>)+64)*num_total_threads);

				if (behavior & ReplicateInput) {
					dpr::internal_mpi::st_bcast(localRoot.get(), rank, 0);
				}

				for (int threadId = 0; threadId < num_total_threads; threadId++) {
					internal_mpi_psr::stealStack[threadId] = stealStackAllocator.aligned_alloc<internal_mpi::StealStack<typename std::remove_reference<T>::type>>(64);
					internal_mpi_psr::ss_init(internal_mpi_psr::stealStack[threadId], internal_mpi::initialStackSize(), behavior);
				}

				internal::do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::info = &info;
				std::vector<std::thread> threads;
				dpr::internal::thread_barrier threadBarrier(num_total_threads);
				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					PROFILEACTION(internal_mpi_psr::ss_initState(internal_mpi_psr::stealStack[threadId]));
					threads.push_back(std::thread(run_thread_test, threadId, num_total_threads, rank, num_total_procs, checkNo, chunksToSteal, static_cast<unsigned int>(pollingInterval), threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount, internal_mpi_psr::stealStack[threadId], std::ref(localRoot), info, body, behavior, std::ref(nNodesArr[threadId]), iSizeTest, std::ref(exitNormallyArr[threadId]), std::ref(threadBarrier)));
				}

				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					threads[threadId].join();
				}
				if (globalExceptionPtr() != nullptr) {
					try {
						std::rethrow_exception(globalExceptionPtr());
					}catch (const std::exception &ex) {
						std::cerr << ex.what() << std::endl;
						std::rethrow_exception(globalExceptionPtr());
					}
				}
				threads.clear();

				for(int threadId = 0; threadId < num_total_threads; threadId++) {
					internal_mpi_psr::stealStack[threadId]->stack.~vector();
					internal_mpi_psr::stealStack[threadId]->baseInputStack.~vector();
				}
				delete[] internal_mpi_psr::stealStack;
				if (num_total_procs > 1) {
					MPI_Barrier(MPI_COMM_WORLD);
				}

				dpr::internal::call_deallocate<T>(localRoot.get());

				for (int i = 0; i < num_total_threads; i++) {
					nNodes += nNodesArr[i];
				}
				if (opt.limitTimeOfEachTest) {
					for (int i = 0; i < num_total_threads; i++) {
						exitNormally &= exitNormallyArr[i];
					}
				}
				resInfo[0] = nNodes;
				resInfo[1] = static_cast<int>(exitNormally);
				MPI_Send(resInfo, 2, MPI_INT, 0, DPSR_MPIWS_TEST_EXECRES, MPI_COMM_WORLD);
			}
			int resSize;
			res.resize(opt.maxNumChunksTestAllowed + (opt.maxNumChunksTestAllowed >> 1) + 2);
			dpr::internal_mpi::st_recv(0, DPSR_MPIWS_TEST_FINALDATA, res.data(), &resSize);
			res.resize(resSize);
			return res;
		}
	}

	static void run_thread(const int thrId, const int numTotalThreads, const int rank, const int numTotalProcs, const int chunkSize, const int chunksToSteal, const unsigned int polling_interval, const int threadsRequestPolicy, const int mpiWorkRequestLimit, const int trpPredictWorkCount, internal_mpi::StealStack<T>* ss, general_reference_wrapper<T>& root, const Info& info, Body body, const Behavior behavior, dpr::internal::thread_barrier& threadBarrier) {
		try {
			const int threadId = thrId;

			internal_mpi_psr threadParStackClass(threadId, numTotalThreads, rank, numTotalProcs, chunkSize, chunksToSteal, polling_interval, threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount);
			threadParStackClass.init_mpi_communications();

			/* put root node on thread 0 stack */
			if (threadId == 0) {
				if ((behavior & DistributedInput) || (rank == 0)) {
					threadParStackClass.ss_push(internal_mpi_psr::stealStack[0], root.get());
				}
			}
			threadBarrier.wait();

			/* tree search */
			bool done = false;
			if (behavior & GatherInput) {
				while(!done) {
					/* local work */
					while (threadParStackClass.ss_localDepth(ss) > 0 && (ss->statusJobSearch == 0)) {
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));
						/* examine node at stack top */
						T *data = threadParStackClass.ss_top(ss);
						body.pre(*data);

						PROFILEACTION(ss->nNodes++);
						if (info.is_base(*data)) {
							PROFILEACTION(ss->nLeaves++);
							body.base(*data);
							threadParStackClass.ss_push_copy(ss, *data);
							threadParStackClass.ss_pop(ss);
						} else {
							body.pre_rec(*data);
							if (body.processNonBase) {
								body.non_base(*data);
								threadParStackClass.ss_push_copy(ss, *data);
							}
							if (!Partitioner::do_parallel(*data))  {
								const int numChildren = info.num_children(*data);
#ifdef DPR_FORCE_ORDER_R_TO_L
								for (int i = numChildren-1; i >= 0; --i) {
#else
								for (int i = 0; i < numChildren; ++i) {
#endif
									internal::do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::do_it_serial(info.child(i, *data), info, body);
								}
								threadParStackClass.ss_pop(ss);
							} else {
								const int numChildren = info.num_children(*data);
								if (numChildren > 0) {
									threadParStackClass.ss_checkStackSize(ss, numChildren, data);
#ifdef DPR_FORCE_ORDER_L_TO_R
									for (int i = numChildren-2; i >= 0; --i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#else
									for (int i = 1; i < numChildren; ++i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#endif
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
										if (threadParStackClass.num_total_procs > 1) {
											if (ss->polling_count % threadParStackClass.polling_interval == 0) {
												threadParStackClass.ws_make_progress(ss);
											}
											ss->polling_count++;
										}
#endif
									}
#ifdef DPR_FORCE_ORDER_L_TO_R
									threadParStackClass.ss_setvalue_pos(ss, info.child(numChildren-1, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#else
									threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#endif
									threadParStackClass.releaseMultipleNodes(ss);
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
									if (threadParStackClass.num_total_procs > 1) {
										if (ss->polling_count % threadParStackClass.polling_interval == 0) {
											threadParStackClass.ws_make_progress(ss);
										}
										ss->polling_count++;
									}
#endif
								} else {
									threadParStackClass.ss_pop(ss);
								}
							}
						}
					}

					/* local work exhausted on this stack - resume tree search if able
					* to re-acquire work from shared portion of this thread's stack
					*/
					if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
						/* no work left in this thread's stack           */
						/* try to steal work from another thread's stack */
						bool goodSteal = false;
						int remoteSteal = 0;
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
						while ((!goodSteal) && (remoteSteal == 0)) {
							// First try steal work from the other local threads
							if (!goodSteal && (threadParStackClass.wrout_request == MPI_REQUEST_NULL) && (threadParStackClass.iw_request == MPI_REQUEST_NULL) && (ss->statusJobSearch == 0)) {	// don't steal locally while there a work request to another process is active
								int victimId = threadParStackClass.findwork(chunkSize);
								while ((victimId != -1) && !goodSteal && (ss->statusJobSearch == 0)) {
									// some work detected, try to steal it
									goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
									if (!goodSteal) {
										victimId = threadParStackClass.findwork(chunkSize);
									}
								}
							}
							if (!goodSteal) {
								// Check other processes and try to steal from them
								remoteSteal = threadParStackClass.ss_get_work(ss);
								if (remoteSteal == 0) {
									// No work has been stolen at the moment
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
								} else if (remoteSteal > 0) {
									// Remote work successfully stolen
									goodSteal = true;
									break;
								} else if (remoteSteal == -1) {
									// Termination: DPSR_RED code
									done = threadParStackClass.cbarrier_wait();
									break;
								} else if (remoteSteal == -2) {
									// Only one process active, nothing to steal from other processes
									/* unable to steal work from shared portion of other stacks -
									* enter quiescent state waiting for termination (done != false)
									* or cancellation because some thread has made work available
									* (done == false).
									*/
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
									done = threadParStackClass.cbarrier_wait();
									break;
								}
							}
						}
					}
				}
			} else {
				while(!done) {
					/* local work */
					while (threadParStackClass.ss_localDepth(ss) > 0 && (ss->statusJobSearch == 0)) {
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));
						/* examine node at stack top */
						T *data = threadParStackClass.ss_top(ss);
						body.pre(*data);

						PROFILEACTION(ss->nNodes++);
						if (info.is_base(*data)) {
							PROFILEACTION(ss->nLeaves++);
							body.base(*data);
							threadParStackClass.ss_pop(ss);
						} else {
							body.pre_rec(*data);
							if (body.processNonBase) {
								body.non_base(*data);
							}
							if (!Partitioner::do_parallel(*data))  {
								const int numChildren = info.num_children(*data);
#ifdef DPR_FORCE_ORDER_R_TO_L
								for (int i = numChildren-1; i >= 0; --i) {
#else
								for (int i = 0; i < numChildren; ++i) {
#endif
									internal::do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::do_it_serial(info.child(i, *data), info, body);
								}
								threadParStackClass.ss_pop(ss);
							} else {
								const int numChildren = info.num_children(*data);
								if (numChildren > 0) {
									threadParStackClass.ss_checkStackSize(ss, numChildren, data);
#ifdef DPR_FORCE_ORDER_L_TO_R
									for (int i = numChildren-2; i >= 0; --i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#else
									for (int i = 1; i < numChildren; ++i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#endif
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
										if (threadParStackClass.num_total_procs > 1) {
											if (ss->polling_count % threadParStackClass.polling_interval == 0) {
												threadParStackClass.ws_make_progress(ss);
											}
											ss->polling_count++;
										}
#endif
									}
#ifdef DPR_FORCE_ORDER_L_TO_R
									threadParStackClass.ss_setvalue_pos(ss, info.child(numChildren-1, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#else
									threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#endif
									threadParStackClass.releaseMultipleNodes(ss);
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
									if (threadParStackClass.num_total_procs > 1) {
										if (ss->polling_count % threadParStackClass.polling_interval == 0) {
											threadParStackClass.ws_make_progress(ss);
										}
										ss->polling_count++;
									}
#endif
								} else {
									threadParStackClass.ss_pop(ss);
								}
							}
						}
					}

					/* local work exhausted on this stack - resume tree search if able
					* to re-acquire work from shared portion of this thread's stack
					*/
					if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
						/* no work left in this thread's stack           */
						/* try to steal work from another thread's stack */
						bool goodSteal = false;
						int remoteSteal = 0;
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
						while ((!goodSteal) && (remoteSteal == 0)) {
							// First try steal work from the other local threads
							if (!goodSteal && (threadParStackClass.wrout_request == MPI_REQUEST_NULL) && (threadParStackClass.iw_request == MPI_REQUEST_NULL) && (ss->statusJobSearch == 0)) {	// don't steal locally while there a work request to another process is active
								int victimId = threadParStackClass.findwork(chunkSize);
								while ((victimId != -1) && !goodSteal && (ss->statusJobSearch == 0)) {
									// some work detected, try to steal it
									goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
									if (!goodSteal) {
										victimId = threadParStackClass.findwork(chunkSize);
									}
								}
							}
							if (!goodSteal) {
								// Check other processes and try to steal from them
								remoteSteal = threadParStackClass.ss_get_work(ss);
								if (remoteSteal == 0) {
									// No work has been stolen at the moment
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
								} else if (remoteSteal > 0) {
									// Remote work successfully stolen
									goodSteal = true;
									break;
								} else if (remoteSteal == -1) {
									// Termination: DPSR_RED code
									done = threadParStackClass.cbarrier_wait();
									break;
								} else if (remoteSteal == -2) {
									// Only one process active, nothing to steal from other processes
									/* unable to steal work from shared portion of other stacks -
									* enter quiescent state waiting for termination (done != false)
									* or cancellation because some thread has made work available
									* (done == false).
									*/
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
									done = threadParStackClass.cbarrier_wait();
									break;
								}
							}
						}
					}
				}
			}
			/* tree search complete ! */
		}catch (...) {
			//Set the global exception pointer in case of an exception
			globalExceptionPtr() = std::current_exception();
			if (numTotalProcs > 1) {
				try {
					std::rethrow_exception(globalExceptionPtr());
				}catch (const std::exception &ex) {
					std::cerr << ex.what() << std::endl;
				}
				MPI_Abort(MPI_COMM_WORLD, -10);
			} else {
#ifdef DPR_DONT_USE_SPINLOCK
				std::lock_guard<std::mutex> lck(internal_mpi_psr::cb.cb_cv_mtx);
				internal_mpi_psr::cb.cb_done = true;
				internal_mpi_psr::cb.cb_cancel = true;
				internal_mpi_psr::cb.cb_cv.notify_all();
#else
				internal_mpi_psr::cb.cb_done = true;
				internal_mpi_psr::cb.cb_cancel = true;
#endif
			}
		}
	}

	static void run_thread_test(const int thrId, const int numTotalThreads, const int rank, const int numTotalProcs, const int chunkSize, const int chunksToSteal, const unsigned int polling_interval, const int threadsRequestPolicy, const int mpiWorkRequestLimit, const int trpPredictWorkCount, internal_mpi::StealStack<T>* ss, general_reference_wrapper<T>& root, const Info& info, Body body, const Behavior behavior, unsigned long long int& nNodes, const unsigned long long int testSize, char& exitNormally, dpr::internal::thread_barrier& threadBarrier) {
		try {
			const int threadId = thrId;
			exitNormally = 1;

			internal_mpi_psr threadParStackClass(threadId, numTotalThreads, rank, numTotalProcs, chunkSize, chunksToSteal, polling_interval, threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount);
			threadParStackClass.init_mpi_communications();

			/* put root node on thread 0 stack */
			if (threadId == 0) {
				if ((behavior & DistributedInput) || (rank == 0)) {
					threadParStackClass.ss_push(internal_mpi_psr::stealStack[0], root.get());
				}
			}
			threadBarrier.wait();

			/* tree search */
			bool done = false;
			if (behavior & GatherInput) {
				while(!done) {
					/* local work */
					while (threadParStackClass.ss_localDepth(ss) > 0 && (ss->statusJobSearch == 0)) {
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));
						/* examine node at stack top */
						T *data = threadParStackClass.ss_top(ss);
						body.pre(*data);
						if ((ss->nNodes > testSize) || internal_mpi_psr::cb.cb_done) {
							ss->nNodes++;
							exitNormally = 0;
							PROFILEACTION(ss->nLeaves++);
#ifdef DPR_DONT_USE_SPINLOCK
							std::lock_guard<std::mutex> lck(internal_mpi_psr::cb.cb_cv_mtx);
							internal_mpi_psr::cb.cb_done = true;
							internal_mpi_psr::cb.cb_cancel = true;
							internal_mpi_psr::cb.cb_cv.notify_all();
#else
							internal_mpi_psr::cb.cb_done = true;
							internal_mpi_psr::cb.cb_cancel = true;
#endif
							threadParStackClass.ss_mkEmpty(ss);
						} else if (info.is_base(*data)) {
							ss->nNodes++;
							PROFILEACTION(ss->nLeaves++);
							body.base(*data);
							threadParStackClass.ss_push_copy(ss, *data);
							threadParStackClass.ss_pop(ss);
						} else {
							ss->nNodes++;
							body.pre_rec(*data);
							if (body.processNonBase) {
								body.non_base(*data);
								threadParStackClass.ss_push_copy(ss, *data);
							}
							if (!Partitioner::do_parallel(*data))  {
								const int numChildren = info.num_children(*data);
#ifdef DPR_FORCE_ORDER_R_TO_L
								for (int i = numChildren-1; i >= 0; --i) {
#else
								for (int i = 0; i < numChildren; ++i) {
#endif
									internal::do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::do_it_serial(info.child(i, *data), info, body);
								}
								threadParStackClass.ss_pop(ss);
							} else {
								const int numChildren = info.num_children(*data);
								if (numChildren > 0) {
									threadParStackClass.ss_checkStackSize(ss, numChildren, data);
#ifdef DPR_FORCE_ORDER_L_TO_R
									for (int i = numChildren-2; i >= 0; --i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#else
									for (int i = 1; i < numChildren; ++i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#endif
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
										if (threadParStackClass.num_total_procs > 1) {
											if (ss->polling_count % threadParStackClass.polling_interval == 0) {
												threadParStackClass.ws_make_progress(ss);
											}
											ss->polling_count++;
										}
#endif
									}
#ifdef DPR_FORCE_ORDER_L_TO_R
									threadParStackClass.ss_setvalue_pos(ss, info.child(numChildren-1, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#else
									threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#endif
									threadParStackClass.releaseMultipleNodes(ss);
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
									if (threadParStackClass.num_total_procs > 1) {
										if (ss->polling_count % threadParStackClass.polling_interval == 0) {
											threadParStackClass.ws_make_progress(ss);
										}
										ss->polling_count++;
									}
#endif
								} else {
									threadParStackClass.ss_pop(ss);
								}
							}
						}
					}

					/* local work exhausted on this stack - resume tree search if able
					* to re-acquire work from shared portion of this thread's stack
					*/
					if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
						/* no work left in this thread's stack           */
						/* try to steal work from another thread's stack */
						bool goodSteal = false;
						int remoteSteal = 0;
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
						while ((!goodSteal) && (remoteSteal == 0)) {
							// First try steal work from the other local threads
							if (!goodSteal && (threadParStackClass.wrout_request == MPI_REQUEST_NULL) && (threadParStackClass.iw_request == MPI_REQUEST_NULL) && (ss->statusJobSearch == 0)) {	// don't steal locally while there a work request to another process is active
								int victimId = threadParStackClass.findwork(chunkSize);
								while ((victimId != -1) && !goodSteal && (ss->statusJobSearch == 0)) {
									// some work detected, try to steal it
									goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
									if (!goodSteal) {
										victimId = threadParStackClass.findwork(chunkSize);
									}
								}
							}
							if (!goodSteal) {
								// Check other processes and try to steal from them
								remoteSteal = threadParStackClass.ss_get_work(ss);
								if (remoteSteal == 0) {
									// No work has been stolen at the moment
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
								} else if (remoteSteal > 0) {
									// Remote work successfully stolen
									goodSteal = true;
									break;
								} else if (remoteSteal == -1) {
									// Termination: DPSR_RED code
									done = threadParStackClass.cbarrier_wait();
									break;
								} else if (remoteSteal == -2) {
									// Only one process active, nothing to steal from other processes
									/* unable to steal work from shared portion of other stacks -
									* enter quiescent state waiting for termination (done != false)
									* or cancellation because some thread has made work available
									* (done == false).
									*/
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
									done = threadParStackClass.cbarrier_wait();
									break;
								}
							}
						}
					}
				}
			} else {
				while(!done) {
					/* local work */
					while (threadParStackClass.ss_localDepth(ss) > 0 && (ss->statusJobSearch == 0)) {
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_WORK));
						/* examine node at stack top */
						T *data = threadParStackClass.ss_top(ss);
						body.pre(*data);
						if ((ss->nNodes > testSize) || internal_mpi_psr::cb.cb_done) {
							ss->nNodes++;
							exitNormally = 0;
							PROFILEACTION(ss->nLeaves++);
#ifdef DPR_DONT_USE_SPINLOCK
							std::lock_guard<std::mutex> lck(internal_mpi_psr::cb.cb_cv_mtx);
							internal_mpi_psr::cb.cb_done = true;
							internal_mpi_psr::cb.cb_cancel = true;
							internal_mpi_psr::cb.cb_cv.notify_all();
#else
							internal_mpi_psr::cb.cb_done = true;
							internal_mpi_psr::cb.cb_cancel = true;
#endif
							threadParStackClass.ss_mkEmpty(ss);
						} else if (info.is_base(*data)) {
							ss->nNodes++;
							PROFILEACTION(ss->nLeaves++);
							body.base(*data);
							threadParStackClass.ss_pop(ss);
						} else {
							ss->nNodes++;
							body.pre_rec(*data);
							if (body.processNonBase) {
								body.non_base(*data);
							}
							if (!Partitioner::do_parallel(*data))  {
								const int numChildren = info.num_children(*data);
#ifdef DPR_FORCE_ORDER_R_TO_L
								for (int i = numChildren-1; i >= 0; --i) {
#else
								for (int i = 0; i < numChildren; ++i) {
#endif
									internal::do_it_serial_stack_struct<void, T, Info, Body, Info::NumChildren>::do_it_serial(info.child(i, *data), info, body);
								}
								threadParStackClass.ss_pop(ss);
							} else {
								const int numChildren = info.num_children(*data);
								if (numChildren > 0) {
									threadParStackClass.ss_checkStackSize(ss, numChildren, data);
#ifdef DPR_FORCE_ORDER_L_TO_R
									for (int i = numChildren-2; i >= 0; --i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#else
									for (int i = 1; i < numChildren; ++i) {
										threadParStackClass.ss_push_simple(ss, info.child(i, *data));
#endif
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
										if (threadParStackClass.num_total_procs > 1) {
											if (ss->polling_count % threadParStackClass.polling_interval == 0) {
												threadParStackClass.ws_make_progress(ss);
											}
											ss->polling_count++;
										}
#endif
									}
#ifdef DPR_FORCE_ORDER_L_TO_R
									threadParStackClass.ss_setvalue_pos(ss, info.child(numChildren-1, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#else
									threadParStackClass.ss_setvalue_pos(ss, info.child(0, *data), threadParStackClass.ss_topPosn(ss)-numChildren+1);
#endif
									threadParStackClass.releaseMultipleNodes(ss);
#ifdef DPR_USE_REGULAR_POLLING_INTERVAL
									if (threadParStackClass.num_total_procs > 1) {
										if (ss->polling_count % threadParStackClass.polling_interval == 0) {
											threadParStackClass.ws_make_progress(ss);
										}
										ss->polling_count++;
									}
#endif
								} else {
									threadParStackClass.ss_pop(ss);
								}
							}
						}
					}

					/* local work exhausted on this stack - resume tree search if able
					* to re-acquire work from shared portion of this thread's stack
					*/
					if (!threadParStackClass.ss_acquire(ss, chunkSize)) {
						/* no work left in this thread's stack           */
						/* try to steal work from another thread's stack */
						bool goodSteal = false;
						int remoteSteal = 0;
						PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_SEARCH));
						while ((!goodSteal) && (remoteSteal == 0)) {
							// First try steal work from the other local threads
							if (!goodSteal && (threadParStackClass.wrout_request == MPI_REQUEST_NULL) && (threadParStackClass.iw_request == MPI_REQUEST_NULL) && (ss->statusJobSearch == 0)) {	// don't steal locally while there a work request to another process is active
								int victimId = threadParStackClass.findwork(chunkSize);
								while ((victimId != -1) && !goodSteal && (ss->statusJobSearch == 0)) {
									// some work detected, try to steal it
									goodSteal = threadParStackClass.ss_steal(ss, victimId, chunkSize);
									if (!goodSteal) {
										victimId = threadParStackClass.findwork(chunkSize);
									}
								}
							}
							if (!goodSteal) {
								// Check other processes and try to steal from them
								remoteSteal = threadParStackClass.ss_get_work(ss);
								if (remoteSteal == 0) {
									// No work has been stolen at the moment
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
								} else if (remoteSteal > 0) {
									// Remote work successfully stolen
									goodSteal = true;
									break;
								} else if (remoteSteal == -1) {
									// Termination: DPSR_RED code
									done = threadParStackClass.cbarrier_wait();
									break;
								} else if (remoteSteal == -2) {
									// Only one process active, nothing to steal from other processes
									/* unable to steal work from shared portion of other stacks -
									* enter quiescent state waiting for termination (done != false)
									* or cancellation because some thread has made work available
									* (done == false).
									*/
									PROFILEACTION(threadParStackClass.ss_setState(ss, DPR_SS_IDLE));
									done = threadParStackClass.cbarrier_wait();
									break;
								}
							}
						}
					}
				}
			}
			nNodes = ss->nNodes;
			/* tree search complete ! */
		}catch (...) {
			//Set the global exception pointer in case of an exception
			globalExceptionPtr() = std::current_exception();
			if (numTotalProcs > 1) {
				try {
					std::rethrow_exception(globalExceptionPtr());
				}catch (const std::exception &ex) {
					std::cerr << ex.what() << std::endl;
				}
				MPI_Abort(MPI_COMM_WORLD, -10);
			} else {
#ifdef DPR_DONT_USE_SPINLOCK
				std::lock_guard<std::mutex> lck(internal_mpi_psr::cb.cb_cv_mtx);
				internal_mpi_psr::cb.cb_done = true;
				internal_mpi_psr::cb.cb_cancel = true;
				internal_mpi_psr::cb.cb_cv.notify_all();
#else
				internal_mpi_psr::cb.cb_done = true;
				internal_mpi_psr::cb.cb_cancel = true;
#endif
			}
		}
	}

	friend class dpr::internal_mpi::simple_stack_partitioner<void, T, Info, Body>;

	friend class dpr::internal_mpi::auto_stack_partitioner<void, T, Info, Body>;

	friend class dpr::internal_mpi::custom_stack_partitioner<void, T, Info, Body>;

};

/// Implementation of the simple parallelization strategy
template<typename Return, typename T, typename Info, typename Body>
struct simple_stack_partitioner {
	typedef simple_stack_partitioner<Return, T, Info, Body> partitioner;

	/** Since in the current implementation Partitioner::do_parallel in only called
	  * after r._info.is_base(_data) has returned false, there is no need to reevaluate it
	  */
	static bool do_parallel(T& data) noexcept {
		return true;
	}
};

/// Implementation of the automatic parallelization strategy
template<typename Return, typename T, typename Info, typename Body>
struct auto_stack_partitioner {
	typedef auto_stack_partitioner<Return, T, Info, Body> partitioner;

	static bool do_parallel(T& data) noexcept {
		//return r._level < (internal::do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info->parLevel_); //BBF: LevelLimit;
		return false;	//TODO: maybe implement auto-partitioner in the future
	}
};

/// Implementation of the customized parallelization strategy
template<typename Return, typename T, typename Info, typename Body>
struct custom_stack_partitioner {
	typedef custom_stack_partitioner<Return, T, Info, Body> partitioner;

	static bool do_parallel(T& data) noexcept(noexcept(internal::do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info->do_parallel(data))) {
		return internal::do_it_serial_stack_struct<Return, T, Info, Body, Info::NumChildren>::info->do_parallel(data);
	}
};

template<typename Return, typename T, typename Info, typename Body, typename Partitioner>
Return start_recursion(T&& root, Info& info, Body body, const int chunkSize, Behavior behavior, Partitioner, Return * const dest, const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default, typename std::decay<T>::type * const rootTestV = nullptr) {

	if (behavior & ReplicateInput) {
		//if we want to replicate the input, it does not make sense it is distributed
		assert(!(behavior & DistributedInput));
		//if we replicate the input in each node, we can make a ReplicatedInput processing
		behavior |= ReplicatedInput;
	}

	if (std::is_void<Return>::value) {
		if (behavior & DistributedOutput) {
			assert(!(behavior & GatherInput)); // contradiction
		} else {
			behavior |= GatherInput;
		}
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &internal_mpi::dpr_rank());
	MPI_Comm_size(MPI_COMM_WORLD, &internal_mpi::dpr_nprocs());

	return dpr::internal_mpi::start_stack_recursion_mpi<Return, typename std::remove_reference<T>::type, Info, Body, Partitioner>::run(std::forward<T>(root), info, body, internal_mpi::dpr_rank(), internal_mpi::dpr_nprocs(), chunkSize, behavior, dest, dspar_config_info, opt, rootTestV);
}

template<typename Return, typename T, typename Info, typename Body, typename Partitioner>
std::vector<dpr::ResultChunkTest> start_recursion_test(T&& root, Info& info, Body body, const int chunkSize, Behavior behavior, Partitioner, const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default) {

	if (behavior & ReplicateInput) {
		//if we want to replicate the input, it does not make sense it is distributed
		assert(!(behavior & DistributedInput));
		//if we replicate the input in each node, we can make a ReplicatedInput processing
		behavior |= ReplicatedInput;
	}

	if (std::is_void<Return>::value) {
		if (behavior & DistributedOutput) {
			assert(!(behavior & GatherInput)); // contradiction
		} else {
			behavior |= GatherInput;
		}
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &internal_mpi::dpr_rank());
	MPI_Comm_size(MPI_COMM_WORLD, &internal_mpi::dpr_nprocs());

	return dpr::internal_mpi::start_stack_recursion_mpi<Return, typename std::remove_reference<T>::type, Info, Body, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, Info, Body>>::run_test(std::forward<T>(root), info, body, internal_mpi::dpr_rank(), internal_mpi::dpr_nprocs(), chunkSize, behavior, dspar_config_info, opt);
}

} //namespace internal_mpi_mpi

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize = dpr::internal_mpi::defaultChunkSize, const Behavior& behavior = Behavior(DefaultBehavior), const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), static_cast<Return*>(nullptr), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior = Behavior(DefaultBehavior), const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), static_cast<Return*>(nullptr), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior = Behavior(DefaultBehavior), const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), static_cast<Return*>(nullptr), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior = Behavior(DefaultBehavior), const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), static_cast<Return*>(nullptr), dspar_config_info, opt);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), static_cast<Return*>(nullptr), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), static_cast<Return*>(nullptr), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), static_cast<Return*>(nullptr), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), static_cast<Return*>(nullptr), dspar_config_info, opt);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, const DSParConfigInfo dspar_config_info, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, const DSParConfigInfo dspar_config_info, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, const DSParConfigInfo dspar_config_info, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, const DSParConfigInfo dspar_config_info, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, opt, &rootTest);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, opt, &rootTest);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info_default, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info_default, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info_default, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info_default, dpr::aco_default, &rootTest);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info, dpr::aco_default, &rootTest);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info_default, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info_default, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info_default, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), nullptr, dspar_config_info_default, opt, &rootTest);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt, &rootTest);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, opt, &rootTest);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, Return& res, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info_default, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, Return& res, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info_default, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, Return& res, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info_default, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, Return& res, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info_default, dpr::aco_default, &rootTest);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, dpr::aco_default, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, Return& res, const DSParConfigInfo dspar_config_info, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info, dpr::aco_default, &rootTest);
}

//** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info_default, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info_default, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info_default, opt, &rootTest);
}

template<typename Return, typename T, typename Info, typename Body>
Return dparallel_stack_recursion(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, Return& res, const AutomaticChunkOptions opt, T&& rootTest) {
	return internal_mpi::start_recursion<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), &res, dspar_config_info_default, opt, &rootTest);
}

/** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> dparallel_stack_recursion_test(T&& root, Info&& info, Body body, const int chunkSize = 0, const Behavior& behavior = Behavior(DefaultBehavior), const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_test_default) {
	return internal_mpi::start_recursion_test<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> dparallel_stack_recursion_test(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior = Behavior(DefaultBehavior), const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_test_default) {
	return internal_mpi::start_recursion_test<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> dparallel_stack_recursion_test(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior = Behavior(DefaultBehavior), const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_test_default) {
	return internal_mpi::start_recursion_test<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> dparallel_stack_recursion_test(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior = Behavior(DefaultBehavior), const DSParConfigInfo dspar_config_info = dspar_config_info_default, const AutomaticChunkOptions opt = dpr::aco_test_default) {
	return internal_mpi::start_recursion_test<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), dspar_config_info, opt);
}

/** When no partitioner is specified, the internal_mpi::simple_stack_partitioner is used */
template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> dparallel_stack_recursion_test(T&& root, Info&& info, Body body, const int chunkSize, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion_test<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> dparallel_stack_recursion_test(T&& root, Info&& info, Body body, const int chunkSize, partitioner::simple, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion_test<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::simple_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> dparallel_stack_recursion_test(T&& root, Info&& info, Body body, const int chunkSize, partitioner::automatic, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion_test<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::auto_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), dspar_config_info, opt);
}

template<typename Return, typename T, typename Info, typename Body>
std::vector<dpr::ResultChunkTest> dparallel_stack_recursion_test(T&& root, Info&& info, Body body, const int chunkSize, partitioner::custom, const Behavior& behavior, const AutomaticChunkOptions opt, const DSParConfigInfo dspar_config_info = dspar_config_info_default) {
	return internal_mpi::start_recursion_test<Return>(std::forward<T>(root), (Info&)info, body, chunkSize, behavior, internal_mpi::custom_stack_partitioner<Return, typename std::remove_reference<T>::type, typename std::remove_reference<Info>::type, Body>(), dspar_config_info, opt);
}

// Public function that allow users get extra info about the last parallel_stack_recursion run (only for normal runs, not test runs)
inline dpr::RunExtraInfo getDpsrLastRunExtraInfo() {
	return dpr::internal_mpi::lastDpsrRunExtraInfo();
}

/// Facilitates the initialization of the parallel_stack_recursion environment
template<typename T>
inline void dprs_init(const T nthreads) {
static_assert(std::is_integral<T>::value, "Integer required.");
	dpr::internal_mpi::num_total_threads() = nthreads;
	dpr::internal_mpi::chunksToStealGl() = dpr::CHUNKS_TO_STEAL_DEFAULT;					//reset to default value
	dpr::internal_mpi::pollingIntervalGl() = dpr::POLLING_INTERVAL_DEFAULT;					//reset to default value
	dpr::internal_mpi::initialStackSize() = dpr::INITIAL_STACKSIZE_DEFAULT;					//reset to default value
	dpr::internal_mpi::threadsRequestPolicyGl() = dpr::THREAD_MPI_STEAL_POLICY_DEFAULT;		//reset to default value
	dpr::internal_mpi::mpiWorkRequestLimitGl() = dpr::MPI_WORKREQUEST_LIMITS_DEFAULT;		//reset to default value
	dpr::internal_mpi::trpPredictWorkCountGl() = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;		//reset to default value
}

/// Facilitates the initialization of the dparallel_stack_recursion environment
template<typename T>
inline void dprs_init(const T nthreads, const int chunksToSteal) {
	static_assert(std::is_integral<T>::value, "Integer required.");
	assert(chunksToSteal > 0);
	dpr::internal_mpi::num_total_threads() = nthreads;
	dpr::internal_mpi::chunksToStealGl() = chunksToSteal;
	dpr::internal_mpi::pollingIntervalGl() = dpr::POLLING_INTERVAL_DEFAULT;					//reset to default value
	dpr::internal_mpi::initialStackSize() = dpr::INITIAL_STACKSIZE_DEFAULT;					//reset to default value
	dpr::internal_mpi::threadsRequestPolicyGl() = dpr::THREAD_MPI_STEAL_POLICY_DEFAULT;		//reset to default value
	dpr::internal_mpi::mpiWorkRequestLimitGl() = dpr::MPI_WORKREQUEST_LIMITS_DEFAULT;		//reset to default value
	dpr::internal_mpi::trpPredictWorkCountGl() = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;		//reset to default value
}

/// Facilitates the initialization of the dparallel_stack_recursion environment
template<typename T>
inline void dprs_init(const T nthreads, const int chunksToSteal, const unsigned int polling_interval) {
	static_assert(std::is_integral<T>::value, "Integer required.");
	assert(chunksToSteal > 0);
	dpr::internal_mpi::num_total_threads() = nthreads;
	dpr::internal_mpi::chunksToStealGl() = chunksToSteal;
	dpr::internal_mpi::pollingIntervalGl() = polling_interval;
	dpr::internal_mpi::initialStackSize() = dpr::INITIAL_STACKSIZE_DEFAULT;					//reset to default value
	dpr::internal_mpi::threadsRequestPolicyGl() = dpr::THREAD_MPI_STEAL_POLICY_DEFAULT;		//reset to default value
	dpr::internal_mpi::mpiWorkRequestLimitGl() = dpr::MPI_WORKREQUEST_LIMITS_DEFAULT;		//reset to default value
	dpr::internal_mpi::trpPredictWorkCountGl() = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;		//reset to default value
}

/// Facilitates the initialization of the dparallel_stack_recursion environment
template<typename T>
inline void dprs_init(const T nthreads, const int chunksToSteal, const unsigned int polling_interval, const int initial_stack_size) {
	static_assert(std::is_integral<T>::value, "Integer required.");
	assert(chunksToSteal > 0);
	assert(initial_stack_size > 0);
	dpr::internal_mpi::num_total_threads() = nthreads;
	dpr::internal_mpi::chunksToStealGl() = chunksToSteal;
	dpr::internal_mpi::pollingIntervalGl() = polling_interval;
	dpr::internal_mpi::initialStackSize() = initial_stack_size;
	dpr::internal_mpi::threadsRequestPolicyGl() = dpr::THREAD_MPI_STEAL_POLICY_DEFAULT;		//reset to default value
	dpr::internal_mpi::mpiWorkRequestLimitGl() = dpr::MPI_WORKREQUEST_LIMITS_DEFAULT;		//reset to default value
	dpr::internal_mpi::trpPredictWorkCountGl() = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;		//reset to default value
}

/// Facilitates the initialization of the dparallel_stack_recursion environment
template<typename T>
inline void dprs_init(const T nthreads, const int chunksToSteal, const unsigned int polling_interval, const int initial_stack_size, const int threads_request_policy) {
	static_assert(std::is_integral<T>::value, "Integer required.");
	assert(chunksToSteal > 0);
	assert(initial_stack_size > 0);
	dpr::internal_mpi::num_total_threads() = nthreads;
	dpr::internal_mpi::chunksToStealGl() = chunksToSteal;
	dpr::internal_mpi::pollingIntervalGl() = polling_interval;
	dpr::internal_mpi::initialStackSize() = initial_stack_size;
	dpr::internal_mpi::threadsRequestPolicyGl() = threads_request_policy;
	dpr::internal_mpi::mpiWorkRequestLimitGl() = (threads_request_policy == dpr::THREAD_MPI_STEAL_POLICY_AGGRESSIVE) ? dpr::MPI_WORKREQUEST_LIMITS_DEFAULT_AGGRESSIVE_TRP : ((dpr::THREAD_MPI_STEAL_POLICY_DEFAULT == dpr::THREAD_MPI_STEAL_POLICY_PREDICTIVE) ? dpr::MPI_WORKREQUEST_LIMITS_DEFAULT_PREDICTIVE_TRP : dpr::MPI_WORKREQUEST_LIMITS_DEFAULT_MULTIPLE_TRP);	//reset to default value
	dpr::internal_mpi::trpPredictWorkCountGl() = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;		//reset to default value
}

/// Facilitates the initialization of the dparallel_stack_recursion environment
template<typename T>
inline void dprs_init(const T nthreads, const int chunksToSteal, const unsigned int polling_interval, const int initial_stack_size, const int threads_request_policy, const int mpi_workrequest_limits) {
	static_assert(std::is_integral<T>::value, "Integer required.");
	assert(chunksToSteal > 0);
	assert(initial_stack_size > 0);
	dpr::internal_mpi::num_total_threads() = nthreads;
	dpr::internal_mpi::chunksToStealGl() = chunksToSteal;
	dpr::internal_mpi::pollingIntervalGl() = polling_interval;
	dpr::internal_mpi::initialStackSize() = initial_stack_size;
	dpr::internal_mpi::threadsRequestPolicyGl() = threads_request_policy;
	dpr::internal_mpi::mpiWorkRequestLimitGl() = mpi_workrequest_limits;
	dpr::internal_mpi::trpPredictWorkCountGl() = dpr::TRP_PREDICT_WORKCOUNT_DEFAULT;		//reset to default value
}

/// Facilitates the initialization of the dparallel_stack_recursion environment
template<typename T>
inline void dprs_init(const T nthreads, const int chunksToSteal, const unsigned int polling_interval, const int initial_stack_size, const int threads_request_policy, const int mpi_workrequest_limits, const int trp_predict_workcount) {
	static_assert(std::is_integral<T>::value, "Integer required.");
	assert(chunksToSteal > 0);
	assert(initial_stack_size > 0);
	dpr::internal_mpi::num_total_threads() = nthreads;
	dpr::internal_mpi::chunksToStealGl() = chunksToSteal;
	dpr::internal_mpi::pollingIntervalGl() = polling_interval;
	dpr::internal_mpi::initialStackSize() = initial_stack_size;
	dpr::internal_mpi::threadsRequestPolicyGl() = threads_request_policy;
	dpr::internal_mpi::mpiWorkRequestLimitGl() = mpi_workrequest_limits;
	dpr::internal_mpi::trpPredictWorkCountGl() = trp_predict_workcount;
}

} // namespace dpr

#ifndef DPR_USE_MPI_THREAD_MULTIPLE

/// Macro that facilitates the initialization of the dparallel_stack_recursion MPI+SPAR environment
///
/// @param argc        number of arguments, 'argc' variable from main function
/// @param argv        argument vector, 'argv' variable from main function
/// @param nprocs      variable to store the number of MPI processes
/// @param rank        variable to store the MPI rank of the process
/// @param ...         a required parameter (nthreads) and other optional parameters (chunksToSteal, pollingInterval, stackSize, threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount)
///                    - nthreads: number of threads of each process
///                    - chunksToSteal: number of chunks to steal with every MPI steal (default: 1)
///                    - pollingInterval: interval of checking for incoming MPI requests (default: POLLING_RATE_DYNAMIC)
///                    - stackSize: initial size of the task stack (default: 1000 or 500000 (if DPR_DENY_STACK_RESIZE is defined))
///                    - threadsRequestPolicy: policy of the MPI requests and responses at thread level (default: THREAD_MPI_STEAL_POLICY_MULTIPLE)
///                    - mpiWorkRequestLimit: limit of MPI work requests allowed simultaneously for a process (default: MPI_WORKREQUEST_LIMITS_NOLIMITS/MPI_WORKREQUEST_LIMITS_HALF/MPI_WORKREQUEST_LIMITS_THIRD)
///                    - trpPredictWorkCount: number of work requested per thread predictively (only used for THREAD_MPI_STEAL_POLICY_PREDICTIVE threadsRequestPolicy) (default: TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_ALL)
#define dpr_stack_init(argc, argv, nprocs, rank, ...) {                              \
          int dpr_provided;                                                          \
          MPI_Init_thread(&(argc), &(argv), MPI_THREAD_SERIALIZED, &dpr_provided);   \
          assert(dpr_provided >= MPI_THREAD_SERIALIZED);                             \
          MPI_Comm_rank(MPI_COMM_WORLD, &(dpr::internal_mpi::dpr_rank()));           \
          MPI_Comm_size(MPI_COMM_WORLD, &(dpr::internal_mpi::dpr_nprocs()));         \
          rank = dpr::internal_mpi::dpr_rank();                                      \
          nprocs = dpr::internal_mpi::dpr_nprocs();                                  \
          dpr::dprs_init(__VA_ARGS__);                                               \
       }

/// Macro that facilitates the initialization of the dparallel_stack_recursion MPI environment
///
/// @param argc        number of arguments, 'argc' variable from main function
/// @param argv        argument vector, 'argv' variable from main function
/// @param nprocs      variable to store the number of MPI processes
/// @param rank        variable to store the MPI rank of the process
#define dpr_stack_mpi_init(argc, argv, nprocs, rank) {                               \
          int dpr_provided;                                                          \
          MPI_Init_thread(&(argc), &(argv), MPI_THREAD_SERIALIZED, &dpr_provided);   \
          assert(dpr_provided >= MPI_THREAD_SERIALIZED);                             \
          MPI_Comm_rank(MPI_COMM_WORLD, &(dpr::internal_mpi::dpr_rank()));           \
          MPI_Comm_size(MPI_COMM_WORLD, &(dpr::internal_mpi::dpr_nprocs()));         \
          rank = dpr::internal_mpi::dpr_rank();                                      \
          nprocs = dpr::internal_mpi::dpr_nprocs();                                  \
       }

#else

/// Macro that facilitates the initialization of the dparallel_stack_recursion MPI+SPAR environment
///
/// @param argc        number of arguments, 'argc' variable from main function
/// @param argv        argument vector, 'argv' variable from main function
/// @param nprocs      variable to store the number of MPI processes
/// @param rank        variable to store the MPI rank of the process
/// @param ...         a required parameter (nthreads) and other optional parameters (chunksToSteal, pollingInterval, stackSize, threadsRequestPolicy, threadsRequestPolicy, mpiWorkRequestLimit, trpPredictWorkCount)
///                    - nthreads: number of threads of each process
///                    - chunksToSteal: number of chunks to steal with every MPI steal (default: 1)
///                    - pollingInterval: interval of checking for incoming MPI requests (default: POLLING_RATE_DYNAMIC)
///                    - stackSize: initial size of the task stack (default: 1000 or 500000 (if DPR_DENY_STACK_RESIZE is defined))
///                    - threadsRequestPolicy: policy of the MPI requests and responses at thread level (default: THREAD_MPI_STEAL_POLICY_MULTIPLE)
///                    - mpiWorkRequestLimit: limit of MPI work requests allowed simultaneously for a process (default: MPI_WORKREQUEST_LIMITS_NOLIMITS/MPI_WORKREQUEST_LIMITS_HALF/MPI_WORKREQUEST_LIMITS_THIRD)
///                    - trpPredictWorkCount: number of work requested per thread predictively (only used for THREAD_MPI_STEAL_POLICY_PREDICTIVE threadsRequestPolicy) (default: TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_ALL)
#define dpr_stack_init(argc, argv, nprocs, rank, ...) {                              \
          int dpr_provided;                                                          \
          MPI_Init_thread(&(argc), &(argv), MPI_THREAD_MULTIPLE, &dpr_provided);     \
          assert(dpr_provided >= MPI_THREAD_MULTIPLE);                               \
          MPI_Comm_rank(MPI_COMM_WORLD, &(dpr::internal_mpi::dpr_rank()));           \
          MPI_Comm_size(MPI_COMM_WORLD, &(dpr::internal_mpi::dpr_nprocs()));         \
          rank = dpr::internal_mpi::dpr_rank();                                      \
          nprocs = dpr::internal_mpi::dpr_nprocs();                                  \
          dpr::dprs_init(__VA_ARGS__);                                               \
       }

/// Macro that facilitates the initialization of the dparallel_stack_recursion MPI environment
///
/// @param argc        number of arguments, 'argc' variable from main function
/// @param argv        argument vector, 'argv' variable from main function
/// @param nprocs      variable to store the number of MPI processes
/// @param rank        variable to store the MPI rank of the process
#define dpr_stack_mpi_init(argc, argv, nprocs, rank) {                               \
          int dpr_provided;                                                          \
          MPI_Init_thread(&(argc), &(argv), MPI_THREAD_MULTIPLE, &dpr_provided);     \
          assert(dpr_provided >= MPI_THREAD_MULTIPLE);                               \
          MPI_Comm_rank(MPI_COMM_WORLD, &(dpr::internal_mpi::dpr_rank()));           \
          MPI_Comm_size(MPI_COMM_WORLD, &(dpr::internal_mpi::dpr_nprocs()));         \
          rank = dpr::internal_mpi::dpr_rank();                                      \
          nprocs = dpr::internal_mpi::dpr_nprocs();                                  \
       }

#endif

#endif /* DPR_PARALLEL_STACK_RECURSION_MPI_H_ */
