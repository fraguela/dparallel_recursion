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
/// \file     ChunkSelector.h
/// \author   Millan A. Martinez  <millan.alvarez@udc.es>
/// \author   Basilio B. Fraguela <basilio.fraguela@udc.es>
/// \author   Jose C. Cabaleiro   <jc.cabaleiro@usc.es>
///

#ifndef DPR_CHUNKSELECTOR_H_
#define DPR_CHUNKSELECTOR_H_

#include <chrono>
#include <cmath>
#include <map>
#include <set>
#include <queue>
#include <utility>
#include <limits>
#include <iomanip>
#include <iostream>
#include <vector>
#include "dparallel_recursion/dpr_utils.h"

namespace dpr {

// Class that allow store data about a chunk test run
struct ResultChunkTest {
	int chunkId;
	int score;
	int ntests;
	double criteriaSpeed;
	std::vector<double> speedArr;

	ResultChunkTest(int _chunkId = 0, int _score = 0, int _ntests = 0, double _criteriaSpeed = 0.0) :
		chunkId(_chunkId), score(_score), ntests(_ntests), criteriaSpeed(_criteriaSpeed)
	{ }

	ResultChunkTest(int _chunkId, int _score, int _ntests, double _criteriaSpeed, std::vector<double> _speedArr) :
		chunkId(_chunkId), score(_score), ntests(_ntests), criteriaSpeed(_criteriaSpeed), speedArr(_speedArr)
	{ }

	template<class Archive>
	void serialize(Archive& ar, const unsigned int version) {
		ar & chunkId & score & ntests & criteriaSpeed & speedArr;
	}

};

// Class that allow users to define the custom options of the automatic search of the best chunk
struct AutomaticChunkOptions {
	bool limitTimeOfEachTest;
	unsigned long long int testSize;
	double targetTimePerTest;
	double maxTime;
	int initTolerance;
	int finalTolerance;
	unsigned int maxNumChunksTestAllowed;
	int mode;
	int subMode;
	int calcMode;
	int verbose;

	AutomaticChunkOptions(const bool _limitTimeOfEachTest = true, const unsigned long long int _testSize = 0, const double _targetTimePerTest = 0.0, const double _maxTime = 0.0, const int _initTolerance = 90000, const int _finalTolerance = 10000, const unsigned int _maxNumChunksTestAllowed = 40, const int _mode = 0, const int _subMode = 0, const int _calcMode = 0, const int _verbose = 0) :
		limitTimeOfEachTest(_limitTimeOfEachTest),
		testSize(_testSize),
		targetTimePerTest(_targetTimePerTest),
		maxTime(_maxTime),
		initTolerance((_initTolerance > 0) ? _initTolerance : 1),
		finalTolerance((_finalTolerance <= initTolerance) ? _finalTolerance : initTolerance),
		maxNumChunksTestAllowed(_maxNumChunksTestAllowed),
		mode((_mode < 4) ? ((_mode >= 0) ? _mode : 0) : 3),
		subMode((mode == 0) ? ((_subMode < 7) ? ((_subMode >= 0) ? _subMode : 0 )  : 6 ) : ((_subMode >= 0) ? _subMode : 0 )),
		calcMode((_calcMode < 3) ? ((_calcMode >= 0) ? _calcMode : 0) : 2),
		verbose(_verbose)
	{ }
};

// Default Presets for easy use (no limit the execution per test by default)
const AutomaticChunkOptions aco_singleRound_besttime                                 (false, 0, 0.0, 0.0, 40000, 40000, 18, 2, 0,  0, 0);
const AutomaticChunkOptions aco_dynamicRounds_gradualTol_precisionLvl4               (false, 0, 0.0, 0.0, 90000, 10000, 46, 0, 0,  0, 0);	//aco_test_default: the default for 'test' when no specified
const AutomaticChunkOptions aco_dynamicRounds_gradualTol_precisionLvl3               (false, 0, 0.0, 0.0, 80000, 10000, 36, 0, 2,  0, 0);
const AutomaticChunkOptions aco_dynamicRounds_gradualTol_precisionLvl2               (false, 0, 0.0, 0.0, 70000, 10000, 26, 0, 5,  0, 0);
const AutomaticChunkOptions aco_dynamicRounds_gradualTol_precisionLvl1               (false, 0, 0.0, 0.0, 60000, 10000, 18, 0, 7,  0, 0);
const AutomaticChunkOptions aco_fixedRounds_gradualTol_precisionLvl4                 (false, 0, 0.0, 0.0, 80000, 10000, 46, 1, 14, 0, 0);
const AutomaticChunkOptions aco_fixedRounds_gradualTol_precisionLvl3                 (false, 0, 0.0, 0.0, 60000, 10000, 36, 1, 6,  0, 0);
const AutomaticChunkOptions aco_fixedRounds_gradualTol_precisionLvl2                 (false, 0, 0.0, 0.0, 50000, 10000, 26, 1, 2,  0, 0);
const AutomaticChunkOptions aco_fixedRounds_gradualTol_precisionLvl1                 (false, 0, 0.0, 0.0, 40000, 10000, 18, 1, 1,  0, 0);
const AutomaticChunkOptions aco_fixedRounds_fixedTol_precisionLvl4                   (false, 0, 0.0, 0.0, 80000, 80000, 46, 2, 14, 0, 0);
const AutomaticChunkOptions aco_fixedRounds_fixedTol_precisionLvl3                   (false, 0, 0.0, 0.0, 60000, 60000, 36, 2, 6,  0, 0);
const AutomaticChunkOptions aco_fixedRounds_fixedTol_precisionLvl2                   (false, 0, 0.0, 0.0, 50000, 50000, 26, 2, 2,  0, 0);
const AutomaticChunkOptions aco_fixedRounds_fixedTol_precisionLvl1                   (false, 0, 0.0, 0.0, 40000, 40000, 18, 2, 1,  0, 0);
const AutomaticChunkOptions aco_fixedRounds_fixedTolAndDiscardWorst_precisionLvl4    (false, 0, 0.0, 0.0, 80000, 80000, 46, 3, 14, 0, 0);
const AutomaticChunkOptions aco_fixedRounds_fixedTolAndDiscardWorst_precisionLvl3    (false, 0, 0.0, 0.0, 60000, 60000, 36, 3, 8,  0, 0);
const AutomaticChunkOptions aco_fixedRounds_fixedTolAndDiscardWorst_precisionLvl2    (false, 0, 0.0, 0.0, 50000, 50000, 26, 3, 3,  0, 0);
const AutomaticChunkOptions aco_fixedRounds_fixedTolAndDiscardWorst_precisionLvl1    (false, 0, 0.0, 0.0, 40000, 40000, 18, 3, 2,  0, 0);
// Automatic version of the Presets (automatic calculation of the time per test)
const AutomaticChunkOptions aco_auto_dynamicRounds_gradualTol_precisionLvl4           (true, 0, 0.0, 0.0, 90000, 10000, 46, 0, 0,  0, 0);
const AutomaticChunkOptions aco_auto_dynamicRounds_gradualTol_precisionLvl3           (true, 0, 0.0, 0.0, 80000, 10000, 36, 0, 2,  0, 0);
const AutomaticChunkOptions aco_auto_dynamicRounds_gradualTol_precisionLvl2           (true, 0, 0.0, 0.0, 70000, 10000, 26, 0, 5,  0, 0);
const AutomaticChunkOptions aco_auto_dynamicRounds_gradualTol_precisionLvl1           (true, 0, 0.0, 0.0, 60000, 10000, 18, 0, 7,  0, 0);	//aco_default: the default for 'run' when chunk=0
const AutomaticChunkOptions aco_auto_fixedRounds_gradualTol_precisionLvl4             (true, 0, 0.0, 0.0, 80000, 10000, 46, 1, 14, 0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_gradualTol_precisionLvl3             (true, 0, 0.0, 0.0, 60000, 10000, 36, 1, 6,  0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_gradualTol_precisionLvl2             (true, 0, 0.0, 0.0, 50000, 10000, 26, 1, 2,  0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_gradualTol_precisionLvl1             (true, 0, 0.0, 0.0, 40000, 10000, 18, 1, 1,  0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_fixedTol_precisionLvl4               (true, 0, 0.0, 0.0, 80000, 80000, 46, 2, 14, 0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_fixedTol_precisionLvl3               (true, 0, 0.0, 0.0, 60000, 60000, 36, 2, 6,  0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_fixedTol_precisionLvl2               (true, 0, 0.0, 0.0, 50000, 50000, 26, 2, 2,  0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_fixedTol_precisionLvl1               (true, 0, 0.0, 0.0, 40000, 40000, 18, 2, 1,  0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_fixedTolAndDiscardWorst_precisionLvl4(true, 0, 0.0, 0.0, 80000, 80000, 46, 3, 14, 0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_fixedTolAndDiscardWorst_precisionLvl3(true, 0, 0.0, 0.0, 60000, 60000, 36, 3, 8,  0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_fixedTolAndDiscardWorst_precisionLvl2(true, 0, 0.0, 0.0, 50000, 50000, 26, 3, 3,  0, 0);
const AutomaticChunkOptions aco_auto_fixedRounds_fixedTolAndDiscardWorst_precisionLvl1(true, 0, 0.0, 0.0, 40000, 40000, 18, 3, 2,  0, 0);

// Default presets
static const AutomaticChunkOptions& aco_default = dpr::aco_auto_dynamicRounds_gradualTol_precisionLvl1;
static const AutomaticChunkOptions& aco_test_default = dpr::aco_dynamicRounds_gradualTol_precisionLvl4;

namespace internal {

// Helper class that stores information about the results of a chunk test
struct chunk_attr {
	std::vector<double> speedArr;		// Vector with the speeds reached for each test
	std::vector<int> scoreArr;			// Vector with the score calculated for each test

	chunk_attr()
	{ }

	chunk_attr(std::vector<double> _speedArr, std::vector<int> _scoreArr) : speedArr(_speedArr), scoreArr(_scoreArr)
	{ }

};

// Main class for managing chunk tests
class ChunkSelector {

public:
	const std::vector<int> initial_mandatory_chunks{ 1, 2, 4, 8, 20 };	// the initial base chunk tests

	const int numToleranceLevels = 4;		// number of test levels (only for mode==0)
	const int nLevelTestsPresets[8][4] = {	// test level presets (only for mode==0) (selected with subMode)
		{2, 4, 6, 16},  //slow
		{2, 4, 6, 8},   //slow+
		{1, 2, 3, 8},   //eq
		{1, 2, 3, 4},   //eq+
		{1, 1, 2, 8},   //fast
		{1, 1, 2, 4},   //fast+
		{1, 1, 1, 4},   //veryfast
		{1, 1, 1, 2}    //veryfast+
	};
	const double availPercTimePhase[3] = { 0.220, 0.460, 0.720 };	// 22% 24% 26% 28% - test level percent time of maxTime available (accumulative)

	const double maxTime;				// max time allowed to do the tests (unlimited if maxTime<=0.0)
	const bool activeTimelimit;			// false if we have unlimited time to do the tests, true otherwise
	const int initTolerance;			// initial level of tolerance
	const int finalTolerance;			// final expected level of tolerance (only for mode==0)
	const int mode;						// 0 -> dynamic number of rounds (decrease tolerance gradually), 1 -> fixed number of rounds (decrease tolerance gradually), 2 -> fixed number of rounds (fixed tolerance), 3 -> fixed number of rounds (fixed tolerance) with discard of the worst chunk every round
	const int subMode;					// if mode!=0 this var is the number of rounds. if mode==0 this var is the levelTestPreset choice (slow, slow+, eq, eq+, fast, fast+, fast++)
	const int calcMode;					// how calculate score: 0 -> my_mean_score/global_best_mean_time, 1 -> my_median_score/global_best_median_time, 2 -> my_best_score/global_best_score
	const unsigned int maxNumChunksTestAllowed;	// max allowed number of chunks to be tested
	const int verbose;					// verbose level of information to stdout
	const int defaultChunk;				// default chunk to add to initial chunk list
	const std::chrono::steady_clock::time_point initTimePoint;	// chunkSelector init timestamp

	std::map<int,chunk_attr> selectedChunks;	// map of currently eligible chunks
	std::set<int> historyOfChunks;				// history of all chunks tested at least once
	std::queue<int> toCheckChunks;				// queue of chunks to test

private:
	int tolerance;		// current tolerance
	int phase = 1;		// current phase (phase 1 -> find eligible neighbors, phase 2 -> do more tests rounds and gradually discard the worst tests)

	double bestSpeedGlobal = -1;		// the best score obtained in any of the tests
	int bestSpeedGlobalChunk = -1;		// the chunk with the best score obtained in any of the tests

	const std::ios::fmtflags cout_ori_flags;	// a copy of the original cout flags to restore them in the class destructor

	// flow control variables
	double phase1Time = 0.0;
	double phase2MaxTime = 0.0;
	int phase2I = 0;
	int phase2J = 0;
	int actualBlock = -1;

public:
	ChunkSelector(const double _maxTime = 0.0, const int _initTolerance = 90000, const int _finalTolerance = 10000, const unsigned int _maxNumChunksTestAllowed = 40, const int _mode = 0, const int _subMode = 0, const int _calcMode = 0, const int _verbose = 0, const int _defaultChunk = 0, const std::chrono::steady_clock::time_point _initTimePoint = getTimepoint()) :
		maxTime(_maxTime),
		activeTimelimit(maxTime >= 0.0000001),
		initTolerance((_initTolerance > 0) ? _initTolerance : 1),
		finalTolerance((_finalTolerance <= initTolerance) ? _finalTolerance : initTolerance),
		mode((_mode < 4) ? ((_mode >= 0) ? _mode : 0) : 3),
		subMode((mode == 0) ? ((_subMode < 8) ? ((_subMode >= 0) ? _subMode : 0 )  : 7 ) : ((_subMode >= 0) ? _subMode : 0 )),
		calcMode((_calcMode < 3) ? ((_calcMode >= 0) ? _calcMode : 0) : 2),
		maxNumChunksTestAllowed((_maxNumChunksTestAllowed > 0) ? ((_maxNumChunksTestAllowed < initial_mandatory_chunks.size()) ? initial_mandatory_chunks.size() : _maxNumChunksTestAllowed) : 50),
		verbose(_verbose),
		defaultChunk(_defaultChunk),
		initTimePoint(_initTimePoint),
		tolerance(initTolerance),
		cout_ori_flags(std::cout.flags())
	{
		for (int i : initial_mandatory_chunks) {
			selectedChunks[i] = chunk_attr();
		}

		if ((defaultChunk > 0) && (selectedChunks.find(defaultChunk) == selectedChunks.end())) {
			selectedChunks[defaultChunk] = chunk_attr();
		}
		for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
			toCheckChunks.push(iChunk.first);
			historyOfChunks.insert(iChunk.first);
		}
		if (verbose > 0) {
			std::cout << std::fixed << std::setprecision(4);
			std::cout << "[+] Start Chunk Tests" << std::endl;
		}
		if (verbose > 1) {
			std::cout << "[+] Start Phase 1" << std::endl;
		}
	}

	ChunkSelector(AutomaticChunkOptions opt, const int _defaultChunk = 0) : ChunkSelector(opt.maxTime, opt.initTolerance, opt.finalTolerance, opt.maxNumChunksTestAllowed, opt.mode, opt.subMode, opt.calcMode, opt.verbose, _defaultChunk)
	{ }

	~ChunkSelector() {
		std::cout.flags(cout_ori_flags);
	}

	// Heuristic function that intends to automatically calculate the needed estimated time per test (which will determine the size of each test)
	double estimateNeededTimePerTest() {
		const int expectedP1Tests = ((maxNumChunksTestAllowed < 12) ? maxNumChunksTestAllowed : 12);	// We can not know how many chunks result from phase 1, so we estimate an average of 12 (if the maxNumChunkTestAllowed allow it)
		double tmpPerTestRes = 2.0;
		if (activeTimelimit) {
			// Time available limited. We estimate the number of total tests expected and calculate the time available for each one
			//TODO: pequenos errores nos estimadores que non estan tendo en conta os 12 test de fase 1 pa alguns dos modos, investigar...
			if (mode == 0) {
			    double nn = 2.0;
			    for (int i = 0; i < numToleranceLevels; ++i) {
					if (i < (numToleranceLevels-1)) {
						nn += static_cast<double>(nLevelTestsPresets[subMode][i])/static_cast<double>(i+2);
					} else {
						nn += static_cast<double>(nLevelTestsPresets[subMode][i]-1)/static_cast<double>(i+2);
					}
				}
				const double n = std::round(static_cast<double>(expectedP1Tests)*nn);
			    tmpPerTestRes = maxTime / n;
			} else if (mode == 1) {
				const double n = std::round(static_cast<double>(expectedP1Tests * (subMode + 2)) / 2.0);
				tmpPerTestRes = maxTime / n;
			} else if (mode == 2) {
				tmpPerTestRes = maxTime / static_cast<double>(expectedP1Tests * (subMode+1));
			} else if (mode == 3) {
				int t;
				int n;
				if ((subMode+1) < expectedP1Tests) {
					t = expectedP1Tests;
					n = (subMode+1);
				} else {
					t = expectedP1Tests;
					n = expectedP1Tests-1;
				}
				tmpPerTestRes = maxTime / static_cast<double>((n-n*n)/2+t*n);
			}
		} else {
			// No time limit, so we simply set fixed times
			if (mode == 0) {
				switch(subMode) {
				case 0:
				case 1:
					tmpPerTestRes = 4.0;
					break;
				case 2:
				case 3:
					tmpPerTestRes = 2.0;
					break;
				case 4:
				case 5:
					tmpPerTestRes = 1.5;
					break;
				case 6:
				case 7:
					tmpPerTestRes = 1.0;
					break;
				default:
					tmpPerTestRes = 2.0;
				}
			} else if (mode == 1) {
				tmpPerTestRes = 2.0;
			} else if (mode == 2) {
				tmpPerTestRes = 2.0;
			} else if (mode == 3) {
				tmpPerTestRes = 2.0;
			}
		}
		return tmpPerTestRes;
	}

	// Function that returns the next chunk to be tested. It also performs all flow control (calls to the calculation of scores, discarding of chunks, ...)
	// Returns -1 when the tests are finished
	int getNextChunk() {
		switch(mode) {
		case 3:
			// 3 -> fixed  number of rounds (fixed tolerance) with discard of the worst chunk every round
			return getNextChunk_m1_m2_m3(3);
		case 2:
			// 2 -> fixed number of rounds (fixed tolerance)
			return getNextChunk_m1_m2_m3(2);
		case 1:
			// 1 -> fixed number of rounds (decrease tolerance gradually)
			return getNextChunk_m1_m2_m3(1);
		default:
			// 0 -> heuristic number of rounds (decrease tolerance gradually)
			return getNextChunk_m0();
		}
	}

	// Function that saves the speed result of a test for a certain chunk
	void setChunkSpeed(int chunk, double speed) {
		selectedChunks[chunk].speedArr.push_back(speed);
		if (speed > bestSpeedGlobal) {
			bestSpeedGlobal = speed;
			bestSpeedGlobalChunk = chunk;
		}
	}

	// Function that returns the current tolerance
	inline int getCurrentTolerance() {
		return tolerance;
	}

	// Function that returns the current tolerance in double percent format
	inline double getCurrentPercTolerance() {
		return static_cast<double>(tolerance) / 10000.0;
	}

	// Function that returns the current phase state
	inline int getCurrentPhase() {
		return phase;
	}

	// Functions that returns the worst scored chunk and its score
	std::pair<int, int> getWorstScoredChunk() {
		int worstScore = std::numeric_limits<int>::max();
		int worstChunk = -1;
		for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
			if ((iChunk.second.scoreArr.size() > 0) && (iChunk.second.scoreArr.back() < worstScore)) {
				worstChunk = iChunk.first;
				worstScore = iChunk.second.scoreArr.back();
			}
		}
		return std::pair<int,int>{worstChunk, worstScore};
	}

	// Function that returns the best scored chunk and its score
	std::pair<int, int> getBestScoredChunk() {
		int bestChunk = -1;
		int bestScore = -1;
		for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
			if ((iChunk.second.scoreArr.size() > 0) && (iChunk.second.scoreArr.back() > bestScore)) {
				bestChunk = iChunk.first;
				bestScore = iChunk.second.scoreArr.back();
			}
		}
		return std::pair<int,int>{bestChunk, bestScore};
	}

	// Function that returns the chunk with the best mean speed and its mean speed
	std::pair<int,double> getBestMeanSpeedChunk() {
		int bestChunk = -1;
		double bestMeanSpeed = -1;
		for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
			const double meanSpeed = calculateMean(iChunk.second.speedArr);
			if (meanSpeed > bestMeanSpeed) {
				bestChunk = iChunk.first;
				bestMeanSpeed = meanSpeed;
			}
		}
		return std::pair<int,double>{bestChunk, bestMeanSpeed};
	}

	// Function that returns the chunk with the best median speed and its median speed
	std::pair<int,double> getBestMedianSpeedChunk() {
		int bestChunk = -1;
		double bestMedianSpeed = -1;
		for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
			const double medianSpeed = calculateMedian(iChunk.second.speedArr);
			if (medianSpeed > bestMedianSpeed) {
				bestChunk = iChunk.first;
				bestMedianSpeed = medianSpeed;
			}
		}
		return std::pair<int,double>{bestChunk, bestMedianSpeed};
	}

	// Function that returns the fragment with the best speed of all the tests of all the chunks and this speed
	std::pair<int,double> getBestSingleSpeedChunk() {
		int bestChunk = -1;
		double bestSpeed = -1;
		for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
			for (const double &speed : iChunk.second.speedArr) {
				if (speed > bestSpeed) {
					bestChunk = iChunk.first;
					bestSpeed = speed;
				}
			}
		}
		return std::pair<int,double>{bestChunk, bestSpeed};
	}

	// Function that returns a ordered vector of eligible chunks with their associated score
	std::vector<dpr::ResultChunkTest> getChunkScores() {
		std::vector<dpr::ResultChunkTest> res;
		for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
			if (iChunk.second.scoreArr.size() > 0) {
				double criteriaSpeed = 0.0;
				switch(calcMode) {
				case 2:
					criteriaSpeed = *(std::max_element(iChunk.second.speedArr.begin(), iChunk.second.speedArr.end()));;
					break;
				case 1:
					criteriaSpeed = calculateMedian(iChunk.second.speedArr);;
					break;
				default:
					criteriaSpeed = calculateMean(iChunk.second.speedArr);;
				}
				res.push_back(dpr::ResultChunkTest(iChunk.first, iChunk.second.scoreArr.back(), iChunk.second.scoreArr.size(), criteriaSpeed, iChunk.second.speedArr));
			}
		}
		std::sort(res.begin(), res.end(), [](dpr::ResultChunkTest const& a, dpr::ResultChunkTest const& b) { return a.score > b.score; });
		return res;
	}

	// Function that prints a score table with the currently eligible chunks
	void print_scores() {
		const auto bestChunk = getBestScoredChunk();
		std::cout << "  \t---------------------------------------- Score Table ----------------------------------------" << std::endl;
		std::cout << "  \tChunk\tScore\tScore Diff\tBestSpeed\tMeanSpeed\tMedianSpeed\tNumber Tests" << std::endl;
		if (bestChunk.first > 0) {
			for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
				if ((iChunk.second.scoreArr.size() > 0) && (iChunk.second.speedArr.size() > 0)) {
					const double scoreDiffPer = 100.0 - static_cast<double>(iChunk.second.scoreArr.back()) / static_cast<double>(bestChunk.second) * 100;
					std::cout << "  \t" << iChunk.first << (iChunk.first == bestChunk.first ? "*" : "") << "\t" << iChunk.second.scoreArr.back() << "\t" << (scoreDiffPer < 10.0 ? " " : "") << (iChunk.first == bestChunk.first ? " " : "-") << scoreDiffPer << "%\t" << *(std::max_element(iChunk.second.speedArr.begin(), iChunk.second.speedArr.end())) << "\t" << calculateMean(iChunk.second.speedArr) << "\t" << calculateMedian(iChunk.second.speedArr) << "\t" << iChunk.second.scoreArr.size() << std::endl;
				}
			}
		}
		std::cout << "  \t---------------------------------------------------------------------------------------------" << std::endl;
	}

private:

	// Function that returns the tolerance associated with a certain level
	inline int getLevelTolerance(const int level) {
		if (level < 0) {
			return initTolerance;
		} else if (level >= numToleranceLevels) {
			return finalTolerance;
		} else {
			const double stepTol = static_cast<double>(initTolerance-finalTolerance)/static_cast<double>(numToleranceLevels);
			return initTolerance-stepTol*(level+1);
		}
	}

	// Function that returns the tolerance associated with a certain level with custom maxLevel
	inline int getLevelTolerance(const int level, const int maxLevel) {
		if (level < 0) {
			return initTolerance;
		} else if (level >= maxLevel) {
			return finalTolerance;
		} else {
			const double stepTol = static_cast<double>(initTolerance-finalTolerance)/static_cast<double>(maxLevel);
			return initTolerance-stepTol*(level+1);
		}
	}

	// Function that returns the next chunk to be tested. It also performs all flow control (calls to the calculation of scores, discarding of chunks, ...)
	// Returns -1 when the tests are finished
	// Specialization for mode=2 and mode=3
	//     mode = 3 -> fixed  number of rounds (fixed tolerance) with discard of the worst chunk every round
	//     mode = 2 -> fixed number of rounds (fixed tolerance)
	//     mode = 1 -> fixed number of rounds (decrease tolerance gradually)
	int getNextChunk_m1_m2_m3(const int m) {
		int r = -1;
		double nowTime = 0.0;
		if (activeTimelimit) {
			nowTime = getTimediff(initTimePoint, getTimepoint());
		}
		// Check Timeout
		if (!activeTimelimit || (nowTime < maxTime)) {
			if (phase == 1) {
				// Phase 1: first tests and search for chunk neighbors of eligible chunks
				if (toCheckChunks.size() > 0) {
					r = toCheckChunks.front();
					toCheckChunks.pop();
				} else {
					calculateScore();
					discardChunks();
					addNeighbors();
					if (toCheckChunks.size() > 0) {
						r = toCheckChunks.front();
						toCheckChunks.pop();
					} else {
						++phase;
						if (verbose > 1) {
							phase1Time = getTimediff(initTimePoint, getTimepoint());
							std::cout << "[+] End Phase 1. Phase time: " << phase1Time << " sec" << std::endl;
							std::cout << "[+] Start Phase 2" << std::endl;
						}
					}
				}
			}
			if (phase == 2) {
				// Phase 2: iterative retests of eligible chunks
				if (toCheckChunks.size() > 0) {
					r = toCheckChunks.front();
					toCheckChunks.pop();
				} else  {
					if (m == 3) {
						calculateScore();
						if (verbose > 4) {
							print_scores();
						}
						discardWorstChunk();
					} else if (m == 1) {
						calculateScore();
						if (verbose > 4) {
							print_scores();
						}
						discardChunks();
						tolerance = getLevelTolerance(phase2I, subMode);
					} else {
						calculateScore();
						if (verbose > 4) {
							print_scores();
						}
					}
					if (selectedChunks.size() > 1) {
						if (phase2I < subMode) {
							if (verbose > 2) {
								std::cout << " [-] Round " << (phase2I+1) << "/" << subMode;
								if (m == 1) {
									std::cout << std::endl;
								} else {
									std::cout << ". Tolerance: " << getCurrentPercTolerance() << "%" << std::endl;
								}
							}
							for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
								toCheckChunks.push(iChunk.first);
							}
							if (toCheckChunks.size() > 0) {
								r = toCheckChunks.front();
								toCheckChunks.pop();
							}
							phase2I++;
						}
					}
				}
			}
		} else {
			// Timeout reached: calculate the score and discard the worst chunk (only for phase 2 and mode == 2)
			if (verbose > 1) {
				std::cout << " [!] Timeout of " << maxTime << " sec reached" << std::endl;
			}
			calculateScore();
			if ((m == 3) && (phase > 1)) {
				discardWorstChunk();
			} else if (m == 1) {
				tolerance = finalTolerance;
			}
		}
		if (r < 0) {
			// With test finalization we recalculate the score for the last time
			calculateScore();
			if (m == 1) {
				discardChunks();
			}
			if (verbose > 0) {
				nowTime = getTimediff(initTimePoint, getTimepoint());
				if (verbose > 1) {
					if (phase > 1) {
						std::cout << "[+] End Phase 2. Phase time: " << (nowTime - phase1Time) << " sec" << std::endl;
					} else {
						phase1Time = nowTime;
						std::cout << "[+] End Phase 1. Phase time: " << phase1Time << " sec" << std::endl;
					}
				}
				std::cout << "[+] End Chunk Tests. Total time: " << nowTime << " sec" << std::endl;
				if (verbose > 2) {
					print_scores();
				}
			}
		}
		return r;
	}

	// Function that returns the next chunk to be tested. It also performs all flow control (calls to the calculation of scores, discarding of chunks, ...)
	// Returns -1 when the tests are finished
	// Specialization for mode = 0
	//     mode = 0 -> heuristic number of rounds (decrease tolerance gradually)
	int getNextChunk_m0() {
		int r = -1;
		double nowTime = 0.0;
		if (activeTimelimit) {
			nowTime = getTimediff(initTimePoint, getTimepoint());
		}
		// Check Timeout
		if (!activeTimelimit || (nowTime < maxTime)) {
			if (phase == 1) {
				// Phase 1: first tests and search for chunk neighbors of eligible chunks
				if (toCheckChunks.size() > 0) {
					r = toCheckChunks.front();
					toCheckChunks.pop();
				} else {
					calculateScore();
					discardChunks();
					addNeighbors();
					if (toCheckChunks.size() > 0) {
						r = toCheckChunks.front();
						toCheckChunks.pop();
					} else {
						++phase;
						if (activeTimelimit || (verbose > 2)) {
							phase1Time = getTimediff(initTimePoint, getTimepoint());
						}
						if (activeTimelimit) {
							phase2MaxTime = maxTime - phase1Time;
						}
						if (verbose > 1) {
							std::cout << "[+] End Phase 1. Phase time: " << phase1Time << " sec" << std::endl;
							if (activeTimelimit) {
								std::cout << "[+] Start Phase 2. Available time: " << phase2MaxTime << " sec" << std::endl;
							} else {
								std::cout << "[+] Start Phase 2" << std::endl;
							}
						}
					}
				}
			}
			if (phase == 2) {
				// Phase 2: iterative retests of eligible chunks
				// Check phase2-levels partial timeouts
				while((activeTimelimit) && ((actualBlock >= 0) && (actualBlock < (numToleranceLevels-1))) && ((nowTime-phase1Time) > phase2MaxTime*availPercTimePhase[actualBlock])) {
					int whoNext = -1;
					if (toCheckChunks.size() > 0) {
						whoNext = toCheckChunks.front();
					}
					if (verbose > 2) {
						std::cout << " [!] Round " << (actualBlock+1) << " timeout of " << phase2MaxTime*availPercTimePhase[actualBlock] << " sec reached (of " << phase2MaxTime << " sec available time in phase 2). Skipping to next round..." << std::endl;
					}
					calculateScore();
					if (verbose > 4) {
						print_scores();
					}
					discardChunks();
					std::queue<int>().swap(toCheckChunks); 	//toCheckChunks = {};
					phase2J = 1;
					actualBlock++;
					phase2I = actualBlock;
					tolerance = getLevelTolerance(phase2I);
					if (verbose > 2) {
						std::cout << " [-] Round " << (phase2I+1) << "/" << numToleranceLevels << ". Iteration " << (phase2J) << "/" << nLevelTestsPresets[subMode][phase2I] << ". Tolerance: " << getCurrentPercTolerance() << "%" << std::endl;
					}
					if (selectedChunks.size() > 1) {
						if (phase2I < numToleranceLevels) {
							for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
								if (iChunk.first >= whoNext) {
									toCheckChunks.push(iChunk.first);
								}
							}
							for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
								if (iChunk.first < whoNext) {
									toCheckChunks.push(iChunk.first);
								}
							}
						}
					}
				}
				if (toCheckChunks.size() > 0) {
					r = toCheckChunks.front();
					toCheckChunks.pop();
				} else  {
					calculateScore();
					if (verbose > 4) {
						print_scores();
					}
					discardChunks();
					if (selectedChunks.size() > 1) {
						if (phase2I < numToleranceLevels) {
							tolerance = getLevelTolerance(phase2I);
							if (verbose > 2) {
								std::cout << " [-] Round " << (phase2I+1) << "/" << numToleranceLevels << ". Iteration " << (phase2J+1) << "/" << nLevelTestsPresets[subMode][phase2I] << ". Tolerance: " << getCurrentPercTolerance() << "%" << std::endl;
							}
							int whoNext = -1;
							size_t sizeNext =  std::numeric_limits<size_t>::max();
							for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
								const size_t speedArrChunkSize = iChunk.second.speedArr.size();
								if (sizeNext > speedArrChunkSize) {
									sizeNext = speedArrChunkSize;
									whoNext = iChunk.first;
								}
							}
							for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
								if (iChunk.first >= whoNext) {
									toCheckChunks.push(iChunk.first);
								}
							}
							for (const std::pair<int, chunk_attr> &iChunk : selectedChunks) {
								if (iChunk.first < whoNext) {
									toCheckChunks.push(iChunk.first);
								}
							}
							if (toCheckChunks.size() > 0) {
								r = toCheckChunks.front();
								toCheckChunks.pop();
							}
							actualBlock = phase2I;
							phase2J++;
							if (phase2J >= nLevelTestsPresets[subMode][phase2I]) {
								phase2J = 0;
								phase2I++;
							}
						}
					}
				}
			}
		} else {
			if (verbose > 1) {
				std::cout << " [!] Timeout of " << maxTime << " sec reached" << std::endl;
			}
			tolerance = finalTolerance;
		}
		if (r < 0) {
			calculateScore();
			discardChunks();
			if (verbose > 0) {
				nowTime = getTimediff(initTimePoint, getTimepoint());
				if (verbose > 1) {
					if (phase > 1) {
						std::cout << "[+] End Phase 2. Phase time: " << (nowTime - phase1Time) << " sec" << std::endl;
					} else {
						phase1Time = nowTime;
						std::cout << "[+] End Phase 1. Phase time: " << phase1Time << " sec" << std::endl;
					}
				}
				std::cout << "[+] End Chunk Tests. Total time: " << nowTime << " sec" << std::endl;
				if (verbose > 2) {
					print_scores();
				}
			}
		}
		return r;
	}

	// Function that calculates score of the last test round
	void calculateScore() {
		switch(calcMode) {
		case 2:
			// 2 -> my_best_score/global_best_score
			calculateScore_m2();
			break;
		case 1:
			// 1 -> my_median_score/global_best_median_time
			calculateScore_m1();
			break;
		default:
			// 0 -> my_mean_score/global_best_mean_time
			calculateScore_m0();
		}
	}

	// Function that calculates score of the last test round (my_mean_score/global_best_mean_score)
	void calculateScore_m0() {
		const std::pair<int,double> bestMeanSpeedChunk = getBestMeanSpeedChunk();
		if (bestMeanSpeedChunk.first >= 0) {
			for (std::map<int,chunk_attr>::iterator itChunk = selectedChunks.begin(); itChunk != selectedChunks.end(); ++itChunk) {
				bool firstItChunkCalc = (itChunk->second.speedArr.size() > 0);
				for(size_t i = 0; (firstItChunkCalc || (i < (itChunk->second.speedArr.size()-itChunk->second.scoreArr.size()))); ++i) {
					firstItChunkCalc = false;
					const double meanSpeed = calculateMean(itChunk->second.speedArr);
					const int newScore = static_cast<int>(std::round(meanSpeed / bestMeanSpeedChunk.second * 1000000.0));
					if (itChunk->second.scoreArr.size() < itChunk->second.speedArr.size()) {
						itChunk->second.scoreArr.push_back(newScore);
					} else {
						itChunk->second.scoreArr.back() = newScore;
					}
				}
			}
		}
	}

	// Function that calculates score of the last test round (my_median_score/global_best_median_score)
	void calculateScore_m1() {
		const std::pair<int,double> bestMedianSpeedChunk = getBestMedianSpeedChunk();
		if (bestMedianSpeedChunk.first >= 0) {
			for (std::map<int,chunk_attr>::iterator itChunk = selectedChunks.begin(); itChunk != selectedChunks.end(); ++itChunk) {
				bool firstItChunkCalc = (itChunk->second.speedArr.size() > 0);
				for(size_t i = 0; (firstItChunkCalc || (i < (itChunk->second.speedArr.size()-itChunk->second.scoreArr.size()))); ++i) {
					firstItChunkCalc = false;
					const double medianSpeed = calculateMedian(itChunk->second.speedArr);
					const int newScore = static_cast<int>(std::round(medianSpeed / bestMedianSpeedChunk.second * 1000000.0));
					if (itChunk->second.scoreArr.size() < itChunk->second.speedArr.size()) {
						itChunk->second.scoreArr.push_back(newScore);
					} else {
						itChunk->second.scoreArr.back() = newScore;
					}
				}
			}
		}
	}

	// Function that calculates score of the last test round (my_best_score/global_best_score)
	void calculateScore_m2() {
		for (std::map<int,chunk_attr>::iterator itChunk = selectedChunks.begin(); itChunk != selectedChunks.end(); ++itChunk) {
			bool firstItChunkCalc = (itChunk->second.speedArr.size() > 0);
			for(size_t i = 0; (firstItChunkCalc || (i < (itChunk->second.speedArr.size()-itChunk->second.scoreArr.size()))); ++i) {
				firstItChunkCalc = false;
				const double bestChunkSpeed = *(std::max_element(itChunk->second.speedArr.begin(), itChunk->second.speedArr.end()));
				const int newScore = static_cast<int>(std::round(bestChunkSpeed / bestSpeedGlobal * 1000000.0));
				if (itChunk->second.scoreArr.size() < itChunk->second.speedArr.size()) {
					itChunk->second.scoreArr.push_back(newScore);
				} else {
					itChunk->second.scoreArr.back() = newScore;
				}
			}
		}
	}

	// Function that discards the chunk with the worst score
	void discardWorstChunk() {
		if (selectedChunks.size() > 1) {
			int worstChunk = getWorstScoredChunk().first;
			if (worstChunk > 0) {
				selectedChunks.erase(worstChunk);
				if (verbose > 2) {
					std::cout << "  - Discard the chunk with the worst score: { " << worstChunk << " }" << std::endl;
				}
			}
		}
	}

	// Function that discards chunks with a worse score than the tolerance range
	void discardChunks() {
		std::pair<int,int> best_chunk_atm = getBestScoredChunk();
		std::vector<std::pair<int,int>> del_history_vec;
		if (best_chunk_atm.first > 0) {
			for (std::map<int,chunk_attr>::iterator it = selectedChunks.begin(); it != selectedChunks.end();) {
				if ((it->first != best_chunk_atm.first) && (it->second.scoreArr.size() > 0)) {
					const int scoreRel = static_cast<int>(static_cast<double>(it->second.scoreArr.back())/static_cast<double>(best_chunk_atm.second)*1000000.0);
					if (scoreRel < (1000000-tolerance)) {
						del_history_vec.push_back(std::pair<int,int>(it->first,1000000-scoreRel));
						it = selectedChunks.erase(it);
						continue;
					}
				}
				++it;
			}
		}
		if (verbose > 2) {
			if (del_history_vec.size() > 0) {
				std::cout << "  - Discarded the " << (del_history_vec.size() > 1 ? "chunks" : "chunk") << " { " << del_history_vec[0].first << " [" << (static_cast<double>(del_history_vec[0].second) / 10000.0) << "%]";
				for (size_t i = 1; i < del_history_vec.size(); ++i) {
					std::cout << ", " << del_history_vec[i].first << " [" << (static_cast<double>(del_history_vec[i].second) / 10000.0) << "%]";
				}
				std::cout << " } using a tolerance of " << getCurrentPercTolerance() << "%" << std::endl;
			} else {
				std::cout << "  - No chunks found to discard using a tolerance of " << getCurrentPercTolerance() << "%" << std::endl;
			}
		}
	}

	// Function to add chunk neighbors of eligible chunks (if they have not already been added previously)
	void addNeighbors() {
		if (historyOfChunks.size() < maxNumChunksTestAllowed) {
			std::set<int> toAdd;
			if (verbose > 2) {
				if (selectedChunks.size() > 0) {
					std::cout << "  - Searching for unexplored neighbors of the " << (selectedChunks.size() > 1 ? "chunks" : "chunk") << " { " << selectedChunks.begin()->first;
					for (std::map<int,chunk_attr>::iterator itChunk = ++(selectedChunks.begin()); itChunk != selectedChunks.end(); ++itChunk) {
						std::cout << ", " << itChunk->first;
					}
					std::cout << " }" << std::endl;
				}
			}
			for (const std::pair<int, chunk_attr> iChunk : selectedChunks) {
				if ((iChunk.second.scoreArr.size() > 0) && (iChunk.second.scoreArr.back() >= 0)) {
					const int chunk = iChunk.first;
					if (chunk > 1) {
						std::set<int>::iterator it;
						it = std::find(historyOfChunks.begin(), historyOfChunks.end(), chunk-1);
						if (it == historyOfChunks.end()) {
							toAdd.insert(chunk-1);
						}
						it = std::find(historyOfChunks.begin(), historyOfChunks.end(), chunk+1);
						if (it == historyOfChunks.end()) {
							toAdd.insert(chunk+1);
						}
					} else if (chunk == 1) {
						std::set<int>::iterator it = std::find(historyOfChunks.begin(), historyOfChunks.end(), chunk+1);
						if ((it == historyOfChunks.end())) {
							toAdd.insert(chunk+1);
						}
					}
				}
			}

			if (toAdd.size() > 0) {
				std::vector<int> finallyAdded;
				std::set<int> noAdded = toAdd;
				for (const int iToAdd : toAdd) {
					if (historyOfChunks.size() < maxNumChunksTestAllowed) {
						selectedChunks[iToAdd] = chunk_attr();
						historyOfChunks.insert(iToAdd);
						toCheckChunks.push(iToAdd);
						finallyAdded.push_back(iToAdd);
						noAdded.erase(iToAdd);
					} else {
						if (verbose > 2) {
							std::cout << "  - No new neighbors are added because the limit of chunks to test has been reached: " << maxNumChunksTestAllowed << std::endl;
						}
						break;
					}
				}
				if (verbose > 2) {
					if (finallyAdded.size() > 0) {
						std::cout << "  - Added the neighboring " << (finallyAdded.size() > 1 ? "chunks" : "chunk") << " { " << finallyAdded[0];
						for (size_t i = 1; i < finallyAdded.size(); ++i) {
							std::cout << ", " << finallyAdded[i];
						}
						std::cout << " }" << std::endl;
						if (noAdded.size() > 0) {
							std::cout << "  - Some eligible neighbor chunks { " << *(noAdded.begin());
							for (std::set<int>::iterator itNoAdded = ++(noAdded.begin()); itNoAdded != noAdded.end(); ++itNoAdded) {
								std::cout << ", " << *itNoAdded;
							}
							std::cout << " } have not been added because the limit of chunks to test has been reached: " << maxNumChunksTestAllowed << std::endl;
						}
					}
				}
			} else {
				if (verbose > 2) {
					std::cout << "  - No eligible neighbors found" << std::endl;
				}
			}
		} else {
			if (verbose > 2) {
				std::cout << "  - No new neighbors are searched because the limit of chunks to test has been reached: " << maxNumChunksTestAllowed << std::endl;
			}
		}
	}

};

}

}

#endif /* DPR_CHUNKSELECTOR_H_ */

