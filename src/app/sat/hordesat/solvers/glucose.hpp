/*
 * Lingeling.h
 *
 *  Created on: Nov 11, 2014
 *      Author: balyo
 */

#ifndef GLUCOSE_H_
#define GLUCOSE_H_

#include "portfolio_solver_interface.hpp"
using namespace Mallob;

#include "util/sys/threading.hpp"
#include "util/logger.hpp"

#include "simp/SimpSolver.h"

#include <map>

class MGlucose : Glucose::SimpSolver, public PortfolioSolverInterface {

private:
	std::string name;
	int stopSolver;
	LearnedClauseCallback learnedClauseCallback;
	Mutex clauseAddMutex;

	Glucose::vec<Glucose::Lit> clause;
	Glucose::vec<Glucose::Lit> assumptions;
	int maxvar = 0;
	int szfmap = 0;
	unsigned char * fmap = 0; 
	bool nomodel = true;
	unsigned long long calls = 0;
    
	// clause addition
	std::vector<std::vector<int> > learnedClausesToAdd;
	std::vector<int> unitsToAdd;
    
    volatile bool suspendSolver;
    Mutex suspendMutex;
    ConditionVariable suspendCond;

	int numDiversifications;
	unsigned int softMaxLbd;
	unsigned int hardMaxLbd;

	// Clause statistics
	unsigned long numReceived = 0;
	unsigned long numDigested = 0;
	unsigned long numDiscarded = 0;

public:
	MGlucose(const SolverSetup& setup);
	 ~MGlucose() override;

	// Add a (list of) permanent clause(s) to the formula
	void addLiteral(int lit) override;

	void diversify(int seed) override;
	void setPhase(const int var, const bool phase) override;

	// Solve the formula with a given set of assumptions
	SatResult solve(size_t numAssumptions, const int* assumptions) override;

	void setSolverInterrupt() override;
	void unsetSolverInterrupt() override;
    void setSolverSuspend() override;
    void unsetSolverSuspend() override;

	std::vector<int> getSolution() override;
	std::set<int> getFailedAssumptions() override;

	// Add a learned clause to the formula
	// The learned clauses might be added later or possibly never
	void addLearnedClause(const Clause& c) override;

	// Set a function that should be called for each learned clause
	void setLearnedClauseCallback(const LearnedClauseCallback& callback) override;

	// Request the solver to produce more clauses
	void increaseClauseProduction() override;
	
	// Get the number of variables of the formula
	int getVariablesCount() override;

	int getNumOriginalDiversifications() override;
	
	// Get a variable suitable for search splitting
	int getSplittingVariable() override;

	// Get solver statistics
	SolvingStatistics getStatistics() override;

	bool supportsIncrementalSat() override {return false;}
	bool exportsConditionalClauses() override {return true;}

private:
	Glucose::Lit encodeLit(int lit);
	int decodeLit(Glucose::Lit lit);
	void resetMaps();
	int solvedValue(int lit);
	bool failed(Glucose::Lit lit);
	void buildFailedMap();

	bool parallelJobIsFinished() override;

	void parallelImportUnaryClauses() override;
	bool parallelImportClauses() override; // true if the empty clause was received
	void parallelExportUnaryClause(Glucose::Lit p) override;
	void parallelExportClause(Glucose::Clause &c, bool fromConflictAnalysis);

	void parallelExportClauseDuringSearch(Glucose::Clause &c) override {
		parallelExportClause(c, false);
	}
	// This method is a misnomer, it should say EXport.
	inline void parallelImportClauseDuringConflictAnalysis(Glucose::Clause &c, Glucose::CRef confl) override {
		parallelExportClause(c, true);
	}
};

#endif /* LINGELING_H_ */
