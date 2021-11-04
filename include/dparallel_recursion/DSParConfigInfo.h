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
/// \file     DSParConfigInfo.h
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#ifndef DPR_DSPARCONFIGINFO_H_
#define DPR_DSPARCONFIGINFO_H_

#include <algorithm>
#include <limits>
#include <thread>

namespace dpr {

/// Convenience thread MPI steal policy constants
const int THREAD_MPI_STEAL_POLICY_MULTIPLE = 0;
const int THREAD_MPI_STEAL_POLICY_AGGRESSIVE = 1;
const int THREAD_MPI_STEAL_POLICY_PREDICTIVE = 2;
const int& THREAD_MPI_STEAL_POLICY_DEFAULT = THREAD_MPI_STEAL_POLICY_MULTIPLE;

namespace internal_mpi {
static const int INTERNAL_MPIWS_CONSTADJUST = -10000;
}
// Convenience MPI WorkRequest limits constants
const int MPI_WORKREQUEST_LIMITS_NOLIMITS = 1 * internal_mpi::INTERNAL_MPIWS_CONSTADJUST;				// num_total_threads
const int MPI_WORKREQUEST_LIMITS_NINE_TENTHS = 0.9000 * internal_mpi::INTERNAL_MPIWS_CONSTADJUST;		// 0.9000 * num_total_threads
const int MPI_WORKREQUEST_LIMITS_FOUR_FIFTHS = 0.8000 * internal_mpi::INTERNAL_MPIWS_CONSTADJUST;		// 0.8000 * num_total_threads
const int MPI_WORKREQUEST_LIMITS_THREE_QUARTERS = 0.7500 * internal_mpi::INTERNAL_MPIWS_CONSTADJUST;	// 0.7500 * num_total_threads
const int MPI_WORKREQUEST_LIMITS_TWO_THIRDS = 0.6667 * internal_mpi::INTERNAL_MPIWS_CONSTADJUST;		// 0.6667 * num_total_threads
const int MPI_WORKREQUEST_LIMITS_HALF = 0.5000 * internal_mpi::INTERNAL_MPIWS_CONSTADJUST;				// 0.5000 * num_total_threads
const int MPI_WORKREQUEST_LIMITS_THIRD = 0.3333 * internal_mpi::INTERNAL_MPIWS_CONSTADJUST;				// 0.3333 * num_total_threads
const int MPI_WORKREQUEST_LIMITS_QUARTER = 0.2500 * internal_mpi::INTERNAL_MPIWS_CONSTADJUST;			// 0.2500 * num_total_threads
const int MPI_WORKREQUEST_LIMITS_FIFTH = 0.2000 * internal_mpi::INTERNAL_MPIWS_CONSTADJUST;				// 0.2000 * num_total_threads
const int MPI_WORKREQUEST_LIMITS_TENTH = 0.1000 * internal_mpi::INTERNAL_MPIWS_CONSTADJUST;				// 0.1000 * num_total_threads
const int MPI_WORKREQUEST_LIMITS_ONE = 1;																// 1
const int& MPI_WORKREQUEST_LIMITS_DEFAULT_AGGRESSIVE_TRP = MPI_WORKREQUEST_LIMITS_NOLIMITS;
const int& MPI_WORKREQUEST_LIMITS_DEFAULT_MULTIPLE_TRP = MPI_WORKREQUEST_LIMITS_HALF;
const int& MPI_WORKREQUEST_LIMITS_DEFAULT_PREDICTIVE_TRP = MPI_WORKREQUEST_LIMITS_THIRD;
const int& MPI_WORKREQUEST_LIMITS_DEFAULT = (THREAD_MPI_STEAL_POLICY_DEFAULT == THREAD_MPI_STEAL_POLICY_AGGRESSIVE) ? MPI_WORKREQUEST_LIMITS_DEFAULT_AGGRESSIVE_TRP : ((THREAD_MPI_STEAL_POLICY_DEFAULT == THREAD_MPI_STEAL_POLICY_PREDICTIVE) ? MPI_WORKREQUEST_LIMITS_DEFAULT_PREDICTIVE_TRP : MPI_WORKREQUEST_LIMITS_DEFAULT_MULTIPLE_TRP);

// Convenience trpPredictWorkCount limits constants
const int TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_ALL = -1;
const int TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_THREE_QUARTERS = -2;
const int TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_HALF = -3;
const int TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_QUARTERS = -4;
const int &TRP_PREDICT_WORKCOUNT_DEFAULT = TRP_PREDICT_WORKCOUNT_RELATIVE_TO_MPIWORKREQUEST_FILL_ALL;

/// Convenience dynamic polling rate constant
const int POLLING_RATE_DYNAMIC = 0;
const int& POLLING_INTERVAL_DEFAULT = POLLING_RATE_DYNAMIC;

// Convenience default chunks to steal constant
const int CHUNKS_TO_STEAL_DEFAULT = 1;

// Constant for a undefined parameter
const int UNDEFINED_PARAMETER = -1900514850;

struct DSParConfigInfo {
	int nthreads;
	int chunksToSteal;
	int polling_interval;
	int initial_stack_size;
	int threads_request_policy;
	int mpi_workrequest_limits;
	int trp_predict_workcount;

	DSParConfigInfo(const int _nthreads = dpr::UNDEFINED_PARAMETER, const int _chunksToSteal = dpr::UNDEFINED_PARAMETER, const int _polling_interval = dpr::UNDEFINED_PARAMETER, const int _initial_stack_size = dpr::UNDEFINED_PARAMETER, const int _threads_request_policy = dpr::UNDEFINED_PARAMETER, const int _mpi_workrequest_limits = dpr::UNDEFINED_PARAMETER, const int _trp_predict_workcount = dpr::UNDEFINED_PARAMETER) :
		nthreads(_nthreads),
		chunksToSteal(_chunksToSteal),
		polling_interval(_polling_interval),
		initial_stack_size(_initial_stack_size),
		threads_request_policy(_threads_request_policy),
		mpi_workrequest_limits(_mpi_workrequest_limits),
		trp_predict_workcount(_trp_predict_workcount)
	{ }

};

const DSParConfigInfo dspar_config_info_default = DSParConfigInfo();

}

#endif /* DPR_DSPARCONFIGINFO_H_ */
