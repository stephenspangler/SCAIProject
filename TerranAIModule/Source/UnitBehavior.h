#pragma once

#include "Shared.h"
#include "ResourceLogic.h"

namespace UnitBehavior {

	//wrapper for a refinery that includes a list of workers mining from it
	typedef struct Refinery_t {
		//the physical refinery
		BWAPI::Unit refinery;
		//workers mining from this refinery
		std::list<BWAPI::Unit> workers;
		//time until we next evaluate this refinery
		int gracePeriod;
	} Refinery;

	typedef struct Goal_t {
		//whether the goal is a technology as opposed to a structure
		bool isResearch;
		//structure to be built for this goal
		BWAPI::UnitType structureType;
		//worker assigned to building the structure
		BWAPI::Unit assignee;
		//the physical structure once it starts construction
		BWAPI::Unit structure;
		//research type if applicable
		BWAPI::TechType tech;
		/*Time we'll wait after assigning a goal to a worker before we start
		checking that they're actually carrying out the goal*/
		int gracePeriod;
	} Goal;

	extern void evaluateGoals();
	extern bool evaluateWorkerLogicFor(BWAPI::Unit worker, int requiredSupplyDepots, int workerCount);
	extern bool evaluateTownhallLogicFor(BWAPI::Unit townhall, int workerCount);
	extern bool evaluateRefineryLogicFor(BWAPI::Unit refinery, int workerCount);
	extern bool evaluateBarracksLogicFor(BWAPI::Unit barracks, bool includeFirebats, bool includeMedics);
	extern bool evaluateFactoryLogicFor(BWAPI::Unit factory);
	bool evaluateAbilityLogicFor(BWAPI::Unit unit);
	extern bool addGoal(BWAPI::UnitType structure, bool front = false);
	extern bool addGoal(Goal &goal, bool front = false);
	extern bool addGoal(BWAPI::TechType tech, bool front = false);
	extern std::deque<Goal> getGoals();

}