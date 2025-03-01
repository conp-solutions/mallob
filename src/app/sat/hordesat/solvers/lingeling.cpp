/*
 * Lingeling.cpp
 *
 *  Created on: Nov 11, 2014
 *      Author: balyo
 */

#include <ctype.h>
#include <stdarg.h>
#include <chrono>
#include <string.h>

#include "lingeling.hpp"
#include "app/sat/hordesat/utilities/debug_utils.hpp"
#include "util/sys/timer.hpp"

extern "C" {
	#include "lglib.h"
}

/***************************** CALLBACKS *****************************/

int cbCheckTerminate(void* solverPtr) {
	Lingeling* lp = (Lingeling*)solverPtr;

	double elapsed = Timer::elapsedSeconds() - lp->lastTermCallbackTime;
	lp->lastTermCallbackTime = Timer::elapsedSeconds();
    
	if (lp->stopSolver) {
		lp->_logger.log(V3_VERB, "STOP (%.2fs since last cb)", elapsed);
		return 1;
	}

    if (lp->suspendSolver) {
        // Stay inside this function call as long as solver is suspended
		lp->_logger.log(V3_VERB, "SUSPEND (%.2fs since last cb)", elapsed);

		lp->suspendCond.wait(lp->suspendMutex, [&lp]{return !lp->suspendSolver;});
		lp->_logger.log(V4_VVER, "RESUME");

		if (lp->stopSolver) {
			lp->_logger.log(V4_VVER, "STOP after suspension", elapsed);
			return 1;
		}
    }
    
    return 0;
}
void cbProduceUnit(void* sp, int lit) {
	((Lingeling*)sp)->doProduceUnit(lit);
}
void cbProduce(void* sp, int* cls, int glue) {
	((Lingeling*)sp)->doProduce(cls, glue);
}
void cbConsumeUnits(void* sp, int** start, int** end) {
	((Lingeling*)sp)->doConsumeUnits(start, end);
}
void cbConsumeCls(void* sp, int** clause, int* glue) {
	((Lingeling*)sp)->doConsume(clause, glue);
}

/************************** END OF CALLBACKS **************************/



Lingeling::Lingeling(const SolverSetup& setup) 
	: PortfolioSolverInterface(setup),
		incremental(setup.doIncrementalSolving),
		learnedClauses(4*setup.anticipatedLitsToImportPerCycle), 
		learnedUnits(2*setup.anticipatedLitsToImportPerCycle + 1) {

	//_logger.log(V4_VVER, "Local buffer sizes: %ld, %ld\n", learnedClauses.getCapacity(), learnedUnits.getCapacity());

	solver = lglinit();
		
	// BCA has to be disabled for valid clause sharing (or freeze all literals)
	lglsetopt(solver, "bca", 0);
	
	lastTermCallbackTime = Timer::elapsedSeconds();

	stopSolver = 0;
	callback = NULL;

	lglsetime(solver, getTime);
	lglseterm(solver, cbCheckTerminate, this);
	glueLimit = _setup.softInitialMaxLbd;
	sizeLimit = _setup.softMaxClauseLength;

    suspendSolver = false;
    maxvar = 0;

	if (_setup.useAdditionalDiversification) {
		numDiversifications = 20;
	} else {
		numDiversifications = 14;
	}
}

void Lingeling::addLiteral(int lit) {
	
	if (lit == 0) {
		lgladd(solver, 0);
		return;
	}
	updateMaxVar(lit);
	lgladd(solver, lit);
}

void Lingeling::updateMaxVar(int lit) {
	lit = abs(lit);
	assert(lit <= 134217723); // lingeling internal literal limit
	if (!incremental) maxvar = std::max(maxvar, lit);
	else while (maxvar < lit) {
		maxvar++;
		// Freezing required for incremental solving only.
		// This loop ensures that each literal that is added
		// or assumed at some point is frozen exactly once.
		lglfreeze(solver, maxvar);
	}
}

void Lingeling::diversify(int seed) {
	
	lglsetopt(solver, "seed", seed);
	int rank = getDiversificationIndex();
	
	// This method is based on Plingeling: OLD from ayv, NEW from bcj

	lglsetopt(solver, "classify", 0); // NEW
	//lglsetopt(solver, "flipping", 0); // OLD

    switch (rank % numDiversifications) {
		
		// Default solver
		case 0: break;

		// Alternative default solver
		case 1: 
			lglsetopt (solver, "plain", 1);
			lglsetopt (solver, "decompose", 1); // NEW 
			break;

		// NEW
		case 2: lglsetopt (solver, "restartint", 1000); break;
		case 3: lglsetopt (solver, "elmresched", 7); break;
		
		// NEW: local search solver
		case 4:
			lglsetopt (solver, "plain", rank % (2*numDiversifications) < numDiversifications);
			lglsetopt (solver, "locs", -1);
			lglsetopt (solver, "locsrtc", 1);
			lglsetopt (solver, "locswait", 0);
			lglsetopt (solver, "locsclim", (1<<24));
			break;

		case 5: lglsetopt (solver, "scincincmin", 250); break;
		case 6: 
			lglsetopt (solver, "block", 0); 
			lglsetopt (solver, "cce", 0); 
			break;
		case 7: lglsetopt (solver, "scincinc", 50); break;
		case 8: lglsetopt (solver, "phase", -1); break;
		case 9: lglsetopt (solver, "phase", 1); break;
		case 10: lglsetopt (solver, "sweeprtc", 1); break;
		case 11: lglsetopt (solver, "restartint", 100); break;
		case 12:
			lglsetopt (solver, "reduceinit", 10000);
			lglsetopt (solver, "reducefixed", 1);
			break;
		case 13: lglsetopt (solver, "restartint", 4); break;

		// OLD
		case 14: lglsetopt (solver, "agilitylim", 100); break; // NEW from "agilelim"
		//case X: lglsetopt (solver, "bias", -1); break; // option removed
		//case X: lglsetopt (solver, "bias", 1); break; // option removed
		//case X: lglsetopt (solver, "activity", 1); break; // omitting; NEW from "acts"
		case 15: lglsetopt (solver, "activity", 2); break; // NEW from "acts", 0
		case 16:
			lglsetopt (solver, "wait", 0);
			lglsetopt (solver, "blkrtc", 1);
			lglsetopt (solver, "elmrtc", 1);
			break;
		case 17: lglsetopt (solver, "prbsimplertc", 1); break;
		//case X: lglsetopt (solver, "gluescale", 1); break; // omitting
		case 18: lglsetopt (solver, "gluescale", 5); break; // from 3 (value "ld" moved)
		case 19: lglsetopt (solver, "move", 1); break;

		default: break;
	}
}

// Set initial phase for a given variable
void Lingeling::setPhase(const int var, const bool phase) {
	lglsetphase(solver, phase ? var : -var);
}

// Solve the formula with a given set of assumptions
// return 10 for SAT, 20 for UNSAT, 0 for UNKNOWN
SatResult Lingeling::solve(size_t numAssumptions, const int* assumptions) {
	
	// set the assumptions
	this->assumptions.clear();
	for (size_t i = 0; i < numAssumptions; i++) {
		// freezing problems
		int lit = assumptions[i];
		updateMaxVar(lit);
		lglassume(solver, lit);
		this->assumptions.push_back(lit);
	}

	int res = lglsat(solver);
	
	switch (res) {
	case LGL_SATISFIABLE:
		return SAT;
	case LGL_UNSATISFIABLE:
		return UNSAT;
	}
	return UNKNOWN;
}

void Lingeling::setSolverInterrupt() {
	stopSolver = 1;
}
void Lingeling::unsetSolverInterrupt() {
	stopSolver = 0;
}
void Lingeling::setSolverSuspend() {
    suspendSolver = true;
}
void Lingeling::unsetSolverSuspend() {
    suspendSolver = false;
	suspendCond.notify();
}


void Lingeling::doProduceUnit(int lit) {
	Clause c{&lit, 1, 1};
	callback(c, getLocalId());
}

void Lingeling::doProduce(int* cls, int glue) {
	
	// unit clause
	if (cls[1] == 0) {
		doProduceUnit(cls[0]);
		return;
	}
	
	// LBD score check
	if (glueLimit != 0 && glue > (int)glueLimit) {
		return;
	}
	// size check
	int size = 0;
	unsigned int i = 0;
	while (cls[i++] != 0) size++;
	if (size > sizeLimit) return;
	
	// export clause
	callback(Clause{cls, size, glue}, getLocalId());
}

void Lingeling::doConsumeUnits(int** start, int** end) {

	// Get as many unit clauses as possible
	learnedUnitsBuffer.clear();
	bool success = true;
	while (success) success = learnedUnits.getUnits(learnedUnitsBuffer);
	
	*start = learnedUnitsBuffer.data();
	*end = learnedUnitsBuffer.data()+learnedUnitsBuffer.size();
	numDigested += learnedUnitsBuffer.size();
}

void Lingeling::doConsume(int** clause, int* glue) {
	*clause = nullptr;

	// Get a single (non-unit) clause
	learnedClausesBuffer.clear();
	bool success = learnedClauses.getClause(learnedClausesBuffer);
	if (!success) return;
	if (learnedClausesBuffer.empty()) return;

	//std::string str = "consume cls : ";
	for (size_t i = 0; i < learnedClausesBuffer.size(); i++) {
		int lit = learnedClausesBuffer[i];
		//str += std::to_string(lit) + " ";
		assert(i == 0 || std::abs(lit) <= maxvar);
	}
	//_logger.log(V4_VVER, "%s\n", str.c_str());

	*glue = learnedClausesBuffer[0];
	*clause = learnedClausesBuffer.data()+1;
	numDigested++;
}

void Lingeling::addLearnedClause(const Clause& c) {

	if (c.size == 1) {
		if (!learnedUnits.insertUnit(*c.begin)) {
			//_logger.log(V4_VVER, "Unit buffer full (recv=%i digs=%i)\n", numReceived, numDigested);
			numDiscarded++;
		}
	} else {
		if (!learnedClauses.insertClause(c)) {
			//_logger.log(V4_VVER, "Clause buffer full (recv=%i digs=%i)\n", numReceived, numDigested);
			numDiscarded++;
		}
	}
	numReceived++;

	//time = _logger.getTime() - time;
	//if (time > 0.2f) lp->_logger.log(-1, "[1] addLearnedClause took %.2fs! (size %i)\n", time, size);
}

void Lingeling::setLearnedClauseCallback(const LearnedClauseCallback& callback) {
	this->callback = callback;
	lglsetproducecls(solver, cbProduce, this);
	lglsetproduceunit(solver, cbProduceUnit, this);
	lglsetconsumeunits(solver, cbConsumeUnits, this);
	lglsetconsumecls(solver, cbConsumeCls, this);
}

void Lingeling::increaseClauseProduction() {
	if (glueLimit != 0 && glueLimit < _setup.softFinalMaxLbd) 
		glueLimit++;
}


std::vector<int> Lingeling::getSolution() {
	std::vector<int> result;
	result.push_back(0);
	for (int i = 1; i <= maxvar; i++) {
		if (lglderef(solver, i) > 0) {
			result.push_back(i);
		} else {
			result.push_back(-i);
		}
	}
	return result;
}

std::set<int> Lingeling::getFailedAssumptions() {
	std::set<int> result;
	for (size_t i = 0; i < assumptions.size(); i++) {
		if (lglfailed(solver, assumptions[i])) {
			result.insert(assumptions[i]);
		}
	}
	return result;
}

int Lingeling::getVariablesCount() {
	return maxvar;
}

int Lingeling::getNumOriginalDiversifications() {
	return numDiversifications;
}

// Get a variable suitable for search splitting
int Lingeling::getSplittingVariable() {
	return lglookahead(solver);
}

SolvingStatistics Lingeling::getStatistics() {
	SolvingStatistics st;
	st.conflicts = lglgetconfs(solver);
	st.decisions = lglgetdecs(solver);
	st.propagations = lglgetprops(solver);
	st.memPeak = lglmaxmb(solver);
	st.receivedClauses = numReceived;
	st.digestedClauses = numDigested;
	st.discardedClauses = numDiscarded;
	return st;
}

Lingeling::~Lingeling() {
	lglrelease(solver);
}
