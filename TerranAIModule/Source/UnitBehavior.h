#pragma once

#include "Shared.h"
#include "ResourceLogic.h"

//abstraction for a refinery that includes a list of workers mining from it
typedef struct Refinery_t {
	//the physical refinery
	BWAPI::Unit refinery;
	//workers mining from this refinery
	std::list<BWAPI::Unit> workers;
	//time until we next evaluate this refinery
	int gracePeriod;
} Refinery;

typedef struct Goal_t {
	//structure to be built for this goal
	BWAPI::UnitType structureType;
	//worker assigned to building the structure
	BWAPI::Unit assignee;
	//the physical structure once it starts construction
	BWAPI::Unit structure;
	/*time we'll wait after assigning a goal to a worker before we start
	checking that they're actually carrying out the goal */
	int gracePeriod;
} Goal;

extern bool evaluateWorkerLogicFor(BWAPI::Unit worker, int requiredSupplyDepots, int workerCount, resourceProjection unallocatedResources);
extern bool evaluateTownhallLogicFor(BWAPI::Unit townhall, int workerCount);
extern bool evaluateRefineryLogicFor(BWAPI::Unit refinery, int workerCount);
extern bool evaluateBarracksLogicFor(BWAPI::Unit barracks);
extern bool addGoal(BWAPI::UnitType structure);
extern std::deque<Goal> getGoals();
extern bool unitIsDisabled(BWAPI::Unit unit);